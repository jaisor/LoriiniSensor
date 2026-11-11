#include "display/pages/NoSensorPage.h"

#include "Configuration.h"
#ifdef SCREEN

#include "display/UI.h"

#ifdef SENSORS
  #define LINE1 "None found"
  #define LINE2 "SDA 11, SCL 10, 3.3V"
#else
  #define LINE1 "No sensor support"
  #define LINE2 ""
#endif

// Plug with two pins, for the card
static void drawPlugIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy) {
  gfx.fillRect(cx - 12, cy - 34, 6, 16, UI_COLOR_MUTED);
  gfx.fillRect(cx + 6, cy - 34, 6, 16, UI_COLOR_MUTED);
  gfx.fillRoundRect(cx - 22, cy - 20, 44, 30, 8, UI_COLOR_ACCENT);
  gfx.fillRect(cx - 6, cy + 10, 12, 10, UI_COLOR_ACCENT);
  gfx.fillRoundRect(cx - 3, cy + 18, 6, 16, 3, UI_COLOR_MUTED);
}

void CNoSensorPage::drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  drawPlugIcon(gfx, x + w / 2, y + 66);
  UI_drawCentered(gfx, getTitle(), y + 134, UI_FONT_TITLE, UI_COLOR_TEXT, x, w);
  UI_drawCentered(gfx, LINE1, y + 168, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
  UI_drawCentered(gfx, LINE2, y + 190, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
}

void CNoSensorPage::drawContent(Arduino_GFX &gfx) {
  UI_drawCentered(gfx, "--", UI_BODY_TOP + 70, UI_FONT_LARGE, UI_COLOR_MUTED);
  #ifdef SENSORS
  UI_drawCentered(gfx, "No sensors found", UI_BODY_TOP + 110, UI_FONT_LABEL, UI_COLOR_TEXT);
  UI_drawCentered(gfx, "Connect one on I2C:", UI_BODY_TOP + 140, UI_FONT_SMALL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, LINE2, UI_BODY_TOP + 160, UI_FONT_SMALL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, "Shows up within seconds", UI_BODY_TOP + 190, UI_FONT_SMALL, UI_COLOR_MUTED);
  #else
  UI_drawCentered(gfx, LINE1, UI_BODY_TOP + 110, UI_FONT_LABEL, UI_COLOR_TEXT);
  #endif
}

PageAction CNoSensorPage::onButton(ButtonId id, ButtonEvent event) {
  if (id == ButtonId::BOOT && (event == ButtonEvent::LONG_PRESS || event == ButtonEvent::CLICK)) {
    return PageAction::EXIT;
  }
  return PageAction::NONE;
}

#endif
