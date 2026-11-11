#pragma once

#include <Arduino.h>

// TI INA219 high-side current / bus voltage monitor on I2C.
//
// Runs continuously with 32-sample averaging on both ADCs (~17ms per conversion each), 32V bus range
// and the widest shunt range (+-320mV). Current is computed from the shunt voltage and the configured
// shunt resistance, so the chip's calibration / current / power registers aren't used: with a 0.1 ohm
// shunt that gives +-3.2A at 0.1mA resolution.
class CINA219 {

public:
  struct reading_t {
    float busVoltage;    // V at VIN- (the load side), 4mV resolution
    float shuntVoltage;  // V across the shunt, 10uV resolution
    float current;       // A, positive from VIN+ to VIN-
    bool overRange;      // Shunt voltage at the end of the +-320mV range: current is at least this
  };

  CINA219();

  // Wire must be started. False if nothing answers at address or it doesn't take the configuration.
  bool begin(uint8_t address, uint16_t shuntMilliOhm);
  bool read(reading_t &r);   // Latest conversion; false on an I2C error

  uint8_t getAddress() const { return address; }

private:
  uint8_t address;
  float shuntOhm;

  bool writeRegister(uint8_t reg, uint16_t value);
  bool readRegister(uint8_t reg, uint16_t &value);
};
