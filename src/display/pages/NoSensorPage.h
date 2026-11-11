#pragma once

#include "display/pages/Page.h"

// Stands in for the sensor pages while no sensor is connected: where to connect one. CSensors looks
// for new ones every few seconds, and their pages replace this one.
class CNoSensorPage : public CPage {

public:
  explicit CNoSensorPage(CDevice &device) : CPage(device) {}

  const char* getTitle() const override { return "Sensors"; }
  void drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) override;
  void drawContent(Arduino_GFX &gfx) override;
  PageAction onButton(ButtonId id, ButtonEvent event) override;
};
