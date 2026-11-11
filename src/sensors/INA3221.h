#pragma once

#include "sensors/PowerSensor.h"

#define INA3221_ADDRESS_MIN 0x40    // A0 to GND / VS / SDA / SCL
#define INA3221_ADDRESS_MAX 0x43

// TI INA3221 three channel current / bus voltage monitor (identified by its manufacturer and die ID).
//
// Continuous shunt and bus conversions of all three channels, 1.1ms each, averaged 16 times: a full
// set every ~106ms. Bus voltage up to 26V (8mV steps), shunt range +-163.8mV (40uV steps). Current
// comes from configuration.ina3221ShuntMilliOhm, the same for all channels: the common boards have
// 0.1 ohm shunts, which gives +-1.6A at 0.4mA resolution.
class CINA3221 : public CPowerSensor {

public:
  explicit CINA3221(uint8_t address) : CPowerSensor(SensorModel::INA3221, address, 3) {}

  static bool identify(uint8_t address);   // Answers with the INA3221's IDs
  bool init() override;

protected:
  bool readChannels(power_reading_t *out) override;
};
