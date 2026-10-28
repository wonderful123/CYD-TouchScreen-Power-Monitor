#define PWR_HOST_BUILD
#include "components/power_monitor/ui/plot.h"
#include <cassert>
#include <cstdio>

void sample(bool valid) {
  float v[pwr::CH] = {12, 12, 12}, i[pwr::CH] = {1, 1, 1};
  bool ok[pwr::CH] = {valid, valid, valid};
  pwr::push(0, v, i, ok);
}
void draw(lv_obj_t &line) {
  drawn_segments.clear();
  if (line.flags & LV_OBJ_FLAG_HIDDEN) return;
  lv_event_t event{&line};
  assert(line.callback);
  line.callback(&event);
}
int main() {
  lv_obj_t line;
  pwr::ui::Trace data{};
  // Two valid segments, separated by a gap, with missing data at both ends.
  for (bool valid : {false, true, true, false, true, true, false}) sample(valid);
  assert(pwr::ui::trace(&line, data, 0, 0, 0, 7, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.size() == 2);
  // Actual draw coordinates retain time position and widget origin.
  assert(drawn_segments[0].p1.x == 30 && drawn_segments[0].p2.x == 40);
  assert(drawn_segments[1].p1.x == 60 && drawn_segments[1].p2.x == 70);
  assert(drawn_segments[0].p1.y == 80);

  // Redrawing after complete data loss must remove the previous segments.
  for (int n = 0; n < 7; ++n) sample(false);
  assert(!pwr::ui::trace(&line, data, 0, 0, 0, 7, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.empty());

  // Two isolated observations do not create a fabricated connecting line.
  sample(true); sample(false); sample(true);
  assert(!pwr::ui::trace(&line, data, 0, 0, 0, 3, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.empty());

  // A new adjacent reading makes the trace visible again.
  sample(true);
  assert(pwr::ui::trace(&line, data, 0, 0, 0, 3, 60, 100, 10, 14));
  draw(line);
  assert(drawn_segments.size() == 1);
  std::puts("PASS plot gaps, timestamps, clearing, isolated points, recovery");
}
