#pragma once
// stats.h - Stats page: 24 h table per channel and the battery balance grid.
#include <cstdio>
#include "lvgl.h"
#include "../../model/config.h"
#include "../../model/stats.h"
#include "../format.h"

namespace pwr::ui {

constexpr int STATS_COLS = 6;   // V min, V max, A pk, W pk, Ah, Wh
constexpr int BALANCE_CELLS = 8;

struct StatsWidgets {
  lv_obj_t *cell[CH][STATS_COLS];
  lv_obj_t *balance[BALANCE_CELLS];
  bool bound;
};
inline StatsWidgets stats_widgets{};

inline void render_stats() {
  const StatsWidgets &W = stats_widgets;
  if (!W.bound) return;

  compute_stats();
  char b[24];

  static const int DEC[STATS_COLS] = {2, 2, 1, 0, 1, 0};
  for (int k = 0; k < CH; k++) {
    const Stats &s = st24[k];
    const float vals[STATS_COLS] = {s.vmin, s.vmax, s.ipk, s.wpk, s.ah, s.wh};
    for (int c = 0; c < STATS_COLS; c++) {
      if (s.any) {
        fmt(b, sizeof(b), vals[c], DEC[c]);
      } else {
        snprintf(b, sizeof(b), "--");
      }
      lv_label_set_text(W.cell[k][c], b);
    }
  }

  const Stats &bt = st24[CH_BATTERY];
  const Stats &ds = st24[CH_DISTRIBUTION];
  const float cv[BALANCE_CELLS] = {bal_ah_in, bal_ah_out, bal_ah_in - bal_ah_out, bt.hours,
                                   bal_wh_in, bal_wh_out, bal_wh_in - bal_wh_out,
                                   (ds.hours > 0.0f) ? (ds.wh / ds.hours) : 0.0f};
  const bool ok[BALANCE_CELLS] = {bt.any, bt.any, bt.any, bt.any, bt.any, bt.any, bt.any, (ds.any && ds.hours > 0.0f)};
  static const int CD[BALANCE_CELLS] = {1, 1, 1, 1, 0, 0, 0, 0};
  static const bool CS[BALANCE_CELLS] = {false, false, true, false, false, false, true, false};

  for (int i = 0; i < BALANCE_CELLS; i++) {
    if (ok[i]) {
      fmt(b, sizeof(b), cv[i], CD[i], CS[i]);
    } else {
      snprintf(b, sizeof(b), "--");
    }
    lv_label_set_text(W.balance[i], b);
  }
}

}  // namespace pwr::ui