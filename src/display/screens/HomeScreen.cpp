#include "display/screens/HomeScreen.h"

#include "Configuration.h"
#ifdef SCREEN

#include <ArduinoLog.h>
#include "Device.h"
#include "display/UI.h"
#include "display/pages/MainPage.h"
#include "display/pages/SettingsPage.h"

#define SLIDE_MS 240

// Card box in the carousel
#define CARD_X  SCREEN_SAFE_INSET
#define CARD_Y  UI_CONTENT_TOP
#define CARD_W  (LCD_WIDTH - 2 * SCREEN_SAFE_INSET)
#define CARD_H  188
#define DOTS_Y  (CARD_Y + CARD_H + 12)

CHomeScreen::CHomeScreen(CDevice &device) : CScreen(device) {
  pages[0] = new CMainPage(device);
  pages[1] = new CSettingsPage(device);
}

CHomeScreen::~CHomeScreen() {
  for (CPage *p : pages) {
    delete p;
  }
}

bool CHomeScreen::update() {
  bool redraw = CScreen::update();
  if (sliding) {
    if (millis() - tsSlide >= SLIDE_MS) {
      sliding = false;
    }
    redraw = true;  // Every frame of the slide, and once more at rest
  }
  return pages[index]->update() || redraw;
}

void CHomeScreen::slide(int8_t dir) {
  slideFrom = index;
  index = (index + HOME_PAGE_COUNT + dir) % HOME_PAGE_COUNT;
  slideDir = dir;
  sliding = true;
  tsSlide = millis();
  Log.verboseln("Home: page %s", pages[index]->getTitle());
}

bool CHomeScreen::onButton(ButtonId id, ButtonEvent event) {
  if (opened) {
    switch (pages[index]->onButton(id, event)) {
      case PageAction::EXIT:
        pages[index]->onClose();
        opened = false;
        return true;
      case PageAction::REDRAW:
        return true;
      default:
        return false;
    }
  }

  if (id == ButtonId::POWER && event == ButtonEvent::CLICK) {
    slide(1);
    return true;
  }
  if (id == ButtonId::POWER && event == ButtonEvent::DOUBLE_CLICK) {
    slide(-1);
    return true;
  }
  if (id == ButtonId::BOOT && event == ButtonEvent::CLICK) {
    sliding = false;
    opened = true;
    pages[index]->onOpen();
    return true;
  }
  return false;
}

void CHomeScreen::drawCard(Arduino_GFX &gfx, uint8_t page, int16_t dx) {
  const int16_t x = CARD_X + dx;
  gfx.fillRoundRect(x, CARD_Y, CARD_W, CARD_H, 18, UI_COLOR_PANEL);
  pages[page]->drawCard(gfx, x, CARD_Y, CARD_W, CARD_H);
}

void CHomeScreen::draw(Arduino_GFX &gfx) {
  UI_drawStatusBar(gfx, device);

  if (opened) {
    CPage *page = pages[index];
    UI_drawHeader(gfx, page->getHeader());
    page->drawContent(gfx);
    UI_drawHint(gfx, page->getHint());
    return;
  }

  if (sliding) {
    // Ease out: fast start, settles into place
    float t = min(1.0f, (millis() - tsSlide) / (float)SLIDE_MS);
    float e = 1 - (1 - t) * (1 - t) * (1 - t);
    int16_t travel = LCD_WIDTH;
    drawCard(gfx, slideFrom, -slideDir * travel * e);
    drawCard(gfx, index, slideDir * travel * (1 - e));
  } else {
    drawCard(gfx, index, 0);
  }

  UI_drawPageDots(gfx, DOTS_Y, HOME_PAGE_COUNT, index);
  UI_drawHint(gfx, "PWR next   BOOT open");
}

#endif
