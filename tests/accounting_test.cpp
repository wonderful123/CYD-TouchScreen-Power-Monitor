#define PWR_HOST_BUILD
#include "components/power_monitor/model/stats.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
constexpr int BUCKET_SLOTS = pwr::FACTOR[1] * pwr::FACTOR[2] * pwr::FACTOR[3];
constexpr float SLOT_H = pwr::SLOT_MS / 3600000.0f;
void near(float actual, float expected) {
  if (std::fabs(actual - expected) > 0.0001f) {
    std::fprintf(stderr, "Expected %.8f, got %.8f\n", expected, actual);
    assert(false);
  }
}
void sample(float v, float i, bool valid = true) {
  float volts[pwr::CH] = {v, v, v}, amps[pwr::CH] = {i, i, i};
  bool ok[pwr::CH] = {valid, valid, valid};
  if (valid) for (int c = 0; c < pwr::CH; ++c) pwr::peak_update(c, v, i);
  pwr::push(0, volts, amps, ok);
}
}

// Each scenario runs in its own process so the inline model state starts clean.
int main(int argc, char **argv) {
  assert(argc == 2);
  const char *scenario = argv[1];
  if (std::strcmp(scenario, "missing") == 0) {
    sample(12, 10);
    for (int n = 1; n < BUCKET_SLOTS; ++n) sample(12, 10, false);
    pwr::compute_stats();
    near(pwr::st24[2].hours, SLOT_H);
    near(pwr::st24[2].ah, 10 * SLOT_H);
    near(pwr::st24[2].wh, 120 * SLOT_H);
  } else if (std::strcmp(scenario, "directions") == 0) {
    for (int n = 0; n < BUCKET_SLOTS; ++n) sample(12, n < BUCKET_SLOTS / 2 ? 10 : -10);
    pwr::compute_stats();
    near(pwr::bal_ah_in, 0.5f);
    near(pwr::bal_ah_out, 0.5f);
    near(pwr::bal_wh_in, 6);
    near(pwr::bal_wh_out, 6);
    near(pwr::st24[2].ah, 0);  // Signed net; IN and OUT remain separate.
    near(pwr::st24[2].wh, 0);
  } else if (std::strcmp(scenario, "correlation") == 0) {
    for (int n = 0; n < BUCKET_SLOTS; ++n) sample(n % 2 ? 24 : 12, n % 2 ? 0 : 10);
    pwr::compute_stats();
    near(pwr::st24[2].wh, 6);  // Mean(V * I), not mean(V) * mean(I).
  } else if (std::strcmp(scenario, "partial") == 0) {
    sample(12, -10);
    pwr::compute_stats();
    assert(pwr::st24[2].any);
    near(pwr::st24[2].hours, SLOT_H);
    near(pwr::bal_ah_out, 10 * SLOT_H);
    near(pwr::bal_wh_out, 120 * SLOT_H);
    // Closing the bucket must not double-count its previously visible integral.
    for (int n = 1; n < BUCKET_SLOTS; ++n) sample(12, 0, false);
    pwr::compute_stats();
    near(pwr::bal_wh_out, 120 * SLOT_H);
  } else if (std::strcmp(scenario, "expiry") == 0) {
    sample(12, 10);
    for (int n = 1; n < BUCKET_SLOTS * (pwr::N + 1); ++n) sample(12, 0, false);
    pwr::compute_stats();
    near(pwr::st24[2].hours, 0);
    near(pwr::st24[2].ah, 0);
    near(pwr::st24[2].wh, 0);
    assert(!pwr::st24[2].any);
  } else {
    assert(false && "Unknown scenario");
  }
  std::printf("PASS %s\n", scenario);
}
