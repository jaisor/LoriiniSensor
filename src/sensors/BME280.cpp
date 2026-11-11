#include "sensors/BME280.h"

#include <ArduinoLog.h>
#include "sensors/I2C.h"

// Registers (BME280 datasheet BST-BME280-DS002, BMP280 BST-BMP280-DS001)
#define REG_CALIB_TP     0x88   // 0x88-0x9F temperature and pressure, 0xA1 H1
#define REG_CALIB_H1     0xA1
#define REG_CHIP_ID      0xD0
#define REG_RESET        0xE0
#define REG_CALIB_H2     0xE1   // 0xE1-0xE7
#define REG_CTRL_HUM     0xF2
#define REG_STATUS       0xF3
#define REG_CTRL_MEAS    0xF4
#define REG_CONFIG       0xF5
#define REG_DATA         0xF7   // press[3], temp[3], hum[2]

#define CHIP_ID_BME280   0x60
#define CHIP_ID_BMP280   0x58   // 0x56 / 0x57 on engineering samples
#define RESET_VALUE      0xB6
#define STATUS_IM_UPDATE 0x01

#define OSRS_X1          0b001
#define OSRS_X2          0b010
#define OSRS_X16         0b101
#define MODE_NORMAL      0b11
#define STANDBY_500MS    0b100
#define FILTER_4         0b010

#define ADC_SKIPPED_20   0x80000  // Temperature / pressure value until the first conversion
#define ADC_SKIPPED_16   0x8000

static int16_t le16(const uint8_t *b) { return (int16_t)(b[0] | (b[1] << 8)); }

CBME280::CBME280(uint8_t address, bool humidity)
: CEnvSensor(humidity ? SensorModel::BME280 : SensorModel::BMP280, address,
    ENV_BIT(EnvValue::TEMPERATURE) | ENV_BIT(EnvValue::PRESSURE) | (humidity ? ENV_BIT(EnvValue::HUMIDITY) : 0),
    BME280_INTERVAL_MS),
  humidity(humidity) {
}

bool CBME280::identify(uint8_t address, bool &humidity) {
  uint8_t id;
  if (!I2C_readReg8(address, REG_CHIP_ID, id)) return false;
  humidity = id == CHIP_ID_BME280;
  return id == CHIP_ID_BME280 || id == CHIP_ID_BMP280 || id == 0x56 || id == 0x57;
}

bool CBME280::readCalibration() {
  const uint8_t addr = getAddress();
  uint8_t b[24];
  if (!I2C_readRegs(addr, REG_CALIB_TP, b, 24)) return false;
  T1 = (uint16_t)le16(b + 0);
  T2 = le16(b + 2);
  T3 = le16(b + 4);
  P1 = (uint16_t)le16(b + 6);
  P2 = le16(b + 8);
  P3 = le16(b + 10);
  P4 = le16(b + 12);
  P5 = le16(b + 14);
  P6 = le16(b + 16);
  P7 = le16(b + 18);
  P8 = le16(b + 20);
  P9 = le16(b + 22);
  if (!humidity) return true;

  uint8_t h[7];
  if (!I2C_readReg8(addr, REG_CALIB_H1, H1) || !I2C_readRegs(addr, REG_CALIB_H2, h, 7)) return false;
  H2 = le16(h + 0);
  H3 = h[2];
  H4 = (int16_t)((int8_t)h[3] * 16 + (h[4] & 0x0F));   // Signed 12 bit: 0xE4 << 4 | 0xE5[3:0]
  H5 = (int16_t)((int8_t)h[5] * 16 + (h[4] >> 4));     // 0xE6 << 4 | 0xE5[7:4]
  H6 = (int8_t)h[6];
  return true;
}

bool CBME280::init() {
  const uint8_t addr = getAddress();
  bool hasHumidity;
  if (!identify(addr, hasHumidity) || hasHumidity != humidity) return false;
  if (!I2C_writeReg8(addr, REG_RESET, RESET_VALUE)) return false;
  delay(3);  // Start-up 2ms
  // Calibration is copied from NVM after the reset
  uint8_t status = STATUS_IM_UPDATE;
  for (uint8_t i = 0; i < 10 && (status & STATUS_IM_UPDATE); i++) {
    delay(1);
    if (!I2C_readReg8(addr, REG_STATUS, status)) return false;
  }
  if (!readCalibration()) return false;

  // ctrl_hum only takes effect with the ctrl_meas write that follows it
  if ((humidity && !I2C_writeReg8(addr, REG_CTRL_HUM, OSRS_X1))
      || !I2C_writeReg8(addr, REG_CONFIG, (STANDBY_500MS << 5) | (FILTER_4 << 2))
      || !I2C_writeReg8(addr, REG_CTRL_MEAS, (OSRS_X2 << 5) | (OSRS_X16 << 2) | MODE_NORMAL)) {
    return false;
  }
  Log.infoln("%s at 0x%x", getName(), addr);
  restart();
  return true;
}

CEnvSensor::ReadResult CBME280::readMeasurement(float *out) {
  uint8_t b[8];
  if (!I2C_readRegs(getAddress(), REG_DATA, b, humidity ? 8 : 6)) {
    return ReadResult::ERROR;
  }
  const int32_t adcP = ((uint32_t)b[0] << 12) | ((uint32_t)b[1] << 4) | (b[2] >> 4);
  const int32_t adcT = ((uint32_t)b[3] << 12) | ((uint32_t)b[4] << 4) | (b[5] >> 4);
  if (adcT == ADC_SKIPPED_20) {
    return ReadResult::BUSY;  // First conversion not done yet
  }

  // Temperature
  double var1 = (adcT / 16384.0 - T1 / 1024.0) * T2;
  double var2 = (adcT / 131072.0 - T1 / 8192.0) * (adcT / 131072.0 - T1 / 8192.0) * T3;
  const double tFine = var1 + var2;
  out[(uint8_t)EnvValue::TEMPERATURE] = tFine / 5120.0;

  // Pressure
  if (adcP != ADC_SKIPPED_20) {
    var1 = tFine / 2.0 - 64000.0;
    var2 = var1 * var1 * P6 / 32768.0;
    var2 = var2 + var1 * P5 * 2.0;
    var2 = var2 / 4.0 + P4 * 65536.0;
    var1 = (P3 * var1 * var1 / 524288.0 + P2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * P1;
    if (var1 != 0) {
      double p = 1048576.0 - adcP;
      p = (p - var2 / 4096.0) * 6250.0 / var1;
      var1 = P9 * p * p / 2147483648.0;
      var2 = p * P8 / 32768.0;
      p = p + (var1 + var2 + P7) / 16.0;
      out[(uint8_t)EnvValue::PRESSURE] = p / 100.0;  // Pa to hPa
    }
  }

  // Humidity
  if (humidity) {
    const int32_t adcH = ((uint32_t)b[6] << 8) | b[7];
    if (adcH != ADC_SKIPPED_16) {
      double h = tFine - 76800.0;
      h = (adcH - (H4 * 64.0 + H5 / 16384.0 * h)) * (H2 / 65536.0 * (1.0 + H6 / 67108864.0 * h * (1.0 + H3 / 67108864.0 * h)));
      h = h * (1.0 - H1 * h / 524288.0);
      out[(uint8_t)EnvValue::HUMIDITY] = constrain(h, 0.0, 100.0);
    }
  }
  return ReadResult::OK;
}
