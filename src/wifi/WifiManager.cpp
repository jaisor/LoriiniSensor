#include <Arduino.h>
#include <WiFiClient.h>
#include <time.h>
#include <ElegantOTAPro.h>
#include <StreamUtils.h>
#include <AsyncJson.h>
#include <version.h>
#include "Configuration.h"
#include "wifi/WifiManager.h"
#include "wifi/HTMLAssets.h"
#include "Device.h"

#define MAX_CONNECT_TIMEOUT_MS 15000 // Time to connect before retrying / creating its own AP
#define RSSI_UPDATE_MS 5000
#define AP_REBOOT_IDLE_MS 60000      // In AP mode with a network configured, reboot to retry it after this long without clients

const int RSSI_MAX = -50; // define maximum strength of signal in dBm
const int RSSI_MIN = -100; // define minimum strength of signal in dBm

#ifdef MQTT
static void setMqttBaseTopic(char *dst, size_t size, String topic);
#endif

int dBmtoPercentage(int dBm) {
  if (dBm <= RSSI_MIN) return 0;
  if (dBm >= RSSI_MAX) return 100;
  return 2 * (dBm + 100);
}

#ifdef SENSORS
// One sensor as JSON: what it is, whether it answers, and its latest values
static void sensorToJson(const CSensor &s, JsonObject o) {
  o["model"] = SENSOR_modelId(s.getModel());
  o["name"] = s.getName();
  o["address"] = s.getAddress();
  o["kind"] = s.getKind() == SensorKind::POWER ? "power" : "environment";
  o["status"] = s.isOnline() ? "ok" : "offline";
  if (!s.hasValues()) return;
  if (s.getKind() == SensorKind::POWER) {
    const CPowerSensor &p = static_cast<const CPowerSensor&>(s);
    JsonArray channels = o["channels"].to<JsonArray>();
    for (uint8_t ch = 0; ch < p.getChannelCount(); ch++) {
      const power_reading_t &r = p.getReading(ch);
      JsonObject c = channels.add<JsonObject>();
      c["voltage_v"] = r.voltage;
      c["current_a"] = r.current;
      c["power_w"] = r.power;
      c["over_range"] = r.overRange;
    }
  } else {
    const CEnvSensor &e = static_cast<const CEnvSensor&>(s);
    for (uint8_t i = 0; i < ENV_VALUE_COUNT; i++) {
      const EnvValue v = (EnvValue)i;
      if (e.has(v) && !isnan(e.get(v))) {
        o[SENSOR_envValueId(v)] = e.get(v);
      }
    }
  }
}
#endif

CWifiManager::CWifiManager(CDevice *device)
: rebootNeeded(false), wifiRetries(0), tMillis(0), tsRssi(0), status(WF_CONNECTING), tsAPReboot(0), device(device)
#ifdef MQTT
, mqtt(device)
#endif
{

  deviceJson["dev_name"] = configuration.name;
  deviceJson["version"] = VERSION;
  deviceJson["version_short"] = VERSION_SHORT;
  deviceJson["board"] = BOARD_NAME;
  deviceJson["chip_model"] = CONFIG_getChipModel();
  deviceJson["chip_revision"] = CONFIG_getChipRevision();
  deviceJson["flash_size_bytes"] = CONFIG_getFlashChipSize();

  strcpy(SSID, configuration.wifiSsid);
  server = new AsyncWebServer(WEB_SERVER_PORT);
  connect();

  #ifdef MQTT
  mqtt.onConfig([this](JsonDocument &json) { applyConfigFromMqtt(json); });
  mqtt.begin();   // Connects once the network is up
  #endif
}

// WiFi.localIP() is 0.0.0.0 while running as a soft AP, so the reachable address has to be
// picked per mode rather than read from localIP() unconditionally.
String CWifiManager::currentIP() {
  return isApMode() ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
}

void CWifiManager::connect() {

  status = WF_CONNECTING;
  strcpy(softAP_SSID, "");
  tMillis = millis();

  uint32_t deviceId = CONFIG_getDeviceId();
  deviceJson["device_id"] = deviceId;
  Log.infoln("Device ID: '%i'", deviceId);

  if (strlen(SSID)) {

    // Join AP from Config
    Log.infoln("Connecting to WiFi: '%s' with power %i", SSID, configuration.wifiPower);
    if (device) {
      device->setNetworkInfo(SSID, "");
      if (device->getState() != DeviceState::WIFI_OFFLINE) {
        device->setState(DeviceState::WIFI_CONNECTING);
      }
    }
    WiFi.begin(SSID, configuration.wifiPassword);
    wifi_power_t txPower = WIFI_POWER_19_5dBm; // default
    switch (configuration.wifiPower) {
      case 76: txPower = WIFI_POWER_19dBm; break;   // 19dBm
      case 74: txPower = WIFI_POWER_18_5dBm; break; // 18.5dBm
      case 68: txPower = WIFI_POWER_17dBm; break;   // 17dBm
      case 60: txPower = WIFI_POWER_15dBm; break;   // 15dBm
      case 52: txPower = WIFI_POWER_13dBm; break;   // 13dBm
      case 44: txPower = WIFI_POWER_11dBm; break;   // 11dBm
      case 34: txPower = WIFI_POWER_8_5dBm; break;  // 8.5dBm
      case 28: txPower = WIFI_POWER_7dBm; break;    // 7dBm
      case 20: txPower = WIFI_POWER_5dBm; break;    // 5dBm
      case 8:  txPower = WIFI_POWER_2dBm; break;    // 2dBm
      case -4: txPower = WIFI_POWER_MINUS_1dBm; break; // -1dBm
      default: txPower = WIFI_POWER_19_5dBm; // 19.5dBm
    }
    WiFi.setTxPower(txPower);
    // Modem sleep (the ESP32 default) misses ARP requests and incoming connections between beacons
    WiFi.setSleep(false);
    wifiRetries = 0;

  } else {

    // Create AP using fallback and chip ID
    snprintf(softAP_SSID, sizeof(softAP_SSID), "%s_%u", WIFI_FALLBACK_SSID, deviceId);
    Log.infoln("Creating WiFi: '%s' / '%s'", softAP_SSID, WIFI_FALLBACK_PASS);

    if (WiFi.softAP(softAP_SSID, WIFI_FALLBACK_PASS)) {
      wifiRetries = 0;
      tsAPReboot = millis();
      Log.infoln("Wifi AP '%s' created, listening on '%s'", softAP_SSID, WiFi.softAPIP().toString().c_str());
      if (device) {
        device->setNetworkInfo(softAP_SSID, WiFi.softAPIP().toString().c_str(), WIFI_FALLBACK_PASS);
      }
    } else {
      Log.errorln("Wifi AP failed");
    }
  }
}

void CWifiManager::listen() {

  status = WF_LISTENING;
  if (!serverStarted) {
    serverStarted = true;
    startServer();
  }
  Log.infoln("Web server listening on http://%s:%i", currentIP().c_str(), WEB_SERVER_PORT);

  deviceJson["ip"] = currentIP();
  deviceJson["mac_address"] = WiFi.macAddress();

  if (device) {
    if (isApMode()) {
      device->setNetworkInfo(softAP_SSID, WiFi.softAPIP().toString().c_str(), WIFI_FALLBACK_PASS);
    } else {
      device->setNetworkInfo(WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      device->setRssi(WiFi.RSSI());
    }
    device->setState(isApMode() ? DeviceState::WIFI_AP_CREATED : DeviceState::WIFI_CONNECTED);
  }

  // NTP - Using POSIX timezone string for automatic DST handling
  const char* timezone = CONFIG_getTzString(configuration.gmtOffset_sec);
  Log.infoln("Configuring time from %s with timezone %s (GMT offset: %l)", configuration.ntpServer, timezone, configuration.gmtOffset_sec);
  configTzTime(timezone, configuration.ntpServer);
}

// Runs once. The server listens on any address, so it keeps working across WiFi drops; restarting it
// on reconnect fails to bind port 80 while earlier connections are still open or in TIME_WAIT.
void CWifiManager::startServer() {
  // Web
  server->on("/", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRoot(request); });
  server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest *request) { handleStyleCSS(request); });
  server->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){ request->send(404); });

  server->on("/wifi", HTTP_GET | HTTP_POST, [this](AsyncWebServerRequest *request) { handleWifi(request); });
  server->on("/device", HTTP_GET | HTTP_POST, [this](AsyncWebServerRequest *request) { handleDevice(request); });
  #ifdef MQTT
  server->on("/mqtt", HTTP_GET | HTTP_POST, [this](AsyncWebServerRequest *request) { handleMqtt(request); });
  #endif

  server->on("/factory_reset", HTTP_POST, [this](AsyncWebServerRequest *request) { handleFactoryReset(request); });
  server->on("/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) { handleReboot(request); });
#ifdef WEB_LOGGING
  server->on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
    Log.traceln("handleLog");
    AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 16384);
    response->println(logStream.str().c_str());
    logStream.str(""); // Clear after reading to free memory
    request->send(response);
  });
#endif
  server->on("/config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRestAPI_Config(request); });
  AsyncCallbackJsonWebHandler* configHandler = new AsyncCallbackJsonWebHandler("/config", [this](AsyncWebServerRequest *request, JsonVariant &json) {
    String error;
    if (this->updateConfigFromJson(json.as<JsonObject>(), &error)) {
      EEPROM_saveConfig();
      handleRestAPI_Config(request);
      tMillis = millis();
      rebootNeeded = true;
    } else {
      AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 128);
      response->printf("ERROR: Invalid configuration data: %s", error.c_str());
      response->setCode(400);
      request->send(response);
    }
  });
  server->addHandler(configHandler);

  server->on("/api", HTTP_GET, [this](AsyncWebServerRequest *request) { handleRestAPI_Status(request); });

  server->begin();

  // OTA at /update
  ElegantOTA.begin(server);
  ElegantOTA.onStart([this]() {
    if (device) device->showMessage("Updating firmware", 120000);
  });
  ElegantOTA.onEnd([this](bool success) {
    if (device) device->showMessage(success ? "Update done, rebooting" : "Update failed", 5000);
  });
}

void CWifiManager::loop() {

  ElegantOTA.loop();

  if (rebootNeeded && millis() - tMillis > 300) {
    Log.noticeln("Rebooting...");
    #ifdef MQTT
    mqtt.stop(1000);   // "offline" now rather than when the broker's keep-alive runs out
    #endif
    ESP.restart();
    return;
  }

  if (WiFi.status() == WL_CONNECTED || isApMode()) {
    // WiFi is connected

    if (status != WF_LISTENING) {
      if (isApMode()) {
        Log.noticeln("WiFi AP '%s' ready, IP address: %s", softAP_SSID, WiFi.softAPIP().toString().c_str());
      } else {
        Log.noticeln("WiFi connected to '%s', IP address: %s, gateway: %s, subnet: %s, RSSI: %i dBm",
          WiFi.SSID().c_str(),
          WiFi.localIP().toString().c_str(),
          WiFi.gatewayIP().toString().c_str(),
          WiFi.subnetMask().toString().c_str(),
          WiFi.RSSI());
      }
      // Start listening for requests
      listen();
      return;
    }

    if (!isApMode() && device && millis() - tsRssi > RSSI_UPDATE_MS) {
      tsRssi = millis();
      device->setRssi(WiFi.RSSI());
    }

    if (isApMode() && strlen(configuration.wifiSsid)) {
      if (WiFi.softAPgetStationNum() > 0) {
        tsAPReboot = millis();
      } else if (millis() - tsAPReboot > AP_REBOOT_IDLE_MS) {
        // Reboot if in AP mode and no connected clients, in hope of connecting to real AP
        Log.infoln(F("Rebooting after a minute in AP with no connections"));
        rebootNeeded = true;
      }
    }

  } else if (WiFi.status() == WL_NO_SSID_AVAIL && !isApMode()) {
    // Can't find desired AP
    if (millis() - tMillis > MAX_CONNECT_TIMEOUT_MS) {
      tMillis = millis();
      if (++wifiRetries > 1) {
        Log.warningln("Failed to find previous AP (wifi status %i), create an AP instead", WiFi.status());
        strcpy(SSID, "");
        WiFi.disconnect(false, true);
        connect();
      } else {
        Log.warningln("Can't find previous AP (wifi status %i) trying again attempt: %i", WiFi.status(), wifiRetries);
      }
    }
  } else {
    // WiFi is down
    switch (status) {
      case WF_LISTENING: {
        Log.infoln("Disconnected, reconnecting");
        if (device) {
          device->setState(DeviceState::WIFI_OFFLINE);
        }
        connect();
      } break;
      case WF_CONNECTING: {
        if (millis() - tMillis > MAX_CONNECT_TIMEOUT_MS) {
          tMillis = millis();
          if (++wifiRetries > 3) {
            Log.warningln("Connecting failed (wifi status %i), create an AP instead", WiFi.status());
            strcpy(SSID, "");
          }
          connect();
        }
      } break;
    } // switch
  }
}

void CWifiManager::handleRoot(AsyncWebServerRequest *request) {
  Log.traceln("handleRoot");

  const network_info_t &net = device->getNetworkInfo();

  String currentTimeStr = "not synchronized";
  struct tm timeinfo;
  if (CONFIG_isTimeSynced() && getLocalTime(&timeinfo, 0)) {
    char timeBuf[64];
    strftime(timeBuf, sizeof(timeBuf), "%A, %B %d %Y %I:%M %p", &timeinfo);
    currentTimeStr = timeBuf;
  }

  unsigned long sec = CONFIG_getUpTime() / 1000;

  AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 6144);
  printHTMLTop(response);
  response->printf(htmlMain,
    deviceStateToString(device->getState()),
    isApMode() ? "Access point" : "Network", net.ssid,
    net.ip,
    isApMode() ? 0 : WiFi.RSSI(), isApMode() ? 0 : dBmtoPercentage(WiFi.RSSI()),
    currentTimeStr.c_str(),
    (unsigned)(sec / 86400), (unsigned)((sec / 3600) % 24), (unsigned)((sec / 60) % 60), (unsigned)(sec % 60),
    device->getBatteryVoltage(),
    (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getHeapSize() / 1024),
    (unsigned)(ESP.getFreePsram() / 1024), (unsigned)(ESP.getPsramSize() / 1024)
  );
  printHTMLBottom(response);
  request->send(response);
}

void CWifiManager::handleWifi(AsyncWebServerRequest *request) {
  Log.traceln("handleWifi: %i - %s", request->method(), request->methodToString());

  if (request->method() == HTTP_POST) {
    String ssid = request->arg("ssid");
    String password = request->arg("password");
    String wifiPowerStr = request->arg("wifiPower");
    int wifiPower = wifiPowerStr.length() > 0 ? wifiPowerStr.toInt() : WIFI_DEFAULT_POWER;

    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 2048);
    printHTMLTop(response);
    response->printf("<p>Connecting to '%s' ... see you on the other side!</p>", ssid.c_str());
    printHTMLBottom(response);
    request->send(response);

    ssid.toCharArray(configuration.wifiSsid, sizeof(configuration.wifiSsid));
    password.toCharArray(configuration.wifiPassword, sizeof(configuration.wifiPassword));
    configuration.wifiPower = wifiPower;

    Log.noticeln("Saved config SSID: '%s'", configuration.wifiSsid);
    Log.noticeln("Saved WiFi Power: %i", configuration.wifiPower);

    EEPROM_saveConfig();

    strcpy(SSID, configuration.wifiSsid);
    tMillis = millis();
    rebootNeeded = true;
  } else {
    // WiFi power options with current selection marked
    static const struct { int8_t value; const char *label; } powers[] = {
      {78, "19.5 dBm (max)"}, {76, "19 dBm"}, {74, "18.5 dBm"}, {68, "17 dBm"}, {60, "15 dBm"}, {52, "13 dBm"},
      {44, "11 dBm"}, {34, "8.5 dBm"}, {28, "7 dBm"}, {20, "5 dBm"}, {8, "2 dBm"}, {-4, "-1 dBm (min)"}
    };
    String wifiPowerOptions;
    for (auto &p : powers) {
      wifiPowerOptions += String("<option value='") + p.value + "'" + (configuration.wifiPower == p.value ? " selected" : "")
        + ">" + p.label + "</option>";
    }
    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 4096);
    printHTMLTop(response);
    response->printf(htmlWifi, configuration.wifiSsid, wifiPowerOptions.c_str());
    printHTMLBottom(response);
    request->send(response);
  }
}

void CWifiManager::handleDevice(AsyncWebServerRequest *request) {
  Log.traceln("handleDevice: %s", request->methodToString());

  if (request->method() == HTTP_POST) {
    String deviceName = request->arg("deviceName");
    deviceName.trim();
    if (deviceName.length()) {
      strlcpy(configuration.name, deviceName.c_str(), sizeof(configuration.name));
    }
    Log.infoln("Device name: %s", configuration.name);

    if (request->hasArg("timezone")) {
      long newOffset = atol(request->arg("timezone").c_str());
      if (newOffset != configuration.gmtOffset_sec) {
        configuration.gmtOffset_sec = newOffset;
        Log.infoln("Timezone offset set to: %l", configuration.gmtOffset_sec);
        configTzTime(CONFIG_getTzString(configuration.gmtOffset_sec), configuration.ntpServer);
      }
    }

    if (request->hasArg("buzzerVolume")) {
      long v = request->arg("buzzerVolume").toInt();
      configuration.buzzerVolume = constrain(v, 0, 100);
    }

    // Shunts, used from the next reading on
    if (request->hasArg("ina219ShuntMilliOhm")) {
      long mohm = request->arg("ina219ShuntMilliOhm").toInt();
      if (mohm >= 1 && mohm <= INA_MAX_SHUNT_MOHM) configuration.ina219ShuntMilliOhm = mohm;
    }
    if (request->hasArg("ina3221ShuntMilliOhm")) {
      long mohm = request->arg("ina3221ShuntMilliOhm").toInt();
      if (mohm >= 1 && mohm <= INA_MAX_SHUNT_MOHM) configuration.ina3221ShuntMilliOhm = mohm;
    }

    #ifdef SCREEN
    if (request->hasArg("screenBrightness")) {
      long b = request->arg("screenBrightness").toInt();
      configuration.screenBrightness = constrain(b, SCREEN_MIN_BRIGHTNESS, 100);  // Applied by the display loop
    }
    #endif

    EEPROM_saveConfig();
    device->configChanged();  // Name and clock on screen

    AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 64);
    response->print("OK");
    request->send(response);
  } else {

    // Build timezone dropdown options
    String tzOptions;
    for (int i = 0; i < TIMEZONE_COUNT; i++) {
      tzOptions += "<option value='";
      tzOptions += TIMEZONES[i].offset;
      tzOptions += "'";
      if (TIMEZONES[i].offset == configuration.gmtOffset_sec) {
        tzOptions += " selected";
      }
      tzOptions += ">";
      tzOptions += TIMEZONES[i].label;
      tzOptions += "</option>";
    }

    int brightness = 0, brightnessMin = 0;
    #ifdef SCREEN
    brightness = configuration.screenBrightness;
    brightnessMin = SCREEN_MIN_BRIGHTNESS;
    #endif

    // Detected sensors, as a list
    String sensorList;
    #ifdef SENSORS
    device->getSensors().forEach([&sensorList](const CSensor &s) {
      char item[64];
      snprintf(item, sizeof(item), "<kbd>%s 0x%02X%s</kbd> ", s.getName(), s.getAddress(), s.isOnline() ? "" : " (offline)");
      sensorList += item;
    });
    #endif
    if (!sensorList.length()) {
      sensorList = "<kbd>none found</kbd>";
    }

    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 8192);
    printHTMLTop(response);
    response->printf(htmlDevice,
      BOARD_NAME,
      CONFIG_getChipModel().c_str(), (unsigned)CONFIG_getChipRevision(),
      (unsigned)(CONFIG_getFlashChipSize() / (1024 * 1024)), (unsigned)(ESP.getPsramSize() / (1024 * 1024)),
      WiFi.macAddress().c_str(),
      configuration.name, tzOptions.c_str(),
      brightnessMin, brightness, brightness,
      configuration.buzzerVolume, configuration.buzzerVolume,
      sensorList.c_str(), (unsigned)configuration.ina219ShuntMilliOhm, (unsigned)configuration.ina3221ShuntMilliOhm,
      configuration.name);
    printHTMLBottom(response);
    request->send(response);
  }
}

void CWifiManager::handleFactoryReset(AsyncWebServerRequest *request) {
  Log.traceln("handleFactoryReset");

  AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 64);
  response->setCode(200);
  response->print("OK");

  EEPROM_wipe();
  tMillis = millis();
  rebootNeeded = true;

  request->send(response);
}

void CWifiManager::handleReboot(AsyncWebServerRequest *request) {
  Log.traceln("handleReboot");

  AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 64);
  response->setCode(200);
  response->print("OK");

  tMillis = millis();
  rebootNeeded = true;

  request->send(response);
}

void CWifiManager::fillStatusJson(JsonDocument &json) {
  json.set(deviceJson);  // Static device info
  json["dev_name"] = configuration.name;   // Can change from the web page or MQTT
  const network_info_t &net = device->getNetworkInfo();
  json["state"] = deviceStateToString(device->getState());
  json["ap_mode"] = isApMode();
  json["ssid"] = net.ssid;
  json["ip"] = net.ip;
  if (!isApMode()) {
    json["wifi_rssi"] = WiFi.RSSI();
    json["wifi_percent"] = dBmtoPercentage(WiFi.RSSI());
  }
  json["battery_v"] = device->getBatteryVoltage();
  JsonArray sensors = json["sensors"].to<JsonArray>();
  #ifdef SENSORS
  device->getSensors().forEach([&sensors](const CSensor &s) {
    sensorToJson(s, sensors.add<JsonObject>());
  });
  #endif
  #ifdef MQTT
  JsonObject m = json["mqtt"].to<JsonObject>();
  m["enabled"] = mqtt.isEnabled();
  m["connected"] = mqtt.isConnected();
  if (mqtt.isEnabled()) {
    m["state"] = CMqttManager::stateText(mqtt.getStateCode());
    m["base_topic"] = CMqttManager::baseTopic();
  }
  #endif
  json["uptime_millis"] = CONFIG_getUpTime();
  json["heap_free"] = ESP.getFreeHeap();
  json["psram_free"] = ESP.getFreePsram();
  json["time_synced"] = CONFIG_isTimeSynced();
  if (CONFIG_isTimeSynced()) {
    time_t now;
    time(&now);
    char buf[sizeof "2011-10-08T07:07:09Z"];
    strftime(buf, sizeof buf, "%FT%TZ", gmtime(&now));
    json["timestamp_iso8601"] = String(buf);
  }
}

void CWifiManager::handleRestAPI_Status(AsyncWebServerRequest *request) {
  Log.traceln("handleRestAPI_Status");
  JsonDocument json;
  fillStatusJson(json);
  AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8", 2048);
  serializeJson(json, *response);
  request->send(response);
}

void CWifiManager::handleRestAPI_Config(AsyncWebServerRequest *request) {
  Log.traceln("handleRestAPI_Config: %s", request->methodToString());

  JsonDocument configJson;
  configJson["name"] = configuration.name;

  #ifdef WIFI
  // The WiFi password is never exported
  configJson["wifiSsid"] = configuration.wifiSsid;
  configJson["wifiPower"] = configuration.wifiPower;
  configJson["ntpServer"] = configuration.ntpServer;
  configJson["gmtOffset_sec"] = configuration.gmtOffset_sec;
  configJson["daylightOffset_sec"] = configuration.daylightOffset_sec;
  #endif

  #ifdef SCREEN
  configJson["screenBrightness"] = configuration.screenBrightness;
  #endif
  configJson["buzzerVolume"] = configuration.buzzerVolume;
  configJson["ina219ShuntMilliOhm"] = configuration.ina219ShuntMilliOhm;
  configJson["ina3221ShuntMilliOhm"] = configuration.ina3221ShuntMilliOhm;

  #ifdef MQTT
  // The MQTT password is never exported either
  configJson["mqttServer"] = configuration.mqttServer;
  configJson["mqttPort"] = configuration.mqttPort;
  configJson["mqttUser"] = configuration.mqttUser;
  configJson["mqttTopic"] = configuration.mqttTopic;
  configJson["mqttDiscovery"] = (bool)configuration.mqttDiscovery;
  configJson["mqttIntervalSec"] = configuration.mqttIntervalSec;
  #endif

  AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8", 1024);
  serializeJson(configJson, *response);
  request->send(response);
}

void CWifiManager::handleStyleCSS(AsyncWebServerRequest *request) {
  Log.traceln("handleStyleCSS");
  // Served from flash as is; the browser may cache it for a day
  AsyncWebServerResponse *response = request->beginResponse(200, "text/css; charset=UTF-8",
    (const uint8_t *)cssPico, strlen(cssPico));
  response->addHeader("Cache-Control", "max-age=86400");
  request->send(response);
}

bool CWifiManager::isApMode() {
  return WiFi.getMode() == WIFI_AP || WiFi.getMode() == WIFI_MODE_APSTA;
}

void CWifiManager::printHTMLTop(Print *p) {
  unsigned long sec = CONFIG_getUpTime() / 1000;
  p->printf(htmlTop,
    configuration.name,
    isApMode() ? softAP_SSID : SSID, isApMode() ? 100 : dBmtoPercentage(WiFi.RSSI()),
    mqttStatusHtml().c_str(),
    (int)(sec / 3600), (int)((sec / 60) % 60), (int)(sec % 60),
    configuration.name
  );
}

void CWifiManager::printHTMLBottom(Print *p) {
  p->printf(htmlBottom, VERSION);
}

// Applies a configuration import (POST /config). Works on a copy and commits only if the whole request is valid.
bool CWifiManager::updateConfigFromJson(JsonDocument jsonObj, String *error) {

  configuration_t c = configuration;

  if (!jsonObj["name"].isNull()) {
    const char *name = jsonObj["name"] | "";
    if (!strlen(name)) {
      if (error) *error = "name can't be blank";
      return false;
    }
    strlcpy(c.name, name, sizeof(c.name));
  }

  #ifdef WIFI
  if (!jsonObj["wifiSsid"].isNull()) {
    strlcpy(c.wifiSsid, jsonObj["wifiSsid"] | "", sizeof(c.wifiSsid));
  }
  if (!jsonObj["wifiPassword"].isNull()) {
    strlcpy(c.wifiPassword, jsonObj["wifiPassword"] | "", sizeof(c.wifiPassword));
  }
  if (!jsonObj["wifiPower"].isNull()) {
    c.wifiPower = jsonObj["wifiPower"].as<int8_t>();
  }
  if (!jsonObj["ntpServer"].isNull()) {
    strlcpy(c.ntpServer, jsonObj["ntpServer"] | "", sizeof(c.ntpServer));
  }
  if (!jsonObj["gmtOffset_sec"].isNull()) {
    long offset = jsonObj["gmtOffset_sec"].as<long>();
    if (offset < -12 * 3600 || offset > 14 * 3600) {
      if (error) *error = "gmtOffset_sec out of range";
      return false;
    }
    c.gmtOffset_sec = offset;
  }
  if (!jsonObj["daylightOffset_sec"].isNull()) {
    c.daylightOffset_sec = jsonObj["daylightOffset_sec"].as<int>();
  }
  #endif

  #ifdef SCREEN
  if (!jsonObj["screenBrightness"].isNull()) {
    int b = jsonObj["screenBrightness"].as<int>();
    if (b < SCREEN_MIN_BRIGHTNESS || b > 100) {
      if (error) *error = "screenBrightness must be " + String(SCREEN_MIN_BRIGHTNESS) + " to 100";
      return false;
    }
    c.screenBrightness = b;
  }
  #endif

  if (!jsonObj["buzzerVolume"].isNull()) {
    int v = jsonObj["buzzerVolume"].as<int>();
    if (v < 0 || v > 100) {
      if (error) *error = "buzzerVolume must be 0 to 100";
      return false;
    }
    c.buzzerVolume = v;
  }

  // Sensors are detected: "sensorType" and "ina219Address" from older exports are ignored
  static const struct { const char *key; uint16_t configuration_t::*field; } shunts[] = {
    { "ina219ShuntMilliOhm", &configuration_t::ina219ShuntMilliOhm },
    { "ina3221ShuntMilliOhm", &configuration_t::ina3221ShuntMilliOhm },
  };
  for (auto &shunt : shunts) {
    if (jsonObj[shunt.key].isNull()) continue;
    long mohm = jsonObj[shunt.key].as<long>();
    if (mohm < 1 || mohm > INA_MAX_SHUNT_MOHM) {
      if (error) *error = String(shunt.key) + " must be 1 to " + String(INA_MAX_SHUNT_MOHM);
      return false;
    }
    c.*shunt.field = mohm;
  }

  #ifdef MQTT
  if (!jsonObj["mqttServer"].isNull()) {
    String server = jsonObj["mqttServer"] | "";
    server.trim();
    strlcpy(c.mqttServer, server.c_str(), sizeof(c.mqttServer));
  }
  if (!jsonObj["mqttPort"].isNull()) {
    long port = jsonObj["mqttPort"].as<long>();
    if (port < 1 || port > 65535) {
      if (error) *error = "mqttPort must be 1 to 65535";
      return false;
    }
    c.mqttPort = port;
  }
  if (!jsonObj["mqttUser"].isNull()) {
    strlcpy(c.mqttUser, jsonObj["mqttUser"] | "", sizeof(c.mqttUser));
  }
  if (!jsonObj["mqttPassword"].isNull()) {
    strlcpy(c.mqttPassword, jsonObj["mqttPassword"] | "", sizeof(c.mqttPassword));
  }
  if (!jsonObj["mqttTopic"].isNull()) {
    setMqttBaseTopic(c.mqttTopic, sizeof(c.mqttTopic), jsonObj["mqttTopic"] | "");
  }
  if (!jsonObj["mqttDiscovery"].isNull()) {
    c.mqttDiscovery = jsonObj["mqttDiscovery"].as<bool>();
  }
  if (!jsonObj["mqttIntervalSec"].isNull()) {
    long interval = jsonObj["mqttIntervalSec"].as<long>();
    if (interval < 1 || interval > MQTT_MAX_INTERVAL_SEC) {
      if (error) *error = "mqttIntervalSec must be 1 to " + String(MQTT_MAX_INTERVAL_SEC);
      return false;
    }
    c.mqttIntervalSec = interval;
  }
  #endif

  configuration = c;
  return true;
}

String CWifiManager::mqttStatusHtml() {
  #ifdef MQTT
  if (!mqtt.isEnabled() || isApMode()) {
    return "";
  }
  if (mqtt.isConnected()) {
    return "<span>MQTT ✅ ▪ </span>";
  }
  return String("<span>MQTT <a href='mqtt' title='") + CMqttManager::stateText(mqtt.getStateCode())
    + "'>❌<sup>" + mqtt.getStateCode() + "</sup></a> ▪ </span>";
  #else
  return "";
  #endif
}

#ifdef MQTT

// Base topic without wildcards or surrounding slashes, MQTT_TOPIC if nothing is left
static void setMqttBaseTopic(char *dst, size_t size, String topic) {
  topic.trim();
  topic.replace("#", "");
  topic.replace("+", "");
  topic.replace(" ", "_");
  while (topic.startsWith("/")) topic.remove(0, 1);
  while (topic.endsWith("/")) topic.remove(topic.length() - 1);
  if (!topic.length()) topic = MQTT_TOPIC;
  strlcpy(dst, topic.c_str(), size);
}

// Saved and applied without a reboot: the MQTT task reconnects with the new settings
void CWifiManager::handleMqtt(AsyncWebServerRequest *request) {
  Log.traceln("handleMqtt: %s", request->methodToString());

  if (request->method() == HTTP_POST) {
    long port = request->arg("mqttPort").toInt();
    long interval = request->arg("mqttIntervalSec").toInt();
    if (port < 1 || port > 65535 || interval < 1 || interval > MQTT_MAX_INTERVAL_SEC) {
      AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 128);
      response->setCode(400);
      response->printf("port must be 1 to 65535 and the interval 1 to %d seconds", MQTT_MAX_INTERVAL_SEC);
      request->send(response);
      return;
    }

    String server = request->arg("mqttServer");
    server.trim();
    strlcpy(configuration.mqttServer, server.c_str(), sizeof(configuration.mqttServer));
    configuration.mqttPort = port;
    String user = request->arg("mqttUser");
    user.trim();
    strlcpy(configuration.mqttUser, user.c_str(), sizeof(configuration.mqttUser));
    if (!user.length()) {
      strcpy(configuration.mqttPassword, "");
    } else if (request->arg("mqttPassword").length()) {
      // Blank keeps the saved password, which is never sent back to the page
      strlcpy(configuration.mqttPassword, request->arg("mqttPassword").c_str(), sizeof(configuration.mqttPassword));
    }
    setMqttBaseTopic(configuration.mqttTopic, sizeof(configuration.mqttTopic), request->arg("mqttTopic"));
    configuration.mqttDiscovery = request->hasArg("mqttDiscovery");
    configuration.mqttIntervalSec = interval;
    Log.infoln("MQTT server '%s:%u', topic '%s', discovery %d, every %u s", configuration.mqttServer,
      configuration.mqttPort, configuration.mqttTopic, configuration.mqttDiscovery, configuration.mqttIntervalSec);

    EEPROM_saveConfig();
    mqtt.reconfigure();

    AsyncResponseStream *response = request->beginResponseStream("text/plain; charset=UTF-8", 64);
    response->print("OK");
    request->send(response);
    return;
  }

  String state;
  if (!mqtt.isEnabled()) {
    state = "off (no server set)";
  } else if (isApMode()) {
    state = "waiting for the WiFi network";
  } else {
    state = CMqttManager::stateText(mqtt.getStateCode());
  }
  String base = CMqttManager::baseTopic();
  String node = String(DEVICE_NAME) + "_" + CONFIG_getDeviceId();

  AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 8192);
  printHTMLTop(response);
  response->printf(htmlMqtt,
    mqtt.isConnected() ? "✅" : (mqtt.isEnabled() ? "❌" : "⏸️"), state.c_str(),
    configuration.mqttServer, (unsigned)configuration.mqttPort, configuration.mqttUser,
    strlen(configuration.mqttPassword) ? "saved - leave blank to keep" : "none",
    configuration.mqttTopic, configuration.mqttDiscovery ? "checked" : "",
    (unsigned)configuration.mqttIntervalSec, MQTT_MAX_INTERVAL_SEC,
    base.c_str(), base.c_str(), base.c_str(), base.c_str(), base.c_str(),
    MQTT_DISCOVERY_PREFIX, node.c_str());
  printHTMLBottom(response);
  request->send(response);
}

// Configuration JSON on <base>/config, from the MQTT task: same as POST /config, but rebooting only
// for WiFi changes; the rest applies live
void CWifiManager::applyConfigFromMqtt(JsonDocument &json) {
  configuration_t before = configuration;
  String error;
  if (!updateConfigFromJson(json, &error)) {
    Log.errorln("Rejected configuration from MQTT: %s", error.c_str());
    return;
  }
  EEPROM_saveConfig();
  Log.noticeln("Applied configuration from MQTT");

  if (strcmp(before.wifiSsid, configuration.wifiSsid) || strcmp(before.wifiPassword, configuration.wifiPassword)
      || before.wifiPower != configuration.wifiPower) {
    Log.noticeln("WiFi settings changed, rebooting");
    tMillis = millis();
    rebootNeeded = true;
    return;
  }
  if (strcmp(before.mqttServer, configuration.mqttServer) || before.mqttPort != configuration.mqttPort
      || strcmp(before.mqttUser, configuration.mqttUser) || strcmp(before.mqttPassword, configuration.mqttPassword)
      || strcmp(before.mqttTopic, configuration.mqttTopic) || before.mqttDiscovery != configuration.mqttDiscovery) {
    mqtt.reconfigure();
  }
  if (before.gmtOffset_sec != configuration.gmtOffset_sec || strcmp(before.ntpServer, configuration.ntpServer)) {
    configTzTime(CONFIG_getTzString(configuration.gmtOffset_sec), configuration.ntpServer);
  }
  device->configChanged();
}

#endif
