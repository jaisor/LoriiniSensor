#include "sensors/EnvSensor.h"

static const char* const VALUE_NAMES[] = { "Temperature", "Humidity", "Pressure", "Gas resistance" };
static const char* const VALUE_IDS[] = { "temperature_c", "humidity_pct", "pressure_hpa", "gas_ohm" };
static_assert(sizeof(VALUE_NAMES) / sizeof(VALUE_NAMES[0]) == ENV_VALUE_COUNT, "VALUE_NAMES out of sync");
static_assert(sizeof(VALUE_IDS) / sizeof(VALUE_IDS[0]) == ENV_VALUE_COUNT, "VALUE_IDS out of sync");

const char* SENSOR_envValueName(EnvValue v) {
  return v < EnvValue::COUNT ? VALUE_NAMES[(uint8_t)v] : "?";
}

const char* SENSOR_envValueId(EnvValue v) {
  return v < EnvValue::COUNT ? VALUE_IDS[(uint8_t)v] : "unknown";
}

CEnvSensor::CEnvSensor(SensorModel model, uint8_t address, uint8_t values, unsigned long intervalMs)
: CSensor(model, address), values(values), interval(intervalMs) {
  for (uint8_t i = 0; i < ENV_VALUE_COUNT; i++) {
    current[i] = NAN;
  }
  resetSums();
}

float CEnvSensor::getHistory(EnvValue v, uint8_t i) const {
  if (v >= EnvValue::COUNT || i >= historyCount) return NAN;
  return history[(uint8_t)v][(historyStart + i) % SENSOR_HISTORY];
}

void CEnvSensor::resetSums() {
  for (uint8_t i = 0; i < ENV_VALUE_COUNT; i++) {
    sum[i] = 0;
    sumCount[i] = 0;
  }
}

void CEnvSensor::restart() {
  converting = false;
  first = true;
  historyStart = historyCount = 0;
  resetSums();
  for (uint8_t i = 0; i < ENV_VALUE_COUNT; i++) {
    current[i] = NAN;
  }
}

void CEnvSensor::publish(const float *v, unsigned long now) {
  for (uint8_t i = 0; i < ENV_VALUE_COUNT; i++) {
    current[i] = v[i];
    if (!isnan(v[i])) {
      sum[i] += v[i];
      sumCount[i]++;
    }
  }
  // The first point goes in right away so the graph starts with the first reading
  if (historyCount == 0 || now - tsHistory >= ENV_HISTORY_MS) {
    tsHistory = now;
    const uint8_t slot = (historyStart + historyCount) % SENSOR_HISTORY;
    for (uint8_t i = 0; i < ENV_VALUE_COUNT; i++) {
      history[i][slot] = sumCount[i] ? sum[i] / sumCount[i] : NAN;
    }
    if (historyCount < SENSOR_HISTORY) {
      historyCount++;
    } else {
      historyStart = (historyStart + 1) % SENSOR_HISTORY;
    }
    resetSums();
  }
  published();
}

bool CEnvSensor::run(unsigned long now) {
  if (!converting) {
    if (!first && now - tsStart < interval) {
      return true;
    }
    if (!startMeasurement()) {
      tsStart = now;  // Next attempt one interval later
      first = false;
      return false;
    }
    first = false;
    converting = true;
    tsStart = now;
    return true;
  }

  if (now - tsStart < conversionMs()) {
    return true;
  }
  float v[ENV_VALUE_COUNT];
  for (uint8_t i = 0; i < ENV_VALUE_COUNT; i++) {
    v[i] = NAN;
  }
  switch (readMeasurement(v)) {
    case ReadResult::OK:
      converting = false;
      publish(v, now);
      return true;
    case ReadResult::BUSY:
      if (now - tsStart < conversionMs() + ENV_CONVERSION_TIMEOUT_MS) {
        return true;
      }
      converting = false;
      return false;
    default:
      converting = false;
      return false;
  }
}
