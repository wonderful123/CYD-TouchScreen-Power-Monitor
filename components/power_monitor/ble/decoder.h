#pragma once
// decoder.h - BLE manufacturer-data payload -> Packet. Pure function, no state.
//
// Payload layout (ESPHome already strips the 2-byte company ID):
//   x[0]      = protocol version (2)
//   x[1]      = packet type (1 = power readings)
//   x[2..9]   = charging channel      (u16 mV, i16 10 mA, i32 coulombs, little endian)
//   x[10..17] = distribution channel
//   x[18..25] = battery channel
//   x[26]     = rolling sequence number (optional, v2+)
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "../model/config.h"

namespace pwr {

constexpr uint8_t PROTOCOL_VERSION = 2;
constexpr uint8_t PACKET_TYPE_POWER = 1;
constexpr size_t FIRST_CHANNEL_OFFSET = 2;
constexpr size_t CHANNEL_BYTES = 8;
constexpr size_t MIN_PACKET_BYTES = FIRST_CHANNEL_OFFSET + CH * CHANNEL_BYTES;  // 26
constexpr size_t SEQ_PACKET_BYTES = MIN_PACKET_BYTES + 1;                       // 27

struct RawChannel {
  bool valid;  // false = sensor reported "channel offline"
  float v;     // volts
  float i;     // amps, signed exactly as sent
  float ah;    // |coulomb counter| in Ah
};

struct Packet {
  bool ok;  // false = not a power packet / too short
  int seq;  // -1 = payload carries no sequence byte
  RawChannel ch[CH];
};

inline Packet decode_packet(const std::vector<uint8_t> &x) {
  Packet p{};
  p.seq = -1;
  if (x.size() < MIN_PACKET_BYTES || x[0] != PROTOCOL_VERSION || x[1] != PACKET_TYPE_POWER) return p;

  for (int c = 0; c < CH; c++) {
    const size_t o = FIRST_CHANNEL_OFFSET + (size_t) c * CHANNEL_BYTES;
    RawChannel &r = p.ch[c];

    const uint16_t mv = (uint16_t) (x[o] | (x[o + 1] << 8));
    if (mv == INVALID_MV) {
      r = RawChannel{false, 0.0f, 0.0f, 0.0f};
      continue;
    }
    const int16_t ma = (int16_t) (x[o + 2] | (x[o + 3] << 8));
    const int32_t coulombs = (int32_t) ((uint32_t) x[o + 4] | ((uint32_t) x[o + 5] << 8) |
                                        ((uint32_t) x[o + 6] << 16) | ((uint32_t) x[o + 7] << 24));
    r.valid = true;
    r.v = mv / 1000.0f;
    r.i = ma / 100.0f;  // 10 mA LSB -> A
    r.ah = fabsf(coulombs / 3600.0f);
  }

  // Sequence = last byte of a 27+ byte payload (kept as before; the layout above says x[26]).
  p.seq = x.size() >= SEQ_PACKET_BYTES ? (int) x[x.size() - 1] : -1;
  p.ok = true;
  return p;
}

}  // namespace pwr