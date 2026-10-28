// history.h
#pragma once
// history.h - multi-resolution history ring buffers.
// Level 0 is the finest; each higher level stores the mean of FACTOR[level]
// samples of the level below. Pure storage: no clock, no UI.
#include <cmath>
#include "config.h"
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

struct HistoryInit {
  HistoryInit() {
    for (int l = 0; l < LEVELS; l++)
      for (int c = 0; c < CH; c++)
        for (int j = 0; j < N; j++) buf[l][c][j] = {INVALID_MV, 0};
  }
};
inline HistoryInit history_init;

// Append one sample per channel to level l and cascade the averages upwards.
inline void push(int l, const float *v, const float *i, const bool *ok) {
  for (int c = 0; c < CH; c++) {
    Sample s{INVALID_MV, 0};
    if (ok[c]) {
      s.mv = (uint16_t) lroundf(v[c] * 1000.0f);
      s.cur = (int16_t) lroundf(i[c] * 100.0f);
    }
    buf[l][c][head[l]] = s;
    if (l == TOP_LEVEL) peak_close(c, head[l]);  // close the peak bucket alongside the 24 h sample
  }
  head[l] = (head[l] + 1) % N;
  if (l + 1 >= LEVELS) return;

  const int n = l + 1;
  for (int c = 0; c < CH; c++) {
    if (ok[c]) {
      acc_v[n][c] += v[c];
      acc_i[n][c] += i[c];
      acc_n[n][c]++;
    }
  }
  if (++ticks[n] >= FACTOR[n]) {
    float av[CH], ai[CH];
    bool ao[CH];
    for (int c = 0; c < CH; c++) {
      ao[c] = acc_n[n][c] > 0;
      av[c] = ao[c] ? acc_v[n][c] / acc_n[n][c] : 0.0f;
      ai[c] = ao[c] ? acc_i[n][c] / acc_n[n][c] : 0.0f;
      acc_v[n][c] = acc_i[n][c] = 0.0f;
      acc_n[n][c] = 0;
    }
    ticks[n] = 0;
    push(n, av, ai, ao);
  }
}

// j = 0 oldest ... N-1 newest
inline bool get(int l, int c, int j, float &v, float &i) {
  const Sample &s = buf[l][c][(head[l] + j) % N];
  if (s.mv == INVALID_MV) return false;
  v = s.mv / 1000.0f;
  i = s.cur / 100.0f;
  return true;
}

// Select the plotted quantity: 0 = V, 1 = A, 2 = W.
inline float metric(int m, float v, float i) { return m == 0 ? v : (m == 1 ? i : v * i); }

}  // namespace pwr