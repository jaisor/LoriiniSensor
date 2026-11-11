# Loriini Sensor

<p align="center">
  <img src="docs/img/rainbow-lorikeet.jpg" alt="Rainbow lorikeet, one of the Loriini" width="320"><br>
  <sub>A rainbow lorikeet, one of the Loriini (lories and lorikeets) the project is named after.
  Photo: ClickerHappy, via <a href="https://lafeber.com/pet-birds/">Lafeber</a>.</sub>
</p>

ESP32-S3 firmware (PlatformIO + Arduino) for the [Waveshare ESP32-S3-Touch-LCD-1.69](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69):
WiFi with a setup access point, a web UI for configuration, OTA updates, and a state-driven UI on the 240x280 LCD.

The WiFi manager, web pages and configuration storage follow [ESP_LED_Controller](https://github.com/jaisor/ESP_LED_Controller).

## Build and flash

```bash
pio run                      # build (env waveshare_s3_lcd169)
pio run -t upload            # flash over USB
pio device monitor           # serial log over the native USB port
```

If the board doesn't show up as a serial port, hold **BOOT**, press **RST**, release BOOT, and upload again.

## First boot

With no network configured the device starts an access point and shows it on screen:

- SSID `LoriiniSensor_<chip id>`, password `password123`
- open <http://192.168.4.1>, go to ⚙️ → **WiFi** and enter your network

After it joins your network the screen shows the IP address of the web UI. If the network can't be found it falls
back to the access point, and from there retries the network every minute while nobody is connected.

## Web UI

| Route | |
|---|---|
| `/` | Status: network, time, live sensor values, battery, memory |
| `/wifi` | Network and transmit power (saves and reboots) |
| `/device` | Name, timezone, screen brightness, button sounds volume and sensor settings (applied live), config export/import, reboot, factory reset |
| `/update` | OTA firmware update (ElegantOTA) |
| `/api` | Status JSON, including the sensor readings |
| `/config` | `GET` settings JSON (no WiFi password), `POST` JSON to import (saves and reboots) |
| `/log` | Recent log lines (reading clears them) |

## OTA updates (ElegantOTA Pro)

The `/update` page uses [ElegantOTA Pro](https://elegantota.pro), the paid version of ElegantOTA. PlatformIO
fetches it from a private Git repository (`softt-io/ElegantOTA-Pro`), so the build fails at dependency install
without access to it. To build with the free [ElegantOTA](https://github.com/ayushsharma82/ElegantOTA) instead:

1. In [platformio.ini](platformio.ini), under `lib_deps`, comment out the Pro line and uncomment the free one:

   ```ini
   ; https://github.com/softt-io/ElegantOTA-Pro.git
   ayushsharma82/ElegantOTA@^3.1.7
   ```

2. In [src/wifi/WifiManager.cpp](src/wifi/WifiManager.cpp), change `#include <ElegantOTAPro.h>` to
   `#include <ElegantOTA.h>`.

The GitHub Actions build ([.github/workflows/main.yml](.github/workflows/main.yml)) clones the Pro repository with a
token from the repository secret `ELEGANTOTA_PRO_TOKEN`: a personal access token of an account that can read
`softt-io/ElegantOTA-Pro`. Builds without the secret, including pull requests from forks, fail at that step.

The firmware only uses the API both versions share (`begin`, `onStart`, `onEnd`, `loop`), and
`ELEGANTOTA_USE_ASYNC_WEBSERVER=1` in `build_flags` applies to both, so nothing else needs to change.

## INA219 sensor

Connect an INA219 current / voltage sensor breakout to the board's I2C bus (also on the extension header):

| INA219 | Board |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 11 |
| SCL | GPIO 10 |

Put the INA219's VIN+ / VIN- in series with the load's supply (high side). The Main page shows current, voltage (at
VIN-, the load side) and power, refreshed 5 times a second, with a graph of the last 20 seconds of current.

It's on by default at address 0x40 with a 100 milliohm shunt (the common breakout boards). Change the type
under Settings › Sensor, and the address (0x40-0x4F, set by the A0/A1 straps) and shunt resistance on the
Device web page. With a 0.1 ohm shunt the range is +-3.2A at 0.1mA resolution, bus voltage up to 26V. A missing
sensor is looked for again every 5 seconds, so it can be plugged in while running.

## Factory reset

Power-cycle the board 3 times, each within 2 seconds of booting, or use **Factory reset** on the Device page.

## On-device UI

A status bar (WiFi, time, battery) sits above a carousel of pages. There are no on-screen titles or button labels;
the controls are the same everywhere, and the **Help** page on the device describes them:

| | Browsing pages | Inside a page |
|---|---|---|
| **PWR** click | next page | next item / increase |
| **PWR** double click | previous page | previous item / decrease |
| Swipe left / right | next / previous page | |
| **BOOT** click, or double tap | open the page | select / confirm |
| **BOOT** hold | | back |
| Long press on the screen | | back to the pages |

- **Main** - live sensor values: current, voltage, power and a current graph
- **Settings** - WiFi (details; BOOT forgets the network and restarts in setup mode), Sensor (type, status), screen
  brightness, button sounds volume, About
- **Help** - buttons and gestures, Settings, power, web setup

In setup mode the screen shows how to join the device's WiFi; press **BOOT** or double tap to go to the pages instead.

## Buttons and power

**BOOT** and **PWR** both report click, double click and long press (0.7s), each with its own sound. Set the volume,
or 0 to mute, under Settings › Sound or on the Device web page. **RST** always restarts the board, and holding BOOT while it resets enters
download mode.

On battery, press **PWR** to switch on; the firmware latches power on at boot and ignores that first press. Hold
**PWR** for 3 seconds to switch off (on USB power the board keeps running).
