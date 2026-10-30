# Power monitor checks

Run the signed-reading regression test from the repository root:

```sh
g++ -std=c++17 -DPWR_HOST_BUILD -I. tests/direction_test.cpp -o /tmp/direction_test
/tmp/direction_test
```

The test exercises BLE signed-current decoding/ingestion on all three channels,
independent polarity selection, existing history, sampling after a toggle,
signed net energy, battery charge/discharge balance and number formatting.

Validate the device configuration and compile with ESPHome 2026.10.1:

```sh
esphome config cyd-43.yaml
esphome compile cyd-43.yaml
```

Use local `secrets.yaml` for the device's credentials; do not commit it.

## Direction controls and display semantics

Settings has three **Reverse** switches, one per shunt. Off uses the signed
current sent by the sensor; on negates it. Switch settings are restored across
restarts through ESPHome globals (subject to its normal flash-write delay).
Choose orientations so positive charging means power from the charger, positive
distribution means power to the loads, and positive battery means charging.
Negative values mean flow opposite to those conventions; they are not alarms.

Voltage stays unchanged. Overview and Scope show explicit signs on current and
power. Charts plot positive magnitudes on a compact auto-scaled axis, with no zero
reference line or forced zero baseline. The |A|/|W| metric labels mark magnitude
plots. NEG identifies an entirely negative window; MIXED means the window
contains both directions. Scope cursor values retain their sign for the selected
sample; the direction badge describes the whole window. Stats Ah/Wh and the
overview 24-hour Wh are signed **net** values, while peaks are absolute
magnitudes (`|A| PK`, `|W| PK`). Overview's `|Ah|` remains the magnitude of the
sensor's cumulative coulomb counter, not the 24-hour net integral.

Raw signed samples remain in RAM. Toggling direction immediately reinterprets
live readings, all history windows and battery balance; it does not clear history
or mix samples from two conventions. History itself does not survive a reboot.

On hardware, check each switch with a known direction of current, verify that
only that channel's A/W and graphs flip while voltage stays unchanged, and check
battery IN/OUT swap correctly. Wait for preference persistence before rebooting,
then verify switch states and readings restore. Inspect negative-value text and
the scrollable settings layout on the 480x280 screen. Cloud tests cannot verify
these display/touch or flash-persistence behaviors.

## Accounting, history and interpolation regressions

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

This exercises the actual plotting helper and its draw callback to verify interpolation between observations, holding the last reading through trailing gaps, blank leading gaps, subtle shading of estimated regions, clearing old traces, and unchanged history data. It does not validate physical screen rendering. The firmware build checks the real LVGL API integration.

## Light sleep lifecycle

The sleep component keeps hardware sequencing in `light_sleep_touch.cpp` and its
public interface in `light_sleep_touch.h`. `core/power.yaml` owns idle timing and
background acquisition policy; `core/backlight.yaml` owns brightness restoration
and fading. Packet arrival and the acquisition timeout share
`finish_bg_acquisition`, then `request_light_sleep`.

Run the component lifecycle regression against the real implementation with
small ESPHome/ESP-IDF fakes:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -pedantic \
  -DUSE_WIFI -DUSE_ESP32_BLE -I tests/stubs/light_sleep_touch -I . \
  tests/light_sleep_touch_test.cpp \
  components/light_sleep_touch/light_sleep_touch.cpp -o /tmp/light-sleep-test
/tmp/light-sleep-test
python tests/light_sleep_touch_config_test.py
```

Use the Python environment containing ESPHome 2026.10.1. Schema checks cover the
ESP32 dependency, active-low internal IRQ input and nonzero sleep duration.
The lifecycle regression covers queued/idempotent entry, waiting for radio
shutdown, shutdown timeout across clock rollover, touch cancellation before and
after stopping radios, restoring only previously enabled radios, timer/touch/noise
classification, and cleanup/recovery after each wake-source setup or sleep-entry
failure. The stubs are isolated from the plotting test's LVGL stub.

`on_before_sleep` must complete synchronously: delayed automation actions do not
postpone hardware sleep. `on_wake` runs after wake, cancellation or failure, after
requesting radio restoration. `get_wake_reason()` distinguishes these outcomes;
`is_touch_wakeup()` is true only for a confirmed GPIO touch. Timer wakes and
rejected GPIO glitches are background wakes. Cancellation, sleep failure and
unexpected wake causes resume the interactive schedule instead of immediately
retrying background sleep.

After configuration validation and a firmware build, check on an ESP32-S3:

- Boot and touch restart the active -> dim -> optional screen-off -> sleep schedule.
- Timer wake acquires a BLE packet or reaches the 3.5-second fallback, then sleeps.
- Sustained touch wakes restore brightness and resume touchscreen polling; GPIO
  noise leaves the screen in its background state.
- Touch during shutdown cancels sleep and returns to normal interaction.
- Wi-Fi and BLE reconnect after repeated cycles, and brightness preview still works.

Host tests do not validate physical IRQ levels, radio reconnection or display timing.
