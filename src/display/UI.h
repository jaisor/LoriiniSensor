#pragma once

#include "Configuration.h"

#ifdef SCREEN

#include <Arduino_GFX_Library.h>
#include <functional>
#include "display/Fonts.h"

class CDevice;

// Palette (RGB565). Accents borrow the lorikeet's colors.
#define UI_COLOR_BG       RGB565(10, 14, 28)
#define UI_COLOR_PANEL    RGB565(28, 34, 56)
#define UI_COLOR_DIVIDER  RGB565(48, 56, 84)
#define UI_COLOR_TEXT     RGB565(240, 242, 248)
#define UI_COLOR_MUTED    RGB565(140, 148, 172)
#define UI_COLOR_ACCENT   RGB565(64, 200, 96)    // green
#define UI_COLOR_BLUE     RGB565(56, 120, 232)
#define UI_COLOR_YELLOW   RGB565(248, 200, 40)
#define UI_COLOR_ORANGE   RGB565(248, 112, 40)
#define UI_COLOR_RED      RGB565(232, 64, 64)

#define UI_FONT_SMALL  (&FreeSans9pt7b)
#define UI_FONT_LABEL  (&FreeSansBold9pt7b)
#define UI_FONT_TITLE  (&FreeSansBold12pt7b)
#define UI_FONT_LARGE  (&FreeSansBold18pt7b)

#define UI_STATUS_BAR_HEIGHT 36   // Clear of the rounded corners for content below

// Layout below the status bar. No titles or button hints: the buttons work the same everywhere
// (see the Help page), so the whole area is content.
#define UI_CONTENT_TOP     44     // First row under the status bar
#define UI_BODY_TOP        UI_CONTENT_TOP
#define UI_BODY_BOTTOM     266    // Lower rows narrow quickly in the rounded corners - keep them centered

// The fonts are 7-bit ASCII: no bullets, arrows or degree signs - draw those (UI_drawChevron)

// Text with its baseline at y, horizontally centered on the screen (or within [x, x + w))
void UI_drawCentered(Arduino_GFX &gfx, const char *text, int16_t y, const GFXfont *font, uint16_t color,
  int16_t x = 0, int16_t w = LCD_WIDTH);
uint16_t UI_textWidth(Arduino_GFX &gfx, const char *text, const GFXfont *font);

// WiFi signal bars (0-4 lit) in a w x h box
void UI_drawSignalBars(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t bars, uint16_t color);

// Battery icon at x,y (UI_BATTERY_W x UI_BATTERY_H, nub included): the level (0-100) filled in and
// written inside in the small built-in font
#define UI_BATTERY_W 37
#define UI_BATTERY_H 16
void UI_drawBattery(Arduino_GFX &gfx, int16_t x, int16_t y, uint8_t percent);

// Small arrow head pointing left (dir -1) or right (dir 1), centered on cx,cy
void UI_drawChevron(Arduino_GFX &gfx, int16_t cx, int16_t cy, int16_t size, int8_t dir, uint16_t color);

// Top row: WiFi (signal bars / AP / offline), clock (once NTP has answered), battery level
void UI_drawStatusBar(Arduino_GFX &gfx, CDevice &device);

// Carousel position: count dots centered on cy, index highlighted
void UI_drawPageDots(Arduino_GFX &gfx, int16_t cy, uint8_t count, uint8_t index);

// Value and its unit on one baseline, centered on cx. A unit starting with '^' gets a degree ring
// for the '^' (the fonts have no degree sign), e.g. "^C".
void UI_drawValue(Arduino_GFX &gfx, int16_t cx, int16_t y, const char *value, const char *unit,
  const GFXfont *valueFont, const GFXfont *unitFont, uint16_t color);
uint16_t UI_unitWidth(Arduino_GFX &gfx, const char *unit, const GFXfont *font);

// Line graph of count values (value(0) oldest) in a panel, newest at the right edge. capacity is the
// full history length, so a partial history starts part way in. Scaled to the values' own range, at
// least minSpan tall; NAN values leave a gap. A zero line is drawn when the range crosses zero.
void UI_drawGraph(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t count, uint8_t capacity,
  std::function<float(uint8_t)> value, float minSpan, uint16_t color);

// Horizontal level bar, e.g. brightness
void UI_drawLevelBar(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t color);

// On-screen < / > button (dir -1 / 1) that responds to taps; lit while pressed
#define UI_ARROW_BUTTON_W 46
#define UI_ARROW_BUTTON_H 44
#define UI_TOUCH_MARGIN   10    // Taps this far outside a button still count
void UI_drawArrowButton(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h, int8_t dir, bool pressed);
bool UI_hit(int16_t px, int16_t py, int16_t x, int16_t y, int16_t w, int16_t h, int16_t margin = UI_TOUCH_MARGIN);

#endif
