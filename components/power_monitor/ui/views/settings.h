#pragma once
// settings.h - Settings page: stepper rules, value text, summary line, uptime, scroll setup.
// Hardware side effects (backlight, globals, timers) stay in YAML; this file only decides values and draws labels.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include "lvgl.h"
#include "../theme.h"

namespace pwr::ui {

// ---------------------------------------------------------------- stepper rules
// Values match the `row_which` numbers used by the settings page rows.
enum class Setting : int {
  Brightness = 0,
  DimLevel = 1,
  DimTimeout = 2,
  SleepTimeout = 3,      // delay after dimming before light sleep
  WakeInterval = 4,      // time between BLE sampling wake-ups
  ScreenOffTimeout = 5,  // delay after dimming before screen off
};
constexpr int SETTING_COUNT = 6;

struct SettingSpec {
  int lo, hi, step;
};
inline constexpr SettingSpec SETTING_SPEC[SETTING_COUNT] = {
    {10, 100, 5},   // Brightness (%)
    {20, 100, 5},   // DimLevel (%)
    {5, 120, 5},    // DimTimeout (s)
    {0, 3600, 60},  // SleepTimeout (s)
    {5, 120, 5},    // WakeInterval (s)
    {0, 3600, 30},  // ScreenOffTimeout (s)
};

inline bool setting_valid(int which) { return which >= 0 && which < SETTING_COUNT; }

// Next value on the step grid, one step up (delta > 0) or down, clamped to the setting's range.
inline int setting_step(Setting which, int cur, int delta) {
  const SettingSpec &sp = SETTING_SPEC[(int) which];
  const int v = (delta > 0) ? (cur / sp.step + 1) * sp.step : ((cur - 1) / sp.step) * sp.step;
  return std::max(sp.lo, std::min(sp.hi, v));
}

// ---------------------------------------------------------------- text formatting
// 0 -> "OFF", <60 -> "30 s", whole minutes -> "5 min", else "1m 30s"
inline void format_secs(int s, char *buf, size_t n) {
  if (s <= 0) {
    snprintf(buf, n, "OFF");
  } else if (s < 60) {
    snprintf(buf, n, "%d s", s);
  } else if (s % 60 == 0) {
    snprintf(buf, n, "%d min", s / 60);
  } else {
    snprintf(buf, n, "%dm %02ds", s / 60, s % 60);
  }
}

// compact: 0 -> "off", whole minutes -> "10m", else "45s"
inline void format_secs_short(int s, char *buf, size_t n) {
  if (s <= 0) {
    snprintf(buf, n, "off");
  } else if (s % 60 == 0) {
    snprintf(buf, n, "%dm", s / 60);
  } else {
    snprintf(buf, n, "%ds", s);
  }
}

inline void format_uptime(uint64_t total_s, char *buf, size_t n) {
  const unsigned d = (unsigned) (total_s / 86400ULL);
  const unsigned h = (unsigned) ((total_s % 86400ULL) / 3600ULL);
  const unsigned m = (unsigned) ((total_s % 3600ULL) / 60ULL);
  const unsigned s = (unsigned) (total_s % 60ULL);
  if (d > 0) {
    snprintf(buf, n, "%ud %02uh %02um %02us", d, h, m, s);
  } else if (h > 0) {
    snprintf(buf, n, "%uh %02um %02us", h, m, s);
  } else {
    snprintf(buf, n, "%um %02us", m, s);
  }
}

// Backlight brightness 0..1 -> displayed percent (never below the minimum setting).
inline int brightness_to_pct(float brightness) {
  return std::max(SETTING_SPEC[(int) Setting::Brightness].lo,
                  std::min(SETTING_SPEC[(int) Setting::Brightness].hi, (int) lroundf(brightness * 100.0f)));
}

// ---------------------------------------------------------------- widgets
struct SettingsValues {
  int brightness_pct;
  int dim_level;
  int dim_timeout;
  int screen_off_timeout;
  int sleep_timeout;
  int wake_interval;
  bool screen_off_in_sleep;
};

struct SettingsWidgets {
  lv_obj_t *scroll;
  lv_obj_t *bright_val, *dim_val, *timeout_val, *screen_off_val, *sleep_val, *wake_val;
  lv_obj_t *sleep_switch;
  lv_obj_t *summary;
  lv_obj_t *uptime;
  lv_obj_t *reboot_reason;
  bool bound;
};
inline SettingsWidgets settings_widgets{};

// Draws every settings value and the summary line from the given state.
inline void render_settings(const SettingsValues &s) {
  const SettingsWidgets &W = settings_widgets;
  if (!W.bound) return;
  char buf[16];

  lv_label_set_text_fmt(W.bright_val, "%d%%", s.brightness_pct);
  lv_label_set_text_fmt(W.dim_val, "%d%%", s.dim_level);

  format_secs(s.dim_timeout, buf, sizeof(buf));
  lv_label_set_text(W.timeout_val, buf);
  format_secs(s.screen_off_timeout, buf, sizeof(buf));
  lv_label_set_text(W.screen_off_val, buf);
  format_secs(s.sleep_timeout, buf, sizeof(buf));
  lv_label_set_text(W.sleep_val, buf);
  format_secs(s.wake_interval, buf, sizeof(buf));
  lv_label_set_text(W.wake_val, buf);

  if (s.screen_off_in_sleep) {
    lv_obj_add_state(W.sleep_switch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(W.sleep_switch, LV_STATE_CHECKED);
  }

  char off_buf[12], sleep_buf[12];
  format_secs_short(s.screen_off_timeout, off_buf, sizeof(off_buf));
  format_secs_short(s.sleep_timeout, sleep_buf, sizeof(sleep_buf));
  lv_label_set_text_fmt(W.summary, "dim %d%% @%ds | off %s after dim | sleep %s after dim | wake %ds",
                        std::min(s.brightness_pct, s.dim_level), s.dim_timeout, off_buf, sleep_buf, s.wake_interval);
}

// Page on_load: vertical-only scrolling with hard stops, slim scrollbar, always open at the top.
inline void settings_page_enter() {
  const SettingsWidgets &W = settings_widgets;
  if (!W.bound) return;
  lv_obj_set_scroll_dir(W.scroll, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(W.scroll, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_clear_flag(W.scroll, LV_OBJ_FLAG_SCROLL_ELASTIC);

  lv_obj_set_style_bg_color(W.scroll, lv_color_hex(COL_SCROLLBAR), LV_PART_SCROLLBAR);
  lv_obj_set_style_bg_opa(W.scroll, LV_OPA_COVER, LV_PART_SCROLLBAR);
  lv_obj_set_style_width(W.scroll, 3, LV_PART_SCROLLBAR);
  lv_obj_set_style_radius(W.scroll, 2, LV_PART_SCROLLBAR);
  lv_obj_set_style_pad_right(W.scroll, 2, LV_PART_SCROLLBAR);

  lv_obj_scroll_to_y(W.scroll, 0, LV_ANIM_OFF);
}

// Toggles the "screen off during sleep" switch and returns its new state.
inline bool toggle_sleep_switch() {
  const SettingsWidgets &W = settings_widgets;
  if (!W.bound) return false;
  const bool on = !lv_obj_has_state(W.sleep_switch, LV_STATE_CHECKED);
  if (on) {
    lv_obj_add_state(W.sleep_switch, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(W.sleep_switch, LV_STATE_CHECKED);
  }
  return on;
}

inline void render_system_info(uint64_t total_s, const char *reboot_reason) {
  const SettingsWidgets &W = settings_widgets;
  if (!W.bound) return;
  char buf[32];
  format_uptime(total_s, buf, sizeof(buf));
  lv_label_set_text(W.uptime, buf);
  lv_label_set_text(W.reboot_reason, reboot_reason);
}

}  // namespace pwr::ui
