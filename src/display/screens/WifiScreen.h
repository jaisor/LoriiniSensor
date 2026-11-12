#pragma once

#include "display/Screen.h"

// Network status: connecting / setup access point / connected with the web server address / offline.
// CDisplay shows it in setup mode (WIFI_AP_CREATED); a BOOT or PWR long press, or a double tap, continues to the home screen.
class CWifiScreen : public CScreen {

public:
  explicit CWifiScreen(CDevice &device) : CScreen(device) {}

  const char* getName() const override { return "WiFi status"; }
  void onEnter() override;
  bool update() override;
  void draw(Arduino_GFX &gfx) override;
  bool onButton(ButtonId id, ButtonEvent event) override;
  #ifdef TOUCH
  bool onTouch(TouchEvent event, int16_t x, int16_t y) override;
  #endif

private:
  uint8_t animationFrame = 0;
  unsigned long tsAnimation = 0;

  bool isAnimating() const;
  void drawWifiIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy, uint8_t lit, uint16_t color);
};
