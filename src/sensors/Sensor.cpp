#include "sensors/Sensor.h"

#include <ArduinoLog.h>

static const char* const MODEL_NAMES[] = { "INA219", "INA3221", "BME280", "BMP280", "BME680", "BME688", "AHT10", "AHT20" };
static const char* const MODEL_IDS[] = { "ina219", "ina3221", "bme280", "bmp280", "bme680", "bme688", "aht10", "aht20" };
static_assert(sizeof(MODEL_NAMES) / sizeof(MODEL_NAMES[0]) == (size_t)SensorModel::COUNT, "MODEL_NAMES out of sync");
static_assert(sizeof(MODEL_IDS) / sizeof(MODEL_IDS[0]) == (size_t)SensorModel::COUNT, "MODEL_IDS out of sync");

const char* SENSOR_modelName(SensorModel model) {
  return model < SensorModel::COUNT ? MODEL_NAMES[(uint8_t)model] : "?";
}

const char* SENSOR_modelId(SensorModel model) {
  return model < SensorModel::COUNT ? MODEL_IDS[(uint8_t)model] : "unknown";
}

static uint16_t nextSensorId = 1;

CSensor::CSensor(SensorModel model, uint8_t address)
: model(model), id(nextSensorId++), address(address) {
}

bool CSensor::loop(unsigned long now) {
  if (!online) {
    if (now - tsRetry >= SENSOR_RETRY_MS) {
      tsRetry = now;
      if (init()) {
        Log.infoln("Sensor %s at 0x%x is back", getName(), address);
        online = true;
        errors = 0;
        return true;
      }
    }
    return false;
  }

  if (run(now)) {
    errors = 0;
    return false;
  }
  if (++errors >= SENSOR_MAX_ERRORS) {
    Log.warningln("Sensor %s at 0x%x stopped answering", getName(), address);
    online = false;
    valid = false;
    tsRetry = tsOffline = now;
    return true;
  }
  return false;
}
