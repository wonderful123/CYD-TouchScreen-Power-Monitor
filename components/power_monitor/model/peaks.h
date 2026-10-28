#pragma once
// peaks.h - true per-packet extremes for the 24 h statistics.
//
// The history buffers only hold averages (each 24 h point is a 6 min mean of
// means), which flattens short surges. These record the real per-packet
// extremes, one slot per 24 h-level bucket, plus the bucket still in progress.
#include <algorithm>
#include <cmath>
#include "config.h"

namespace pwr {

struct Pk {
  uint16_t vmin;  // mV, INVALID_MV = empty
  uint16_t vmax;  // mV
  uint16_t i;     // peak |current|, 10 mA units
  uint16_t w;     // peak |power|, W
};
inline Pk pk_ring[CH][N];
inline Pk pk_cur[CH];

inline Pk pk_empty() { return Pk{INVALID_MV, 0, 0, 0}; }

struct PeaksInit {
  PeaksInit() {
    for (int c = 0; c < CH; c++) {
      pk_cur[c] = pk_empty();
      for (int j = 0; j < N; j++) pk_ring[c][j] = pk_empty();
    }
  }
};
inline PeaksInit peaks_init;

// Call for every decoded packet of channel c.
inline void peak_update(int c, float v, float i) {
  Pk &p = pk_cur[c];
  const uint16_t mv = (uint16_t) lroundf(std::max(0.0f, std::min(65534.0f, v * 1000.0f)));
  const uint16_t ia = (uint16_t) lroundf(std::min(65535.0f, fabsf(i) * 100.0f));
  const uint16_t wa = (uint16_t) lroundf(std::min(65535.0f, fabsf(v * i)));
  if (mv < p.vmin) p.vmin = mv;
  if (mv > p.vmax) p.vmax = mv;
  if (ia > p.i) p.i = ia;
  if (wa > p.w) p.w = wa;
}

// Store the bucket in progress into ring slot `slot` and start a new one.
inline void peak_close(int c, int slot) {
  pk_ring[c][slot] = pk_cur[c];
  pk_cur[c] = pk_empty();
}

}  // namespace pwr