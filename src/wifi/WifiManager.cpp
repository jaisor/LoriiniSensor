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

static int dBmtoPercentage(int dBm) {
  if (dBm <= RSSI_MIN) return 0;
  if (dBm >= RSSI_MAX) return 100;
  return 2 * (dBm + 100);
}

CWifiManager::CWifiManager(CDevice *device)
: rebootNeeded(false), wifiRetries(0), tMillis(0), tsRssi(0), status(WF_CONNECTING), tsAPReboot(0), device(device) {

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

  AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 4096);
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
      deviceJson["dev_name"] = configuration.name;
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

    AsyncResponseStream *response = request->beginResponseStream("text/html; charset=UTF-8", 6144);
    printHTMLTop(response);
    response->printf(htmlDevice,
      BOARD_NAME,
      CONFIG_getChipModel().c_str(), (unsigned)CONFIG_getChipRevision(),
      (unsigned)(CONFIG_getFlashChipSize() / (1024 * 1024)), (unsigned)(ESP.getPsramSize() / (1024 * 1024)),
      WiFi.macAddress().c_str(),
      configuration.name, tzOptions.c_str(),
      brightnessMin, brightness, brightness,
      configuration.buzzerVolume, configuration.buzzerVolume,
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
  AsyncResponseStream *response = request->beginResponseStream("application/json; charset=UTF-8", 1024);
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

  configuration = c;
  return true;
}
