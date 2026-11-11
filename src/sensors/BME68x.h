#pragma once

#include "sensors/EnvSensor.h"

#define BME68X_INTERVAL_MS   3000   // Gas readings settle at this rate (Bosch's low power cadence)
#define BME68X_HEATER_C      320    // Hot plate target temperature
#define BME68X_HEATER_MS     150    // and how long it is held there for each gas reading

// Bosch BME680 / BME688 (told apart by the variant ID): temperature, humidity, pressure and the
// resistance of the heated metal oxide gas sensor, at 0x76 / 0x77.
//
// Forced mode every BME68X_INTERVAL_MS: temperature x2, pressure x4, humidity x1 oversampling,
// IIR filter 3, then the gas reading with the hot plate at BME68X_HEATER_C. The heater is on for
// 150ms of every 3s, which keeps its effect on the temperature reading small.
//
// The gas value is the raw resistance: higher is cleaner air. It drifts for the first minutes after
// power-up and differs per chip, so compare it with itself over time. (An IAQ index needs Bosch's
// closed source BSEC library, which isn't used.)
class CBME68x : public CEnvSensor {

public:
  explicit CBME68x(uint8_t address);

  static bool identify(uint8_t address);   // Chip ID of a BME680 / BME688
  bool init() override;

protected:
  bool startMeasurement() override;
  unsigned long conversionMs() const override;
  ReadResult readMeasurement(float *out) override;

private:
  bool highGas = false;   // BME688
  uint16_t T1;
  int16_t T2;
  int8_t T3;
  uint16_t P1;
  int16_t P2, P4, P5, P8, P9;
  int8_t P3, P6, P7;
  uint8_t P10;
  uint16_t H1, H2;
  int8_t H3, H4, H5, H7;
  uint8_t H6;
  int8_t GH1, GH3;
  int16_t GH2;
  uint8_t resHeatRange;
  int8_t resHeatVal;
  int8_t rangeSwErr;

  float ambient = 25;     // Last temperature, for the heater set point

  bool readCalibration();
  uint8_t heaterResistance(uint16_t targetC) const;
};
