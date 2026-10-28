#pragma once
// scope.h - Scope page: V/A/W traces, axes, selector chips, touch cursor and readout.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "lvgl.h"
#include "../../model/config.h"
#include "../../model/history.h"
#include "../format.h"
#include "../plot.h"
#include "../theme.h"

namespace pwr::ui {

constexpr int SCOPE_METRICS = 3;  // 0 = V, 1 = A, 2 = W
constexpr int SCOPE_RANGES = 4;   // 0 = 10 min, 1 = 1 h, 2 = 6 h, 3 = 24 h

struct ScopeWidgets {
  lv_obj_t *line[CH];                      // trace line objects
  lv_obj_t *y_label[5];
  lv_obj_t *x_start, *x_mid;
  lv_obj_t *metric_chip[SCOPE_METRICS];
  lv_obj_t *metric_label[SCOPE_METRICS];
  lv_obj_t *range_chip[SCOPE_RANGES];
  lv_obj_t *range_label[SCOPE_RANGES];
  lv_obj_t *cross;                         // cursor line
  lv_obj_t *time_label;
  lv_obj_t *value[CH];                     // readout per channel
  lv_obj_t *touch;                         // touch surface over the plot
  bool bound;
};
inline ScopeWidgets scope_widgets{};

// Touch x (screen px) -> history index 0..N-1, clamped to the plot area.
inline int cursor_index_from_x(int x) {
  static_assert(N > 1, "N must be greater than one");
  const int j = (int) lroundf((x - (float) SCOPE_X) * (N - 1) / (float) SCOPE_W);
  return std::max(0, std::min(N - 1, j));
}

// Polls the touch surface. Updates *cursor (-1 = no cursor) and returns true when it changed.
inline bool poll_scope_cursor(int *cursor) {
  const ScopeWidgets &W = scope_widgets;
  if (!W.bound) return false;

  bool changed = false;
  if (lv_obj_has_state(W.touch, LV_STATE_PRESSED)) {
    lv_indev_t *indev = lv_indev_get_next(NULL);
    if (indev != nullptr) {
      lv_point_t p;
      lv_indev_get_point(indev, &p);
      const int j = cursor_index_from_x(p.x);
      if (j != *cursor) {
        *cursor = j;
        changed = true;
      }
    }
  } else if (*cursor >= 0) {
    *cursor = -1;
    changed = true;
  }
  return changed;
}

// Seconds-ago -> "now", "-5m30s" or "-1h20m".
inline void format_age(int sec, char *out, size_t n) {
  if (sec == 0) {
    snprintf(out, n, "now");
  } else if (sec >= 3600) {
    snprintf(out, n, "-%dh%02dm", sec / 3600, (sec % 3600) / 60);
  } else {
    snprintf(out, n, "-%dm%02ds", sec / 60, sec % 60);
  }
}

inline void render_scope(int m, int lv, int cur) {
  const ScopeWidgets &W = scope_widgets;
  if (!W.bound || m < 0 || m >= SCOPE_METRICS || lv < 0 || lv >= SCOPE_RANGES) return;

  const int dec = (m == 0) ? 3 : ((m == 1) ? 2 : 1);
  char b[32], b2[40];

  // traces + y axis
  float mn, mx;
  scale(lv, m, N, (1 << CH) - 1, &mn, &mx);
  for (int k = 0; k < CH; k++) trace(W.line[k], pts_scope[k], lv, k, m, N, SCOPE_W, SCOPE_H, mn, mx);
  for (int i = 0; i < 5; i++) {
    fmt(b, sizeof(b), mx - (mx - mn) * i / 4.0f, dec);
    lv_label_set_text(W.y_label[i], b);
  }

  // x axis
  static const char *XL[SCOPE_RANGES] = {"-10m", "-1h", "-6h", "-24h"};
  static const char *XM[SCOPE_RANGES] = {"-5m", "-30m", "-3h", "-12h"};
  lv_label_set_text(W.x_start, XL[lv]);
  lv_label_set_text(W.x_mid, XM[lv]);

  // selector chips
  for (int i = 0; i < SCOPE_METRICS; i++) {
    const bool active = (i == m);
    lv_obj_set_style_border_color(W.metric_chip[i], lv_color_hex(active ? COL_ACCENT : COL_RULE), 0);
    lv_obj_set_style_text_color(W.metric_label[i], lv_color_hex(active ? COL_TEXT : COL_DIM), 0);
  }
  for (int i = 0; i < SCOPE_RANGES; i++) {
    const bool active = (i == lv);
    lv_obj_set_style_border_color(W.range_chip[i], lv_color_hex(active ? COL_ACCENT : COL_RULE), 0);
    lv_obj_set_style_text_color(W.range_label[i], lv_color_hex(active ? COL_TEXT : COL_DIM), 0);
  }

  // cursor
  const int j = (cur < 0) ? (N - 1) : cur;
  if (cur < 0) {
    lv_obj_add_flag(W.cross, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_set_x(W.cross, SCOPE_X + (int) lroundf(j * (float) SCOPE_W / (N - 1)));
    lv_obj_clear_flag(W.cross, LV_OBJ_FLAG_HIDDEN);
  }

  // readout
  format_age((int) lroundf(SPAN_S[lv] * (1.0f - (float) j / (N - 1))), b, sizeof(b));
  lv_label_set_text(W.time_label, b);

  static const char *UNITS[SCOPE_METRICS] = {"V", "A", "W"};
  for (int k = 0; k < CH; k++) {
    float v, i;
    if (get(lv, k, j, v, i)) {
      fmt(b, sizeof(b), metric(m, v, i), dec, m != 0);
      snprintf(b2, sizeof(b2), "%s%s", b, UNITS[m]);
    } else {
      snprintf(b2, sizeof(b2), "--");
    }
    lv_label_set_text(W.value[k], b2);
  }
}

}  // namespace pwr::ui