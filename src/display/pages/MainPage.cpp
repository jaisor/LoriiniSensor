#include "display/pages/MainPage.h"

#include "Configuration.h"
#ifdef SCREEN

#include "Device.h"
#include "display/UI.h"

// Dial icon centered on cx,cy
static void drawGaugeIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy, uint16_t color) {
  // 270 degree dial open at the bottom (0 degrees = 3 o'clock, clockwise)
  gfx.fillArc(cx, cy, 30, 24, 135, 359.9, color);
  gfx.fillArc(cx, cy, 30, 24, 0, 45, color);
  // Needle
  for (int8_t t = -1; t <= 1; t++) {
    gfx.drawLine(cx + t, cy, cx + 14 + t, cy - 14, UI_COLOR_TEXT);
  }
  gfx.fillCircle(cx, cy, 5, UI_COLOR_TEXT);
}

void CMainPage::drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  drawGaugeIcon(gfx, x + w / 2, y + 58, UI_COLOR_ACCENT);
  UI_drawCentered(gfx, getTitle(), y + 118, UI_FONT_TITLE, UI_COLOR_TEXT, x, w);
  // TODO: the primary sensor value goes here
  UI_drawCentered(gfx, "--", y + 154, UI_FONT_LARGE, UI_COLOR_MUTED, x, w);
  UI_drawCentered(gfx, "No sensor yet", y + 178, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
}

void CMainPage::drawContent(Arduino_GFX &gfx) {
  // TODO: list every reading of the configured sensor(s) with units and age
  UI_drawCentered(gfx, "--", UI_BODY_TOP + 60, UI_FONT_LARGE, UI_COLOR_MUTED);
  UI_drawCentered(gfx, "No sensor configured", UI_BODY_TOP + 96, UI_FONT_LABEL, UI_COLOR_TEXT);
  UI_drawCentered(gfx, "Sensor values will", UI_BODY_TOP + 130, UI_FONT_SMALL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, "show up here", UI_BODY_TOP + 150, UI_FONT_SMALL, UI_COLOR_MUTED);
}

PageAction CMainPage::onButton(ButtonId id, ButtonEvent event) {
  if (id == ButtonId::BOOT && (event == ButtonEvent::LONG_PRESS || event == ButtonEvent::CLICK)) {
    return PageAction::EXIT;
  }
  return PageAction::NONE;
}

#endif
