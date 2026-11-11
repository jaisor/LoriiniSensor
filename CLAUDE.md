# CLAUDE.md

Firmware for a sensor device on the Waveshare ESP32-S3-Touch-LCD-1.69 (ESP32-S3R8, 16MB flash, 8MB octal PSRAM, 240x280 ST7789V2 LCD, CST816T touch, QMI8658 IMU, PCF85063 RTC, buzzer, battery). PlatformIO + Arduino-ESP32 core 3.x (pioarduino platform). Serves a configuration web UI and JSON API, persists settings in EEPROM, supports OTA, and shows a state-driven UI on the LCD. The WiFi/web/config code is ported from [ESP_LED_Controller](https://github.com/jaisor/ESP_LED_Controller) without its LED and MQTT parts.

## Build / flash

```bash
pio run                      # env waveshare_s3_lcd169 (the only one)
pio run -t upload
pio device monitor           # native USB CDC
```

There are no unit tests; verification means building and testing on hardware. `buildscript_versioning.py` runs pre-build and writes `include/version.h` (`SEMVER` env var, else `dev.<timestamp>`); the file is gitignored.

## Architecture

- [src/main.cpp](src/main.cpp) — `setup()`/`loop()` only: power latch, logging, factory-reset boot counter, creates `CDevice` then `CWifiManager`.
- [src/Board.h](src/Board.h) — every GPIO of the board. Values are for the "new" board revision (model printed on it); the old one differs in buzzer, RTC INT and power latch pins (noted inline).
- [src/Configuration.h](src/Configuration.h) / [.cpp](src/Configuration.cpp) — compile-time feature flags (`WIFI`, `SCREEN`, `WEB_LOGGING`, `DISABLE_LOGGING`), `configuration_t` + EEPROM load/save/wipe, timezone table (`TIMEZONES`, `CONFIG_getTzString()` → POSIX TZ strings).
- [src/Device.cpp](src/Device.cpp) — `DeviceState` state machine and the data screens show (`network_info_t`, battery voltage, boot progress). Every change bumps `getRevision()`; screens redraw when it moves. Also the power latch (`PWR_SYS_EN` held high) and PWR long-press power off.
- [src/display/](src/display/) — LCD UI.
  - `CDisplay` ([Display.cpp](src/display/Display.cpp)) owns the Arduino_GFX ST7789 driver, a full-screen `Arduino_Canvas` framebuffer (134KB, PSRAM), the backlight PWM and the screens. `screenForState()` maps each `DeviceState` to a `ScreenId` — **the place to add state-driven screens**. `showMessage()` draws a temporary overlay on any screen.
  - `CScreen` ([Screen.h](src/display/Screen.h)) — base class: `draw(gfx)` paints the whole screen (canvas is cleared first, flushed after); `update()` returns true to request a redraw (default: device revision or clock minute changed). Redraws are capped at ~30 fps.
  - [UI.h](src/display/UI.h) — palette, fonts (`UI_FONT_*`), `UI_drawCentered()`, signal bars, the shared status bar (signal / clock / battery).
  - [screens/](src/display/screens/) — `CBootScreen` (INITIALIZING), `CWifiScreen` (connecting / setup AP / connected / offline variants).
  - [fonts/](src/display/fonts/) — Adafruit GFXfont headers, included only from [Fonts.cpp](src/display/Fonts.cpp) and declared `extern` in [Fonts.h](src/display/Fonts.h).
- [src/wifi/WifiManager.cpp](src/wifi/WifiManager.cpp) — STA connect with soft-AP fallback (`LoriiniSensor_<chip id>` / `password123`, 192.168.4.1), ESPAsyncWebServer routes, NTP via POSIX TZ, ElegantOTA at `/update`. Pushes network info and state into `CDevice`. HTML templates in [HTMLAssets.cpp](src/wifi/HTMLAssets.cpp), Pico CSS in [CSSAssets.cpp](src/wifi/CSSAssets.cpp).

### Web routes
`/` (status), `/wifi` (saves and reboots), `/device` (name, timezone, screen brightness; saves without reboot), `/style.css`, `/log` (if `WEB_LOGGING`; reading clears it), `/api` (status JSON), `GET /config` (settings JSON, no WiFi password), `POST /config` (JSON import via `updateConfigFromJson()`, validated on a copy, saves and reboots), `POST /factory_reset`, `POST /reboot`, `/update` (OTA).

## Conventions and gotchas

- **`configuration_t` is raw-`EEPROM.put` serialized.** Adding/reordering/resizing fields shifts the layout and corrupts saved settings. The `_loaded == "lsens1"` marker identifies a valid config. Add new fields at the end, after `_loaded`, in a block with its own marker string; set defaults in `EEPROM_loadConfig()` when that marker is missing, range-check them there, and add them to `/config` GET and `updateConfigFromJson()`.
- Factory reset: 3 boots that don't survive `FACTORY_RESET_CLEAR_TIMER_MS` (2s) wipe EEPROM. Counter is EEPROM byte 0; config starts at byte 1.
- Web handlers run on the AsyncTCP task, not `loop()`. Don't drive the display or other hardware from them: change `configuration` / call `CDevice::configChanged()` and let the loop pick it up (as screen brightness does in `CDisplay::loop()`).
- The display is only drawn from `loop()` (and `setBootProgress()` during setup). Screens never call `flush()`; keep `draw()` free of blocking work.
- The panel has ~43px-radius rounded corners: keep text centered or inside `SCREEN_SAFE_INSET`, and status-bar items ~40px from the sides.
- The LCD is 240x280 in a 240x320 controller: `LCD_ROW_OFFSET` 20.
- `CDevice::initHardware()` must stay first in `setup()`: on battery the board loses power when PWR is released unless `PWR_SYS_EN` is high. Keep `BUZZER_PIN` low when idle (the passive buzzer heats up otherwise).
- HTML templates are `printf` formats: literal `%` is `%%`, and the argument list must match the comment above each template.
- Logging uses ArduinoLog (`Log.infoln`, ...). The `while (!Serial)` wait in `setup()` is bounded on purpose (USB CDC).
- Style: classes prefixed with `C`, 2-space indent, `#pragma once`; feature code guarded with `#ifdef SCREEN` / `#ifdef WIFI`, and should build with each flag off.

## Not implemented yet

Touch (CST816T on I2C 0x15, INT 14, RST 13), IMU, RTC, buzzer, the BOOT button, MQTT, and the sensor itself.
