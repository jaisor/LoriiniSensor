#pragma once

#include <Arduino_GFX_Library.h>
#include "Configuration.h"
#include "Button.h"
#include "Touch.h"

class CDevice;

// One full-screen view. CDisplay owns the screens, shows one at a time and draws it into an off-screen
// canvas that is sent to the panel in one go, so draw() always paints the whole screen from scratch.
//
// Adding a screen: subclass this, add a ScreenId, create it in CDisplay::begin() and map a DeviceState
// to it in CDisplay::screenForState() (or call CDisplay::show() for screens not tied to a state).
class CScreen {

public:
  explicit CScreen(CDevice &device) : device(device) {}
  virtual ~CScreen() {}

  virtual const char* getName() const = 0;

  // Becoming visible / being replaced
  virtual void onEnter() { invalidate(); }
  virtual void onExit() {}

  // Called every display loop while visible; return true to be redrawn. The default redraws when
  // device data changed or the minute rolled over (for the clock in the status bar).
  virtual bool update();

  // Paint the whole screen, the canvas is cleared to the background color beforehand
  virtual void draw(Arduino_GFX &gfx) = 0;

  // Button gesture while this screen is showing; return true to redraw. The device plays the button's
  // sound right after (so a volume change made here is heard). Holding PWR for 3s still powers off.
  virtual bool onButton(ButtonId id, ButtonEvent event) { return false; }

  #ifdef TOUCH
  // Touch gesture (reported when the finger lifts) at its last point; return true to redraw
  virtual bool onTouch(TouchEvent event, int16_t x, int16_t y) { return false; }
  #endif

  void invalidate() { dirty = true; }
  bool isDirty() const { return dirty; }
  void drawn();

protected:
  CDevice &device;

private:
  bool dirty = true;
  uint32_t drawnRevision = 0;
  int drawnMinute = -1;
};
