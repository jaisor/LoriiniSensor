#pragma once

#include "Configuration.h"

#ifdef SCREEN

#include <Arduino_GFX_Library.h>
#include "Device.h"
#include "display/Screen.h"

enum class ScreenId : uint8_t {
  BOOT = 0,
  WIFI_STATUS,
  COUNT
};

class CDisplay {

public:
  explicit CDisplay(CDevice &device);
  ~CDisplay();

  void begin();
  void loop();

  // Device state changed: show the screen mapped to it
  void onStateChanged(DeviceState state);

  void show(ScreenId id);
  CScreen* getScreen() const { return current; }

  // Overlay drawn on top of whatever screen is showing
  void showMessage(const char *message, unsigned long durationMs);

  bool isReady() const { return ready; }

private:
  CDevice &device;
  Arduino_DataBus *bus;
  Arduino_GFX *panel;
  Arduino_Canvas *canvas;   // Full-screen framebuffer (in PSRAM), flushed to the panel after each draw
  bool ready;

  CScreen *screens[(uint8_t)ScreenId::COUNT];
  CScreen *current;

  uint8_t appliedBrightness;
  unsigned long tsLastFlush;

  char message[64];
  unsigned long tsMessageEnd;
  bool messageShowing;

  ScreenId screenForState(DeviceState state);
  void applyBrightness();
  void render();
  void drawMessage();
};

#endif
