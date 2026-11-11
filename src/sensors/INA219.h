#pragma once

#include "sensors/PowerSensor.h"

#define INA219_ADDRESS_MIN 0x40     // A0/A1 straps select 0x40-0x4F
#define INA219_ADDRESS_MAX 0x4F

// TI INA219 high-side current / bus voltage monitor.
//
// Runs continuously with 32-sample averaging on both ADCs (~17ms per conversion each), 32V bus range
// and the widest shunt range (+-320mV). Current is computed from the shunt voltage and
// configuration.ina219ShuntMilliOhm (applied live), so the chip's calibration / current / power
// registers aren't used: with a 0.1 ohm shunt that gives +-3.2A at 0.1mA resolution.
class CINA219 : public CPowerSensor {

public:
  explicit CINA219(uint8_t address) : CPowerSensor(SensorModel::INA219, address, 1) {}

  // Resets and configures the chip, checking it reads the configuration back. Refuses TI chips that
  // carry ID registers (INA3221, INA226, HDC1080, ...), which the INA219 doesn't have.
  bool init() override;

protected:
  bool readChannels(power_reading_t *out) override;
};
