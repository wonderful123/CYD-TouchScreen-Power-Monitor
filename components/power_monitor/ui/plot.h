#pragma once
// plot.h - turns history into LVGL line points (the only header that knows LVGL).
#include <algorithm>
#include <cmath>
#include "lvgl.h"
#include "../model/config.h"
#include "../model/history.h"

#if LVGL_VERSION_MAJOR >= 9
using pt_t = lv_point_precise_t;
#else
using pt_t = lv_point_t;
#endif

namespace pwr::ui {

// Overview sparkline time frames (tap to cycle): 5 min, 1 h, 6 h, 24 h.
constexpr int SPARK_MODES = 4;
inline constexpr int SPARK_LVL[SPARK_MODES] = {0, 1, 2, 3};          // history level used
inline constexpr int SPARK_CNT[SPARK_MODES] = {SPARK_N, N, N, N};    // newest points of that level

inline pt_t pts_scope[CH][N];
inline pt_t pts_spark[CH][N];  // sized for the longest sparkline window (N points)

// Common y scale (with 10 % padding) over the channels in `mask` for the newest `count` points.
inline void scale(int lvl, int m, int count, int mask, float *mn, float *mx) {
  float lo = 1e30f, hi = -1e30f;
  for (int c = 0; c < CH; c++) {
    if (!(mask & (1 << c))) continue;
    for (int j = N - count; j < N; j++) {
      float v, i;
      if (!get(lvl, c, j, v, i)) continue;
      const float y = metric(m, v, i);
      lo = std::min(lo, y);
      hi = std::max(hi, y);
    }
  }
  if (lo > hi) {
    lo = 0.0f;
    hi = 1.0f;
  }
  if (hi - lo < 1e-6f) {
    lo -= 1.0f;
    hi += 1.0f;
  }
  const float pad = (hi - lo) * 0.1f;
  *mn = lo - pad;
  *mx = hi + pad;
}

// Fills an LVGL line widget. Leading gaps are skipped, gaps inside the trace hold the last value.
// Returns false (and hides the line) if there is nothing to draw.
inline bool trace(lv_obj_t *line, pt_t *pts, int lvl, int c, int m, int count, int w, int h, float mn, float mx) {
  int first = -1, n = 0;
  float y = 0.0f;
  for (int k = 0; k < count; k++) {
    float v, i;
    if (get(lvl, c, N - count + k, v, i)) {
      y = h - (metric(m, v, i) - mn) / (mx - mn) * h;
      if (first < 0) first = k;
    }
    if (first < 0) continue;
    pts[n].x = (int) lroundf((float) k * w / (count - 1));
    pts[n].y = (int) lroundf(y);
    n++;
  }
  if (n < 2) {
    lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    return false;
  }
  lv_line_set_points(line, pts, n);
  lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
  return true;
}

}  // namespace pwr::ui