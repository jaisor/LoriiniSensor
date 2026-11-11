#pragma once

#include <Arduino_GFX_Library.h>
#include "Button.h"

class CDevice;

enum class PageAction : uint8_t {
  NONE = 0,   // Nothing changed
  REDRAW,     // Page changed what it shows
  EXIT        // Back to the carousel
};

// One item of the home screen carousel. Shown two ways: as a card while browsing the carousel, and
// full screen (under the status bar) once opened with BOOT. There are no titles or button hints on
// screen, so pages follow the same button conventions (the Help page explains them):
//   PWR click = next item / increase, PWR double click = previous item / decrease,
//   BOOT click = select / confirm, BOOT long press = back (EXIT when at the page's top level).
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

  // Polled while opened or showing as the card; return true to redraw (animations, live values)
  virtual bool update() { return false; }

protected:
  CDevice &device;
};
