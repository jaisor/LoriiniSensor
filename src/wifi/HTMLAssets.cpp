#include "wifi/HTMLAssets.h"

// name, SSID, signal %, uptime h, m, s, name
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

// state, network label, SSID, IP, RSSI, signal %, local time, uptime d h m s, battery V,
// heap free/total KB, PSRAM free/total KB
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
// brightness min, brightness value, brightness value (label), export file name
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
        </fieldset>
        <button type='submit' value='Submit'>Save</button>
      </form>
      <h3>Backup</h3>
      <fieldset>
        <legend><strong>💾 Configuration file</strong></legend>
        <p><small>Device, time and screen settings as JSON. The WiFi password is never exported.</small></p>
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
