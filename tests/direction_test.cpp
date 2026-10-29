// Run: g++ -std=c++17 -DPWR_HOST_BUILD -I. tests/direction_test.cpp -o /tmp/direction_test && /tmp/direction_test
#include <cassert>
#include <cmath>
#include <cstring>
#include "components/power_monitor/ble/ingest.h"
#include "components/power_monitor/model/sampler.h"
#include "components/power_monitor/model/stats.h"
#include "components/power_monitor/ui/format.h"
#include "components/power_monitor/ui/chart_values.h"

bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }
int main() {
  // Wire-format signed current must survive decoding and ingestion on ALL channels.
  std::vector<uint8_t> bytes(27, 0);
  bytes[0] = 2; bytes[1] = 1; bytes[26] = 1;
  for (int c = 0; c < pwr::CH; ++c) {
    int o = 2 + 8*c;
    bytes[o] = 0xe0; bytes[o+1] = 0x2e; // 12 V
    bytes[o+2] = 0x38; bytes[o+3] = 0xff; // -2 A
  }
  assert(pwr::ingest_advert(bytes));
  pwr::tick();
  for (int c = 0; c < pwr::CH; ++c) {
    assert(near(pwr::live.i[c], -2));
    assert(near(pwr::live.w[c], -24));
    float v, i;
    assert(pwr::get(0, c, pwr::N-1, v, i));
    assert(near(v, 12) && near(i, -2));
    pwr::reversed[c] = true;
    assert(pwr::get(0, c, pwr::N-1, v, i) && near(i, 2));
    assert(near(pwr::directed(c, pwr::live.w[c]), 24));
    // Other channels and raw storage must be unchanged.
    assert(near(pwr::live.i[c], -2));
    pwr::reversed[c] = false;
  }
  float volts[] = {12,12,12}, amps[] = {-2,3,-4};
  bool valid[] = {true,true,true};
  pwr::push(pwr::TOP_LEVEL, volts, amps, valid);
  pwr::compute_stats();
  assert(pwr::st24[0].ah < 0 && pwr::st24[0].wh < 0);
  assert(near(pwr::bal_ah_in, 0) && pwr::bal_ah_out > 0);
  const float out = pwr::bal_ah_out;
  pwr::reversed[2] = true;
  pwr::compute_stats();
  assert(near(pwr::bal_ah_in, out) && near(pwr::bal_ah_out, 0));
  assert(pwr::st24[0].wh < 0 && pwr::st24[1].wh > 0 && pwr::st24[2].wh > 0);
  pwr::host_now_ms = pwr::SLOT_MS;
  pwr::tick(); // history must not apply the direction twice
  float v, i;
  assert(pwr::get(0, 2, pwr::N-1, v, i) && near(i, 2));
  assert(near(pwr::ui::chart_value(1, 12, -2), 2));
  assert(near(pwr::ui::chart_value(2, 12, -2), 24));
  assert(near(pwr::ui::chart_value(2, 12, 2), 24));
  assert(std::strcmp(pwr::ui::chart_direction(0, 0, pwr::N), "NEG") == 0);
  assert(std::strcmp(pwr::ui::chart_direction(0, 2, pwr::N), "") == 0);
  float positive[] = {2, 3, 4};
  pwr::push(0, volts, positive, valid);
  assert(std::strcmp(pwr::ui::chart_direction(0, 0, pwr::N), "MIXED") == 0);
  assert(std::strcmp(pwr::ui::chart_direction(0, 2, pwr::N), "MIXED") == 0);
  char label[24];
  pwr::ui::fmt(label, sizeof(label), -0.004f, 2, true);
  assert(std::strcmp(label, "+0.00") == 0);
  pwr::ui::fmt(label, sizeof(label), -2.0f, 2, true);
  assert(std::strcmp(label, "-2.00") == 0);
  pwr::ui::fmt(label, sizeof(label), 2.0f, 2, true);
  assert(std::strcmp(label, "+2.00") == 0);
}
