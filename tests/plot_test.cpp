#define PWR_HOST_BUILD
#include "components/power_monitor/ui/plot.h"
#include <cassert>
#include <cstdio>
#include <string>

void sample(bool valid, float volts = 12) {
  float v[pwr::CH] = {volts, volts, volts}, i[pwr::CH] = {1, 1, 1};
  bool ok[pwr::CH] = {valid, valid, valid};
  pwr::push(0, v, i, ok);
}
void draw(lv_obj_t &line) {
  drawn_segments.clear();
  shaded_bands.clear();
  if (line.flags & LV_OBJ_FLAG_HIDDEN) return;
  lv_event_t event{&line};
  assert(line.callback);
  line.callback(&event);
}
int main() {
  lv_obj_t line;
  pwr::ui::Trace data{};
  // Leading gaps remain blank; internal and trailing gaps are drawn and shaded.
  for (bool valid : {false, true, true, false, true, true, false}) sample(valid);
  assert(pwr::ui::trace(&line, data, 0, 0, 0, 7, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.size() == 5);
  assert(drawn_segments.front().p1.x == 30);  // Leading slot stays blank.
  assert(drawn_segments.back().p2.x == 80);   // Last value extends to the present.
  assert(shaded_bands.size() == 2);
  assert(!data.estimated[0] && data.estimated[3] && data.estimated[6]);
  for (const auto &band : shaded_bands) {
    assert(band.style.bg_opa == 8);
    assert(band.area.x1 >= 30 && band.area.x2 <= 80);
    assert(band.area.y1 == 30 && band.area.y2 == 130);
  }

  // Complete data loss clears both the previous curve and shading.
  for (int n = 0; n < 7; ++n) sample(false);
  assert(!pwr::ui::trace(&line, data, 0, 0, 0, 7, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.empty() && shaded_bands.empty());

  // Interpolation uses both endpoints rather than holding the previous value.
  sample(true, 10); sample(false); sample(true, 14);
  assert(pwr::ui::trace(&line, data, 0, 0, 0, 3, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.size() == 2 && shaded_bands.size() == 1);
  assert(drawn_segments[0].p1.y == 130);
  assert(drawn_segments[0].p2.y == 80);  // Midpoint = 12V.
  assert(drawn_segments[1].p2.y == 30);
  assert(data.estimated[1] && !data.estimated[0] && !data.estimated[2]);
  float v, i;
  assert(!pwr::get(0, 0, pwr::N - 2, v, i));  // Display interpolation never writes history.

  // One observation is sufficient to extend the trailing gap.
  sample(true, 12); sample(false); sample(false);
  assert(pwr::ui::trace(&line, data, 0, 0, 0, 3, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.size() == 2 && shaded_bands.size() == 1);
  assert(data.points[0].y == data.points[1].y && data.points[1].y == data.points[2].y);

  // Entirely measured windows have no shading, including after an estimated window.
  sample(true); sample(true); sample(true);
  assert(pwr::ui::trace(&line, data, 0, 0, 0, 3, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.size() == 2 && shaded_bands.empty());

  // A lone newest observation must not be drawn backwards into leading gaps.
  sample(false); sample(false); sample(true);
  assert(!pwr::ui::trace(&line, data, 0, 0, 0, 3, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.empty() && shaded_bands.empty());
  // Magnitude scaling must not force zero, and orientation cannot change geometry.
  pwr::clear_history();
  auto current_sample = [](float current) {
    float volts[pwr::CH] = {12, 12, 12}, amps[pwr::CH] = {current, current, current};
    bool ok[pwr::CH] = {true, true, true};
    pwr::push(0, volts, amps, ok);
  };
  current_sample(-10); sample(false); current_sample(-14);
  float mn, mx;
  pwr::ui::scale(0, 1, 3, 1, &mn, &mx);
  assert(mn > 9 && mx > 14 && mx < 15);
  assert(pwr::ui::trace(&line, data, 0, 0, 1, 3, 60, 100, mn, mx));
  const auto a = data.points[0], b = data.points[1], c = data.points[2];
  assert(data.estimated[1]);
  assert(std::string(pwr::ui::chart_direction(0, 0, 3)) == "NEG");
  pwr::reversed[0] = true;
  assert(pwr::ui::trace(&line, data, 0, 0, 1, 3, 60, 100, mn, mx));
  assert(data.points[0].y == a.y && data.points[1].y == b.y && data.points[2].y == c.y);
  assert(std::string(pwr::ui::chart_direction(0, 0, 3)).empty());
  pwr::reversed[0] = false;
  std::puts("PASS interpolation, subtle shading, leading/trailing gaps, clearing, history isolation");
}
