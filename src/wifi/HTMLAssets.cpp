#include "wifi/HTMLAssets.h"

// name, SSID, signal %, MQTT status (HTML, may be empty), uptime h, m, s, name
const char htmlTop[] = R"=====(
<!doctype html>
<html lang="en" data-theme="dark">
  <head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="color-scheme" content="light dark" />
    <link rel="stylesheet" href="style.css" />
    <title>%s - Loriini Sensor</title>
    <script>
      document.addEventListener("DOMContentLoaded", function() {
        // Plain POST forms; pages with their own submit handling use forms without a method
        var form = document.querySelector("form[method]");
        if (!form) return;
        form.addEventListener("submit", function (event) {
          var data = this;
          var submit = data.querySelector("button[type='submit']");
          var label = submit.innerHTML;
          var d = data.getAttribute("delay");
          submit.setAttribute("aria-busy", true);
          submit.setAttribute("disabled", "");
          submit.innerHTML = "";
          fetch(data.getAttribute("action"), {
            method: data.getAttribute("method"),
            body: new FormData(data)
          }).then(function (res) {
              return res.text().then(function (text) {
                if (res.status >= 400) {
                  alert("Not saved: " + text);
                  submit.removeAttribute("aria-busy");
                  submit.removeAttribute("disabled");
                  submit.innerHTML = label;
                  throw null;
                }
                return text;
              });
            })
            .then(function (data) {
              console.log("Server response: " + data);
              setTimeout(function () {
                console.log("Reloading page after " + d + " ms");
                location.reload(true);
              }, d);
            })
            .catch((e) => {
              if (e === null) return; // rejected by the device, form left as is
              console.error(e);
              location.reload(true);
            });
          event.preventDefault();
        });
      });
    </script>
  </head>
  <body>
    <header class="container">
      <span>🛜 %s <b>%i%%</b> ▪ </span>
      %s
      <span>⌛<b>%02d:%02d:%02d</b></span>
      <nav>
        <ul><li>
          <hgroup>
            <h2><a href=".">🏠%s</a></h2>
            <p>Loriini Sensor</p>
          </hgroup>
        </li></ul>
        <ul><li>
          <details class="dropdown">
            <summary>⚙️</summary>
            <ul dir="rtl">
              <li><a href="wifi">WiFi 🛜</a></li>
              <li><a href="device">Device 📟</a></li>
              <li><a href="mqtt">MQTT 📡</a></li>
              <li><a href="update">Firmware update ⬆️</a></li>
            </ul>
          </details>
      </li></ul>
      </nav>
    </header>
    <main class="container">
)=====";

// version
const char htmlBottom[] = R"=====(
    </main>
    <footer class="container">
      <small>Version %s</small>
    </footer>
  </body>
</html>
)=====";

// state, network label, SSID, IP, RSSI, signal %, local time, uptime d h m s,
// battery V, heap free/total KB, PSRAM free/total KB (the sensors are filled in from /api)
const char htmlMain[] = R"=====(
      <h3>Status</h3>
      <fieldset>
        <legend><strong>🛜 Network</strong></legend>
        <label>State <kbd>%s</kbd></label>
        <br/>
        <label>%s <kbd>%s</kbd></label>
        <br/>
        <label>IP address <kbd>%s</kbd></label>
        <br/>
        <label>Signal <kbd>%d dBm</kbd> <kbd>%d%%</kbd></label>
      </fieldset>
      <fieldset>
        <legend><strong>🕒 Time</strong></legend>
        <label>Local time <kbd>%s</kbd></label>
        <br/>
        <label>Uptime <kbd>%ud %02u:%02u:%02u</kbd></label>
      </fieldset>
      <fieldset>
        <legend><strong>📈 Sensors</strong></legend>
        <div id='sensors'><small>Loading...</small></div>
      </fieldset>
      <script>
        // Sensor values from /api, refreshed every second while the page is open
        function fixed(x, digits) { return x === undefined ? '-' : x.toFixed(digits); }
        function current(a) { return Math.abs(a) < 1 ? (a * 1000).toFixed(1) + ' mA' : a.toFixed(3) + ' A'; }
        function power(w) { return Math.abs(w) < 1 ? (w * 1000).toFixed(0) + ' mW' : w.toFixed(2) + ' W'; }
        function gas(ohm) { return ohm >= 1e6 ? (ohm / 1e6).toFixed(2) + ' M&Omega;' : (ohm / 1e3).toFixed(1) + ' k&Omega;'; }
        function row(label, value) { return '<br/><label>' + label + ' <kbd>' + value + '</kbd></label>'; }
        function renderSensor(s) {
          var html = '<p><label><kbd>' + s.name + ' 0x' + s.address.toString(16).toUpperCase() + '</kbd> <kbd>' + s.status + '</kbd></label>';
          if (s.channels) {
            s.channels.forEach(function (c, i) {
              var label = s.channels.length > 1 ? 'Channel ' + (i + 1) : 'Power';
              if (c.connected === false) { html += row(label, 'not connected (no voltage)'); return; }
              html += row(label, c.voltage_v.toFixed(2) + ' V') + ' <kbd>' + current(c.current_a) + (c.over_range ? ' (over range)' : '')
                + '</kbd> <kbd>' + power(c.power_w) + '</kbd>';
            });
          }
          if (s.temperature_c !== undefined) html += row('Temperature', fixed(s.temperature_c, 1) + ' &deg;C');
          if (s.humidity_pct !== undefined) html += row('Humidity', fixed(s.humidity_pct, 1) + ' %%');
          if (s.pressure_hpa !== undefined) html += row('Pressure', fixed(s.pressure_hpa, 1) + ' hPa');
          if (s.gas_ohm !== undefined) html += row('Gas resistance', gas(s.gas_ohm));
          return html + '</p>';
        }
        function refreshSensors() {
          fetch('api').then(function (r) { return r.json(); }).then(function (j) {
            var list = j.sensors || [];
            document.getElementById('sensors').innerHTML = list.length ? list.map(renderSensor).join('')
              : '<small>None found. Connect sensors to the I2C bus (SDA GPIO 11, SCL GPIO 10, 3.3V); they are detected within seconds.</small>';
          }).catch(function () {});
        }
        refreshSensors();
        setInterval(refreshSensors, 1000);
      </script>
      <fieldset>
        <legend><strong>🔋 Power and memory</strong></legend>
        <label>Battery <kbd>%.2f V</kbd></label>
        <br/>
        <label>Heap <kbd>%u / %u KB free</kbd></label>
        <br/>
        <label>PSRAM <kbd>%u / %u KB free</kbd></label>
      </fieldset>
      <p><small>JSON: <a href="api">/api</a> (status), <a href="config">/config</a> (settings)</small></p>
)=====";

// SSID, power options
const char htmlWifi[] = R"=====(
      <h3>WiFi Settings</h3>
      <form method='POST' action='wifi' enctype='application/x-www-form-urlencoded' delay='10000'>
        <fieldset>
          <label>
            Access point name (SSID)
            <input type='text' id='ssid' name='ssid' value='%s'>
          </label>
          <label>WPA2 password
            <input type='password' id='pass' name='password' minlength='8' autocomplete='off' required>
          </label>
          <label>
            WiFi Power
            <select id='wifiPower' name='wifiPower'>
              %s
            </select>
            <sub><small>Transmit power; lower it if the device is close to the access point</small></sub>
          </label>
        </fieldset>
        <button type='submit' value='Submit'>Connect...</button>
      </form>
)=====";

// board, chip model, revision, flash MB, PSRAM MB, MAC, name, timezone options,
// brightness min, brightness value, brightness value (label), screen saver options, volume, volume (label),
// detected sensors (HTML), INA219 shunt mOhm, INA3221 shunt mOhm, export file name
const char htmlDevice[] = R"=====(
      <h3>Device Settings</h3>
      <fieldset>
        <legend><strong>🔩 Hardware</strong></legend>
        <label>Board <kbd>%s</kbd></label>
        <br/>
        <label>Chip <kbd>%s</kbd> rev <kbd>%u</kbd></label>
        <br/>
        <label>Flash <kbd>%u MB</kbd> PSRAM <kbd>%u MB</kbd></label>
        <br/>
        <label>MAC address <kbd>%s</kbd></label>
      </fieldset>
      <form method='POST' action='device' enctype='application/x-www-form-urlencoded' delay='500'>
        <fieldset>
          <label>
            Device name
            <input type='text' id='deviceName' name='deviceName' value='%s' maxlength='127'>
          </label>
          <label>
            Timezone
            <select name='timezone' id='timezone'>
              %s
            </select>
          </label>
          <label>
            Screen brightness
            <input type='range' id='screenBrightness' name='screenBrightness' min='%d' max='100' value='%d'
              oninput='document.getElementById("brightnessValue").textContent = this.value'>
            <sub><small><span id='brightnessValue'>%d</span>%%</small></sub>
          </label>
          <label>
            Screen saver
            <select name='screenSaverSec' id='screenSaverSec'>
              %s
            </select>
            <sub><small>Without a button press or touch for this long the screen goes dark, dimming to half
              brightness halfway there. The press or touch that wakes it does nothing else.</small></sub>
          </label>
          <label>
            Button sounds volume
            <input type='range' id='buzzerVolume' name='buzzerVolume' min='0' max='100' value='%d'
              oninput='document.getElementById("volumeValue").textContent = this.value'>
            <sub><small><span id='volumeValue'>%d</span>%% - 0 turns button sounds off</small></sub>
          </label>
        </fieldset>
        <fieldset>
          <legend><strong>📈 Sensors</strong></legend>
          <label>Detected %s</label>
          <sub><small>Found automatically on the I2C bus (SDA GPIO 11, SCL GPIO 10, 3.3V): INA219, INA3221, BME280,
            BMP280, BME680, BME688, AHT10, AHT20. Several at once work as long as their addresses differ.</small></sub>
          <br/><br/>
          <label>
            INA219 shunt resistor (milliohm)
            <input type='number' id='ina219ShuntMilliOhm' name='ina219ShuntMilliOhm' min='1' max='10000' value='%u'>
            <sub><small>Common breakout boards have an R100 (100 milliohm) shunt: up to 3.2A at 0.1mA steps</small></sub>
          </label>
          <label>
            INA3221 shunt resistors (milliohm, all channels)
            <input type='number' id='ina3221ShuntMilliOhm' name='ina3221ShuntMilliOhm' min='1' max='10000' value='%u'>
            <sub><small>Common boards have R100 (100 milliohm) shunts: up to 1.6A at 0.4mA steps</small></sub>
          </label>
        </fieldset>
        <button type='submit' value='Submit'>Save</button>
      </form>
      <h3>Backup</h3>
      <fieldset>
        <legend><strong>💾 Configuration file</strong></legend>
        <p><small>Device, time, screen, sound, sensor and MQTT settings as JSON. The WiFi and MQTT passwords are never exported.</small></p>
        <a href='config' download='%s.json' role='button' class='secondary'>Export</a>
        <br/><br/>
        <label>
          Import from file
          <input type='file' id='importFile' accept='.json,application/json'>
        </label>
        <label>
          <input type='checkbox' id='importWifi'>
          Also import the WiFi network (SSID and power)
        </label>
        <button type='button' id='importButton'>Import and reboot...</button>
        <small id='importStatus'></small>
      </fieldset>
      <h3>Maintenance</h3>
      <fieldset>
        <button type='button' class='secondary' id='rebootButton'>Reboot</button>
        <button type='button' class='contrast' id='resetButton'>Factory reset...</button>
      </fieldset>
      <script>
        function post(url, confirmText) {
          if (confirmText && !confirm(confirmText)) return;
          fetch(url, { method: 'POST' }).finally(function () {
            setTimeout(function () { location.href = '.'; }, 8000);
          });
        }
        document.getElementById('rebootButton').addEventListener('click', function () { post('reboot'); });
        document.getElementById('resetButton').addEventListener('click', function () {
          post('factory_reset', 'Erase all settings, including WiFi, and reboot into setup mode?');
        });
        document.getElementById('importButton').addEventListener('click', function () {
          var button = this;
          var status = document.getElementById('importStatus');
          var file = document.getElementById('importFile').files[0];
          if (!file) {
            status.textContent = 'Choose a configuration file first';
            return;
          }
          file.text().then(function (text) {
            var config = JSON.parse(text);
            if (!config || typeof config !== 'object' || Array.isArray(config)) {
              throw new Error('not a configuration object');
            }
            if (!document.getElementById('importWifi').checked) {
              ['wifiSsid', 'wifiPassword', 'wifiPower'].forEach(function (k) { delete config[k]; });
            }
            button.setAttribute('aria-busy', 'true');
            button.disabled = true;
            status.textContent = '';
            return fetch('config', {
              method: 'POST',
              headers: { 'Content-Type': 'application/json' },
              body: JSON.stringify(config)
            });
          }).then(function (res) {
            return res.text().then(function (text) {
              if (!res.ok) throw new Error(text);
              status.textContent = 'Imported, rebooting...';
              setTimeout(function () { location.reload(); }, 10000);
            });
          }).catch(function (e) {
            status.textContent = 'Import failed: ' + e.message;
            button.removeAttribute('aria-busy');
            button.disabled = false;
          });
        });
      </script>
)=====";

// status icon, status text, server, port, user, password placeholder, base topic, discovery checked,
// interval, max interval, full base topic x5, discovery prefix, discovery node
const char htmlMqtt[] = R"=====(
      <h3>MQTT and Home Assistant</h3>
      <fieldset>
        <legend><strong>📡 Connection</strong></legend>
        <label>Status %s <kbd>%s</kbd></label>
      </fieldset>
      <form method='POST' action='mqtt' enctype='application/x-www-form-urlencoded' delay='1500'>
        <fieldset>
          <label>
            MQTT server
            <input type='text' id='mqttServer' name='mqttServer' value='%s' maxlength='127' placeholder='e.g. homeassistant.local - blank turns MQTT off'>
          </label>
          <label>
            Port
            <input type='number' id='mqttPort' name='mqttPort' value='%u' min='1' max='65535'>
          </label>
          <label>
            Username
            <input type='text' id='mqttUser' name='mqttUser' value='%s' maxlength='63' autocomplete='off' placeholder='blank for anonymous'>
          </label>
          <label>
            Password
            <input type='password' id='mqttPassword' name='mqttPassword' maxlength='63' autocomplete='new-password' placeholder='%s'>
          </label>
          <label>
            Base topic
            <input type='text' id='mqttTopic' name='mqttTopic' value='%s' maxlength='63'>
            <sub><small>The device id is appended to keep devices apart</small></sub>
          </label>
          <label>
            <input type='checkbox' role='switch' id='mqttDiscovery' name='mqttDiscovery' %s>
            Home Assistant discovery
            <br/><sub><small>Adds the device to Home Assistant with an entity per sensor value, plus signal and battery.
              Sensors that are plugged in show up by themselves; unplugged for a minute, they are removed.</small></sub>
          </label>
          <label>
            Publish interval (seconds)
            <input type='number' id='mqttIntervalSec' name='mqttIntervalSec' value='%u' min='1' max='%d'>
            <sub><small>Each value sent is the average of the readings since the last one</small></sub>
          </label>
        </fieldset>
        <button type='submit' value='Submit'>Save and reconnect</button>
      </form>
      <h3>Topics</h3>
      <fieldset>
        <p><small>
          <code>%s/availability</code> online / offline<br/>
          <code>%s/json</code> device: IP, signal, battery, memory (every minute)<br/>
          <code>%s/&lt;sensor&gt;/state</code> values, the same fields as in <a href='api'>/api</a>;
            <code>&lt;sensor&gt;</code> is model and address, e.g. <code>bme280_76</code><br/>
          <code>%s/&lt;sensor&gt;/availability</code> online / offline as the sensor answers<br/>
          <code>%s/config</code> configuration JSON in, the same fields as the backup file (WiFi changes reboot)<br/>
          Discovery: <code>%s/sensor/%s/&lt;entity&gt;/config</code>
        </small></p>
      </fieldset>
)=====";
