#pragma once

#include <cstdint>
#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esp_sleep.h"

namespace esphome::light_sleep_touch {

// Application policy can distinguish actual wakeups from failed/cancelled sleep.
enum class WakeReason : uint8_t { NONE, TOUCH, TIMER, GPIO_NOISE, OTHER, CANCELLED, ERROR };

class LightSleepTouchComponent : public Component {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void set_touch_irq_pin(InternalGPIOPin *pin) { this->touch_irq_pin_ = pin; }
  void set_inactivity_timeout(uint32_t timeout_ms);
  void set_sleep_duration(uint32_t duration_ms) { this->sleep_duration_ms_ = duration_ms; }
  void reset_timer();

  // Queue sleep; never stop the BLE stack inside an advertisement callback.
  void enter_light_sleep();

  esp_sleep_wakeup_cause_t get_last_wakeup_cause() const { return this->last_wakeup_cause_; }
  WakeReason get_wake_reason() const { return this->wake_reason_; }
  bool is_touch_wakeup() const { return this->wake_reason_ == WakeReason::TOUCH; }
  bool is_background_wakeup() const {
    return this->wake_reason_ == WakeReason::TIMER || this->wake_reason_ == WakeReason::GPIO_NOISE;
  }

  // Before-sleep callbacks must complete synchronously, before hardware sleep starts.
  template<typename F> void add_on_before_sleep_callback(F &&callback) {
    this->before_sleep_callback_.add(std::forward<F>(callback));
  }
  // Called after wake, cancellation, or failure, with radios requested back on.
  template<typename F> void add_on_wake_callback(F &&callback) {
    this->wake_callback_.add(std::forward<F>(callback));
  }

 protected:
  enum class State : uint8_t { AWAKE, SLEEP_REQUESTED, STOPPING_RADIOS, SLEEPING };

  void stop_radios_();
  bool radios_stopped_() const;
  void restore_radios_();
  void cancel_sleep_();
  void finish_sleep_(WakeReason reason);
  void process_sleep_request_();
  void perform_light_sleep_();
  WakeReason classify_wakeup_();

  InternalGPIOPin *touch_irq_pin_{nullptr};
  uint32_t inactivity_timeout_ms_{0};
  uint32_t sleep_duration_ms_{30000};
  uint32_t last_activity_ms_{0};
  State state_{State::AWAKE};
  bool restore_wifi_{false};
  bool restore_ble_{false};
  uint32_t radio_stop_ms_{0};
  WakeReason wake_reason_{WakeReason::NONE};
  esp_sleep_wakeup_cause_t last_wakeup_cause_{ESP_SLEEP_WAKEUP_UNDEFINED};
  CallbackManager<void()> before_sleep_callback_;
  CallbackManager<void()> wake_callback_;
};

}  // namespace esphome::light_sleep_touch
