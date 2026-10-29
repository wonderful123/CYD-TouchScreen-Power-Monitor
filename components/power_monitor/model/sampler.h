#pragma once
// sampler.h - turns live readings into time-based history slots.
#include "clock.h"
#include "config.h"
#include "history.h"
#include "live.h"

namespace pwr {

inline bool slot_started = false;
inline uint32_t last_slot_ms = 0;
inline float prev_v[CH];
inline float prev_i[CH];
inline bool prev_ok[CH];

// Time-based history tick. Safe to call as often as you like (awake interval,
// background BLE wake, acquisition timeout): it only records when at least one
// SLOT_MS slot has elapsed. If several slots elapsed (light sleep), the missing
// slots are back-filled by linear interpolation between the previous and the
// new reading, so every level keeps a true time axis while sleeping, just with
// fewer real points.
inline void tick() {
  const uint32_t now = now_ms();
  const bool f = fresh();

  // Just woke and the first packet has not arrived yet: don't record a false gap.
  if (!f && slot_started && (now - wake_ms) < WAKE_GRACE_MS) return;

  uint32_t n = 1;
  bool expired = false;
  if (!slot_started) {
    slot_started = true;
    last_slot_ms = now;
  } else {
    const uint32_t elapsed = now - last_slot_ms;
    if (elapsed < SLOT_MS) return;
    n = elapsed / SLOT_MS;
    last_slot_ms += n * SLOT_MS;
    if (n >= MAX_FILL) {
      n = MAX_FILL;
      expired = true;
      clear_history();
      for (int c = 0; c < CH; ++c) prev_ok[c] = false;
    }
  }

  float v[CH], i[CH];
  bool ok[CH];
  for (int c = 0; c < CH; c++) {
    ok[c] = f && live.valid[c];
    v[c] = live.v[c];
    i[c] = live.i[c];
  }

  // Never interpolate from an endpoint outside the retained window. Keep any
  // fresh packet peaks out of the expired backfill buckets and attach them to
  // the current slot instead.
  Pk recent_peaks[CH];
  for (int c = 0; c < CH; ++c) {
    if (expired) {
      peak_expire_current(c);
      recent_peaks[c] = pk_cur[c];
      pk_cur[c] = pk_empty();
    }
  }

  // Back-fill the slots skipped while asleep.
  for (uint32_t k = 1; k < n; k++) {
    const float t = (float) k / (float) n;
    float gv[CH], gi[CH];
    bool gok[CH];
    for (int c = 0; c < CH; c++) {
      gok[c] = prev_ok[c] && ok[c];
      gv[c] = prev_v[c] + (v[c] - prev_v[c]) * t;
      gi[c] = prev_i[c] + (i[c] - prev_i[c]) * t;
    }
    push(0, gv, gi, gok);
  }
  if (expired) {
    for (int c = 0; c < CH; ++c) pk_cur[c] = recent_peaks[c];
  }
  push(0, v, i, ok);

  for (int c = 0; c < CH; c++) {
    prev_v[c] = v[c];
    prev_i[c] = i[c];
    prev_ok[c] = ok[c];
  }
}

}  // namespace pwr