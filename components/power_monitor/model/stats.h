#pragma once
// stats.h - derived statistics: 24 h table, battery balance, last-5-minute window.
#include <algorithm>
#include <cmath>
#include "config.h"
#include "history.h"
#include "peaks.h"

namespace pwr {

// ----------------------------------------------------------- 24 h statistics
struct Stats {
  bool any;
  float vmin, vmax, ipk, wpk, ah, wh, hours;
};
inline Stats st24[CH];
inline float bal_ah_in, bal_ah_out, bal_wh_in, bal_wh_out;  // battery, positive current = charging

inline void merge_pk(Stats &s, const Pk &p) {
  if (p.vmin == INVALID_MV) return;
  s.any = true;
  s.vmin = std::min(s.vmin, p.vmin / 1000.0f);
  s.vmax = std::max(s.vmax, p.vmax / 1000.0f);
  s.ipk = std::max(s.ipk, p.i / 100.0f);
  s.wpk = std::max(s.wpk, (float) p.w);
}

inline void compute_stats() {
  const float dt_h = (SPAN_S[TOP_LEVEL] / N) / 3600.0f;
  bal_ah_in = bal_ah_out = bal_wh_in = bal_wh_out = 0.0f;
  for (int c = 0; c < CH; c++) {
    Stats s{false, 99.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    for (int j = 0; j < N; j++) {
      // True extremes first (also covers buckets whose averaged sample is missing).
      merge_pk(s, pk_ring[c][(head[TOP_LEVEL] + j) % N]);

      float v, i;
      if (!get(TOP_LEVEL, c, j, v, i)) continue;
      s.any = true;
      s.vmin = std::min(s.vmin, v);
      s.vmax = std::max(s.vmax, v);
      s.ipk = std::max(s.ipk, fabsf(i));
      s.wpk = std::max(s.wpk, fabsf(v * i));
      s.ah += fabsf(i) * dt_h;
      s.wh += fabsf(v * i) * dt_h;
      s.hours += dt_h;
      if (c == CH_BATTERY) {
        if (i > 0) {
          bal_ah_in += i * dt_h;
          bal_wh_in += v * i * dt_h;
        } else {
          bal_ah_out -= i * dt_h;
          bal_wh_out -= v * i * dt_h;
        }
      }
    }
    // The bucket still in progress, so values show up straight after boot.
    merge_pk(s, pk_cur[c]);
    if (!s.any) s.vmin = 0.0f;
    st24[c] = s;
  }
}

// ------------------------------------------------- last 5 minutes (overview)
struct Win {
  bool any;
  float vmin, vmax, ipk, wavg;
};
inline Win win5(int c) {
  Win w{false, 99.0f, 0.0f, 0.0f, 0.0f};
  int n = 0;
  for (int j = N - SPARK_N; j < N; j++) {
    float v, i;
    if (!get(0, c, j, v, i)) continue;
    w.any = true;
    w.vmin = std::min(w.vmin, v);
    w.vmax = std::max(w.vmax, v);
    w.ipk = std::max(w.ipk, fabsf(i));
    w.wavg += v * i;
    n++;
  }
  if (n) w.wavg /= n;
  if (!w.any) w.vmin = 0.0f;
  return w;
}

}  // namespace pwr