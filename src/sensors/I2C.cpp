#include "sensors/I2C.h"

#include <Wire.h>

bool I2C_probe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool I2C_write(uint8_t address, const uint8_t *data, size_t len) {
  Wire.beginTransmission(address);
  Wire.write(data, len);
  return Wire.endTransmission() == 0;
}

bool I2C_read(uint8_t address, uint8_t *data, size_t len) {
  if (Wire.requestFrom(address, (size_t)len) != len) return false;
  for (size_t i = 0; i < len; i++) {
    data[i] = Wire.read();
  }
  return true;
}

bool I2C_writeReg8(uint8_t address, uint8_t reg, uint8_t value) {
  const uint8_t data[] = { reg, value };
  return I2C_write(address, data, sizeof(data));
}

bool I2C_writeReg16(uint8_t address, uint8_t reg, uint16_t value) {
  const uint8_t data[] = { reg, (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) };
  return I2C_write(address, data, sizeof(data));
}

bool I2C_readRegs(uint8_t address, uint8_t reg, uint8_t *data, size_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // Repeated start
  return I2C_read(address, data, len);
}

bool I2C_readReg8(uint8_t address, uint8_t reg, uint8_t &value) {
  return I2C_readRegs(address, reg, &value, 1);
}

bool I2C_readReg16(uint8_t address, uint8_t reg, uint16_t &value) {
  uint8_t b[2];
  if (!I2C_readRegs(address, reg, b, 2)) return false;
  value = ((uint16_t)b[0] << 8) | b[1];
  return true;
}
