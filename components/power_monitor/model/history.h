// history.h
#pragma once
// history.h - multi-resolution history ring buffers.
// Level 0 is the finest; each higher level stores the mean of FACTOR[level]
// samples of the level below, weighted by their valid level-0 sample counts. Pure storage: no clock, no UI.
#include <cmath>
#include "config.h"
#include "direction.h"
#include "peaks.h"

namespace pwr {

struct Sample {
  uint16_t mv;  // millivolts, INVALID_MV = no data
  int16_t cur;  // 10 mA units
};
inline Sample buf[LEVELS][CH][N];
inline int head[LEVELS] = {0, 0, 0, 0};
inline float acc_v[LEVELS][CH];
inline float acc_i[LEVELS][CH];
inline int acc_n[LEVELS][CH];
inline int ticks[LEVELS];

// Integrate at the finest cadence before signed currents are averaged. These
// buckets share the 24 h history's ring positions but retain valid duration and
// both energy directions. Keep the open bucket visible between six-minute closes.
struct Energy {
  float hours, ah_in, ah_out, wh_in, wh_out;
};
inline Energy energy_ring[CH][N]{};
inline Energy energy_cur[CH]{};

inline void integrate_sample(int c, float v, float i) {
  constexpr float dt_h = SLOT_MS / 3600000.0f;
  Energy &e = energy_cur[c];
  e.hours += dt_h;
  if (i >= 0.0f) {
    e.ah_in += i * dt_h;
    e.wh_in += v * i * dt_h;
  } else {
    e.ah_out -= i * dt_h;
    e.wh_out -= v * i * dt_h;
  }
}

struct HistoryInit {
  HistoryInit() {
    for (int l = 0; l < LEVELS; l++)
      for (int c = 0; c < CH; c++)
        for (int j = 0; j < N; j++) buf[l][c][j] = {INVALID_MV, 0};
  }
};
inline HistoryInit history_init;

// Drop expired rings and unfinished averages after a gap of at least 24 h.
// Current packet peaks are handled separately so a newly received packet survives.
inline void clear_history() {
  for (int l = 0; l < LEVELS; ++l) {
    head[l] = ticks[l] = 0;
    for (int c = 0; c < CH; ++c) {
      acc_v[l][c] = acc_i[l][c] = 0;
      acc_n[l][c] = 0;
      for (int j = 0; j < N; ++j) buf[l][c][j] = {INVALID_MV, 0};
    }
  }
  for (int c = 0; c < CH; ++c) {
    energy_cur[c] = {};
    for (int j = 0; j < N; ++j) {
      energy_ring[c][j] = {};
      pk_ring[c][j] = pk_empty();
    }
  }
}

// Append one sample per channel to level l and cascade the averages upwards.
inline void push(int l, const float *v, const float *i, const bool *ok, const int *weights = nullptr) {
  for (int c = 0; c < CH; c++) {
    if (l == 0 && ok[c]) integrate_sample(c, v[c], i[c]);
    Sample s{INVALID_MV, 0};
    if (ok[c]) {
      s.mv = (uint16_t) lroundf(v[c] * 1000.0f);
      s.cur = (int16_t) lroundf(i[c] * 100.0f);
    }
    buf[l][c][head[l]] = s;
    if (l == TOP_LEVEL) {
      peak_close(c, head[l]);
      energy_ring[c][head[l]] = energy_cur[c];
      energy_cur[c] = {};
    }
  }
  head[l] = (head[l] + 1) % N;
  if (l + 1 >= LEVELS) return;

  const int n = l + 1;
  for (int c = 0; c < CH; c++) {
    if (ok[c]) {
      const int weight = weights == nullptr ? 1 : weights[c];
      acc_v[n][c] += v[c] * weight;
      acc_i[n][c] += i[c] * weight;
      acc_n[n][c] += weight;
    }
  }
  if (++ticks[n] >= FACTOR[n]) {
    float av[CH], ai[CH];
    bool ao[CH];
    int counts[CH];
    for (int c = 0; c < CH; c++) {
      counts[c] = acc_n[n][c];
      ao[c] = counts[c] > 0;
      av[c] = ao[c] ? acc_v[n][c] / acc_n[n][c] : 0.0f;
      ai[c] = ao[c] ? acc_i[n][c] / acc_n[n][c] : 0.0f;
      acc_v[n][c] = acc_i[n][c] = 0.0f;
      acc_n[n][c] = 0;
    }
    ticks[n] = 0;
    push(n, av, ai, ao, counts);
  }
}

// j = 0 oldest ... N-1 newest
inline bool get(int l, int c, int j, float &v, float &i) {
  const Sample &s = buf[l][c][(head[l] + j) % N];
  if (s.mv == INVALID_MV) return false;
  v = s.mv / 1000.0f;
  i = directed(c, s.cur / 100.0f);
  return true;
}

// Diagnostic count: retained observations/estimates, independent of link freshness.
inline int history_count(int level, int channel) {
  int count = 0;
  for (const auto &sample : buf[level][channel]) count += sample.mv != INVALID_MV;
  return count;
}

// Select the plotted quantity: 0 = V, 1 = A, 2 = W.
inline float metric(int m, float v, float i) { return m == 0 ? v : (m == 1 ? i : v * i); }

}  // namespace pwr
