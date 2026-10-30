// Use the device clock branch, not PWR_HOST_BUILD: wall time advances through
// manual sleep while the fake FreeRTOS millis clock is paused.
#include "components/light_sleep_touch/light_sleep_touch.h"
#include "components/power_monitor/ble/ingest.h"
#include "components/power_monitor/model/sampler.h"
#include "components/power_monitor/ui/views/overview.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>

using namespace esphome;
using namespace esphome::light_sleep_touch;

std::vector<uint8_t> payload(int seq, int mv = 12000) {
  std::vector<uint8_t> bytes(27, 0);
  bytes[0] = 2; bytes[1] = 1; bytes[26] = seq;
  for (int c = 0; c < pwr::CH; ++c) {
    const int o = 2 + 8 * c;
    bytes[o] = mv & 255; bytes[o + 1] = mv >> 8;
    bytes[o + 2] = 100;  // 1A
  }
  return bytes;
}
int count(int level, int channel = 0) {
  int result = 0;
  float v, i;
  for (int j = 0; j < pwr::N; ++j) result += pwr::get(level, channel, j, v, i);
  return result;
}
void awake(uint32_t ms) { esphome::delay(ms); }
void receive(int seq, int mv = 12000) { assert(pwr::ingest_advert(payload(seq, mv))); pwr::tick(); }
void wake_after(uint32_t ms) {
  fake::wall_us += static_cast<int64_t>(ms) * 1000;
  pwr::note_wake();
}
int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string scenario = argv[1];
  if (scenario == "clock") {
    receive(1);
    const auto rtos_before = fake::now;
    wake_after(30000);
    assert(fake::now == rtos_before);
    assert(pwr::now_ms() == 30000);  // Must include manual sleep.
    receive(2);
    assert(count(0) == 13);
  } else if (scenario == "late_packet") {
    receive(1);
    wake_after(30000);
    for (int n = 0; n < 6; ++n) { awake(500); pwr::tick(); }
    assert(pwr::head[0] == 1);  // No missing slots before the 3.5s deadline.
    awake(200); receive(2, 14000);
    assert(count(0) == 14);
    pwr::compute_stats();
    assert(std::fabs(pwr::st24[0].hours * 3600 - 35.0f) < 0.01f);
  } else if (scenario == "same_sequence") {
    receive(7);
    wake_after(30000);
    awake(100); receive(7, 14000);  // Sequence wrap/restart can alias across sleep.
    assert(pwr::fresh());
    assert(count(0) == 13);
    // Repeated packets while continuously awake must still become stale.
    awake(6000); receive(7);
    assert(!pwr::fresh());
  } else if (scenario == "overview_stale" || scenario == "overview_offline") {
    for (int n = 0; n < 12; ++n) { receive(n); awake(2500); }
    const int stored = count(0);
    assert(stored == 12);
    lv_obj_t header, widgets[pwr::CH][13];
    auto &view = pwr::ui::overview_widgets;
    view.spark_hdr = &header; view.bound = true;
    for (int c = 0; c < pwr::CH; ++c) {
      auto *w = widgets[c];
      view.row[c] = {&w[0], &w[1], &w[2], &w[3], &w[4], &w[5], &w[6], &w[7], &w[8], &w[9], &w[10], &w[11], &w[12]};
    }
    if (scenario == "overview_stale") awake(6000);
    else {
      auto bytes = payload(20);
      for (int c = 0; c < pwr::CH; ++c) bytes[2 + c * 8] = bytes[3 + c * 8] = 255;
      assert(pwr::ingest_advert(bytes));
    }
    pwr::ui::render_overview(0);
    for (int c = 0; c < pwr::CH; ++c) {
      assert(widgets[c][0].text == "--");  // Live values must not imply freshness.
      assert(widgets[c][4].text != "24h --");
      assert(!(widgets[c][11].flags & LV_OBJ_FLAG_HIDDEN));
      assert(count(0, c) == stored);  // Rendering never mutates storage.
    }
  } else if (scenario == "cycles") {
    InternalGPIOPin pin;
    wifi::WiFiComponent wifi;
    esp32_ble::ESP32BLE ble;
    wifi::global_wifi_component = &wifi;
    esp32_ble::global_ble = &ble;
    LightSleepTouchComponent component;
    component.set_touch_irq_pin(&pin);
    component.set_sleep_duration(30000);
    component.setup();
    int wakes = 0;
    component.add_on_wake_callback([&] { pwr::note_wake(); ++wakes; });
    fake::on_sleep = [] { fake::wall_us += static_cast<int64_t>(fake::sleep_us); };
    receive(1);
    int cycles = 0;
    while (fake::wall_us < 2LL * 86400000 * 1000) {
      ++cycles;
      const auto *raw = reinterpret_cast<const unsigned char *>(pwr::buf);
      const std::vector<unsigned char> stored(raw, raw + sizeof(pwr::buf));
      component.enter_light_sleep();
      component.loop();  // Queue radio shutdown outside the BLE callback.
      component.loop();  // Hardware sleep, then on_wake -> note_wake.
      assert(component.get_wake_reason() == WakeReason::TIMER);
      assert(std::memcmp(stored.data(), pwr::buf, sizeof(pwr::buf)) == 0);
      if (cycles % 7 == 0) {
        for (int n = 0; n < 7; ++n) { awake(500); pwr::tick(); }
        assert(!pwr::fresh());  // Acquisition timeout: advance with missing slots.
      } else {
        for (int n = 0; n < 6; ++n) { awake(500); pwr::tick(); }
        awake(200);
        // Include first-packet sequence aliases and three signed channels.
        auto bytes = payload(cycles % 11 == 0 ? pwr::last_seq : cycles % 256);
        bytes[12] = 0x9c; bytes[13] = 0xff;  // Distribution = -1A.
        assert(pwr::ingest_advert(bytes));
        pwr::tick();
        assert(pwr::fresh());
      }
      pwr::compute_stats();
      const uint32_t slots = pwr::now_ms() / pwr::SLOT_MS + 1;
      int stride = 1;
      for (int level = 0; level < pwr::LEVELS; ++level) {
        stride *= pwr::FACTOR[level];
        assert(pwr::head[level] == static_cast<int>((slots / stride) % pwr::N));
        if (slots >= static_cast<uint32_t>(stride))
          assert(count(level) > 0);  // No wholesale reset after the first bucket closes.
      }
      if (cycles > 240) assert(count(3) >= 20);
    }
    assert(wakes == cycles && fake::sleep_calls == cycles);
    assert(wifi.enables == cycles && ble.enables == cycles);
    assert(count(3) == pwr::N);  // All retained six-minute buckets populated.
    assert(pwr::st24[0].hours > 15 && pwr::st24[0].hours < 24);
    assert(pwr::st24[0].wh > 0 && pwr::st24[1].wh < 0);
    for (int level = 0; level < pwr::LEVELS; ++level) {
      lv_obj_t line;
      pwr::ui::Trace trace;
      float lo, hi;
      pwr::ui::scale(level, 2, pwr::N, 1, &lo, &hi);
      const int retained = count(level);
      assert(pwr::ui::trace(&line, trace, level, 0, 2, pwr::N, 424, 176, lo, hi));
      drawn_segments.clear();
      lv_event_t event{&line};
      line.callback(&event);
      assert(!drawn_segments.empty());
      assert(count(level) == retained);
    }
    std::cout << "Validated " << cycles << " actual component sleep cycles over two simulated days\n";
  } else return 2;
  std::cout << "PASS " << scenario << '\n';
}
