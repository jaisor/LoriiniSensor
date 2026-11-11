#pragma once

#include "display/pages/Page.h"

// Current sensor values. TODO: no sensor drivers exist yet, so it shows placeholders.
class CMainPage : public CPage {

public:
  explicit CMainPage(CDevice &device) : CPage(device) {}

  const char* getTitle() const override { return "Main"; }
  void drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) override;
  void drawContent(Arduino_GFX &gfx) override;
  PageAction onButton(ButtonId id, ButtonEvent event) override;
};
