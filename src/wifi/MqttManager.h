#pragma once

#include "Configuration.h"

#ifdef MQTT

#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <atomic>
#include <functional>
#include <vector>
#include "Device.h"

#define MQTT_TASK_MS          50      // Task cadence: client loop and sensor sampling
#define MQTT_TASK_STACK       8192
#define MQTT_RECONNECT_MS     15000   // Between connection attempts (they block only the MQTT task)
#define MQTT_TELEMETRY_MS     60000   // Device diagnostics (<base>/json)
#define MQTT_CLEANUP_MS       20000   // After connecting, remove discovery of sensors no longer present
                                      // (sensors take up to SENSOR_IDENTIFY_TRIES scans to be found)
#define MQTT_STALE_MAX        64      // Retained discovery topics collected for that cleanup

#ifdef SENSORS
  #include "sensors/Sensors.h"
  #define MQTT_SENSOR_VALUES (POWER_MAX_CHANNELS * 3 > ENV_VALUE_COUNT ? POWER_MAX_CHANNELS * 3 : ENV_VALUE_COUNT)

// A sensor as MQTT sees it, mirrored from CSensors by id. Values are averaged between publishes.
struct mqtt_sensor_t {
  uint16_t id;
  char key[16];          // "<model id>_<address>", e.g. "bme280_76": topic level and entity id prefix
  char label[16];        // "BME280 0x76", entity name prefix
  SensorModel model;
  SensorKind kind;
  uint8_t address;
  uint8_t channels;      // Power: 1-3
  uint8_t values;        // Environment: ENV_BIT mask
  bool online;
  bool seen;             // Still in CSensors at the last sync
  bool announced;        // Availability and discovery sent on this connection
  bool availabilityDirty;
  uint32_t sample;       // Last CSensor::getSample() accumulated
  // Power: index channel * 3 + (voltage, current, power). Environment: index EnvValue.
  double sum[MQTT_SENSOR_VALUES];
  uint32_t n[MQTT_SENSOR_VALUES];
  float last[MQTT_SENSOR_VALUES];     // NAN until known
  bool overRange[POWER_MAX_CHANNELS], lastOverRange[POWER_MAX_CHANNELS];
  bool connected[POWER_MAX_CHANNELS];   // Latest reading's
};
#endif

// Publishes the sensors to an MQTT broker, with Home Assistant discovery. Runs in its own task, so a
// broker that's slow or unreachable never stalls the display: connecting blocks for seconds.
//
// Topics, all under <base> = <configuration.mqttTopic>/<device id>:
//   availability          "online" / "offline" (last will), retained
//   json                  device diagnostics (IP, signal, battery, memory), every MQTT_TELEMETRY_MS, retained
//   <sensor>/state        sensor values (same fields as in /api), averaged over mqttIntervalSec, retained
//   <sensor>/availability "online" / "offline" as the sensor answers, retained
//   config                configuration JSON in (same fields as POST /config), applied and cleared
// <sensor> is "<model>_<address>", e.g. ina3221_40. A sensor that is dropped (unplugged for a minute)
// has its topics cleared. Discovery: <MQTT_DISCOVERY_PREFIX>/sensor/<node>/<entity>/config, with
// <node> = DEVICE_NAME_<device id>; entities of sensors that are no longer there are removed.
class CMqttManager {

public:
  explicit CMqttManager(CDevice *device);

  void begin();      // Starts the task; it waits for the network and a configured server
  void reconfigure() { reconfigureRequested = true; }   // Settings changed (any task)
  void stop(unsigned long timeoutMs);   // Publish "offline" and disconnect, before a reboot

  // Configuration JSON received on <base>/config, from the MQTT task
  void onConfig(std::function<void(JsonDocument&)> f) { configHandler = f; }

  // Any task
  bool isEnabled() const { return enabled.load(); }
  bool isConnected() const { return connected.load(); }
  int getStateCode() const { return stateCode.load(); }   // PubSubClient state()
  static const char* stateText(int code);
  static String baseTopic();    // From the saved settings

private:
  CDevice *device;
  WiFiClient client;
  PubSubClient mqtt;
  TaskHandle_t task = nullptr;
  std::function<void(JsonDocument&)> configHandler;

  std::atomic<bool> reconfigureRequested{true};
  std::atomic<bool> stopRequested{false};
  std::atomic<bool> stopped{false};
  std::atomic<bool> enabled{false};
  std::atomic<bool> connected{false};
  std::atomic<int> stateCode{MQTT_DISCONNECTED};

  // Settings in use, copied when (re)configured: PubSubClient keeps the server pointer
  char server[sizeof(configuration.mqttServer)];
  uint16_t port = MQTT_PORT;
  char user[sizeof(configuration.mqttUser)];
  char password[sizeof(configuration.mqttPassword)];
  String base;             // <topic>/<device id>
  String node;             // DEVICE_NAME_<device id>: discovery node and unique id prefix

  unsigned long tsConnect = 0;     // Last attempt, 0 = try now
  unsigned long tsConnected = 0;
  unsigned long tsTelemetry = 0;
  unsigned long tsPublish = 0;
  bool wasConnected = false;
  bool announceDevice = false;     // Diagnostics discovery pending
  bool forceStates = false;        // Publish every sensor's state now, averaged or not
  bool collectingStale = false;
  std::vector<String> stale;       // Retained discovery object ids found under our node

  #ifdef SENSORS
  std::vector<mqtt_sensor_t> sensors;
  #endif

  static void taskEntry(void *arg);
  void run();
  void applySettings();
  bool networkUp();
  void connect();
  void disconnect();
  void onMessage(char *topic, uint8_t *payload, unsigned int length);

  String topic(const char *suffix) const { return base + "/" + suffix; }
  String discoveryTopic(const char *objectId) const;
  void publishJson(const String &topic, JsonDocument &doc, bool retain);
  void publishEmpty(const String &topic);

  struct entity_t {
    char objectId[40];
    char name[48];
    char valueTemplate[80];
    const char *deviceClass;
    const char *unit;
    int8_t precision;     // suggested_display_precision, -1 for none
    const char *icon;
    bool diagnostic;
  };
  void publishDiscovery(const entity_t &e, const String &stateTopic, const String *sensorAvailability);
  void forEachDeviceEntity(std::function<void(const entity_t&)> f) const;
  void announceDiagnostics();
  void publishTelemetry();
  void cleanupStale();

  #ifdef SENSORS
  void syncSensors();
  void accumulate(mqtt_sensor_t &slot, const CSensor &s);
  void resetAverages(mqtt_sensor_t &slot);
  void forEachSensorEntity(const mqtt_sensor_t &slot, std::function<void(const entity_t&)> f) const;
  void announceSensor(mqtt_sensor_t &slot);
  void removeSensor(const mqtt_sensor_t &slot);
  void publishAvailability(mqtt_sensor_t &slot);
  void publishState(mqtt_sensor_t &slot, bool force);
  #endif
};

#endif
