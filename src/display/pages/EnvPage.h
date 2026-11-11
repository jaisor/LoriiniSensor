#pragma once

#include "display/pages/SensorPage.h"

#if defined(SCREEN) && defined(SENSORS)

// An environment sensor (BME280, BMP280, BME680 / BME688, AHT10 / AHT20), refreshed with every
// reading (1-3s, per chip).
//   Card:   temperature big, then whichever of humidity, pressure and gas resistance it measures
//   Opened: one value at a time - big, with its range and a graph over the last 10 minutes.
//           PWR / 2x PWR step through the values, BOOT leaves.
class CEnvPage : public CSensorPage {

public:
  CEnvPage(CDevice &device, const CEnvSensor &sensor) : CSensorPage(device, sensor) {}

  void drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) override;
  void onOpen() override { selected = 0; }
  void drawContent(Arduino_GFX &gfx) override;
  PageAction onButton(ButtonId id, ButtonEvent event) override;

  static void drawIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy);   // Thermometer

private:
  uint8_t selected = 0;   // Index into the values the sensor has

  const CEnvSensor* env() const { return static_cast<const CEnvSensor*>(sensor()); }
  // The values this sensor measures, temperature first; returns how many
  static uint8_t valuesOf(const CEnvSensor &s, EnvValue *out);
};

#endif
