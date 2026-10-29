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
