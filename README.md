# CYD Touchscreen Power Monitor

This is my ESPHome power-monitor project for a CYD 4.3-inch ESP32-S3 touchscreen. It receives three channels of power readings over BLE and shows charging, distribution and battery data in a local LVGL interface.

I'm using this repository to develop the display, history tracking and sleep behaviour for my setup. The hardware pinout and BLE format are specific to this project, so check them before using it on another board or with another sensor.

## What it does

- Displays voltage, signed current, power and the sensor's Ah counter.
- Shows Overview sparklines, Scope graphs and rolling 24-hour statistics.
- Keeps history at 10-minute, 1-hour, 6-hour and 24-hour resolutions.
- Lets me reverse each channel's current direction without clearing history.
- Provides brightness, dimming, screen-off and sleep settings on the touchscreen.
- Uses light sleep with periodic BLE sampling and touch wake.
- Shows uptime and the last reboot reason at the bottom of Settings.

## Hardware and data

The current configuration uses an ESP32-S3 with 16 MB flash, octal PSRAM, an RGB display configured at **480 × 280**, and an XPT2046 touchscreen. The board connections are in [core/hardware.yaml](core/hardware.yaml); the touch-wake IRQ is GPIO18 in [core/power.yaml](core/power.yaml).

The receiver expects manufacturer ID `0xFFFF` and my version-2 power packet format. It is not a generic BLE shunt receiver. The packet layout is documented in [decoder.h](components/power_monitor/ble/decoder.h); transmitter firmware is not included here.

## Build and install

I currently validate this project with **ESPHome 2026.10.1** and the ESP-IDF framework.

From the repository root, preferably in a Python virtual environment:

```sh
python -m pip install -r requirements.txt
cp secrets.example.yaml secrets.yaml
```

On PowerShell, use `Copy-Item secrets.example.yaml secrets.yaml` instead. Fill in the Wi-Fi details and a new API encryption key in `secrets.yaml`. Generate the key with:

```sh
python -c "import base64, secrets; print(base64.b64encode(secrets.token_bytes(32)).decode())"
```

Then validate, build and install:

```sh
esphome config main.yaml
esphome compile main.yaml
esphome run main.yaml
```

Use USB for the first installation. OTA updates need the device awake and connected to Wi-Fi; temporarily set the sleep delay to OFF if needed. Home Assistant supplies the header clock when its ESPHome integration is connected.

## History and sleep

History lives in RAM. **Light sleep retains it; a reboot, power loss or firmware installation clears it.** It is not saved to flash.

While asleep, the device wakes periodically to acquire a BLE sample. Values between successful samples are interpolated estimates, not continuous measurements. Failed acquisitions leave gaps, and older samples expire from their rolling time windows. Calculated energy totals can therefore be incomplete or estimated during sleep and sensor outages.

If history appears to reset, check **Settings → LAST REBOOT** and uptime. Serial logs also report acquisition timeouts and retained-history counts. Wi-Fi/API connections normally disconnect during sleep.

## Project layout and checks

- `main.yaml` — main ESPHome configuration.
- `core/` — hardware, backlight and power scheduling.
- `sensors/` — BLE receiver wiring.
- `components/` — sleep component, decoding, history, accounting and C++ views.
- `ui/` — LVGL pages, styles and scripts.
- `tests/` — host regressions and device checks; see [tests/README.md](tests/README.md).

Keep `secrets.yaml` private; it and generated ESPHome build files are ignored by Git. Automated tests simulate sleep and history behaviour, but touch timing, radio reconnection and the physical display still need testing on the board.
