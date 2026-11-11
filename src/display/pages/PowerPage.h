#pragma once

#include "display/pages/SensorPage.h"

#if defined(SCREEN) && defined(SENSORS)

// A current / voltage monitor (INA219, INA3221), refreshed with every published value (5x a second).
//   Card:   one channel - current big, voltage and power under it; three channels - a row each with
//           current, voltage and power
//   Opened: one channel at a time - current big, voltage and power, a graph of the last 20s of
//           current. PWR / 2x PWR step through the channels, BOOT leaves.
class CPowerPage : public CSensorPage {

public:
  CPowerPage(CDevice &device, const CPowerSensor &sensor) : CSensorPage(device, sensor) {}

  void drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) override;
  void onOpen() override { channel = 0; }
  void drawContent(Arduino_GFX &gfx) override;
  PageAction onButton(ButtonId id, ButtonEvent event) override;

  static void drawIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy);   // Dial

private:
  uint8_t channel = 0;

  const CPowerSensor* power() const { return static_cast<const CPowerSensor*>(sensor()); }
  void drawChannelRow(Arduino_GFX &gfx, const CPowerSensor &s, uint8_t ch, int16_t x, int16_t y, int16_t w);
};

#endif
