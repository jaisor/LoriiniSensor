#pragma once

#include "sensors/EnvSensor.h"

#define BME280_INTERVAL_MS 1000

// Bosch BME280 (temperature, humidity, pressure) and BMP280 (no humidity), at 0x76 / 0x77, told
// apart by the chip ID.
//
// Normal mode: the chip converts on its own every ~0.55s (temperature x2, pressure x16 and humidity
// x1 oversampling, 500ms standby, IIR filter 4), read once a second. That keeps self-heating well
// under 0.1C. Compensation is the datasheet's floating point version.
class CBME280 : public CEnvSensor {

public:
  CBME280(uint8_t address, bool humidity);

  static bool identify(uint8_t address, bool &humidity);   // Chip ID of a BME280 or a BMP280
  bool init() override;

protected:
  bool startMeasurement() override { return true; }
  unsigned long conversionMs() const override { return 0; }
  ReadResult readMeasurement(float *out) override;

private:
  bool humidity;
  uint16_t T1, P1;
  int16_t T2, T3, P2, P3, P4, P5, P6, P7, P8, P9;
  uint8_t H1, H3;
  int16_t H2, H4, H5;
  int8_t H6;

  bool readCalibration();
};
