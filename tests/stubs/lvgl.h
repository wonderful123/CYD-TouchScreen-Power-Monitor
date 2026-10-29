#pragma once
// Small LVGL event/draw recorder for testing plot segments on the host.
#include <cstdint>
#include <vector>
#define LVGL_VERSION_MAJOR 9
constexpr int LV_OBJ_FLAG_HIDDEN = 1, LV_PART_MAIN = 0, LV_EVENT_DRAW_MAIN = 1;
struct lv_point_precise_t { int32_t x, y; };
struct lv_area_t { int32_t x1, y1, x2, y2; };
struct lv_draw_line_dsc_t {
  struct { void *layer; } base{};
  lv_point_precise_t p1{}, p2{};
};
struct lv_draw_rect_dsc_t {
  struct { void *layer; } base{};
  uint32_t bg_color = 0;
  uint8_t bg_opa = 0;
};
struct ShadedBand { lv_draw_rect_dsc_t style; lv_area_t area; };
inline std::vector<ShadedBand> shaded_bands;
inline uint32_t lv_color_hex(uint32_t color) { return color; }
inline void lv_draw_rect_dsc_init(lv_draw_rect_dsc_t *d) { *d = {}; }
inline void lv_draw_rect(void *, lv_draw_rect_dsc_t *d, const lv_area_t *a) {
  shaded_bands.push_back({*d, *a});
}
struct lv_event_t;
struct lv_obj_t {
  int flags = 0;
  void (*callback)(lv_event_t *) = nullptr;
  void *user_data = nullptr;
  lv_area_t area{20, 30, 120, 130};
};
struct lv_event_t { lv_obj_t *target; };
inline std::vector<lv_draw_line_dsc_t> drawn_segments;
inline void *lv_event_get_user_data(lv_event_t *e) { return e->target->user_data; }
inline lv_obj_t *lv_event_get_current_target(lv_event_t *e) { return e->target; }
inline void *lv_event_get_layer(lv_event_t *) { return nullptr; }
inline void lv_obj_get_coords(lv_obj_t *o, lv_area_t *a) { *a = o->area; }
inline void lv_draw_line_dsc_init(lv_draw_line_dsc_t *d) { *d = {}; }
inline void lv_obj_init_draw_line_dsc(lv_obj_t *, int, lv_draw_line_dsc_t *) {}
inline void lv_draw_line(void *, lv_draw_line_dsc_t *d) { drawn_segments.push_back(*d); }
inline void lv_line_set_points(lv_obj_t *, const lv_point_precise_t *, int) {}
inline void lv_obj_add_event_cb(lv_obj_t *o, void (*cb)(lv_event_t *), int, void *data) {
  o->callback = cb; o->user_data = data;
}
inline void lv_obj_add_flag(lv_obj_t *o, int f) { o->flags |= f; }
inline void lv_obj_clear_flag(lv_obj_t *o, int f) { o->flags &= ~f; }
inline void lv_obj_invalidate(lv_obj_t *) {}
