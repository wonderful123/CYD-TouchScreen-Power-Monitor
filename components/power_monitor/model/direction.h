#pragma once
#include "config.h"

namespace pwr {
// Store raw signed readings; apply orientation only when reading them. This
// reinterprets live values and every history level without losing samples.
inline bool reversed[CH]{};
inline float directed(int channel, float value) {
  return reversed[channel] ? -value : value;
}
}  // namespace pwr
