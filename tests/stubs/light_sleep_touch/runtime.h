#pragma once

// Minimal hardware/ESPHome fakes for compiling the real sleep component on host.
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
using gpio_num_t = int;
constexpr int GPIO_PULLUP_ONLY = 1;
constexpr int GPIO_INTR_LOW_LEVEL = 0;
enum esp_sleep_wakeup_cause_t { ESP_SLEEP_WAKEUP_UNDEFINED, ESP_SLEEP_WAKEUP_GPIO, ESP_SLEEP_WAKEUP_TIMER };
constexpr int ESP_BT_CONTROLLER_STATUS_IDLE = 0;
constexpr int ESP_BLUEDROID_STATUS_UNINITIALIZED = 0;

namespace fake {
inline uint32_t now = 0;
inline int64_t wall_us = 0;
inline bool pin_high = true;
inline bool release_on_debounce = false;
inline int controller = 0, bluedroid = 0;
inline int fail_at = 0, setup_step = 0;
inline int sleep_calls = 0, cleanup_calls = 0;
inline uint64_t sleep_us = 0;
inline esp_err_t sleep_error = ESP_OK;
inline esp_sleep_wakeup_cause_t cause = ESP_SLEEP_WAKEUP_TIMER;
inline std::function<void()> on_sleep;
inline esp_err_t configure() { return ++setup_step == fail_at ? -1 : ESP_OK; }
inline void reset() {
  now = 0;
  wall_us = 0;
  pin_high = true;
  release_on_debounce = false;
  controller = bluedroid = fail_at = setup_step = sleep_calls = cleanup_calls = 0;
  sleep_us = 0;
  sleep_error = ESP_OK;
  cause = ESP_SLEEP_WAKEUP_TIMER;
  on_sleep = nullptr;
}
}  // namespace fake

namespace esphome {
inline uint32_t millis() { return fake::now; }
inline void delay(uint32_t ms) {
  fake::now += ms;
  fake::wall_us += static_cast<int64_t>(ms) * 1000;
  if (ms == 15 && fake::release_on_debounce)
    fake::pin_high = true;
}
class Component {
 public:
  virtual ~Component() = default;
  virtual void setup() {}
  virtual void loop() {}
  virtual void dump_config() {}
  bool is_failed() const { return this->failed_; }
  void mark_failed() { this->failed_ = true; }
 private:
  bool failed_{false};
};
namespace gpio {
constexpr int FLAG_INPUT = 1, FLAG_PULLUP = 2;
}
class InternalGPIOPin {
 public:
  void setup() {}
  void pin_mode(int) {}
  int get_pin() const { return 18; }
  bool digital_read() const { return fake::pin_high; }
};
template<typename> class CallbackManager;
template<> class CallbackManager<void()> {
 public:
  template<typename F> void add(F &&fn) { this->callbacks_.emplace_back(std::forward<F>(fn)); }
  void call() { for (const auto &fn : this->callbacks_) fn(); }
 private:
  std::vector<std::function<void()>> callbacks_;
};
namespace wifi {
struct WiFiComponent {
  bool disabled = false;
  int enables = 0, disables = 0;
  bool is_disabled() const { return disabled; }
  void disable() { disabled = true; disables++; }
  void enable() { disabled = false; enables++; }
};
inline WiFiComponent *global_wifi_component = nullptr;
}
namespace esp32_ble {
struct ESP32BLE {
  bool active = true;
  int enables = 0, disables = 0;
  bool is_active() const { return active; }
  // Model a queued shutdown, rather than immediately changing hardware status.
  void disable() { active = false; disables++; }
  void enable() { active = true; enables++; }
};
inline ESP32BLE *global_ble = nullptr;
}
}  // namespace esphome

inline int esp_bt_controller_get_status() { return fake::controller; }
inline int esp_bluedroid_get_status() { return fake::bluedroid; }
inline void gpio_pullup_en(int) {}
inline void gpio_sleep_set_pull_mode(int, int) {}
inline void gpio_sleep_sel_dis(int) {}
inline esp_err_t gpio_wakeup_enable(int, int) { return fake::configure(); }
inline esp_err_t esp_sleep_enable_gpio_wakeup() { return fake::configure(); }
inline esp_err_t esp_sleep_enable_timer_wakeup(uint64_t us) { fake::sleep_us = us; return fake::configure(); }
inline esp_err_t esp_light_sleep_start() {
  fake::sleep_calls++;
  if (fake::on_sleep) fake::on_sleep();
  return fake::sleep_error;
}
inline void gpio_wakeup_disable(int) { fake::cleanup_calls++; }
inline void esp_sleep_disable_wakeup_source(esp_sleep_wakeup_cause_t) { fake::cleanup_calls++; }
inline esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause() { return fake::cause; }
inline const char *esp_err_to_name(esp_err_t) { return "test error"; }
// Evaluate log arguments to keep strict compiler checks useful.
inline void fake_log(const char *, const char *, ...) {}
#define ESP_LOGE(...) fake_log(__VA_ARGS__)
#define ESP_LOGW(...) fake_log(__VA_ARGS__)
#define ESP_LOGI(...) fake_log(__VA_ARGS__)
#define ESP_LOGCONFIG(...) fake_log(__VA_ARGS__)
#define LOG_PIN(...) ((void) 0)
