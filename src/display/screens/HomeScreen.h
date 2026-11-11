#pragma once

#include "display/Screen.h"
#include "display/pages/Page.h"

#define HOME_PAGE_COUNT 3

// Status bar over a carousel of pages (Main, Settings, Help).
//   Browsing: PWR click or swipe left = next page (slides in from the right), PWR double click or swipe
//             right = previous, BOOT click or double tap = open.
//   Opened:   buttons go to the page; it returns PageAction::EXIT (BOOT long press by convention) to browse
//             again. A long press on the screen leaves the page from anywhere in it.
class CHomeScreen : public CScreen {

public:
  explicit CHomeScreen(CDevice &device);
  ~CHomeScreen();

  const char* getName() const override { return "Home"; }
  bool update() override;
  void draw(Arduino_GFX &gfx) override;
  bool onButton(ButtonId id, ButtonEvent event) override;
  #ifdef TOUCH
  bool onTouch(TouchEvent event, int16_t x, int16_t y) override;
  #endif

private:
  CPage *pages[HOME_PAGE_COUNT];
  uint8_t index = 0;
  bool opened = false;

  // Slide between cards
  bool sliding = false;
  int8_t slideDir = 0;          // 1 = new card enters from the right
  uint8_t slideFrom = 0;
  unsigned long tsSlide = 0;

  void slide(int8_t dir);
  void drawCard(Arduino_GFX &gfx, uint8_t page, int16_t dx);
};
