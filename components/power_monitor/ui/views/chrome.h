#pragma once
// chrome.h - persistent chrome: header title/status/rule and the tab bar.
#include "lvgl.h"
#include "../status.h"
#include "../theme.h"

namespace pwr::ui {

constexpr int PAGE_COUNT = 4;
inline const char *const PAGE_TITLES[PAGE_COUNT] = {"PWR·MON", "SCOPE", "STATS", "SETTINGS"};

struct ChromeWidgets {
  lv_obj_t *title;
  lv_obj_t *status;
  lv_obj_t *rule;
  lv_obj_t *tab_lbl[PAGE_COUNT];
  lv_obj_t *tab_underline[PAGE_COUNT];
  bool bound;
};
inline ChromeWidgets chrome_widgets{};

// Highlights tab `idx`, sets the title and positions the status text. Returns false for an invalid index.
inline bool select_tab(int idx) {
  const ChromeWidgets &W = chrome_widgets;
  if (!W.bound || idx < 0 || idx >= PAGE_COUNT) return false;

  lv_label_set_text(W.title, PAGE_TITLES[idx]);
  for (int i = 0; i < PAGE_COUNT; i++) {
    const bool active = (i == idx);
    lv_obj_set_style_text_color(W.tab_lbl[i], lv_color_hex(active ? COL_TEXT : COL_DIM), 0);
    if (active) {
      lv_obj_clear_flag(W.tab_underline[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(W.tab_underline[i], LV_OBJ_FLAG_HIDDEN);
    }
  }

  // Status placement: right-aligned on Overview, left of the chips elsewhere.
  lv_obj_set_x(W.status, idx == 0 ? 268 : 96);
  lv_obj_set_style_text_align(W.status, idx == 0 ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_LEFT, 0);
  return true;
}

inline void render_header(const char *clock) {
  const ChromeWidgets &W = chrome_widgets;
  if (!W.bound) return;
  const Status s = status(clock);
  const lv_color_t col = lv_color_hex(s.level == 2 ? COL_CRIT : (s.level == 1 ? COL_WARN : COL_DIM));
  lv_label_set_text(W.status, s.msg);
  lv_obj_set_style_text_color(W.status, col, 0);
  lv_obj_set_style_bg_color(W.rule, s.level ? col : lv_color_hex(COL_RULE), 0);
}

}  // namespace pwr::ui