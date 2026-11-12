#include "wifi/MqttManager.h"

#ifdef MQTT

#include <StreamUtils.h>
#include <version.h>
#include "wifi/WifiManager.h"

#define PROJECT_URL "https://github.com/jaisor/LoriiniSensor"

static double roundTo(float x, uint8_t decimals) {
  double scale = pow(10, decimals);
  return round(x * scale) / scale;
}

CMqttManager::CMqttManager(CDevice *device) : device(device) {
  server[0] = user[0] = password[0] = '\0';
  node = String(DEVICE_NAME) + "_" + CONFIG_getDeviceId();
  mqtt.setClient(client);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setCallback([this](char *topic, uint8_t *payload, unsigned int length) { onMessage(topic, payload, length); });
}

void CMqttManager::begin() {
  // Core 0, beside the WiFi stack: the loop task (display, sensors) has core 1 to itself
  xTaskCreatePinnedToCore(taskEntry, "mqtt", MQTT_TASK_STACK, this, 1, &task, 0);
}

void CMqttManager::taskEntry(void *arg) {
  static_cast<CMqttManager*>(arg)->run();
}

void CMqttManager::stop(unsigned long timeoutMs) {
  if (!task) return;
  stopRequested = true;
  unsigned long ts = millis();
  while (!stopped && millis() - ts < timeoutMs) {
    delay(10);
  }
}

String CMqttManager::baseTopic() {
  return String(configuration.mqttTopic) + "/" + CONFIG_getDeviceId();
}

const char* CMqttManager::stateText(int code) {
  switch (code) {
    case MQTT_CONNECTION_TIMEOUT: return "no answer from the broker";
    case MQTT_CONNECTION_LOST: return "connection lost";
    case MQTT_CONNECT_FAILED: return "can't reach the broker";
    case MQTT_DISCONNECTED: return "disconnected";
    case MQTT_CONNECTED: return "connected";
    case MQTT_CONNECT_BAD_PROTOCOL: return "protocol not supported by the broker";
    case MQTT_CONNECT_BAD_CLIENT_ID: return "client id rejected";
    case MQTT_CONNECT_UNAVAILABLE: return "broker unavailable";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "wrong username or password";
    case MQTT_CONNECT_UNAUTHORIZED: return "not authorized";
    default: return "unknown";
  }
}

String CMqttManager::discoveryTopic(const char *objectId) const {
  return String(MQTT_DISCOVERY_PREFIX "/sensor/") + node + "/" + objectId + "/config";
}

bool CMqttManager::networkUp() {
  return WiFi.status() == WL_CONNECTED && !(WiFi.getMode() & WIFI_MODE_AP);
}

void CMqttManager::applySettings() {
  disconnect();
  strlcpy(server, configuration.mqttServer, sizeof(server));
  port = configuration.mqttPort;
  strlcpy(user, configuration.mqttUser, sizeof(user));
  strlcpy(password, configuration.mqttPassword, sizeof(password));
  base = baseTopic();
  enabled = strlen(server) > 0;
  tsConnect = 0;
  if (enabled) {
    Log.noticeln("MQTT server '%s:%u', topics under '%s'", server, port, base.c_str());
  } else {
    Log.noticeln("MQTT off (no server set)");
  }
}

void CMqttManager::run() {
  for (;;) {
    if (stopRequested) {
      disconnect();
      stopped = true;
      vTaskDelay(portMAX_DELAY);
      continue;
    }
    if (reconfigureRequested.exchange(false)) {
      applySettings();
    }

    #ifdef SENSORS
    syncSensors();
    #endif

    if (!enabled || !networkUp()) {
      if (mqtt.connected()) {
        mqtt.disconnect();
      }
      connected = false;
      wasConnected = false;
      stateCode = MQTT_DISCONNECTED;
      vTaskDelay(pdMS_TO_TICKS(MQTT_TASK_MS * 4));
      continue;
    }

    if (!mqtt.connected()) {
      connected = false;
      if (wasConnected) {
        wasConnected = false;
        Log.warningln("MQTT connection lost (state %i)", mqtt.state());
        tsConnect = millis();   // Retry after the usual delay, not in a tight loop
      }
      if (!tsConnect || millis() - tsConnect > MQTT_RECONNECT_MS) {
        tsConnect = millis();
        connect();
      }
      stateCode = mqtt.state();
      if (!mqtt.connected()) {
        vTaskDelay(pdMS_TO_TICKS(MQTT_TASK_MS * 4));
        continue;
      }
    }

    mqtt.loop();
    stateCode = mqtt.state();
    if (!mqtt.connected()) {
      continue;
    }

    if (announceDevice) {
      announceDiagnostics();
    }
    #ifdef SENSORS
    for (auto &slot : sensors) {
      if (!slot.announced) {
        announceSensor(slot);
      } else if (slot.availabilityDirty) {
        publishAvailability(slot);
      }
    }
    #endif

    if (collectingStale && millis() - tsConnected > MQTT_CLEANUP_MS) {
      cleanupStale();
    }

    if (!tsTelemetry || millis() - tsTelemetry > MQTT_TELEMETRY_MS) {
      publishTelemetry();
    }

    unsigned long interval = (unsigned long)configuration.mqttIntervalSec * 1000;
    if (forceStates || millis() - tsPublish >= interval) {
      tsPublish = millis();
      #ifdef SENSORS
      for (auto &slot : sensors) {
        publishState(slot, forceStates);
      }
      #endif
      forceStates = false;
    }

    vTaskDelay(pdMS_TO_TICKS(MQTT_TASK_MS));
  }
}

void CMqttManager::connect() {
  String clientId = node;
  String availability = topic("availability");
  bool auth = strlen(user) > 0;

  Log.noticeln("Connecting to MQTT '%s:%u' as '%s'", server, port, clientId.c_str());
  mqtt.setServer(server, port);
  if (!mqtt.connect(clientId.c_str(), auth ? user : nullptr, auth ? password : nullptr,
      availability.c_str(), 0, true, "offline")) {
    Log.warningln("MQTT connect failed: %s (%i), retrying in %i s", stateText(mqtt.state()), mqtt.state(), MQTT_RECONNECT_MS / 1000);
    return;
  }

  Log.noticeln("MQTT connected, topics under '%s'", base.c_str());
  connected = true;
  wasConnected = true;
  tsConnected = millis();
  mqtt.publish(availability.c_str(), "online", true);
  mqtt.subscribe(topic("config").c_str());
  mqtt.subscribe(MQTT_DISCOVERY_PREFIX "/status");

  // Retained discovery under our node arrives right after subscribing; what isn't announced again
  // by MQTT_CLEANUP_MS belongs to sensors (or settings) that are gone, and is removed then
  stale.clear();
  collectingStale = true;
  mqtt.subscribe(discoveryTopic("+").c_str());

  announceDevice = true;
  #ifdef SENSORS
  for (auto &slot : sensors) {
    slot.announced = false;
    resetAverages(slot);   // Samples from before the connection: send the latest values instead
  }
  #endif
  tsTelemetry = 0;
  forceStates = true;
}

void CMqttManager::disconnect() {
  if (mqtt.connected()) {
    mqtt.publish(topic("availability").c_str(), "offline", true);
    mqtt.disconnect();
    Log.noticeln("MQTT disconnected");
  }
  connected = false;
  wasConnected = false;
}

void CMqttManager::publishJson(const String &topic, JsonDocument &doc, bool retain) {
  // Streamed, so messages aren't limited by the client buffer
  if (!mqtt.beginPublish(topic.c_str(), measureJson(doc), retain)) {
    Log.warningln("MQTT publish to '%s' failed", topic.c_str());
    return;
  }
  BufferingPrint buffered(mqtt, 64);
  serializeJson(doc, buffered);
  buffered.flush();
  mqtt.endPublish();
}

void CMqttManager::publishEmpty(const String &topic) {
  mqtt.publish(topic.c_str(), "", true);
}

void CMqttManager::onMessage(char *topic, uint8_t *payload, unsigned int length) {
  Log.verboseln("MQTT message on '%s', %u bytes", topic, length);

  if (!strcmp(topic, MQTT_DISCOVERY_PREFIX "/status")) {
    // Home Assistant (re)started and may have lost retained discovery: send it again
    if (length == 6 && !memcmp(payload, "online", 6)) {
      announceDevice = true;
      #ifdef SENSORS
      for (auto &slot : sensors) {
        slot.announced = false;
      }
      #endif
      forceStates = true;
    }
    return;
  }

  String prefix = discoveryTopic("");   // ".../<node>//config"
  prefix.remove(prefix.length() - strlen("/config"));
  String t(topic);
  if (t.startsWith(prefix) && t.endsWith("/config")) {
    if (collectingStale && length > 0 && stale.size() < MQTT_STALE_MAX) {
      String objectId = t.substring(prefix.length(), t.length() - strlen("/config"));
      for (auto &s : stale) {
        if (s == objectId) return;
      }
      stale.push_back(objectId);
    }
    return;
  }

  if (this->topic("config") == t) {
    if (length == 0) {
      return;   // Our own clearing of the retained message
    }
    // Copies the payload, which lives in the client buffer the next publish reuses
    JsonDocument json;
    DeserializationError de = deserializeJson(json, (const uint8_t*)payload, length);
    // Clear a retained message so it isn't applied again on every reconnect
    publishEmpty(this->topic("config"));
    if (de || !json.is<JsonObject>()) {
      Log.errorln("Invalid configuration JSON on MQTT: %s", de ? de.c_str() : "not an object");
      return;
    }
    if (configHandler) {
      configHandler(json);
    }
    announceDevice = true;   // The name may have changed
    #ifdef SENSORS
    for (auto &slot : sensors) {
      slot.announced = false;
    }
    #endif
  }
}

// Home Assistant -------------------------------------------------------------------------------

void CMqttManager::publishDiscovery(const entity_t &e, const String &stateTopic, const String *sensorAvailability) {
  JsonDocument doc;
  doc["name"] = e.name;
  doc["unique_id"] = node + "_" + e.objectId;
  doc["state_topic"] = stateTopic;
  doc["value_template"] = e.valueTemplate;
  if (e.deviceClass) doc["device_class"] = e.deviceClass;
  if (e.unit) {
    doc["unit_of_measurement"] = e.unit;
    doc["state_class"] = "measurement";
  }
  if (e.precision >= 0) doc["suggested_display_precision"] = e.precision;
  if (e.icon) doc["icon"] = e.icon;
  if (e.diagnostic) doc["entity_category"] = "diagnostic";

  JsonArray availability = doc["availability"].to<JsonArray>();
  availability.add<JsonObject>()["topic"] = topic("availability");
  if (sensorAvailability) {
    availability.add<JsonObject>()["topic"] = *sensorAvailability;
    doc["availability_mode"] = "all";   // Device online and the sensor answering
  }

  JsonObject dev = doc["device"].to<JsonObject>();
  dev["identifiers"][0] = node;
  dev["name"] = configuration.name;
  dev["model"] = DEVICE_NAME;
  dev["manufacturer"] = "Custom";
  dev["hw_version"] = BOARD_NAME;
  dev["sw_version"] = VERSION_SHORT;
  dev["configuration_url"] = String("http://") + WiFi.localIP().toString() + "/";

  JsonObject origin = doc["origin"].to<JsonObject>();
  origin["name"] = DEVICE_NAME;
  origin["sw_version"] = VERSION_SHORT;
  origin["support_url"] = PROJECT_URL;

  publishJson(discoveryTopic(e.objectId), doc, true);
}

void CMqttManager::forEachDeviceEntity(std::function<void(const entity_t&)> f) const {
  static const entity_t entities[] = {
    { "rssi", "WiFi signal", "{{ value_json.wifi_rssi }}", "signal_strength", "dBm", 0, nullptr, true },
    { "ip", "IP address", "{{ value_json.ip }}", nullptr, nullptr, -1, "mdi:ip-network", true },
    { "battery", "Battery", "{{ value_json.battery_pct }}", "battery", "%", 0, nullptr, true },
    { "battery_voltage", "Battery voltage", "{{ value_json.battery_v }}", "voltage", "V", 2, nullptr, true },
  };
  for (auto &e : entities) {
    f(e);
  }
}

void CMqttManager::announceDiagnostics() {
  announceDevice = false;
  if (!configuration.mqttDiscovery) return;
  String state = topic("json");
  forEachDeviceEntity([&](const entity_t &e) { publishDiscovery(e, state, nullptr); });
  Log.noticeln("Published Home Assistant discovery for device %s", node.c_str());
}

void CMqttManager::publishTelemetry() {
  tsTelemetry = millis();

  JsonDocument doc;
  doc["name"] = configuration.name;
  doc["device_id"] = CONFIG_getDeviceId();
  doc["version"] = VERSION;
  doc["board"] = BOARD_NAME;
  doc["ip"] = WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["wifi_percent"] = dBmtoPercentage(WiFi.RSSI());
  doc["uptime_millis"] = CONFIG_getUpTime();
  doc["battery_v"] = roundTo(device->getBatteryVoltage(), 3);
  doc["battery_pct"] = device->getBatteryPercent();
  doc["heap_free"] = ESP.getFreeHeap();
  doc["psram_free"] = ESP.getFreePsram();
  #ifdef SENSORS
  doc["sensor_count"] = sensors.size();
  #endif
  doc["publish_interval_sec"] = configuration.mqttIntervalSec;
  doc["config_topic"] = topic("config");
  publishJson(topic("json"), doc, true);
}

void CMqttManager::cleanupStale() {
  collectingStale = false;
  mqtt.unsubscribe(discoveryTopic("+").c_str());

  std::vector<String> wanted;
  if (configuration.mqttDiscovery) {
    auto add = [&wanted](const entity_t &e) { wanted.push_back(e.objectId); };
    forEachDeviceEntity(add);
    #ifdef SENSORS
    for (auto &slot : sensors) {
      forEachSensorEntity(slot, add);
    }
    #endif
  }

  for (auto &objectId : stale) {
    bool keep = false;
    for (auto &w : wanted) {
      if (w == objectId) { keep = true; break; }
    }
    if (!keep) {
      Log.noticeln("Removing Home Assistant entity '%s', no longer present", objectId.c_str());
      publishEmpty(discoveryTopic(objectId.c_str()));
    }
  }
  stale.clear();
}

// Sensors --------------------------------------------------------------------------------------

#ifdef SENSORS

void CMqttManager::resetAverages(mqtt_sensor_t &slot) {
  for (uint8_t i = 0; i < MQTT_SENSOR_VALUES; i++) {
    slot.sum[i] = 0;
    slot.n[i] = 0;
  }
  for (uint8_t ch = 0; ch < POWER_MAX_CHANNELS; ch++) {
    slot.overRange[ch] = false;
  }
}

// Mirrors CSensors (under its lock, so nothing is deleted while read): new sensors get a slot, new
// samples are added to the averages, and slots of dropped sensors are removed along with their topics
void CMqttManager::syncSensors() {
  for (auto &slot : sensors) {
    slot.seen = false;
  }

  device->getSensors().forEach([this](const CSensor &s) {
    mqtt_sensor_t *slot = nullptr;
    for (auto &existing : sensors) {
      if (existing.id == s.getId()) { slot = &existing; break; }
    }
    if (!slot) {
      mqtt_sensor_t added = {};
      added.id = s.getId();
      added.model = s.getModel();
      added.kind = s.getKind();
      added.address = s.getAddress();
      snprintf(added.key, sizeof(added.key), "%s_%02x", SENSOR_modelId(s.getModel()), s.getAddress());
      snprintf(added.label, sizeof(added.label), "%s 0x%02X", s.getName(), s.getAddress());
      if (added.kind == SensorKind::POWER) {
        added.channels = static_cast<const CPowerSensor&>(s).getChannelCount();
      } else {
        const CEnvSensor &e = static_cast<const CEnvSensor&>(s);
        for (uint8_t v = 0; v < ENV_VALUE_COUNT; v++) {
          if (e.has((EnvValue)v)) added.values |= ENV_BIT(v);
        }
      }
      added.online = s.isOnline();
      added.sample = s.getSample() - 1;   // Take the current values in below
      for (uint8_t i = 0; i < MQTT_SENSOR_VALUES; i++) added.last[i] = NAN;
      resetAverages(added);
      sensors.push_back(added);
      slot = &sensors.back();
      Log.infoln("MQTT: sensor %s as '%s'", slot->label, slot->key);
    }

    slot->seen = true;
    if (slot->online != s.isOnline()) {
      slot->online = s.isOnline();
      slot->availabilityDirty = true;
      resetAverages(*slot);   // Start over once it answers again
    }
    if (s.hasValues() && s.getSample() != slot->sample) {
      slot->sample = s.getSample();
      accumulate(*slot, s);
    }
  });

  for (auto it = sensors.begin(); it != sensors.end();) {
    if (it->seen) {
      ++it;
      continue;
    }
    Log.infoln("MQTT: sensor '%s' dropped", it->key);
    if (mqtt.connected()) {
      removeSensor(*it);
    }
    it = sensors.erase(it);
  }
}

void CMqttManager::accumulate(mqtt_sensor_t &slot, const CSensor &s) {
  if (slot.kind == SensorKind::POWER) {
    const CPowerSensor &p = static_cast<const CPowerSensor&>(s);
    for (uint8_t ch = 0; ch < slot.channels; ch++) {
      const power_reading_t &r = p.getReading(ch);
      const float v[3] = { r.voltage, r.current, r.power };
      for (uint8_t k = 0; k < 3; k++) {
        slot.sum[ch * 3 + k] += v[k];
        slot.n[ch * 3 + k]++;
        slot.last[ch * 3 + k] = v[k];
      }
      slot.overRange[ch] |= r.overRange;
      slot.lastOverRange[ch] = r.overRange;
      slot.connected[ch] = r.connected;
    }
  } else {
    const CEnvSensor &e = static_cast<const CEnvSensor&>(s);
    for (uint8_t v = 0; v < ENV_VALUE_COUNT; v++) {
      if (!(slot.values & ENV_BIT(v))) continue;
      float x = e.get((EnvValue)v);
      if (isnan(x)) continue;
      slot.sum[v] += x;
      slot.n[v]++;
      slot.last[v] = x;
    }
  }
}

// Entities of a sensor: voltage / current / power per channel, or the values the sensor has
void CMqttManager::forEachSensorEntity(const mqtt_sensor_t &slot, std::function<void(const entity_t&)> f) const {
  if (slot.kind == SensorKind::POWER) {
    static const struct { const char *id, *field, *deviceClass, *unit; int8_t precision; } QUANTITIES[] = {
      { "voltage", "voltage_v", "voltage", "V", 2 },
      { "current", "current_a", "current", "A", 3 },
      { "power", "power_w", "power", "W", 2 },
    };
    for (uint8_t ch = 0; ch < slot.channels; ch++) {
      for (auto &q : QUANTITIES) {
        entity_t e = {};
        if (slot.channels > 1) {
          snprintf(e.objectId, sizeof(e.objectId), "%s_ch%u_%s", slot.key, ch + 1, q.id);
          snprintf(e.name, sizeof(e.name), "%s CH%u %s", slot.label, ch + 1, q.id);
        } else {
          snprintf(e.objectId, sizeof(e.objectId), "%s_%s", slot.key, q.id);
          snprintf(e.name, sizeof(e.name), "%s %s", slot.label, q.id);
        }
        snprintf(e.valueTemplate, sizeof(e.valueTemplate), "{{ value_json.channels[%u].%s }}", ch, q.field);
        e.deviceClass = q.deviceClass;
        e.unit = q.unit;
        e.precision = q.precision;
        f(e);
      }
    }
  } else {
    static const struct { const char *id, *deviceClass, *unit; int8_t precision; const char *icon; } VALUES[ENV_VALUE_COUNT] = {
      { "temperature", "temperature", "°C", 1, nullptr },
      { "humidity", "humidity", "%", 1, nullptr },
      { "pressure", "atmospheric_pressure", "hPa", 1, nullptr },
      { "gas", nullptr, "Ω", 0, "mdi:air-filter" },   // Raw MOX resistance: no device class
    };
    for (uint8_t v = 0; v < ENV_VALUE_COUNT; v++) {
      if (!(slot.values & ENV_BIT(v))) continue;
      entity_t e = {};
      snprintf(e.objectId, sizeof(e.objectId), "%s_%s", slot.key, VALUES[v].id);
      snprintf(e.name, sizeof(e.name), "%s %s", slot.label, v == (uint8_t)EnvValue::GAS ? "gas resistance" : VALUES[v].id);
      // A value a reading didn't have is left out: "None" makes it unknown rather than an error
      snprintf(e.valueTemplate, sizeof(e.valueTemplate), "{{ value_json.%s | default(None) }}", SENSOR_envValueId((EnvValue)v));
      e.deviceClass = VALUES[v].deviceClass;
      e.unit = VALUES[v].unit;
      e.precision = VALUES[v].precision;
      e.icon = VALUES[v].icon;
      f(e);
    }
  }
}

void CMqttManager::announceSensor(mqtt_sensor_t &slot) {
  slot.announced = true;
  publishAvailability(slot);
  if (!configuration.mqttDiscovery) return;
  String state = topic(slot.key) + "/state";
  String availability = topic(slot.key) + "/availability";
  forEachSensorEntity(slot, [&](const entity_t &e) { publishDiscovery(e, state, &availability); });
  Log.noticeln("Published Home Assistant discovery for sensor %s", slot.label);
}

void CMqttManager::removeSensor(const mqtt_sensor_t &slot) {
  forEachSensorEntity(slot, [&](const entity_t &e) { publishEmpty(discoveryTopic(e.objectId)); });
  publishEmpty(topic(slot.key) + "/state");
  publishEmpty(topic(slot.key) + "/availability");
}

void CMqttManager::publishAvailability(mqtt_sensor_t &slot) {
  slot.availabilityDirty = false;
  mqtt.publish((topic(slot.key) + "/availability").c_str(), slot.online ? "online" : "offline", true);
}

// The average since the last publish; with force, the latest values when there was no new sample
void CMqttManager::publishState(mqtt_sensor_t &slot, bool force) {
  bool fresh = false, known = false;
  for (uint8_t i = 0; i < MQTT_SENSOR_VALUES; i++) {
    if (slot.n[i]) fresh = true;
    if (!isnan(slot.last[i])) known = true;
  }
  if (!slot.online || !(fresh || (force && known))) {
    return;   // Nothing new (or not answering, which its availability says)
  }

  auto value = [&slot](uint8_t i) -> float {
    return slot.n[i] ? (float)(slot.sum[i] / slot.n[i]) : slot.last[i];
  };

  JsonDocument doc;
  doc["model"] = SENSOR_modelId(slot.model);
  doc["name"] = SENSOR_modelName(slot.model);
  doc["address"] = slot.address;
  doc["kind"] = slot.kind == SensorKind::POWER ? "power" : "environment";
  doc["status"] = "ok";
  if (slot.kind == SensorKind::POWER) {
    doc["samples"] = slot.n[0];
    JsonArray channels = doc["channels"].to<JsonArray>();
    for (uint8_t ch = 0; ch < slot.channels; ch++) {
      JsonObject c = channels.add<JsonObject>();
      c["voltage_v"] = roundTo(value(ch * 3), 3);
      c["current_a"] = roundTo(value(ch * 3 + 1), 5);
      c["power_w"] = roundTo(value(ch * 3 + 2), 4);
      c["over_range"] = slot.n[ch * 3] ? slot.overRange[ch] : slot.lastOverRange[ch];
      c["connected"] = slot.connected[ch];
    }
  } else {
    static const uint8_t DECIMALS[ENV_VALUE_COUNT] = { 2, 2, 2, 0 };
    for (uint8_t v = 0; v < ENV_VALUE_COUNT; v++) {
      if (!(slot.values & ENV_BIT(v)) || isnan(value(v))) continue;
      doc[SENSOR_envValueId((EnvValue)v)] = roundTo(value(v), DECIMALS[v]);
    }
  }
  publishJson(topic(slot.key) + "/state", doc, true);
  resetAverages(slot);
}

#endif // SENSORS

#endif // MQTT
