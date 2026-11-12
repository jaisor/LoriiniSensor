#pragma once

#include <Arduino_GFX_Library.h>
#include "Button.h"
#include "Touch.h"

class CDevice;

enum class PageAction : uint8_t {
  NONE = 0,   // Nothing changed
  REDRAW,     // Page changed what it shows
  EXIT        // Back to the carousel
};

// The button convention below, for pages to test events against
inline int8_t PAGE_step(ButtonId id, ButtonEvent event) {   // 1 = next / more, -1 = previous / less, 0 = neither
  if (event != ButtonEvent::CLICK) return 0;
  return id == ButtonId::POWER ? 1 : id == ButtonId::BOOT ? -1 : 0;
}
inline bool PAGE_isUp(ButtonId id, ButtonEvent event) { return id == ButtonId::POWER && event == ButtonEvent::DOUBLE_CLICK; }
inline bool PAGE_isOk(ButtonId id, ButtonEvent event) { return id == ButtonId::BOOT && event == ButtonEvent::LONG_PRESS; }
inline bool PAGE_isCancel(ButtonId id, ButtonEvent event) { return id == ButtonId::BOOT && event == ButtonEvent::DOUBLE_CLICK; }
inline bool PAGE_isHome(ButtonId id, ButtonEvent event) { return id == ButtonId::POWER && event == ButtonEvent::LONG_PRESS; }

// One item of the home screen carousel. Shown two ways: as a card while browsing the carousel, and
// full screen (under the status bar) once opened with a BOOT long press. There are no titles or button
// hints on screen, so pages follow the same button conventions (the Help page explains them):
//   PWR click   = next item / increase         BOOT click  = previous item / decrease
//   PWR double  = one level up: EXIT at the page's top level, dropping an unsaved edit
//   BOOT long   = OK: select, open, save the value being edited
//   BOOT double = cancel: drop the edit (and go up)
//   PWR long    = home, handled by the home screen (closes the page)
// Touch: the home screen keeps the long press (closes the page); the rest comes to onTouch(), with
// taps reported on lift (no double tap) so on-screen buttons can be tapped quickly.
//
// Adding a page: subclass this, add it in CHomeScreen's constructor and describe it on the Help page.
class CPage {

public:
  explicit CPage(CDevice &device) : device(device) {}
  virtual ~CPage() {}

  virtual const char* getTitle() const = 0;  // Card title in the carousel, and for logs

  // Carousel card contents inside the card's box. While sliding the box can be partly off-screen;
  // draw relative to x,y and the canvas clips the rest.
  virtual void drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) = 0;

  // Opened
  virtual void onOpen() {}
  virtual void onClose() {}
  // Content between UI_BODY_TOP and UI_BODY_BOTTOM; the status bar is drawn above it
  virtual void drawContent(Arduino_GFX &gfx) = 0;
  virtual PageAction onButton(ButtonId id, ButtonEvent event) = 0;
  #ifdef TOUCH
  virtual PageAction onTouch(TouchEvent event, int16_t x, int16_t y) { return PageAction::NONE; }
  #endif

  // Polled while opened or showing as the card; return true to redraw (animations, live values)
  virtual bool update() { return false; }

protected:
  CDevice &device;
};
