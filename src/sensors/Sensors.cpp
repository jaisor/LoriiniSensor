#include "sensors/Sensors.h"

#ifdef SENSORS

#include <ArduinoLog.h>

CSensors::CSensors()
: type(SENSOR_NONE), address(0), shuntMilliOhm(0), status(SensorStatus::OFF), errors(0),
  tsRead(0), tsPublish(0), tsRetry(0), sumVoltage(0), sumCurrent(0), polls(0), pollOverRange(false),
  readings{}, history{}, historyStart(0), historyCount(0) {
}

void CSensors::begin() {
  apply();
}

bool CSensors::configChanged() const {
  return type != configuration.sensorType || address != configuration.ina219Address
    || shuntMilliOhm != configuration.ina219ShuntMilliOhm;
}

void CSensors::resetSamples() {
  sumVoltage = sumCurrent = 0;
  polls = 0;
  pollOverRange = false;
}

void CSensors::apply() {
  type = configuration.sensorType;
  address = configuration.ina219Address;
  shuntMilliOhm = configuration.ina219ShuntMilliOhm;
  readings = {};
  historyStart = historyCount = 0;
  resetSamples();
  errors = 0;

  if (type == SENSOR_NONE) {
    status = SensorStatus::OFF;
    Log.infoln("Sensor: none");
    return;
  }
  status = probe() ? SensorStatus::OK : SensorStatus::NOT_FOUND;
  if (status == SensorStatus::NOT_FOUND) {
    Log.warningln("Sensor %s not found at 0x%x", getTypeName(), address);
  }
  tsRetry = tsRead = tsPublish = millis();
}

bool CSensors::probe() {
  switch (type) {
    case SENSOR_INA219: return ina219.begin(address, shuntMilliOhm);
    default:            return false;
  }
}

float CSensors::getHistory(uint8_t i) const {
  return i < historyCount ? history[(historyStart + i) % SENSOR_HISTORY] : 0;
}

void CSensors::publish() {
  if (polls == 0) return;
  readings.voltage = sumVoltage / polls;
  readings.current = sumCurrent / polls;
  readings.power = readings.voltage * readings.current;
  readings.overRange = pollOverRange;
  readings.valid = true;
  readings.sample++;

  uint8_t slot = (historyStart + historyCount) % SENSOR_HISTORY;
  history[slot] = readings.current;
  if (historyCount < SENSOR_HISTORY) {
    historyCount++;
  } else {
    historyStart = (historyStart + 1) % SENSOR_HISTORY;
  }
  resetSamples();
}

bool CSensors::loop() {
  const SensorStatus before = status;

  // Changed from the web UI or Settings
  if (configChanged()) {
    apply();
    return true;
  }

  const unsigned long now = millis();
  switch (status) {

    case SensorStatus::NOT_FOUND:
      if (now - tsRetry >= SENSOR_RETRY_MS) {
        tsRetry = now;
        if (probe()) {
          Log.infoln("Sensor %s found at 0x%x", getTypeName(), address);
          status = SensorStatus::OK;
          errors = 0;
          resetSamples();
          tsRead = tsPublish = now;
        }
      }
      break;

    case SensorStatus::OK:
      if (now - tsRead >= SENSOR_READ_MS) {
        tsRead = now;
        CINA219::reading_t r;
        if (ina219.read(r)) {
          errors = 0;
          sumVoltage += r.busVoltage;
          sumCurrent += r.current;
          pollOverRange |= r.overRange;
          polls++;
        } else if (++errors >= SENSOR_MAX_ERRORS) {
          Log.warningln("Sensor %s at 0x%x stopped answering", getTypeName(), address);
          status = SensorStatus::NOT_FOUND;
          readings.valid = false;
          tsRetry = now;
        }
      }
      if (status == SensorStatus::OK && now - tsPublish >= SENSOR_PUBLISH_MS) {
        tsPublish = now;
        publish();
      }
      break;

    default:
      break;
  }

  return status != before;
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

#endif
