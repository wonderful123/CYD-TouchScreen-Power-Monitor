Run the accounting regressions from the repository root:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -I . tests/accounting_test.cpp -o /tmp/pwr-accounting-test
for scenario in missing directions correlation partial expiry; do
  /tmp/pwr-accounting-test "$scenario" || break
done
```

Each scenario runs in a fresh process to reset the model's inline state. They cover missing-data duration, separate charge/discharge totals, instantaneous power integration, partial buckets without double counting, and expiry from the history ring.

The firmware build checks the radio shutdown/restart integration. Physical sleep, timer wake, touch wake, BLE reacquisition, and Wi-Fi reconnection require a connected ESP32-S3 to verify.

Run the history validation scenarios from the repository root:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -I . tests/history_test.cpp -o /tmp/pwr-history-test
failed=0
for scenario in timeframes awake sleep missed wake_timeout offline clock_wrap sleep_timeframes sleep_missed outage_expiry weighted_gaps long_gap exact_window_gap long_gap_timeout; do
  /tmp/pwr-history-test "$scenario" || failed=1
done
test "$failed" -eq 0
```

The host clock simulates more than two days to check all four timeframes and ring rollover. Sleep tests call the same `note_wake()`, packet freshness, and `tick()` model operations used on-device; they do not exercise physical ESP32 sleep or radio restart.

All history scenarios pass, including the regression cases for unequal valid sample counts, expiry after gaps of exactly/over 24 hours, and recovery after a long failed acquisition. Shorter sleep gaps deliberately interpolate between fresh endpoints; those readings and their integrated energy are estimates. Gaps of at least 24 hours discard the expired endpoint and retain only new observations.

Run the plotting regression with a minimal LVGL draw recorder:

```bash
g++ -std=c++17 -Wall -Wextra -Werror -pedantic -I tests/stubs -I . tests/plot_test.cpp -o /tmp/pwr-plot-test
/tmp/pwr-plot-test
```

This exercises the actual plotting helper and its draw callback to verify separate segments across missing data, preserved time-axis positions, trailing gaps, clearing old traces, and recovery. It does not validate physical screen rendering. The firmware build checks the real LVGL API integration.
