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
| `/` | Status: network, time, battery, memory |
| `/wifi` | Network and transmit power (saves and reboots) |
| `/device` | Name, timezone, screen brightness and button sounds volume (applied live), config export/import, reboot, factory reset |
| `/update` | OTA firmware update (ElegantOTA) |
| `/api` | Status JSON |
| `/config` | `GET` settings JSON (no WiFi password), `POST` JSON to import (saves and reboots) |
| `/log` | Recent log lines (reading clears them) |

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
| **BOOT** click | open the page | select / confirm |
| **BOOT** hold | | back |

- **Main** - sensor values (placeholder until a sensor is added)
- **Settings** - WiFi (details; BOOT forgets the network and restarts in setup mode), Sensor (placeholder), screen
  brightness, button sounds volume, About
- **Help** - buttons and gestures, Settings, power, web setup

In setup mode the screen shows how to join the device's WiFi; press **BOOT** to go to the pages instead.

## Buttons and power

**BOOT** and **PWR** both report click, double click and long press (0.7s), each with its own sound. Set the volume,
or 0 to mute, under Settings › Sound or on the Device web page. **RST** always restarts the board, and holding BOOT while it resets enters
download mode.

On battery, press **PWR** to switch on; the firmware latches power on at boot and ignores that first press. Hold
**PWR** for 3 seconds to switch off (on USB power the board keeps running).
