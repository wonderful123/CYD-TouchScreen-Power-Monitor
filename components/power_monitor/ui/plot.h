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

struct Trace {
  pt_t points[N];
  bool estimated[N]{};
  int width = 0, height = 0;
  int count = 0;
  bool bound = false;
};
inline Trace traces_scope[CH];
inline Trace traces_spark[CH];

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

// Draw the interpolated trace over very faint bands marking estimated samples.
inline void draw_trace(lv_event_t *event) {
  const auto &data = *static_cast<const Trace *>(lv_event_get_user_data(event));
  auto *line = static_cast<lv_obj_t *>(lv_event_get_current_target(event));
  lv_area_t area;
  lv_obj_get_coords(line, &area);
  lv_draw_line_dsc_t dsc;
  lv_draw_line_dsc_init(&dsc);
  lv_obj_init_draw_line_dsc(line, LV_PART_MAIN, &dsc);
#if LVGL_VERSION_MAJOR >= 9
  auto *layer = lv_event_get_layer(event);
  dsc.base.layer = layer;
#else
  auto *context = lv_event_get_draw_ctx(event);
#endif
  // Missing observations are shown with a neutral tint at ~3% opacity. Each
  // channel draws its own bands; even overlapping scope bands stay subdued.
  lv_draw_rect_dsc_t shade;
  lv_draw_rect_dsc_init(&shade);
  shade.bg_color = lv_color_hex(0xD3E6EB);
  shade.bg_opa = 8;
#if LVGL_VERSION_MAJOR >= 9
  shade.base.layer = layer;
#endif
  for (int k = 0; k < data.count; ++k) {
    if (!data.estimated[k]) continue;
    const int start = k;
    while (k + 1 < data.count && data.estimated[k + 1]) ++k;
    const float step = static_cast<float>(data.width) / (data.count - 1);
    lv_area_t band;
    band.x1 = area.x1 + std::max(0, (int) lroundf((start - 0.5f) * step));
    band.x2 = area.x1 + std::min(data.width, (int) lroundf((k + 0.5f) * step));
    band.y1 = area.y1;
    band.y2 = area.y1 + data.height;
#if LVGL_VERSION_MAJOR >= 9
    lv_draw_rect(layer, &shade, &band);
#else
    lv_draw_rect(context, &shade, &band);
#endif
  }
  for (int k = 1; k < data.count; ++k) {
    const auto &a = data.points[k - 1];
    const auto &b = data.points[k];
    if (a.x < 0 || b.x < 0) continue;
#if LVGL_VERSION_MAJOR >= 9
    dsc.p1 = {a.x + area.x1, a.y + area.y1};
    dsc.p2 = {b.x + area.x1, b.y + area.y1};
    lv_draw_line(layer, &dsc);
#else
    lv_point_t from = {static_cast<lv_coord_t>(a.x + area.x1), static_cast<lv_coord_t>(a.y + area.y1)};
    lv_point_t to = {static_cast<lv_coord_t>(b.x + area.x1), static_cast<lv_coord_t>(b.y + area.y1)};
    lv_draw_line(context, &dsc, &from, &to);
#endif
  }
}

// Interpolate missing samples between observations, and hold the last value
// for a trailing gap. Leave everything before the first observation blank.
// These are display points only; history and energy accounting are untouched.
inline bool trace(lv_obj_t *line, Trace &data, int lvl, int c, int m, int count, int w, int h, float mn, float mx) {
  if (!data.bound) {
    lv_line_set_points(line, nullptr, 0);
    lv_obj_add_event_cb(line, draw_trace, LV_EVENT_DRAW_MAIN, &data);
    data.bound = true;
  }
  int first = -1, previous = -1;
  data.count = count;
  data.width = w;
  data.height = h;
  for (int k = 0; k < count; ++k) {
    float v, i;
    data.estimated[k] = false;
    if (get(lvl, c, N - count + k, v, i)) {
      data.points[k].x = (int) lroundf((float) k * w / (count - 1));
      data.points[k].y = (int) lroundf(h - (metric(m, v, i) - mn) / (mx - mn) * h);
      if (first < 0) first = k;
      if (previous >= 0) {
        for (int j = previous + 1; j < k; ++j) {
          const float t = static_cast<float>(j - previous) / (k - previous);
          data.points[j].x = (int) lroundf((float) j * w / (count - 1));
          data.points[j].y = (int) lroundf(data.points[previous].y +
                                         (data.points[k].y - data.points[previous].y) * t);
          data.estimated[j] = true;
        }
      }
      previous = k;
    } else {
      data.points[k] = {-1, -1};
    }
  }
  if (previous >= 0) {
    for (int k = previous + 1; k < count; ++k) {
      data.points[k].x = (int) lroundf((float) k * w / (count - 1));
      data.points[k].y = data.points[previous].y;
      data.estimated[k] = true;
    }
  }
  if (first < 0 || first == count - 1) {
    lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    return false;
  }
  lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
  lv_obj_invalidate(line);
  return true;
}

}  // namespace pwr::ui
