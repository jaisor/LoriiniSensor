# Loriini Sensor

<p align="center">
  <img src="docs/img/rainbow-lorikeet.jpg" alt="Rainbow lorikeet, one of the Loriini" width="320"><br>
  <sub>A rainbow lorikeet, one of the Loriini (lories and lorikeets) the project is named after.
  Photo: ClickerHappy, via <a href="https://lafeber.com/pet-birds/">Lafeber</a>.</sub>
</p>

ESP32-S3 firmware (PlatformIO + Arduino) for the [Waveshare ESP32-S3-Touch-LCD-1.69](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.69):
WiFi with a setup access point, a web UI for configuration, OTA updates, and a state-driven UI on the 240x280 LCD
showing the I2C sensors it finds: current / voltage monitors and environment sensors, several at once. The
readings are also published over MQTT, with Home Assistant discovery.

The WiFi manager, web pages, MQTT topics and configuration storage follow [ESP_LED_Controller](https://github.com/jaisor/ESP_LED_Controller).

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
| `/` | Status: network, time, live values of every sensor, battery, memory |
| `/wifi` | Network and transmit power (saves and reboots) |
| `/device` | Name, timezone, screen brightness, screen saver, button sounds volume, detected sensors and shunt resistors (applied live), config export/import, reboot, factory reset |
| `/mqtt` | MQTT broker, base topic, Home Assistant discovery and publish interval (applied live), connection status, topic list |
| `/update` | OTA firmware update (ElegantOTA) |
| `/api` | Status JSON, including a `sensors` array with each sensor's readings |
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

## Sensors

Sensors go on the board's I2C bus (also on the extension header) and are found automatically: at boot, and every
5 seconds after, so they can be plugged in while it runs. Each one gets its own page in the carousel, and any
mix of them can be connected at once as long as their addresses differ (up to 8).

| I2C pin | Board |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 11 |
| SCL | GPIO 10 |

| Sensor | Measures | Addresses | Updates |
|---|---|---|---|
| INA219 | Bus voltage, current, power | 0x40-0x4F | 5x a second |
| INA3221 | The same on 3 channels | 0x40-0x43 | 5x a second |
| BME280 | Temperature, humidity, pressure | 0x76, 0x77 | every second |
| BMP280 | Temperature, pressure | 0x76, 0x77 | every second |
| BME680, BME688 | Temperature, humidity, pressure, gas resistance | 0x76, 0x77 | every 3 seconds |
| AHT10 | Temperature, humidity | 0x38, 0x39 | every 2 seconds |
| AHT20 (AHT21, AHT25) | Temperature, humidity | 0x38 | every 2 seconds |

The environment sensors update as often as their datasheets allow without self-heating skewing the temperature:
the AHTs ask for 2 seconds or more between measurements, and the BME68x heats its gas sensor to 320C for each
reading. Its gas value is the raw resistance of that sensor (higher means cleaner air); it drifts for the first
minutes after power-up and differs between chips, so compare it with itself over time. An air quality index
would need Bosch's closed-source BSEC library, which isn't used.

**Power monitors.** Put VIN+ / VIN- (INA3221: each channel's IN+ / IN-) in series with the load's supply (high
side). The voltage is measured on the load side. Current comes from the shunt resistor value set on the Device web
page, one for all INA219s and one for all INA3221 channels; both default to 100 milliohm, as on the common boards.
With 0.1 ohm the INA219 reads +-3.2A at 0.1mA resolution with up to 26V bus voltage, and the INA3221 +-1.6A at
0.4mA. Each INA219 or INA3221 needs its own address, set with its A0/A1 straps.

**Nothing connected.** With its wires off, a monitor's inputs float and the current reading swings by amps of
noise, while the voltage reads about 0 V. So a channel counts as connected only once its voltage reaches 0.5 V
(and as disconnected again below 0.3 V): until then it shows "Not connected", its current and power are 0
(also in `/api` and MQTT, with `"connected": false`), and the noise stays out of the averages and the graph.
That also covers unused INA3221 channels. It assumes high-side wiring, as above: with the shunt in the ground
return (low side) VIN- sits near 0 V and the channel would never count as connected. A 100 kOhm resistor from
VIN- to GND keeps disconnected inputs at 0 V in hardware as well; it adds V / 100 kOhm to the measured current
(0.12 mA at 12 V, about one step of the INA219's resolution).

**Detection.** Only the addresses above are probed. The INA3221 and the Bosch chips are identified by their ID
registers before anything is written to them. At 0x38-0x39 the device is soft-reset and taken as an AHT if it then
reports itself calibrated; the AHT10 and AHT20 are told apart by the CRC byte only the AHT20 sends. Other TI and Bosch chips at these addresses are left alone. The INA219
has no ID register, so any other device at 0x40-0x4F gets the INA219 reset and configuration written to it to see
whether it takes them: keep chips such as a PCA9685 or Si7021 (both at 0x40) off this bus. A device that isn't
recognized is not tried again until it leaves the bus. A sensor that stops answering shows "Not responding" and is
retried every 5 seconds; after a minute its page is dropped.

## MQTT and Home Assistant

Set a broker on the **MQTT** web page (⚙️ → MQTT); MQTT stays off while the server is blank. Changes apply
without a reboot. The device then publishes every sensor it finds, under `<base>` = `<base topic>/<device id>`
(base topic `loriini_sensor` by default):

| Topic | |
|---|---|
| `<base>/availability` | `online`, or `offline` (last will), retained |
| `<base>/<sensor>/state` | The sensor's values, retained: the same JSON as its entry in `/api` |
| `<base>/<sensor>/availability` | `online` / `offline` as the sensor answers, retained |
| `<base>/json` | Device: IP, signal, battery, memory, every minute, retained |
| `<base>/config` | Configuration JSON in, the same fields as the backup file; applied live, WiFi changes reboot |

`<sensor>` is the model and I2C address, e.g. `bme280_76` or `ina3221_40`, so a sensor keeps its topics across
reboots. Values are sent every publish interval (10 seconds by default, 1 to 3600), each the average of the
readings since the previous one; a power monitor reading 5 times a second averages 50 readings into each value.
A sensor that stops answering is marked `offline` right away, and one dropped (unplugged for a minute) has its
topics cleared.

**Home Assistant discovery** (on by default) adds the device with an entity per value: voltage, current and
power per channel for the power monitors (`INA3221 0x40 CH1 current`, ...), temperature, humidity, pressure and
gas resistance for the environment sensors, and WiFi signal, IP address and battery as diagnostics. Entities
are unavailable while their sensor doesn't answer. Sensors plugged in later get their entities when they are
found; dropped ones, and ones that are gone at the next connection (20 seconds after it, the time sensors take
to be identified), are removed. Turning discovery off removes them all. Discovery is sent again whenever Home
Assistant restarts (`homeassistant/status`). The topics are
`homeassistant/sensor/LoriiniSensor_<device id>/<entity>/config`.

MQTT runs in its own task, so a broker that can't be reached (connection attempts every 15 seconds, each
blocking for seconds) never stalls the screen or buttons.

## Factory reset

Power-cycle the board 3 times, each within 2 seconds of booting, or use **Factory reset** on the Device page.

## On-device UI

A status bar (WiFi, time, battery) sits above a carousel of pages. There are no on-screen titles or button labels;
the controls are the same everywhere, and the **Help** page on the device describes them:

| | Browsing pages | Inside a page | Editing a value |
|---|---|---|---|
| **PWR** click | next page | next item | increase |
| **BOOT** click | previous page | previous item | decrease |
| **PWR** double click | | one level up (closes the page at the top) | back, without saving |
| **PWR** hold | home | home | home, without saving |
| **BOOT** hold | open the page | select | save |
| **BOOT** double click | | cancel, one level up | cancel: the old value comes back |
| Swipe left / right | next / previous page | | |
| Double tap | open the page | | |
| Tap **<** / **>** | | | decrease / increase |
| Long press on the screen | | back to the pages | back to the pages, without saving |

Home is the first sensor page, or Settings while no sensor is connected. Values (brightness, screen saver,
sound) apply as they change, so you see or hear them, but only a BOOT hold keeps them; going up, home or
cancelling puts the old value back.

- **One page per sensor**, sorted by I2C address. Power monitors: current, voltage and power, with a graph of the
  last 20 seconds of current once opened; on an INA3221 the card shows all three channels and PWR / BOOT step
  through them. Environment sensors: temperature, then humidity, pressure and gas resistance as the sensor has them; opened,
  one value at a time with its minimum, maximum and a graph of the last 10 minutes, PWR / BOOT stepping through
  the values.
  With no sensors connected a "Sensors" page shows where to connect one.
- **Settings** - WiFi (details; a BOOT hold forgets the network and restarts in setup mode), Sensors (what was
  found, and whether it answers), screen brightness, screen saver, button sounds volume, About. The values have
  on-screen **<** and **>** buttons to tap besides the PWR / BOOT clicks.
- **Help** - buttons and gestures, sensors, Settings, power, web setup

In setup mode the screen shows how to join the device's WiFi; hold **BOOT** or **PWR**, or double tap, to go to the
pages instead.

## Screen saver

Without a button press or touch for the screen saver time, the screen goes dark (backlight off, panel asleep);
halfway there it dims to half its brightness. Any button or touch brings the brightness back. While dimmed the
input works as usual; when the screen is dark, the press or touch only wakes it, and its click, swipe or tap
is ignored. Set the time under Settings › Screen saver or on the Device web page: off, 15 or 30 seconds, 1, 2, 5
(the default), 10, 15 or 30 minutes, or an hour (a configuration import takes any value from 10 to 3600 seconds).
Holding PWR to switch off works with the screen dark too.

## Buttons and power

**BOOT** and **PWR** both report click, double click and long press (0.7s), each with its own sound. Set the volume,
or 0 to mute, under Settings › Sound or on the Device web page. **RST** always restarts the board, and holding BOOT while it resets enters
download mode.

On battery, press **PWR** to switch on; the firmware latches power on at boot and ignores that first press. Hold
**PWR** for 3 seconds to switch off (on USB power the board keeps running); on the way it goes home at 0.7s.
