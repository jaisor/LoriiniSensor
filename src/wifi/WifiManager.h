#pragma once

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Print.h>

#include "Configuration.h"
#include "Device.h"
#ifdef MQTT
  #include "wifi/MqttManager.h"
#endif

int dBmtoPercentage(int dBm);   // 0-100 from RSSI

typedef enum {
  WF_CONNECTING = 0,
  WF_LISTENING = 1
} wifi_status;

// Joins the configured network, or runs a soft AP (<DEVICE_NAME>_<chip id>) when there is none or it
// can't be reached, and serves the configuration web UI, JSON API and OTA updates on either. Owns the
// MQTT client (CMqttManager, its own task), which publishes while joined to a network.
class CWifiManager {

private:
  bool rebootNeeded;
  uint8_t wifiRetries;
  unsigned long tMillis;
  unsigned long tsRssi;
  wifi_status status;
  char softAP_SSID[32];
  char SSID[32];
  unsigned long tsAPReboot;

  AsyncWebServer* server;
  bool serverStarted = false;  // Routes are registered once; the server stays up across WiFi drops

  JsonDocument deviceJson;

  CDevice *device;

  #ifdef MQTT
  CMqttManager mqtt;
  void handleMqtt(AsyncWebServerRequest *request);
  void applyConfigFromMqtt(JsonDocument &json);
  #endif
  String mqttStatusHtml();   // For the page header, "" when MQTT is off

  String currentIP();
  void connect();
  void listen();
  void startServer();

  void handleRoot(AsyncWebServerRequest *request);
  void handleWifi(AsyncWebServerRequest *request);
  void handleDevice(AsyncWebServerRequest *request);
  void handleFactoryReset(AsyncWebServerRequest *request);
  void handleReboot(AsyncWebServerRequest *request);
  void handleStyleCSS(AsyncWebServerRequest *request);

  void handleRestAPI_Status(AsyncWebServerRequest *request);
  void handleRestAPI_Config(AsyncWebServerRequest *request);
  void fillStatusJson(JsonDocument &json);

  void printHTMLTop(Print *p);
  void printHTMLBottom(Print *p);

  bool isApMode();

  bool updateConfigFromJson(JsonDocument jsonObj, String *error = nullptr);

public:
  explicit CWifiManager(CDevice *device);
  void loop();

  bool isRebootNeeded() const { return rebootNeeded; }
};
