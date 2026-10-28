#define PWR_HOST_BUILD
#include "components/power_monitor/model/sampler.h"
#include "components/power_monitor/model/stats.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace {
int failures = 0;
void check(bool ok, const char *message) {
  if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); ++failures; }
}
void near(float got, float expected, float tolerance, const char *message) {
  if (std::fabs(got - expected) > tolerance) {
    std::fprintf(stderr, "FAIL: %s (got %.6f, expected %.6f)\n", message, got, expected);
    ++failures;
  }
}
void packet(uint32_t now, float v = 12, float i = 10) {
  pwr::host_now_ms = now;
  for (int c = 0; c < pwr::CH; ++c) {
    pwr::live.valid[c] = true;
    pwr::live.v[c] = v;
    pwr::live.i[c] = i;
    pwr::peak_update(c, v, i);
  }
  pwr::note_packet(-1);
}
bool read(int level, int j, float &v, float &i, int channel = 0) {
  return pwr::get(level, channel, j, v, i);
}
int valid_count(int level, int channel = 0) {
  int count = 0;
  for (int j = 0; j < pwr::N; ++j) {
    float v, i;
    count += read(level, j, v, i, channel);
  }
  return count;
}
void push_sample(float v, bool valid = true) {
  float volts[pwr::CH] = {v, v, v}, amps[pwr::CH] = {1, 1, 1};
  bool ok[pwr::CH] = {valid, valid, valid};
  pwr::push(0, volts, amps, ok);
}
}

int main(int argc, char **argv) {
  if (argc != 2) return 2;
  const char *scenario = argv[1];
  float v = 0, i = 0;
  if (std::strcmp(scenario, "timeframes") == 0) {
    // More than two days: every ring wraps, including the 24-hour level.
    const int total = 2 * pwr::MAX_FILL + 17;
    for (int n = 0; n < total; ++n) push_sample(10 + (n % 1000) * 0.001f);
    int stride = 1;
    for (int l = 0; l < pwr::LEVELS; ++l) {
      stride *= pwr::FACTOR[l];
      near(pwr::SPAN_S[l], pwr::N * stride * pwr::SLOT_MS / 1000.0f, 0.01f, "timeframe geometry");
      check(pwr::head[l] == (total / stride) % pwr::N, "ring position follows elapsed slots");
      check(valid_count(l) == pwr::N, "filled timeframe retains 240 samples");
      const int end = (total / stride) * stride;
      for (int j = 0; j < pwr::N; ++j) {
        float expected = 0;
        int start = end - (pwr::N - j) * stride;
        for (int k = 0; k < stride; ++k) expected += 10 + ((start + k) % 1000) * 0.001f;
        check(read(l, j, v, i), "wrapped sample exists");
        near(v, expected / stride, 0.0006f, "timeframe average and chronology");
      }
    }
  } else if (std::strcmp(scenario, "awake") == 0) {
    for (uint32_t now = 0; now <= 30000; now += 500) { packet(now); pwr::tick(); }
    check(valid_count(0) == 13, "500ms polls produce one slot per 2.5s, plus initial sample");
    check(valid_count(1) == 2, "awake samples cascade into 15s buckets");
  } else if (std::strcmp(scenario, "sleep") == 0) {
    packet(0, 12); pwr::tick();
    pwr::host_now_ms = 30000; pwr::note_wake();
    packet(30000, 24); pwr::tick();
    check(valid_count(0) == 13, "30s sleep backfills 12 elapsed slots");
    for (int k = 0; k < 13; ++k) {
      check(read(0, pwr::N - 13 + k, v, i), "sleep interpolation exists");
      near(v, 12.0f + k, 0.001f, "sleep interpolation order");
    }
    check(valid_count(1) == 2, "sleep backfill cascades to higher timeframe");
    pwr::compute_stats();
    near(pwr::st24[0].hours, 13 * 2.5f / 3600, 0.00001f, "sleep estimates preserve elapsed duration");
  } else if (std::strcmp(scenario, "missed") == 0) {
    packet(0); pwr::tick();
    for (uint32_t now = 2500; now <= 17500; now += 2500) { pwr::host_now_ms = now; pwr::tick(); }
    check(valid_count(0) == 3, "only first 6s of unchanged data stays fresh");
    check(pwr::head[0] == 8, "missing data still advances time axis");
    packet(20000, 13); pwr::tick();
    check(valid_count(0) == 4, "recovery adds a new sample without filling observed outage");
    for (int j = pwr::N - 6; j < pwr::N - 1; ++j) check(!read(0, j, v, i), "outage remains invalid");
    pwr::compute_stats();
    near(pwr::st24[0].hours, 4 * 2.5f / 3600, 0.00001f, "outage excluded from accounting");
  } else if (std::strcmp(scenario, "wake_timeout") == 0) {
    packet(0); pwr::tick();
    pwr::host_now_ms = 30000; pwr::note_wake(); pwr::tick();
    check(pwr::head[0] == 1, "wake grace defers history while awaiting packet");
    pwr::host_now_ms = 33500; pwr::tick();
    check(pwr::head[0] == 14, "acquisition timeout advances all elapsed slots");
    check(valid_count(0) == 1, "failed acquisition does not fabricate readings");
    packet(35000); pwr::tick();
    check(valid_count(0) == 2, "packet after timeout resumes capture");
  } else if (std::strcmp(scenario, "offline") == 0) {
    for (uint32_t now = 0; now < 360000; now += 2500) {
      packet(now); pwr::live.valid[1] = false; pwr::tick();
    }
    for (int l = 0; l < pwr::LEVELS; ++l) {
      check(valid_count(l, 0) > 0, "online channel cascades");
      check(valid_count(l, 1) == 0, "offline channel stays missing at every timeframe");
    }
    pwr::compute_stats(); near(pwr::st24[1].hours, 0, 0.00001f, "offline channel has no integrated duration");
  } else if (std::strcmp(scenario, "clock_wrap") == 0) {
    uint32_t start = std::numeric_limits<uint32_t>::max() - 5000;
    for (uint32_t elapsed = 0; elapsed <= 20000; elapsed += 500) {
      packet(start + elapsed); pwr::tick();
    }
    check(valid_count(0) == 9, "sampling survives 32-bit millis rollover");
    check(valid_count(1) == 1, "rollover maintains higher-level cadence");
  } else if (std::strcmp(scenario, "sleep_timeframes") == 0) {
    packet(0); pwr::tick();
    for (uint32_t now = 30000; now <= 2 * 86400000u; now += 30000) {
      pwr::host_now_ms = now; pwr::note_wake(); packet(now); pwr::tick();
    }
    int stride = 1;
    for (int l = 0; l < pwr::LEVELS; ++l) {
      stride *= pwr::FACTOR[l];
      check(valid_count(l) == pwr::N, "repeated sleep populates every timeframe");
      check(pwr::head[l] == static_cast<int>(((2 * pwr::MAX_FILL + 1) / stride) % pwr::N),
            "repeated sleep keeps timeframe boundaries aligned");
      for (int j = 0; j < pwr::N; ++j) {
        check(read(l, j, v, i), "sleep history survives ring wrap");
        near(v, 12, 0.001f, "sleep history voltage");
        near(i, 10, 0.01f, "sleep history current");
      }
    }
    pwr::compute_stats();
    near(pwr::st24[0].hours, 24 + 2.5f / 3600, 0.001f, "sleep accounting duration");
  } else if (std::strcmp(scenario, "sleep_missed") == 0) {
    packet(0); pwr::tick();
    for (uint32_t cycle = 1; cycle <= 2880; ++cycle) {
      const uint32_t wake = cycle * 30000;
      pwr::host_now_ms = wake; pwr::note_wake();
      if (cycle % 5 == 0) {
        pwr::tick();
        pwr::host_now_ms = wake + 3500; pwr::tick();
      } else { packet(wake); pwr::tick(); }
    }
    const uint32_t slots = pwr::host_now_ms / pwr::SLOT_MS + 1;
    int stride = 1;
    for (int l = 0; l < pwr::LEVELS; ++l) {
      stride *= pwr::FACTOR[l];
      check(pwr::head[l] == static_cast<int>((slots / stride) % pwr::N), "missed wake packets preserve timeframe cadence");
    }
    check(valid_count(0) > 0 && valid_count(0) < pwr::N, "failed acquisitions leave gaps among recovered readings");
    pwr::compute_stats();
    check(pwr::st24[0].hours > 0 && pwr::st24[0].hours < 24, "missed wake durations excluded from accounting");
  } else if (std::strcmp(scenario, "outage_expiry") == 0) {
    packet(0); pwr::tick();
    for (uint32_t now = 2500; now <= 2 * 86400000u; now += 2500) {
      pwr::host_now_ms = now; pwr::tick();
    }
    for (int l = 0; l < pwr::LEVELS; ++l) check(valid_count(l) == 0, "continuous outage expires all timeframes");
    pwr::compute_stats();
    check(!pwr::st24[0].any, "continuous outage expires old packet peaks");
    near(pwr::st24[0].hours, 0, 0.00001f, "expired outage has zero valid duration");
  } else if (std::strcmp(scenario, "weighted_gaps") == 0) {
    // One 30V sample in the first 15s bucket, then 138 observed samples at 12V.
    push_sample(30);
    for (int n = 1; n < 144; ++n) push_sample(12, n >= 6);
    const float expected = (30 + 138 * 12.0f) / 139;
    for (int l = 1; l < pwr::LEVELS; ++l) {
      check(read(l, pwr::N - 1, v, i), "aggregate with gaps exists");
      near(v, l == 3 ? expected : 12.0f, 0.0006f, "newest aggregate reflects valid sample weights");
    }
  } else if (std::strcmp(scenario, "long_gap") == 0 ||
             std::strcmp(scenario, "exact_window_gap") == 0) {
    packet(0, 50); pwr::tick();
    const uint32_t gap = std::strcmp(scenario, "long_gap") == 0 ? 2 * 86400000u : 86400000u;
    packet(gap, 12); pwr::note_wake(); pwr::tick();
    float packet_peak = pwr::pk_cur[0].vmax / 1000.0f;
    for (int j = 0; j < pwr::N; ++j)
      packet_peak = std::max(packet_peak, pwr::pk_ring[0][j].vmax / 1000.0f);
    near(packet_peak, 12, 0.001f, "packet peak outside the 24h window expires");
    for (int l = 0; l < pwr::LEVELS; ++l) {
      check(valid_count(l) == 1, "expired endpoint is not interpolated into retained history");
      check(read(l, pwr::N - 1, v, i), "new packet retained at every timeframe");
      near(v, 12, 0.001f, "new reading is not mixed with expired averages");
    }
    pwr::compute_stats();
    near(pwr::st24[0].hours, 2.5f / 3600, 0.00001f, "long gap records only newly observed duration");
    near(pwr::st24[0].vmax, 12, 0.001f, "stats discard old peak and interpolated values");
  } else if (std::strcmp(scenario, "long_gap_timeout") == 0) {
    packet(0, 50); pwr::tick();
    pwr::host_now_ms = 2 * 86400000u; pwr::note_wake(); pwr::tick();
    pwr::host_now_ms += 3500; pwr::tick();
    for (int l = 0; l < pwr::LEVELS; ++l) check(valid_count(l) == 0, "long failed acquisition expires old samples");
    pwr::compute_stats();
    check(!pwr::st24[0].any, "long failed acquisition expires old peaks");
    near(pwr::st24[0].hours, 0, 0.00001f, "long failed acquisition expires integrated energy");
    packet(2 * 86400000u + 5000, 12); pwr::tick();
    check(valid_count(0) == 1, "capture resumes after long failed acquisition");
  } else return 2;
  if (!failures) std::printf("PASS %s\n", scenario);
  return failures ? 1 : 0;
}
