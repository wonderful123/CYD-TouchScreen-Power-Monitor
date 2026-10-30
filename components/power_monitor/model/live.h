#pragma once
// live.h - latest decoded readings and sensor-link freshness.
#include "clock.h"
#include "config.h"

namespace pwr {

struct Live {
  bool valid[CH];
  float v[CH];
  float i[CH];
  float ah[CH];  // sensor coulomb counter, |Ah|
  float w[CH];   // smoothed power
};
inline Live live{};

inline uint32_t last_rx_ms = 0;
inline int last_seq = -1;
inline bool ever_rx = false;
inline uint32_t wake_ms = 0;
inline bool wake_packet_pending = false;

inline bool fresh() { return ever_rx && (now_ms() - last_rx_ms) < STALE_MS; }

// Call from every light-sleep wake (touch or timer).
inline void note_wake() {
  wake_ms = now_ms();
  wake_packet_pending = true;
}

// seq < 0: payload has no sequence byte, so any packet counts as fresh.
inline void note_packet(int seq) {
  // Sequence numbers can wrap or restart while our receiver is asleep. The
  // first received packet of a wake is an observation even if its byte matches.
  if (wake_packet_pending || seq < 0 || seq != last_seq) last_rx_ms = now_ms();
  wake_packet_pending = false;
  last_seq = seq;
  ever_rx = true;
}

}  // namespace pwr
