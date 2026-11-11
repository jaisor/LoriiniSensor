#include "sensors/INA219.h"

#include <Wire.h>
#include <ArduinoLog.h>

// Registers (datasheet SBOS448)
#define REG_CONFIG       0x00
#define REG_SHUNT        0x01  // Signed, LSB 10uV
#define REG_BUS          0x02  // Bits 15-3, LSB 4mV; bit 1 CNVR, bit 0 OVF

#define CONFIG_RESET     0x8000
#define CONFIG_BRNG_32V  (1 << 13)
#define CONFIG_PG_320MV  (3 << 11)
#define CONFIG_BADC(x)   ((x) << 7)
#define CONFIG_SADC(x)   ((x) << 3)
#define ADC_32_SAMPLES   0b1101      // 12-bit, 32 sample average, 17.02ms
#define MODE_CONTINUOUS  0b111       // Shunt and bus, continuous

#define CONFIG_VALUE (CONFIG_BRNG_32V | CONFIG_PG_320MV | CONFIG_BADC(ADC_32_SAMPLES) | CONFIG_SADC(ADC_32_SAMPLES) | MODE_CONTINUOUS)

#define SHUNT_FULL_SCALE 32000       // Counts at +-320mV

CINA219::CINA219() : address(0), shuntOhm(0.1f) {
}

bool CINA219::writeRegister(uint8_t reg, uint16_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value >> 8);
  Wire.write(value & 0xFF);
  return Wire.endTransmission() == 0;
}

bool CINA219::readRegister(uint8_t reg, uint16_t &value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, (uint8_t)2) != 2) return false;
  value = (uint16_t)Wire.read() << 8;
  value |= Wire.read();
  return true;
}

bool CINA219::begin(uint8_t addr, uint16_t shuntMilliOhm) {
  address = addr;
  shuntOhm = shuntMilliOhm / 1000.0f;

  if (!writeRegister(REG_CONFIG, CONFIG_RESET)) {
    return false;  // Nothing at this address
  }
  delay(1);
  uint16_t config = 0;
  if (!writeRegister(REG_CONFIG, CONFIG_VALUE) || !readRegister(REG_CONFIG, config) || config != CONFIG_VALUE) {
    Log.warningln("INA219 at 0x%x didn't take the configuration (read back 0x%x)", address, config);
    return false;
  }
  Log.infoln("INA219 at 0x%x, shunt %u mOhm", address, shuntMilliOhm);
  return true;
}

bool CINA219::read(reading_t &r) {
  uint16_t shunt, bus;
  if (!readRegister(REG_SHUNT, shunt) || !readRegister(REG_BUS, bus)) {
    return false;
  }
  int16_t shuntRaw = (int16_t)shunt;
  r.shuntVoltage = shuntRaw * 10e-6f;
  r.busVoltage = (bus >> 3) * 0.004f;
  r.current = r.shuntVoltage / shuntOhm;
  r.overRange = abs(shuntRaw) >= SHUNT_FULL_SCALE;
  return true;
}
