#pragma once
#include "runtime.h"
inline int64_t esp_timer_get_time() { return fake::wall_us; }
