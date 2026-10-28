#pragma once
// clock.h - the one place the power-monitor model reads the time.
//
// On the device this is esphome::millis(). Define PWR_HOST_BUILD when compiling
// on a PC (unit tests) and drive the clock yourself via pwr::host_now_ms.
#include <cstdint>

#ifdef PWR_HOST_BUILD
namespace pwr {
inline uint32_t host_now_ms = 0;
inline uint32_t now_ms() { return host_now_ms; }
}  // namespace pwr
#else
#include "esphome/core/hal.h"
namespace pwr {
inline uint32_t now_ms() { return esphome::millis(); }
}  // namespace pwr
#endif