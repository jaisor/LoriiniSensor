#include "sensors/AHTx0.h"

#include <ArduinoLog.h>
#include "sensors/I2C.h"

// Commands (AHT10 and AHT20 datasheets)
#define CMD_SOFT_RESET   0xBA
#define CMD_INIT_AHT20   0xBE   // + 0x08 0x00
#define CMD_INIT_AHT10   0xE1   // + 0x08 0x00
#define CMD_MEASURE      0xAC   // + 0x33 0x00

#define STATUS_BUSY      0x80
#define STATUS_CAL       0x08

CAHTx0::CAHTx0(uint8_t address)
: CEnvSensor(SensorModel::AHT20, address, ENV_BIT(EnvValue::TEMPERATURE) | ENV_BIT(EnvValue::HUMIDITY), AHT_INTERVAL_MS) {
}

// CRC-8, polynomial 0x31, initial 0xFF
static uint8_t crc8(const uint8_t *data, uint8_t len) {
  uint8_t crc = 0xFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (crc << 1) ^ 0x31 : crc << 1;
    }
  }
  return crc;
}

bool CAHTx0::readStatus(uint8_t &status) {
  return I2C_read(getAddress(), &status, 1);
}

bool CAHTx0::init() {
  const uint8_t addr = getAddress();
  const uint8_t reset = CMD_SOFT_RESET;
  if (!I2C_write(addr, &reset, 1)) return false;
  delay(20);

  uint8_t status = 0;
  if (!readStatus(status)) return false;
  if (!(status & STATUS_CAL)) {
    // Either chip ignores (or NACKs) the other's command, so try both
    const uint8_t init20[] = { CMD_INIT_AHT20, 0x08, 0x00 };
    const uint8_t init10[] = { CMD_INIT_AHT10, 0x08, 0x00 };
    I2C_write(addr, init20, sizeof(init20));
    delay(10);
    if (!readStatus(status)) return false;
    if (!(status & STATUS_CAL)) {
      I2C_write(addr, init10, sizeof(init10));
      delay(10);
      if (!readStatus(status)) return false;
    }
  }
  if (!(status & STATUS_CAL)) {
    Log.verboseln("0x%x is not an AHT10 / AHT20 (status 0x%x)", addr, status);
    return false;
  }

  // One measurement: only the AHT20 follows the data with its CRC
  if (!startMeasurement()) return false;
  delay(conversionMs());
  uint8_t b[7];
  ReadResult r = ReadResult::BUSY;
  for (uint8_t i = 0; i < 5 && r == ReadResult::BUSY; i++) {
    r = readRaw(b, true);
    if (r == ReadResult::BUSY) delay(10);
  }
  if (r != ReadResult::OK) return false;
  hasCrc = crc8(b, 6) == b[6];
  model = hasCrc ? SensorModel::AHT20 : SensorModel::AHT10;

  Log.infoln("%s at 0x%x", getName(), addr);
  restart();
  return true;
}

bool CAHTx0::startMeasurement() {
  const uint8_t cmd[] = { CMD_MEASURE, 0x33, 0x00 };
  return I2C_write(getAddress(), cmd, sizeof(cmd));
}

CEnvSensor::ReadResult CAHTx0::readRaw(uint8_t *b, bool crc) {
  if (!I2C_read(getAddress(), b, crc ? 7 : 6)) return ReadResult::ERROR;
  return (b[0] & STATUS_BUSY) ? ReadResult::BUSY : ReadResult::OK;
}

CEnvSensor::ReadResult CAHTx0::readMeasurement(float *out) {
  uint8_t b[7];
  ReadResult r = readRaw(b, hasCrc);
  if (r != ReadResult::OK) return r;
  if (hasCrc && crc8(b, 6) != b[6]) {
    return ReadResult::ERROR;
  }
  const uint32_t rawH = ((uint32_t)b[1] << 12) | ((uint32_t)b[2] << 4) | (b[3] >> 4);
  const uint32_t rawT = ((uint32_t)(b[3] & 0x0F) << 16) | ((uint32_t)b[4] << 8) | b[5];
  out[(uint8_t)EnvValue::HUMIDITY] = rawH * 100.0f / 1048576.0f;
  out[(uint8_t)EnvValue::TEMPERATURE] = rawT * 200.0f / 1048576.0f - 50.0f;
  return ReadResult::OK;
}
