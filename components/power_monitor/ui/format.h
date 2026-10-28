#pragma once
// format.h - number-to-text helper for display labels.
#include <cstddef>
#include <cstdio>

namespace pwr::ui {

inline void fmt(char *out, size_t n, float v, int dec, bool sign = false) {
  snprintf(out, n, sign ? "%+.*f" : "%.*f", dec, v);
}

}  // namespace pwr::ui