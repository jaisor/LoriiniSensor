#pragma once

#include "display/pages/Page.h"

// Menu of device settings, each opening its own view:
//   WiFi        network details; forget the network to restart in setup mode
//   Sensors     the detected sensors, their addresses and whether they answer
//   Brightness  backlight level
//   Screen saver  idle time before the screen dims, then goes dark (presets)
//   Sound       button sounds volume
// The three values apply live while edited: BOOT long press saves, BOOT or PWR double click (or
// leaving the page) puts the old value back. PWR / BOOT click or the on-screen > / < step them.
//   About       name, version, memory, uptime
class CSettingsPage : public CPage {

public:
  explicit CSettingsPage(CDevice &device) : CPage(device) {}

  const char* getTitle() const override { return "Settings"; }
  void drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) override;

  void onOpen() override;
  void onClose() override;
  void drawContent(Arduino_GFX &gfx) override;
  PageAction onButton(ButtonId id, ButtonEvent event) override;
  #ifdef TOUCH
  PageAction onTouch(TouchEvent event, int16_t x, int16_t y) override;
  #endif
  bool update() override;

private:
  enum class View : uint8_t { MENU, NETWORK, FORGET_WIFI, SENSOR, BRIGHTNESS, SCREEN_SAVER, VOLUME, ABOUT };

  enum Item : uint8_t { ITEM_WIFI, ITEM_SENSOR, ITEM_BRIGHTNESS, ITEM_SCREEN_SAVER, ITEM_VOLUME, ITEM_ABOUT, ITEM_BACK, ITEM_COUNT };
  static const char* const ITEM_LABELS[ITEM_COUNT];

  View view = View::MENU;
  uint8_t selected = 0;
  uint16_t editOriginal = 0;   // Value of the edit view's setting when it was opened, for cancel
  int8_t pressedDir = 0;       // Arrow lit after a step: -1 <, 1 >, 0 none
  unsigned long tsPressed = 0;

  bool canForgetWifi() const;
  bool isEditView() const;     // Brightness, screen saver, volume
  uint16_t editValue() const;
  void setEditValue(uint16_t value);
  void startEdit(View v);
  void stepEdit(int8_t dir);
  void saveEdit();
  void cancelEdit();

  void drawMenu(Arduino_GFX &gfx);
  void drawWifi(Arduino_GFX &gfx);
  void drawForgetWifi(Arduino_GFX &gfx);
  void drawSensors(Arduino_GFX &gfx);
  void drawLevel(Arduino_GFX &gfx, const char *label, uint8_t value, const char *offText);
  void drawScreenSaver(Arduino_GFX &gfx);
  void drawStepper(Arduino_GFX &gfx, uint8_t percent);
  void drawAbout(Arduino_GFX &gfx);
};
