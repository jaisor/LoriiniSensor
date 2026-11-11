#include "sensors/INA3221.h"

#include <ArduinoLog.h>
#include "Configuration.h"
#include "sensors/I2C.h"

// Registers (datasheet SBOS576)
#define REG_CONFIG       0x00
#define REG_SHUNT(ch)    (0x01 + 2 * (ch))   // Bits 15-3 signed, LSB 40uV
#define REG_BUS(ch)      (0x02 + 2 * (ch))   // Bits 15-3 signed, LSB 8mV
#define REG_MFR_ID       0xFE
#define REG_DIE_ID       0xFF
#define MFR_ID           0x5449              // "TI"
#define DIE_ID           0x3220

#define CONFIG_RESET     0x8000
#define CONFIG_CH_ALL    (0b111 << 12)
#define CONFIG_AVG(x)    ((x) << 9)
#define CONFIG_VBUSCT(x) ((x) << 6)
#define CONFIG_VSHCT(x)  ((x) << 3)
#define AVG_16           0b010
#define CT_1100US        0b100
#define MODE_CONTINUOUS  0b111               // Shunt and bus, continuous

#define CONFIG_VALUE (CONFIG_CH_ALL | CONFIG_AVG(AVG_16) | CONFIG_VBUSCT(CT_1100US) | CONFIG_VSHCT(CT_1100US) | MODE_CONTINUOUS)

#define SHUNT_FULL_SCALE 4095                // Counts (after the shift) at +-163.8mV

bool CINA3221::identify(uint8_t address) {
  uint16_t mfr = 0, die = 0;
  return I2C_readReg16(address, REG_MFR_ID, mfr) && I2C_readReg16(address, REG_DIE_ID, die)
    && mfr == MFR_ID && die == DIE_ID;
}

bool CINA3221::init() {
  const uint8_t addr = getAddress();
  if (!identify(addr) || !I2C_writeReg16(addr, REG_CONFIG, CONFIG_RESET)) {
    return false;
  }
  delay(1);
  uint16_t config = 0;
  if (!I2C_writeReg16(addr, REG_CONFIG, CONFIG_VALUE) || !I2C_readReg16(addr, REG_CONFIG, config) || config != CONFIG_VALUE) {
    Log.warningln("INA3221 at 0x%x didn't take the configuration (read back 0x%x)", addr, config);
    return false;
  }
  Log.infoln("INA3221 at 0x%x, shunts %u mOhm", addr, configuration.ina3221ShuntMilliOhm);
  restart();
  return true;
}

bool CINA3221::readChannels(power_reading_t *out) {
  const float shuntOhm = configuration.ina3221ShuntMilliOhm / 1000.0f;
  for (uint8_t ch = 0; ch < 3; ch++) {
    uint16_t shunt, bus;
    if (!I2C_readReg16(getAddress(), REG_SHUNT(ch), shunt) || !I2C_readReg16(getAddress(), REG_BUS(ch), bus)) {
      return false;
    }
    const int16_t shuntRaw = (int16_t)shunt >> 3;
    out[ch].voltage = ((int16_t)bus >> 3) * 0.008f;
    out[ch].current = shuntRaw * 40e-6f / shuntOhm;
    out[ch].overRange = abs(shuntRaw) >= SHUNT_FULL_SCALE;
  }
  return true;
}
