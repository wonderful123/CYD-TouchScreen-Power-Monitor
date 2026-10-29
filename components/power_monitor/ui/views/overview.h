#pragma once
// overview.h - Overview page: per-channel V/A/W, windowed min/max/peak and sparklines.
#include <cmath>
#include <cstdio>
#include "lvgl.h"
#include "../../model/config.h"
#include "../../model/live.h"
#include "../../model/stats.h"
#include "../format.h"
#include "../plot.h"
#include "../theme.h"

namespace pwr::ui {

struct OverviewRow {
  lv_obj_t *v, *a, *w;            // big live values
  lv_obj_t *ah, *wh;              // left column: Ah counter, 24 h Wh
  lv_obj_t *v1, *v2;              // voltage range: window / 24 h
  lv_obj_t *a1, *a2;              // current peak: window / 24 h
  lv_obj_t *w1, *w2;              // power: window avg / 24 h peak
  lv_obj_t *spark;                // line widget object
  lv_obj_t *direction;            // NEG / MIXED over the plotted window
};

struct OverviewWidgets {
  lv_obj_t *spark_hdr;
  OverviewRow row[CH];
  bool bound;
};
inline OverviewWidgets overview_widgets{};

inline const char *const SPARK_HDR[SPARK_MODES] = {"|W| · LAST 5 MIN", "|W| · LAST 1 H", "|W| · LAST 6 H", "|W| · LAST 24 H"};

inline void clear_overview_row(const OverviewRow &r) {
  lv_label_set_text(r.v, "--");
  lv_label_set_text(r.a, "--");
  lv_label_set_text(r.w, "--");
  lv_label_set_text(r.ah, "Ah --");
  lv_label_set_text(r.wh, "24h --");
  lv_label_set_text(r.v1, "");
  lv_label_set_text(r.v2, "");
  lv_label_set_text(r.a1, "");
  lv_label_set_text(r.a2, "");
  lv_label_set_text(r.w1, "");
  lv_label_set_text(r.w2, "");
  lv_obj_add_flag(r.spark, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_text(r.direction, "");
}

inline void render_overview_row(int k, const OverviewRow &r, int spark_level, int spark_count) {
  char b[40];
  const bool sign = true;
  const float v = live.v[k], i = directed(k, live.i[k]), w = directed(k, live.w[k]);
  const lv_color_t col = lv_color_hex(fabsf(i) < 0.05f ? COL_DIM : COL_TEXT);

  fmt(b, sizeof(b), v, 2);
  lv_label_set_text(r.v, b);
  fmt(b, sizeof(b), i, 2, sign);
  lv_label_set_text(r.a, b);
  fmt(b, sizeof(b), w, 0, sign);
  lv_label_set_text(r.w, b);
  lv_obj_set_style_text_color(r.v, col, 0);
  lv_obj_set_style_text_color(r.a, col, 0);
  lv_obj_set_style_text_color(r.w, col, 0);

  snprintf(b, sizeof(b), "|Ah| %.1f", live.ah[k]);
  lv_label_set_text(r.ah, b);

  const Stats &s = st24[k];
  const Win w5 = win5(k);

  snprintf(b, sizeof(b), s.any ? "24h %+.0fWh" : "24h --", s.wh);
  lv_label_set_text(r.wh, b);

  if (w5.any) snprintf(b, sizeof(b), "%.2f–%.2f", w5.vmin, w5.vmax);
  else b[0] = '\0';
  lv_label_set_text(r.v1, b);

  if (s.any) snprintf(b, sizeof(b), "24h %.1f–%.1f", s.vmin, s.vmax);
  else snprintf(b, sizeof(b), "24h --");
  lv_label_set_text(r.v2, b);

  if (w5.any) snprintf(b, sizeof(b), "pk %.1f", w5.ipk);
  else b[0] = '\0';
  lv_label_set_text(r.a1, b);

  if (s.any) snprintf(b, sizeof(b), "24h pk %.1f", s.ipk);
  else snprintf(b, sizeof(b), "24h pk --");
  lv_label_set_text(r.a2, b);

  if (w5.any) snprintf(b, sizeof(b), "avg %+.0f", w5.wavg);
  else b[0] = '\0';
  lv_label_set_text(r.w1, b);

  if (s.any) snprintf(b, sizeof(b), "24h pk %.0f", s.wpk);
  else snprintf(b, sizeof(b), "24h pk --");
  lv_label_set_text(r.w2, b);

  float mn, mx;
  scale(spark_level, 2, spark_count, 1 << k, &mn, &mx);
  trace(r.spark, pts_spark[k], spark_level, k, 2, spark_count, SPARK_W, SPARK_H, mn, mx);
  lv_label_set_text(r.direction, chart_direction(spark_level, k, spark_count));
}

inline void render_overview(int spark_mode) {
  const OverviewWidgets &W = overview_widgets;
  if (!W.bound || spark_mode < 0 || spark_mode >= SPARK_MODES) return;

  compute_stats();
  lv_label_set_text(W.spark_hdr, SPARK_HDR[spark_mode]);
  const bool ok_all = fresh();

  for (int k = 0; k < CH; k++) {
    if (!ok_all || !live.valid[k]) {
      clear_overview_row(W.row[k]);
    } else {
      render_overview_row(k, W.row[k], SPARK_LVL[spark_mode], SPARK_CNT[spark_mode]);
    }
  }
}

}  // namespace pwr::ui