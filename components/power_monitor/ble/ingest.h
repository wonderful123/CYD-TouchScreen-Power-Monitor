#pragma once
// ingest.h - applies a decoded Packet to the live model (smoothing, peaks, link freshness).
#include <cmath>
#include <vector>
#include "decoder.h"
#include "../model/live.h"
#include "../model/peaks.h"

namespace pwr {

constexpr float POWER_EMA_ALPHA = 0.25f;  // exponential power smoothing weight

inline void apply_packet(const Packet &p) {
  for (int c = 0; c < CH; c++) {
    const RawChannel &r = p.ch[c];
    if (!r.valid) {
      live.valid[c] = false;
      continue;
    }
    // Only the battery current is signed (charge +, discharge -); the others are magnitudes.
    const float i = (c == CH_BATTERY) ? r.i : fabsf(r.i);
    const float w = r.v * i;

    peak_update(c, r.v, i);

    if (!live.valid[c]) {
      live.w[c] = w;
    } else {
      live.w[c] += POWER_EMA_ALPHA * (w - live.w[c]);
    }
    live.valid[c] = true;
    live.v[c] = r.v;
    live.i[c] = i;
    live.ah[c] = r.ah;
  }
  note_packet(p.seq);
}

// Decode a raw advertisement and apply it. Returns false (and changes nothing) if it was not a power packet.
inline bool ingest_advert(const std::vector<uint8_t> &payload) {
  const Packet p = decode_packet(payload);
  if (!p.ok) return false;
  apply_packet(p);
  return true;
}

}  // namespace pwr