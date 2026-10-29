#pragma once
// format.h - number-to-text helper for display labels.
#include <cstddef>
#include <cstdio>
#include <cmath>

namespace pwr::ui {

inline void fmt(char *out, size_t n, float v, int dec, bool sign = false) {
  // Suppress misleading negative zero after display rounding.
  if (std::fabs(v) < 0.5f * std::pow(10.0f, -dec)) v = 0.0f;
  snprintf(out, n, sign ? "%+.*f" : "%.*f", dec, v);
}

}  // namespace pwr::ui