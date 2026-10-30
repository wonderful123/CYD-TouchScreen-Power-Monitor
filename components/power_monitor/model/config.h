#pragma once
// config.h - compile-time constants shared by the power-monitor model and UI helpers.
// No state, no functions, no framework includes.
#include <cstdint>

namespace pwr {

// ---- channels
constexpr int CH = 3;
constexpr int CH_CHARGING = 0;
constexpr int CH_DISTRIBUTION = 1;
constexpr int CH_BATTERY = 2;

// ---- history geometry
constexpr int N = 240;        // points per history level
constexpr int LEVELS = 4;     // 10 min, 1 h, 6 h, 24 h
constexpr int TOP_LEVEL = LEVELS - 1;  // the 24 h level
constexpr int SPARK_N = 120;  // 5 min = newest 120 points of level 0
inline constexpr int FACTOR[LEVELS] = {1, 6, 6, 4};  // level l gets 1 sample per FACTOR[l] of level l-1
inline constexpr float SPAN_S[LEVELS] = {600.0f, 3600.0f, 21600.0f, 86400.0f};  // seconds covered by N points
constexpr uint16_t INVALID_MV = 0xFFFF;

// ---- sampling cadence
// Level 0 slot length: SPAN_S[0] / N = 2.5 s. History is time-based, so a
// slot is recorded per elapsed SLOT_MS whether the device was awake or asleep.
constexpr uint32_t SLOT_MS = 2500;
// Longest gap that is back-filled: a full 24 h worth of level-0 slots.
constexpr uint32_t MAX_FILL = (uint32_t) N * FACTOR[1] * FACTOR[2] * FACTOR[3];
// One shared deadline for wake sampling and the ESPHome acquisition timeout.
// Do not write missing slots while the acquisition still has time to succeed.
constexpr uint32_t WAKE_GRACE_MS = 3500;

// ---- link / alarm thresholds
constexpr uint32_t STALE_MS = 6000;  // no new sensor sample for this long = no data
constexpr float LOW_BATT_V = 12.0f;  // battery voltage alarm (critical)

}  // namespace pwr
