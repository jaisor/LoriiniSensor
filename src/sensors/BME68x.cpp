#include "sensors/BME68x.h"

#include <ArduinoLog.h>
#include "sensors/I2C.h"

// Registers (datasheets BST-BME680-DS001, BST-BME688-DS000; compensation as in Bosch's BME68x
// Sensor API, floating point version)
#define REG_COEFF3       0x00   // 5 bytes: res_heat_val, -, res_heat_range, -, range_sw_err
#define REG_FIELD0       0x1D   // 17 bytes: status, index, press[3], temp[3], hum[2], -, -, -, gas low[2], gas high[2]
#define REG_RES_HEAT0    0x5A
#define REG_GAS_WAIT0    0x64
#define REG_CTRL_GAS0    0x70
#define REG_CTRL_GAS1    0x71
#define REG_CTRL_HUM     0x72
#define REG_CTRL_MEAS    0x74
#define REG_CONFIG       0x75
#define REG_COEFF1       0x8A   // 23 bytes
#define REG_CHIP_ID      0xD0
#define REG_RESET        0xE0
#define REG_COEFF2       0xE1   // 14 bytes
#define REG_VARIANT_ID   0xF0

#define CHIP_ID          0x61
#define VARIANT_GAS_HIGH 0x01   // BME688
#define RESET_VALUE      0xB6

#define OSRS_X1          0b001
#define OSRS_X2          0b010
#define OSRS_X4          0b011
#define FILTER_3         0b010
#define MODE_FORCED      0b01
#define RUN_GAS_LOW      (0x01 << 4)   // ctrl_gas_1 run_gas, BME680
#define RUN_GAS_HIGH     (0x02 << 4)   // BME688

#define STATUS_NEW_DATA  0x80
#define GAS_VALID        0x20
#define HEAT_STAB        0x10
#define GAS_RANGE_MASK   0x0F

// Oversampling x2 + x4 + x1 = 7 cycles of 1.963ms, plus the fixed parts (Bosch API), then the heater
#define TPH_DURATION_MS  19

CBME68x::CBME68x(uint8_t address)
: CEnvSensor(SensorModel::BME680, address,
    ENV_BIT(EnvValue::TEMPERATURE) | ENV_BIT(EnvValue::HUMIDITY) | ENV_BIT(EnvValue::PRESSURE) | ENV_BIT(EnvValue::GAS),
    BME68X_INTERVAL_MS) {
}

bool CBME68x::identify(uint8_t address) {
  uint8_t id;
  return I2C_readReg8(address, REG_CHIP_ID, id) && id == CHIP_ID;
}

static uint16_t u16(uint8_t msb, uint8_t lsb) { return ((uint16_t)msb << 8) | lsb; }

bool CBME68x::readCalibration() {
  const uint8_t addr = getAddress();
  uint8_t c[23 + 14 + 5];
  if (!I2C_readRegs(addr, REG_COEFF1, c, 23) || !I2C_readRegs(addr, REG_COEFF2, c + 23, 14)
      || !I2C_readRegs(addr, REG_COEFF3, c + 37, 5)) {
    return false;
  }
  // Offsets into c[] as in the Bosch API: 0x8A.. at 0, 0xE1.. at 23, 0x00.. at 37
  T1 = u16(c[32], c[31]);
  T2 = (int16_t)u16(c[1], c[0]);
  T3 = (int8_t)c[2];
  P1 = u16(c[5], c[4]);
  P2 = (int16_t)u16(c[7], c[6]);
  P3 = (int8_t)c[8];
  P4 = (int16_t)u16(c[11], c[10]);
  P5 = (int16_t)u16(c[13], c[12]);
  P6 = (int8_t)c[15];
  P7 = (int8_t)c[14];
  P8 = (int16_t)u16(c[19], c[18]);
  P9 = (int16_t)u16(c[21], c[20]);
  P10 = c[22];
  H1 = ((uint16_t)c[25] << 4) | (c[24] & 0x0F);
  H2 = ((uint16_t)c[23] << 4) | (c[24] >> 4);
  H3 = (int8_t)c[26];
  H4 = (int8_t)c[27];
  H5 = (int8_t)c[28];
  H6 = c[29];
  H7 = (int8_t)c[30];
  GH1 = (int8_t)c[35];
  GH2 = (int16_t)u16(c[34], c[33]);
  GH3 = (int8_t)c[36];
  resHeatVal = (int8_t)c[37];
  resHeatRange = (c[39] & 0x30) >> 4;
  rangeSwErr = ((int8_t)(c[41] & 0xF0)) / 16;
  return true;
}

// res_heat_x for a hot plate temperature, at the current ambient temperature
uint8_t CBME68x::heaterResistance(uint16_t targetC) const {
  if (targetC > 400) targetC = 400;
  const float var1 = GH1 / 16.0f + 49.0f;
  const float var2 = GH2 / 32768.0f * 0.0005f + 0.00235f;
  const float var3 = GH3 / 1024.0f;
  const float var4 = var1 * (1.0f + var2 * targetC);
  const float var5 = var4 + var3 * ambient;
  return (uint8_t)(3.4f * (var5 * (4.0f / (4.0f + resHeatRange)) * (1.0f / (1.0f + resHeatVal * 0.002f)) - 25));
}

// gas_wait_x: 6 bit duration with a x1 / x4 / x16 / x64 multiplier
static uint8_t gasWait(uint16_t ms) {
  if (ms >= 0xFC0) return 0xFF;
  uint8_t factor = 0;
  while (ms > 0x3F) {
    ms /= 4;
    factor++;
  }
  return (uint8_t)(ms + factor * 64);
}

bool CBME68x::init() {
  const uint8_t addr = getAddress();
  if (!identify(addr) || !I2C_writeReg8(addr, REG_RESET, RESET_VALUE)) return false;
  delay(10);
  uint8_t variant = 0;
  if (!identify(addr) || !I2C_readReg8(addr, REG_VARIANT_ID, variant) || !readCalibration()) return false;
  highGas = variant == VARIANT_GAS_HIGH;
  model = highGas ? SensorModel::BME688 : SensorModel::BME680;

  if (!I2C_writeReg8(addr, REG_CTRL_HUM, OSRS_X1)
      || !I2C_writeReg8(addr, REG_CONFIG, FILTER_3 << 2)
      || !I2C_writeReg8(addr, REG_CTRL_MEAS, (OSRS_X2 << 5) | (OSRS_X4 << 2))   // Sleep until triggered
      || !I2C_writeReg8(addr, REG_GAS_WAIT0, gasWait(BME68X_HEATER_MS))
      || !I2C_writeReg8(addr, REG_CTRL_GAS0, 0)                                  // Heater on
      || !I2C_writeReg8(addr, REG_CTRL_GAS1, highGas ? RUN_GAS_HIGH : RUN_GAS_LOW)) {
    return false;
  }
  Log.infoln("%s at 0x%x", getName(), addr);
  restart();
  return true;
}

bool CBME68x::startMeasurement() {
  const uint8_t addr = getAddress();
  return I2C_writeReg8(addr, REG_RES_HEAT0, heaterResistance(BME68X_HEATER_C))
    && I2C_writeReg8(addr, REG_CTRL_MEAS, (OSRS_X2 << 5) | (OSRS_X4 << 2) | MODE_FORCED);
}

unsigned long CBME68x::conversionMs() const {
  return TPH_DURATION_MS + BME68X_HEATER_MS;
}

CEnvSensor::ReadResult CBME68x::readMeasurement(float *out) {
  uint8_t b[17];
  if (!I2C_readRegs(getAddress(), REG_FIELD0, b, sizeof(b))) {
    return ReadResult::ERROR;
  }
  if (!(b[0] & STATUS_NEW_DATA)) {
    return ReadResult::BUSY;
  }
  const uint32_t adcP = ((uint32_t)b[2] << 12) | ((uint32_t)b[3] << 4) | (b[4] >> 4);
  const uint32_t adcT = ((uint32_t)b[5] << 12) | ((uint32_t)b[6] << 4) | (b[7] >> 4);
  const uint16_t adcH = ((uint16_t)b[8] << 8) | b[9];

  // Temperature
  float var1 = (adcT / 16384.0f - T1 / 1024.0f) * T2;
  float var2 = (adcT / 131072.0f - T1 / 8192.0f) * (adcT / 131072.0f - T1 / 8192.0f) * (T3 * 16.0f);
  const float tFine = var1 + var2;
  const float temperature = tFine / 5120.0f;
  out[(uint8_t)EnvValue::TEMPERATURE] = temperature;
  ambient = temperature;

  // Pressure
  var1 = tFine / 2.0f - 64000.0f;
  var2 = var1 * var1 * (P6 / 131072.0f);
  var2 = var2 + var1 * P5 * 2.0f;
  var2 = var2 / 4.0f + P4 * 65536.0f;
  var1 = (P3 * var1 * var1 / 16384.0f + P2 * var1) / 524288.0f;
  var1 = (1.0f + var1 / 32768.0f) * P1;
  if ((int)var1 != 0) {
    float p = 1048576.0f - adcP;
    p = (p - var2 / 4096.0f) * 6250.0f / var1;
    var1 = P9 * p * p / 2147483648.0f;
    var2 = p * (P8 / 32768.0f);
    const float var3 = (p / 256.0f) * (p / 256.0f) * (p / 256.0f) * (P10 / 131072.0f);
    p = p + (var1 + var2 + var3 + P7 * 128.0f) / 16.0f;
    out[(uint8_t)EnvValue::PRESSURE] = p / 100.0f;  // Pa to hPa
  }

  // Humidity
  {
    const float h1 = adcH - (H1 * 16.0f + (H3 / 2.0f) * temperature);
    const float h2 = h1 * (H2 / 262144.0f * (1.0f + H4 / 16384.0f * temperature + H5 / 1048576.0f * temperature * temperature));
    const float h3 = H6 / 16384.0f;
    const float h4 = H7 / 2097152.0f;
    const float h = h2 + (h3 + h4 * temperature) * h2 * h2;
    out[(uint8_t)EnvValue::HUMIDITY] = constrain(h, 0.0f, 100.0f);
  }

  // Gas: only once the hot plate reached its temperature
  const uint8_t gasMsb = highGas ? b[15] : b[13];
  const uint8_t gasLsb = highGas ? b[16] : b[14];
  if ((gasLsb & GAS_VALID) && (gasLsb & HEAT_STAB)) {
    const uint16_t adcGas = ((uint16_t)gasMsb << 2) | (gasLsb >> 6);
    const uint8_t range = gasLsb & GAS_RANGE_MASK;
    if (highGas) {
      const uint32_t g1 = 262144UL >> range;
      const int32_t g2 = 4096 + ((int32_t)adcGas - 512) * 3;
      out[(uint8_t)EnvValue::GAS] = 1000000.0f * g1 / g2;
    } else {
      static const float K1[16] = { 0, 0, 0, 0, 0, -1, 0, -0.8f, 0, 0, -0.2f, -0.5f, 0, -1, 0, 0 };
      static const float K2[16] = { 0, 0, 0, 0, 0.1f, 0.7f, 0, -0.8f, -0.1f, 0, 0, 0, 0, 0, 0, 0 };
      const float g1 = 1340.0f + 5.0f * rangeSwErr;
      const float g2 = g1 * (1.0f + K1[range] / 100.0f);
      const float g3 = 1.0f + K2[range] / 100.0f;
      out[(uint8_t)EnvValue::GAS] = 1.0f / (g3 * 0.000000125f * (float)(1UL << range) * ((adcGas - 512.0f) / g2 + 1.0f));
    }
  }
  return ReadResult::OK;
}
