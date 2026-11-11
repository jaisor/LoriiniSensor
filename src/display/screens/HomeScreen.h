#pragma once

#include "display/Screen.h"
#include "display/pages/Page.h"

#define HOME_PAGE_COUNT 2

// Status bar over a carousel of pages (Main, Settings).
//   Browsing: PWR click = next page (slides in from the right), PWR double click = previous, BOOT click = open.
//   Opened:   buttons go to the page; it returns PageAction::EXIT (BOOT long press by convention) to browse again.
class CHomeScreen : public CScreen {

public:
  explicit CHomeScreen(CDevice &device);
  ~CHomeScreen();

  const char* getName() const override { return "Home"; }
  bool update() override;
  void draw(Arduino_GFX &gfx) override;
  bool onButton(ButtonId id, ButtonEvent event) override;

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
