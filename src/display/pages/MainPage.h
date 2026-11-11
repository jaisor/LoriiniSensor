#pragma once

#include "display/pages/Page.h"

// Live sensor values: current, voltage and power from the INA219 with a graph of recent current.
// Redraws whenever the sensor publishes a value (SENSOR_PUBLISH_MS), as the card and when opened.
class CMainPage : public CPage {

public:
  explicit CMainPage(CDevice &device) : CPage(device) {}

  const char* getTitle() const override { return "Main"; }
  void drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) override;
  void drawContent(Arduino_GFX &gfx) override;
  PageAction onButton(ButtonId id, ButtonEvent event) override;
  bool update() override;

private:
  uint32_t drawnSample = 0;

  bool hasReadings() const;
  // Why there are no values, two short lines
  void statusText(const char **line1, const char **line2) const;
  void drawGraph(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h);
};
