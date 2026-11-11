#pragma once

#include <Arduino.h>

// Register access on the shared I2C bus (Wire, started in CDevice::begin()). All return false on a
// NACK or a short read. 16-bit values are big endian (the TI chips); the Bosch chips' little endian
// calibration words are assembled by their drivers.

bool I2C_probe(uint8_t address);                                  // Something acknowledges at address
bool I2C_write(uint8_t address, const uint8_t *data, size_t len); // Raw bytes, e.g. a command
bool I2C_read(uint8_t address, uint8_t *data, size_t len);        // Raw bytes, no register pointer
bool I2C_writeReg8(uint8_t address, uint8_t reg, uint8_t value);
bool I2C_writeReg16(uint8_t address, uint8_t reg, uint16_t value);
bool I2C_readRegs(uint8_t address, uint8_t reg, uint8_t *data, size_t len);
bool I2C_readReg8(uint8_t address, uint8_t reg, uint8_t &value);
bool I2C_readReg16(uint8_t address, uint8_t reg, uint16_t &value);
