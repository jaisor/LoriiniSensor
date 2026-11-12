#pragma once

#include "Configuration.h"

#ifdef SCREEN

#include <Arduino_GFX_Library.h>
#include "Device.h"
#include "display/Screen.h"

enum class ScreenId : uint8_t {
  BOOT = 0,
  WIFI_STATUS,   // Setup mode instructions
  HOME,          // Status bar + page carousel
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

  // Button gesture, passed to the screen that is showing
  void onButton(ButtonId id, ButtonEvent event);
  #ifdef TOUCH
  // Touch gesture, passed to the screen that is showing; true if it acted on it
  bool onTouch(TouchEvent event, int16_t x, int16_t y);
  bool wantsDoubleTap() const { return !current || current->wantsDoubleTap(); }
  #endif

  // Overlay drawn on top of whatever screen is showing
  void showMessage(const char *message, unsigned long durationMs);

  // Screen saver: a button is down or the screen touched. Restarts the idle time and restores the
  // brightness; true if the screen was dark, in which case the input only wakes it (the caller drops it).
  bool wake();
  bool isDark() const { return saver == SaverState::DARK; }

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

  // Screen saver (configuration.screenSaverSec): DIM at half the idle time, DARK at all of it
  enum class SaverState : uint8_t { AWAKE, DIM, DARK };
  SaverState saver;
  unsigned long tsActivity;      // Last button press or touch
  uint16_t appliedSaverSec;      // Changing the duration restarts the idle time

  char message[64];
  unsigned long tsMessageEnd;
  bool messageShowing;

  ScreenId screenForState(DeviceState state);
  void applyBrightness();
  SaverState saverStateFor(unsigned long idleMs) const;
  void setSaver(SaverState state);
  void render();
  void drawMessage();
};

#endif
