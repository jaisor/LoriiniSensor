#pragma once

#include "sensors/EnvSensor.h"

#define AHT_ADDRESS      0x38   // AHT10 can be strapped to 0x39
#define AHT_ADDRESS_ALT  0x39
#define AHT_INTERVAL_MS  2000   // The datasheets ask for 2s or more between measurements (self-heating)

// ASAIR AHT10 / AHT20 (and the AHT21 / AHT25, which behave like the AHT20) temperature and humidity
// sensors. Both use the same measurement command and data layout; the AHT20 adds a CRC byte, which is
// how init() tells them apart (the AHT10 has no ID register).
//
// Each measurement is triggered, takes ~80ms, and is read when the busy bit clears.
class CAHTx0 : public CEnvSensor {

public:
  explicit CAHTx0(uint8_t address);

  // Soft reset, calibration, then one blocking measurement (~80ms) that identifies the model
  bool init() override;

protected:
  bool startMeasurement() override;
  unsigned long conversionMs() const override { return 80; }
  ReadResult readMeasurement(float *out) override;

private:
  bool hasCrc = false;   // AHT20

  bool readStatus(uint8_t &status);
  // A finished measurement: 6 bytes, and the CRC as a 7th when crc is set
  ReadResult readRaw(uint8_t *b, bool crc);
};
