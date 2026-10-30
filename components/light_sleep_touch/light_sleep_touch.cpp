#include "light_sleep_touch.h"

#include <cinttypes>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "driver/gpio.h"
#ifdef USE_ESP32_BLE
#include "esphome/components/esp32_ble/ble.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#endif
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome::light_sleep_touch {

static const char *const TAG = "light_sleep_touch";
static constexpr uint32_t RADIO_STOP_TIMEOUT_MS = 5000;
static constexpr uint32_t SLEEP_SETTLE_MS = 50;
static constexpr uint32_t TOUCH_DEBOUNCE_MS = 15;

void LightSleepTouchComponent::setup() {
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

void LightSleepTouchComponent::dump_config() {
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

void LightSleepTouchComponent::loop() {
  if (this->is_failed() || this->touch_irq_pin_ == nullptr) {
    return;
  }

  if (this->state_ != State::AWAKE) {
    this->process_sleep_request_();
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

void LightSleepTouchComponent::reset_timer() {
  this->last_activity_ms_ = millis();
}

void LightSleepTouchComponent::set_inactivity_timeout(uint32_t timeout_ms) {
  this->inactivity_timeout_ms_ = timeout_ms;
  this->reset_timer();
}

void LightSleepTouchComponent::enter_light_sleep() {
  if (this->is_failed() || this->touch_irq_pin_ == nullptr || this->state_ != State::AWAKE)
    return;
  this->state_ = State::SLEEP_REQUESTED;
}

void LightSleepTouchComponent::stop_radios_() {
  this->radio_stop_ms_ = millis();
#ifdef USE_WIFI
  this->restore_wifi_ = wifi::global_wifi_component != nullptr && !wifi::global_wifi_component->is_disabled();
  if (this->restore_wifi_)
    wifi::global_wifi_component->disable();
#endif
#ifdef USE_ESP32_BLE
  this->restore_ble_ = esp32_ble::global_ble != nullptr && esp32_ble::global_ble->is_active();
  if (this->restore_ble_)
    esp32_ble::global_ble->disable();
#endif
}

bool LightSleepTouchComponent::radios_stopped_() const {
#ifdef USE_ESP32_BLE
  return esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE &&
         esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED;
#else
  return true;
#endif
}

void LightSleepTouchComponent::restore_radios_() {
#ifdef USE_WIFI
  if (this->restore_wifi_)
    wifi::global_wifi_component->enable();
#endif
#ifdef USE_ESP32_BLE
  if (this->restore_ble_)
    esp32_ble::global_ble->enable();
#endif
  this->restore_wifi_ = this->restore_ble_ = false;
}

void LightSleepTouchComponent::finish_sleep_(WakeReason reason) {
  this->wake_reason_ = reason;
  this->restore_radios_();
  if (!this->is_background_wakeup())
    this->reset_timer();
  this->state_ = State::AWAKE;
  this->wake_callback_.call();
}

void LightSleepTouchComponent::cancel_sleep_() {
  this->last_wakeup_cause_ = ESP_SLEEP_WAKEUP_UNDEFINED;
  this->finish_sleep_(WakeReason::CANCELLED);
}

void LightSleepTouchComponent::process_sleep_request_() {
  if (!this->touch_irq_pin_->digital_read()) {
    this->cancel_sleep_();
    return;
  }
  if (this->state_ == State::SLEEP_REQUESTED) {
    this->state_ = State::STOPPING_RADIOS;
    this->stop_radios_();
    return;  // BLE disable is processed by its component loop, never inline.
  }
  if (!this->radios_stopped_()) {
    if (millis() - this->radio_stop_ms_ >= RADIO_STOP_TIMEOUT_MS) {
      ESP_LOGW(TAG, "Radio shutdown timed out; cancelling sleep");
      this->cancel_sleep_();
    }
    return;
  }
  this->perform_light_sleep_();
}

WakeReason LightSleepTouchComponent::classify_wakeup_() {
  if (this->last_wakeup_cause_ == ESP_SLEEP_WAKEUP_TIMER)
    return WakeReason::TIMER;
  if (this->last_wakeup_cause_ != ESP_SLEEP_WAKEUP_GPIO)
    return WakeReason::OTHER;

  // PENIRQ must remain low across debounce. Released GPIO glitches should not
  // turn the screen on; application policy can sample BLE and return to sleep.
  if (!this->touch_irq_pin_->digital_read()) {
    delay(TOUCH_DEBOUNCE_MS);
    if (!this->touch_irq_pin_->digital_read())
      return WakeReason::TOUCH;
  }
  ESP_LOGW(TAG, "Phantom GPIO wake (PENIRQ released), ignoring");
  return WakeReason::GPIO_NOISE;
}

void LightSleepTouchComponent::perform_light_sleep_() {
  this->state_ = State::SLEEPING;
  this->last_wakeup_cause_ = ESP_SLEEP_WAKEUP_UNDEFINED;
  ESP_LOGI(TAG, "Entering light sleep for %" PRIu32 " ms", this->sleep_duration_ms_);
  this->before_sleep_callback_.call();
  // Give the backlight, LVGL and logger time to settle.
  delay(SLEEP_SETTLE_MS);

  const auto pin = static_cast<gpio_num_t>(this->touch_irq_pin_->get_pin());
  gpio_pullup_en(pin);
  gpio_sleep_set_pull_mode(pin, GPIO_PULLUP_ONLY);
  gpio_sleep_sel_dis(pin);

  esp_err_t err = gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
  if (err == ESP_OK)
    err = esp_sleep_enable_gpio_wakeup();
  if (err == ESP_OK)
    err = esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(this->sleep_duration_ms_) * 1000ULL);

  WakeReason reason = WakeReason::ERROR;
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "Failed to configure wakeup sources: %s", esp_err_to_name(err));
  } else {
    err = esp_light_sleep_start();
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "Light sleep did not start: %s", esp_err_to_name(err));
    } else {
      this->last_wakeup_cause_ = esp_sleep_get_wakeup_cause();
      ESP_LOGI(TAG, "Woke from light sleep, cause=%d", static_cast<int>(this->last_wakeup_cause_));
      reason = this->classify_wakeup_();
    }
  }

  // Always release the wake sources we own, including partial setup failures.
  gpio_wakeup_disable(pin);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
  this->finish_sleep_(reason);
}

}  // namespace esphome::light_sleep_touch
