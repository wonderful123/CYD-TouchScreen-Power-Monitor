Run the accounting regressions from the repository root:

```bash
g++ -std=c++17 -Wall -Wextra -pedantic -I . tests/accounting_test.cpp -o /tmp/pwr-accounting-test
for scenario in missing directions correlation partial expiry; do
  /tmp/pwr-accounting-test "$scenario" || break
done
```

Each scenario runs in a fresh process to reset the model's inline state. They cover missing-data duration, separate charge/discharge totals, instantaneous power integration, partial buckets without double counting, and expiry from the history ring.

The firmware build checks the radio shutdown/restart integration. Physical sleep, timer wake, touch wake, BLE reacquisition, and Wi-Fi reconnection require a connected ESP32-S3 to verify.
