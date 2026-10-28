#pragma once
// status.h - header status line: severity level and message text.
#include <cstdio>
#include "../model/config.h"
#include "../model/live.h"

namespace pwr::ui {

struct Status {
  int level;  // 0 ok, 1 warning, 2 critical
  char msg[48];
};

inline Status status(const char *clock) {
  static const char *names[CH] = {"CHG", "DIST", "BATT"};
  Status s{0, {0}};
  if (!fresh()) {
    s.level = 1;
    snprintf(s.msg, sizeof s.msg, "NO DATA FROM SENSOR");
    return s;
  }
  if (live.valid[CH_BATTERY] && live.v[CH_BATTERY] < LOW_BATT_V) {
    s.level = 2;
    snprintf(s.msg, sizeof s.msg, "LOW BATT %.2fV", live.v[CH_BATTERY]);
    return s;
  }
  int on = 0;
  for (int c = 0; c < CH; c++) on += live.valid[c] ? 1 : 0;
  if (on < CH) {
    s.level = 1;
    int p = snprintf(s.msg, sizeof s.msg, "OFFLINE:");
    for (int c = 0; c < CH; c++)
      if (!live.valid[c] && p < (int) sizeof s.msg - 8) p += snprintf(s.msg + p, sizeof s.msg - p, " %s", names[c]);
    return s;
  }
  snprintf(s.msg, sizeof s.msg, "%s  3/3 ONLINE", clock);
  return s;
}

}  // namespace pwr::ui