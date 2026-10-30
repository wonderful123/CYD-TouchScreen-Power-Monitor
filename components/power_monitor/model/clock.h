#pragma once
// clock.h - the one place the power-monitor model reads the time.
//
// Device time includes manual light sleep via the RTC-compensated ESP timer.
// ESPHome millis() may use FreeRTOS ticks, which manual sleep does not advance.
// Define PWR_HOST_BUILD when compiling
// on a PC (unit tests) and drive the clock yourself via pwr::host_now_ms.
#include <cstdint>

#ifdef PWR_HOST_BUILD
namespace pwr {
inline uint32_t host_now_ms = 0;
inline uint32_t now_ms() { return host_now_ms; }
}  // namespace pwr
#else
#include "esp_timer.h"
namespace pwr {
inline uint32_t now_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }
}  // namespace pwr
#endif
