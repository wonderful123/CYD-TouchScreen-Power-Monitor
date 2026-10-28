// components/light_sleep_touch/light_sleep_touch.h
#pragma once

#include <cinttypes>
#include <utility>

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "driver/gpio.h"
#include "esp_sleep.h"

namespace esphome {
namespace light_sleep_touch {

static const char *const TAG = "light_sleep_touch";

class LightSleepTouchComponent : public Component {
 public:
  void set_touch_irq_pin(InternalGPIOPin *pin) {
    this->touch_irq_pin_ = pin;
  }

  void set_inactivity_timeout(uint32_t timeout_ms) {
    this->inactivity_timeout_ms_ = timeout_ms;
    this->reset_timer();
  }

  void set_sleep_duration(uint32_t duration_ms) {
    this->sleep_duration_ms_ = duration_ms;
  }

  esp_sleep_wakeup_cause_t get_last_wakeup_cause() const {
    return this->last_wakeup_cause_;
  }

  // True only for a GPIO wake that was confirmed as a sustained touch.
  bool is_touch_wakeup() const {
    return this->touch_wakeup_;
  }

  template <typename F>
  void add_on_before_sleep_callback(F &&callback) {
    this->before_sleep_callback_.add(std::forward<F>(callback));
  }

  template <typename F>
  void add_on_wake_callback(F &&callback) {
    this->wake_callback_.add(std::forward<F>(callback));
  }

  void setup() override {
    if (this->touch_irq_pin_ == nullptr) {
      ESP_LOGE(TAG, "Touch IRQ pin is not configured");
      this->mark_failed();
      return;
    }

    this->touch_irq_pin_->setup();
    this->touch_irq_pin_->pin_mode(
        gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

    const gpio_num_t pin =
        static_cast<gpio_num_t>(this->touch_irq_pin_->get_pin());
    gpio_pullup_en(pin);
    gpio_sleep_set_pull_mode(pin, GPIO_PULLUP_ONLY);

    this->reset_timer();
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG, "Light Sleep Touch:");

    if (this->touch_irq_pin_ != nullptr) {
      LOG_PIN("  Touch IRQ Pin: ", this->touch_irq_pin_);
    }

    ESP_LOGCONFIG(
        TAG,
        "  Inactivity timeout: %" PRIu32 " ms (0 = disabled)",
        this->inactivity_timeout_ms_);

    ESP_LOGCONFIG(
        TAG,
        "  Sleep duration: %" PRIu32 " ms",
        this->sleep_duration_ms_);
  }

  void loop() override {
    if (this->is_failed() || this->touch_irq_pin_ == nullptr) {
      return;
    }

    if (this->inactivity_timeout_ms_ == 0) {
      return;
    }

    // XPT2046 PENIRQ is active low.
    if (!this->touch_irq_pin_->digital_read()) {
      this->reset_timer();
      return;
    }

    if ((millis() - this->last_activity_ms_) >=
        this->inactivity_timeout_ms_) {
      this->enter_light_sleep();
    }
  }

  void reset_timer() {
    this->last_activity_ms_ = millis();
  }

  void enter_light_sleep() {
    if (this->is_failed() || this->touch_irq_pin_ == nullptr) {
      return;
    }

    // Do not enter sleep while PENIRQ is already active.
    if (!this->touch_irq_pin_->digital_read()) {
      this->reset_timer();
      return;
    }

    ESP_LOGI(
        TAG,
        "Entering light sleep for %" PRIu32 " ms",
        this->sleep_duration_ms_);

    this->before_sleep_callback_.call();

    // Give the backlight, LVGL and logger time to settle.
    delay(50);

    const gpio_num_t pin =
        static_cast<gpio_num_t>(this->touch_irq_pin_->get_pin());

    // Maintain internal pull-up during light sleep mode to prevent floating LOW false wakes.
    gpio_pullup_en(pin);
    gpio_sleep_set_pull_mode(pin, GPIO_PULLUP_ONLY);
    gpio_sleep_sel_dis(pin);

    // GPIO wake source: touch.
    gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();

    // Timer wake source: periodic BLE sampling.
    const uint64_t sleep_us =
        static_cast<uint64_t>(this->sleep_duration_ms_) * 1000ULL;

    const esp_err_t timer_err =
        esp_sleep_enable_timer_wakeup(sleep_us);

    if (timer_err != ESP_OK) {
      ESP_LOGW(
          TAG,
          "Failed to configure timer wakeup: %s",
          esp_err_to_name(timer_err));
    }

    const esp_err_t err = esp_light_sleep_start();

    // Remove wake sources after returning to normal execution.
    gpio_wakeup_disable(pin);
    esp_sleep_disable_wakeup_source(
        ESP_SLEEP_WAKEUP_GPIO);

    esp_sleep_disable_wakeup_source(
        ESP_SLEEP_WAKEUP_TIMER);

    this->touch_wakeup_ = false;

    if (err != ESP_OK) {
      this->last_wakeup_cause_ = ESP_SLEEP_WAKEUP_UNDEFINED;
      ESP_LOGW(
          TAG,
          "Light sleep did not start: %s",
          esp_err_to_name(err));
    } else {
      this->last_wakeup_cause_ = esp_sleep_get_wakeup_cause();

      ESP_LOGI(
          TAG,
          "Woke from light sleep, cause=%d",
          static_cast<int>(this->last_wakeup_cause_));

      if (this->last_wakeup_cause_ == ESP_SLEEP_WAKEUP_GPIO) {
        // A real touch holds PENIRQ low. Noise glitches do not. Require the
        // line to still be low after a short debounce, otherwise treat the
        // wake as a background wake (no screen on, back to sleep after BLE).
        bool still_low = !this->touch_irq_pin_->digital_read();
        if (still_low) {
          delay(15);
          still_low = !this->touch_irq_pin_->digital_read();
        }

        if (still_low) {
          this->touch_wakeup_ = true;
        } else {
          ESP_LOGW(TAG, "Phantom GPIO wake (PENIRQ released), ignoring");
        }
      }
    }

    // Only reset inactivity timer if woken by a confirmed touch.
    if (this->touch_wakeup_) {
      this->reset_timer();
    }

    this->wake_callback_.call();
  }

 protected:
  InternalGPIOPin *touch_irq_pin_{nullptr};

  uint32_t inactivity_timeout_ms_{0};
  uint32_t sleep_duration_ms_{30000};
  uint32_t last_activity_ms_{0};

  bool touch_wakeup_{false};
  esp_sleep_wakeup_cause_t last_wakeup_cause_{ESP_SLEEP_WAKEUP_UNDEFINED};

  CallbackManager<void()> before_sleep_callback_;
  CallbackManager<void()> wake_callback_;
};

template <typename... Ts>
class EnterAction final
    : public Action<Ts...>,
      public Parented<LightSleepTouchComponent> {
 public:
  void play(const Ts &...) override {
    this->parent_->enter_light_sleep();
  }
};

template <typename... Ts>
class ResetTimerAction final
    : public Action<Ts...>,
      public Parented<LightSleepTouchComponent> {
 public:
  void play(const Ts &...) override {
    this->parent_->reset_timer();
  }
};

template <typename... Ts>
class SetTimeoutAction final
    : public Action<Ts...>,
      public Parented<LightSleepTouchComponent> {
 public:
  TEMPLATABLE_VALUE(uint32_t, timeout)

  void play(const Ts &...x) override {
    this->parent_->set_inactivity_timeout(
        this->timeout_.value(x...));
  }
};

}  // namespace light_sleep_touch
}  // namespace esphome