#pragma once

#include "display/pages/Page.h"

// Menu of device settings, each opening its own view:
//   WiFi        network details; forget the network to restart in setup mode
//   Sensor      sensor selection and configuration (TODO: no sensor drivers yet)
//   Brightness  backlight level, live
//   Sound       button sounds volume, live
//   About       name, version, memory, uptime
class CSettingsPage : public CPage {

public:
  explicit CSettingsPage(CDevice &device) : CPage(device) {}

  const char* getTitle() const override { return "Settings"; }
  void drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) override;

  void onOpen() override;
  void onClose() override;
  const char* getHeader() const override;
  const char* getHint() const override;
  void drawContent(Arduino_GFX &gfx) override;
  PageAction onButton(ButtonId id, ButtonEvent event) override;

private:
  enum class View : uint8_t { MENU, NETWORK, FORGET_WIFI, SENSOR, BRIGHTNESS, VOLUME, ABOUT };

  enum Item : uint8_t { ITEM_WIFI, ITEM_SENSOR, ITEM_BRIGHTNESS, ITEM_VOLUME, ITEM_ABOUT, ITEM_BACK, ITEM_COUNT };
  static const char* const ITEM_LABELS[ITEM_COUNT];

  View view = View::MENU;
  uint8_t selected = 0;
  bool levelChanged = false;   // Brightness / volume edited, saved when leaving the view

  bool canForgetWifi() const;
  void leaveLevelView();

  void drawMenu(Arduino_GFX &gfx);
  void drawWifi(Arduino_GFX &gfx);
  void drawForgetWifi(Arduino_GFX &gfx);
  void drawSensor(Arduino_GFX &gfx);
  void drawLevel(Arduino_GFX &gfx, uint8_t value, const char *offText);
  void drawAbout(Arduino_GFX &gfx);
};
