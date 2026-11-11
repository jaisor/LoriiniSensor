#include "sensors/INA219.h"

#include <ArduinoLog.h>
#include "Configuration.h"
#include "sensors/I2C.h"

// Registers (datasheet SBOS448)
#define REG_CONFIG       0x00
#define REG_SHUNT        0x01  // Signed, LSB 10uV
#define REG_BUS          0x02  // Bits 15-3, LSB 4mV; bit 1 CNVR, bit 0 OVF
#define REG_TI_MFR_ID    0xFE  // Not on the INA219; other TI chips at these addresses answer 0x5449 ("TI")
#define TI_MFR_ID        0x5449

#define CONFIG_RESET     0x8000
#define CONFIG_BRNG_32V  (1 << 13)
#define CONFIG_PG_320MV  (3 << 11)
#define CONFIG_BADC(x)   ((x) << 7)
#define CONFIG_SADC(x)   ((x) << 3)
#define ADC_32_SAMPLES   0b1101      // 12-bit, 32 sample average, 17.02ms
#define MODE_CONTINUOUS  0b111       // Shunt and bus, continuous

#define CONFIG_VALUE (CONFIG_BRNG_32V | CONFIG_PG_320MV | CONFIG_BADC(ADC_32_SAMPLES) | CONFIG_SADC(ADC_32_SAMPLES) | MODE_CONTINUOUS)

#define SHUNT_FULL_SCALE 32000       // Counts at +-320mV

bool CINA219::init() {
  const uint8_t addr = getAddress();
  uint16_t id = 0;
  if (I2C_readReg16(addr, REG_TI_MFR_ID, id) && id == TI_MFR_ID) {
    return false;
  }
  if (!I2C_writeReg16(addr, REG_CONFIG, CONFIG_RESET)) {
    return false;
  }
  delay(1);
  uint16_t config = 0;
  if (!I2C_writeReg16(addr, REG_CONFIG, CONFIG_VALUE) || !I2C_readReg16(addr, REG_CONFIG, config) || config != CONFIG_VALUE) {
    Log.verboseln("0x%x is not an INA219 (config read back 0x%x)", addr, config);
    return false;
  }
  Log.infoln("INA219 at 0x%x, shunt %u mOhm", addr, configuration.ina219ShuntMilliOhm);
  restart();
  return true;
}

bool CINA219::readChannels(power_reading_t *out) {
  uint16_t shunt, bus;
  if (!I2C_readReg16(getAddress(), REG_SHUNT, shunt) || !I2C_readReg16(getAddress(), REG_BUS, bus)) {
    return false;
  }
  const int16_t shuntRaw = (int16_t)shunt;
  out[0].voltage = (bus >> 3) * 0.004f;
  out[0].current = shuntRaw * 10e-6f / (configuration.ina219ShuntMilliOhm / 1000.0f);
  out[0].overRange = abs(shuntRaw) >= SHUNT_FULL_SCALE;
  return true;
}
