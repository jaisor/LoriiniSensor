#pragma once

#include "display/pages/Page.h"

// How to use the buttons, touch and the pages, in sections: PWR click / double click steps through
// them, BOOT leaves. Keep it in step with what the pages actually do.
class CHelpPage : public CPage {

public:
  explicit CHelpPage(CDevice &device) : CPage(device) {}

  const char* getTitle() const override { return "Help"; }
  void drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) override;

  void onOpen() override { section = 0; }
  void drawContent(Arduino_GFX &gfx) override;
  PageAction onButton(ButtonId id, ButtonEvent event) override;

private:
  uint8_t section = 0;
};
