#include "display/screens/BootScreen.h"

#include "Configuration.h"
#ifdef SCREEN

#include <version.h>
#include "Device.h"
#include "display/UI.h"

void CBootScreen::draw(Arduino_GFX &gfx) {
  const int16_t cx = LCD_WIDTH / 2;

  // Lorikeet stripe
  const uint16_t stripe[] = { UI_COLOR_BLUE, UI_COLOR_ACCENT, UI_COLOR_YELLOW, UI_COLOR_ORANGE, UI_COLOR_RED };
  const int16_t sw = 24, sh = 6;
  const int16_t sx = cx - (5 * sw) / 2;
  for (uint8_t i = 0; i < 5; i++) {
    gfx.fillRect(sx + i * sw, 96, sw, sh, stripe[i]);
  }

  UI_drawCentered(gfx, configuration.name, 140, UI_FONT_TITLE, UI_COLOR_TEXT);
  UI_drawCentered(gfx, VERSION_SHORT, 166, UI_FONT_SMALL, UI_COLOR_MUTED);

  // Progress bar
  const int16_t bw = 160, bh = 10;
  const int16_t bx = cx - bw / 2, by = 196;
  gfx.drawRoundRect(bx, by, bw, bh, bh / 2, UI_COLOR_DIVIDER);
  int16_t fill = (bw - 4) * device.getBootProgress() / 100;
  if (fill > 0) {
    gfx.fillRoundRect(bx + 2, by + 2, fill, bh - 4, (bh - 4) / 2, UI_COLOR_ACCENT);
  }

  UI_drawCentered(gfx, "Starting up", 232, UI_FONT_SMALL, UI_COLOR_MUTED);
}

#endif
