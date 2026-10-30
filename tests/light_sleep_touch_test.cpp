#include <cassert>
#include <iostream>

#include "components/light_sleep_touch/light_sleep_touch.h"

using namespace esphome;
using namespace esphome::light_sleep_touch;

struct Fixture {
  InternalGPIOPin pin;
  wifi::WiFiComponent wifi;
  esp32_ble::ESP32BLE ble;
  LightSleepTouchComponent component;
  int before = 0, after = 0;

  Fixture() {
    fake::reset();
    wifi::global_wifi_component = &wifi;
    esp32_ble::global_ble = &ble;
    component.set_touch_irq_pin(&pin);
    component.setup();
    component.add_on_before_sleep_callback([this] {
      before++;
      assert(wifi.disabled && !ble.active);
      assert(fake::sleep_calls == 0);
    });
    component.add_on_wake_callback([this] {
      after++;
      assert(!wifi.disabled && ble.active);
    });
  }

  void request() {
    component.enter_light_sleep();
    component.enter_light_sleep();  // Requests must be idempotent.
    assert(ble.disables == 0 && wifi.disables == 0);
    component.loop();
    assert(fake::sleep_calls == 0);  // Allow the BLE component loop to run.
  }

  void cycle() {
    request();
    component.loop();
    assert(before == 1 && after == 1);
    assert(fake::cleanup_calls == 3);
    assert(wifi.enables == 1 && ble.enables == 1);
    component.loop();
    assert(after == 1);
  }
};

int main() {
  {
    Fixture f;
    f.component.set_sleep_duration(45000);
    f.cycle();
    assert(fake::sleep_us == 45000000);
    assert(f.component.get_wake_reason() == WakeReason::TIMER);
    assert(f.component.is_background_wakeup() && !f.component.is_touch_wakeup());
    assert(f.component.get_last_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
  }
  for (bool released : {false, true}) {
    Fixture f;
    fake::cause = ESP_SLEEP_WAKEUP_GPIO;
    fake::release_on_debounce = released;
    fake::on_sleep = [] { fake::pin_high = false; };
    f.cycle();
    assert(f.component.is_touch_wakeup() == !released);
    assert(f.component.is_background_wakeup() == released);
    assert(f.component.get_wake_reason() == (released ? WakeReason::GPIO_NOISE : WakeReason::TOUCH));
  }
  {
    Fixture f;
    fake::cause = ESP_SLEEP_WAKEUP_GPIO;  // Already released when read.
    f.cycle();
    assert(f.component.get_wake_reason() == WakeReason::GPIO_NOISE);
  }
  for (int fail_at : {1, 2, 3, 4}) {
    Fixture f;
    if (fail_at == 4)
      fake::sleep_error = -1;
    else
      fake::fail_at = fail_at;
    f.cycle();
    assert(fake::sleep_calls == (fail_at == 4 ? 1 : 0));
    assert(f.component.get_wake_reason() == WakeReason::ERROR);
    assert(!f.component.is_touch_wakeup() && !f.component.is_background_wakeup());
    assert(f.component.get_last_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED);
  }
  {
    Fixture f;
    f.component.enter_light_sleep();
    fake::pin_high = false;  // A touch before radios stop cancels immediately.
    f.component.loop();
    assert(f.before == 0 && f.after == 1);
    assert(f.ble.disables == 0 && fake::sleep_calls == 0);
    assert(f.component.get_wake_reason() == WakeReason::CANCELLED);
    assert(!f.component.is_touch_wakeup());
  }
  {
    Fixture f;
    fake::now = UINT32_MAX - 1000;
    fake::controller = fake::bluedroid = 1;
    f.request();
    fake::now += 4999;
    f.component.loop();
    assert(f.after == 0 && fake::sleep_calls == 0);
    fake::now++;
    f.component.loop();
    assert(f.before == 0 && f.after == 1);
    assert(f.component.get_wake_reason() == WakeReason::CANCELLED);
    assert(!f.component.is_background_wakeup() && !f.component.is_touch_wakeup());
    // Recovery leaves the component able to process another request.
    fake::controller = fake::bluedroid = 0;
    f.component.enter_light_sleep();
    f.component.loop();
    f.component.loop();
    assert(fake::sleep_calls == 1 && f.after == 2);
  }
  {
    Fixture f;
    f.request();
    fake::pin_high = false;  // Touch during shutdown also restores radios.
    f.component.loop();
    assert(f.before == 0 && f.after == 1);
    assert(f.ble.enables == 1 && f.wifi.enables == 1);
    assert(f.component.get_wake_reason() == WakeReason::CANCELLED);
  }
  {
    // Sleep must not enable radios that were disabled before the request.
    fake::reset();
    wifi::WiFiComponent wifi;
    wifi.disabled = true;
    esp32_ble::ESP32BLE ble;
    ble.active = false;
    wifi::global_wifi_component = &wifi;
    esp32_ble::global_ble = &ble;
    InternalGPIOPin pin;
    LightSleepTouchComponent component;
    component.set_touch_irq_pin(&pin);
    component.setup();
    component.enter_light_sleep();
    component.loop();
    component.loop();
    assert(wifi.enables == 0 && ble.enables == 0);
    assert(fake::sleep_calls == 1);
  }
  std::cout << "Light sleep lifecycle checks passed\n";
}
