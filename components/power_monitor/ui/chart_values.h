#pragma once
#include <cmath>
#include "../model/history.h"

namespace pwr::ui {
// Charts show magnitude, while numeric readouts retain the signed value.
inline float chart_value(int metric_id, float voltage, float current) {
  return std::fabs(metric(metric_id, voltage, current));
}

// Describe direction over the displayed window, rather than only its latest point.
// Zero has no direction. Missing samples do not imply positive flow.
inline const char *chart_direction(int level, int channel, int count) {
  bool positive = false, negative = false;
  for (int j = N - count; j < N; ++j) {
    float v, i;
    if (!get(level, channel, j, v, i)) continue;
    positive |= i > 0.0f;
    negative |= i < 0.0f;
  }
  return negative ? (positive ? "MIXED" : "NEG") : "";
}
}  // namespace pwr::ui
