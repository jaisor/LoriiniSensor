#include "sensors/Sensors.h"

#ifdef SENSORS

#include <ArduinoLog.h>
#include "sensors/I2C.h"
#include "sensors/INA219.h"
#include "sensors/INA3221.h"
#include "sensors/BME280.h"
#include "sensors/BME68x.h"
#include "sensors/AHTx0.h"

// Every address a supported chip can have, in scan order
static const uint8_t SCAN_ADDRESSES[] = {
  AHT_ADDRESS, AHT_ADDRESS_ALT,
  0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
  0x76, 0x77
};
static_assert(sizeof(SCAN_ADDRESSES) == SENSOR_SCAN_ADDRESSES, "SENSOR_SCAN_ADDRESSES out of sync");

CSensors::CSensors() : sensors{}, count(0), listRevision(0), failures{}, tsScan(0) {
}

CSensors::~CSensors() {
  for (uint8_t i = 0; i < count; i++) {
    delete sensors[i];
  }
}

void CSensors::begin() {
  scan();
  tsScan = millis();
  Log.infoln("Sensors: %u found", count);
}

const CSensor* CSensors::find(uint16_t id) const {
  for (uint8_t i = 0; i < count; i++) {
    if (sensors[i]->getId() == id) return sensors[i];
  }
  return nullptr;
}

void CSensors::forEach(std::function<void(const CSensor&)> f) const {
  std::lock_guard<std::mutex> guard(lock);
  for (uint8_t i = 0; i < count; i++) {
    f(*sensors[i]);
  }
}

bool CSensors::owned(uint8_t address) const {
  for (uint8_t i = 0; i < count; i++) {
    if (sensors[i]->getAddress() == address) return true;
  }
  return false;
}

CSensor* CSensors::tryInit(CSensor *candidate) {
  if (candidate->init()) return candidate;
  delete candidate;
  return nullptr;
}

CSensor* CSensors::identify(uint8_t address) {
  if (address == AHT_ADDRESS || address == AHT_ADDRESS_ALT) {
    return tryInit(new CAHTx0(address));
  }
  if (address >= INA219_ADDRESS_MIN && address <= INA219_ADDRESS_MAX) {
    if (address <= INA3221_ADDRESS_MAX && CINA3221::identify(address)) {
      return tryInit(new CINA3221(address));
    }
    return tryInit(new CINA219(address));  // Refuses other TI chips by their ID registers
  }
  bool humidity;
  if (CBME280::identify(address, humidity)) {
    return tryInit(new CBME280(address, humidity));
  }
  if (CBME68x::identify(address)) {
    return tryInit(new CBME68x(address));
  }
  return nullptr;
}

void CSensors::add(CSensor *sensor) {
  std::lock_guard<std::mutex> guard(lock);
  uint8_t i = count;
  while (i > 0 && sensors[i - 1]->getAddress() > sensor->getAddress()) {
    sensors[i] = sensors[i - 1];
    i--;
  }
  sensors[i] = sensor;
  count++;
  listRevision++;
}

void CSensors::remove(uint8_t index) {
  CSensor *sensor;
  {
    std::lock_guard<std::mutex> guard(lock);
    sensor = sensors[index];
    for (uint8_t i = index; i + 1 < count; i++) {
      sensors[i] = sensors[i + 1];
    }
    count--;
    sensors[count] = nullptr;
    listRevision++;
  }
  delete sensor;
}

bool CSensors::scan() {
  bool added = false;
  for (uint8_t a = 0; a < SENSOR_SCAN_ADDRESSES; a++) {
    const uint8_t address = SCAN_ADDRESSES[a];
    if (count >= SENSOR_MAX) break;
    if (owned(address)) continue;
    if (!I2C_probe(address)) {
      failures[a] = 0;  // Gone: identify whatever shows up there next
      continue;
    }
    if (failures[a] >= SENSOR_IDENTIFY_TRIES) continue;

    CSensor *sensor = identify(address);
    if (sensor) {
      Log.noticeln("Sensor %s found at 0x%x", sensor->getName(), address);
      failures[a] = 0;
      add(sensor);
      added = true;
    } else if (++failures[a] >= SENSOR_IDENTIFY_TRIES) {
      Log.infoln("Unknown device at 0x%x, left alone", address);
    }
  }
  return added;
}

bool CSensors::loop() {
  const unsigned long now = millis();
  bool changed = false;

  for (uint8_t i = 0; i < count; i++) {
    changed |= sensors[i]->loop(now);
  }

  for (uint8_t i = 0; i < count;) {
    if (!sensors[i]->isOnline() && now - sensors[i]->getOfflineSince() >= SENSOR_REMOVE_MS) {
      Log.noticeln("Sensor %s at 0x%x removed", sensors[i]->getName(), sensors[i]->getAddress());
      remove(i);
      changed = true;
    } else {
      i++;
    }
  }

  if (now - tsScan >= SENSOR_SCAN_MS) {
    tsScan = now;
    changed |= scan();
  }
  return changed;
}

void SENSOR_formatCurrent(float amps, char *value, size_t size, const char **unit) {
  if (fabsf(amps) < 1.0f) {
    float ma = amps * 1000;
    if (fabsf(ma) < 0.05f) ma = 0;  // No "-0.0"
    snprintf(value, size, "%.1f", ma);
    *unit = "mA";
  } else {
    snprintf(value, size, "%.3f", amps);
    *unit = "A";
  }
}

void SENSOR_formatVoltage(float volts, char *value, size_t size, const char **unit) {
  if (fabsf(volts) < 0.005f) volts = 0;
  snprintf(value, size, "%.2f", volts);
  *unit = "V";
}

void SENSOR_formatPower(float watts, char *value, size_t size, const char **unit) {
  if (fabsf(watts) < 1.0f) {
    float mw = watts * 1000;
    if (fabsf(mw) < 0.5f) mw = 0;
    snprintf(value, size, "%.0f", mw);
    *unit = "mW";
  } else {
    snprintf(value, size, "%.2f", watts);
    *unit = "W";
  }
}

void SENSOR_formatEnv(EnvValue v, float x, char *value, size_t size, const char **unit) {
  const bool known = !isnan(x);
  switch (v) {
    case EnvValue::TEMPERATURE:
      if (known && fabsf(x) < 0.05f) x = 0;  // No "-0.0"
      snprintf(value, size, "%.1f", x);
      *unit = "^C";
      break;
    case EnvValue::HUMIDITY:
      snprintf(value, size, "%.1f", x);
      *unit = "%";
      break;
    case EnvValue::PRESSURE:
      snprintf(value, size, "%.1f", x);
      *unit = "hPa";
      break;
    case EnvValue::GAS:
      if (known && x >= 1e6f) {
        snprintf(value, size, "%.2f", x / 1e6f);
        *unit = "MOhm";
      } else if (known && x < 1e3f) {
        snprintf(value, size, "%.0f", x);
        *unit = "Ohm";
      } else {
        snprintf(value, size, known && x >= 1e5f ? "%.0f" : "%.1f", x / 1e3f);
        *unit = "kOhm";
      }
      break;
    default:
      snprintf(value, size, "%.2f", x);
      *unit = "";
      break;
  }
  if (!known) {
    strlcpy(value, "--", size);
  }
}

#endif
