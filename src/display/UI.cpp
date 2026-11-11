#include "display/UI.h"

#ifdef SCREEN

#include <time.h>
#include "Device.h"

uint16_t UI_textWidth(Arduino_GFX &gfx, const char *text, const GFXfont *font) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx.setFont(font);
  gfx.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  return w;
}

void UI_drawCentered(Arduino_GFX &gfx, const char *text, int16_t y, const GFXfont *font, uint16_t color, int16_t x, int16_t w) {
  int16_t x1, y1;
  uint16_t tw, th;
  gfx.setFont(font);
  gfx.setTextSize(1);
  gfx.getTextBounds(text, 0, y, &x1, &y1, &tw, &th);
  gfx.setTextColor(color);
  gfx.setCursor(x + (w - (int16_t)tw) / 2 - x1, y);
  gfx.print(text);
}

void UI_drawSignalBars(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t bars, uint16_t color) {
  const int16_t gap = 2;
  const int16_t bw = (w - 3 * gap) / 4;
  for (uint8_t i = 0; i < 4; i++) {
    int16_t bh = h * (i + 1) / 4;
    int16_t bx = x + i * (bw + gap);
    gfx.fillRect(bx, y + h - bh, bw, bh, i < bars ? color : UI_COLOR_DIVIDER);
  }
}

void UI_drawBattery(Arduino_GFX &gfx, int16_t x, int16_t y, uint8_t percent) {
  const int16_t w = UI_BATTERY_W - 3, h = UI_BATTERY_H;  // 3px for the terminal nub
  percent = min<uint8_t>(percent, 100);
  uint16_t color = percent <= 10 ? UI_COLOR_RED : percent <= 20 ? UI_COLOR_ORANGE : UI_COLOR_ACCENT;
  gfx.drawRoundRect(x, y, w, h, 4, UI_COLOR_MUTED);
  gfx.fillRoundRect(x + w, y + 4, 3, h - 8, 1, UI_COLOR_MUTED);
  int16_t fill = (w - 4) * percent / 100;
  if (fill > 0) {
    gfx.fillRoundRect(x + 2, y + 2, fill, h - 4, 2, color);
  }

  // Percentage inside, in the built-in 5x7 font (the GFX fonts are too big for it). Each glyph takes a 6px
  // cell with its last column blank; a dark shadow keeps it readable over the fill and the empty part.
  char text[6];
  snprintf(text, sizeof(text), "%u%%", percent);
  const int16_t tw = strlen(text) * 6 - 1, th = 7;
  const int16_t tx = x + (w - tw) / 2, ty = y + (h - th) / 2;
  gfx.setFont(nullptr);
  gfx.setTextSize(1);
  gfx.setTextColor(UI_COLOR_BG);
  gfx.setCursor(tx + 1, ty + 1);
  gfx.print(text);
  gfx.setTextColor(UI_COLOR_TEXT);
  gfx.setCursor(tx, ty);
  gfx.print(text);
}

void UI_drawChevron(Arduino_GFX &gfx, int16_t cx, int16_t cy, int16_t size, int8_t dir, uint16_t color) {
  int16_t tip = cx + dir * size / 2;
  int16_t back = cx - dir * size / 2;
  // Two strokes, 2px thick
  for (int8_t t = 0; t < 2; t++) {
    gfx.drawLine(back + dir * t, cy - size, tip + dir * t, cy, color);
    gfx.drawLine(tip + dir * t, cy, back + dir * t, cy + size, color);
  }
}

void UI_drawStatusBar(Arduino_GFX &gfx, CDevice &device) {
  const int16_t baseline = 26;
  // The ~43px corner radius only cuts ~13px in at the top of this row (y=12), so 24px clears it
  const int16_t side = 24;

  // WiFi
  const network_info_t &net = device.getNetworkInfo();
  switch (device.getState()) {
    case DeviceState::WIFI_CONNECTED:
      UI_drawSignalBars(gfx, side, 13, 22, 14, CDevice::signalBars(net.rssi), UI_COLOR_TEXT);
      break;
    case DeviceState::WIFI_AP_CREATED:
      gfx.setFont(UI_FONT_LABEL);
      gfx.setTextColor(UI_COLOR_BLUE);
      gfx.setCursor(side, baseline);
      gfx.print("AP");
      break;
    case DeviceState::WIFI_CONNECTING:
      UI_drawSignalBars(gfx, side, 13, 22, 14, 0, UI_COLOR_TEXT);
      gfx.fillCircle(side + 26, 24, 2, UI_COLOR_YELLOW);
      break;
    case DeviceState::WIFI_OFFLINE:
      UI_drawSignalBars(gfx, side, 13, 22, 14, 0, UI_COLOR_TEXT);
      gfx.drawLine(side + 3, 13, side + 19, 27, UI_COLOR_RED);
      gfx.drawLine(side + 4, 13, side + 20, 27, UI_COLOR_RED);
      gfx.drawLine(side + 19, 13, side + 3, 27, UI_COLOR_RED);
      gfx.drawLine(side + 20, 13, side + 4, 27, UI_COLOR_RED);
      break;
    default:
      break;
  }

  // Clock
  struct tm timeinfo;
  if (CONFIG_isTimeSynced() && getLocalTime(&timeinfo, 0)) {
    char buf[8];
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    UI_drawCentered(gfx, buf, baseline, UI_FONT_LABEL, UI_COLOR_TEXT);
  }

  // Battery, level in the icon
  if (device.getBatteryVoltage() > 0.5f) {
    UI_drawBattery(gfx, LCD_WIDTH - side - UI_BATTERY_W, 12, device.getBatteryPercent());
  }

  gfx.drawFastHLine(SCREEN_SAFE_INSET, UI_STATUS_BAR_HEIGHT, LCD_WIDTH - 2 * SCREEN_SAFE_INSET, UI_COLOR_DIVIDER);
}

void UI_drawPageDots(Arduino_GFX &gfx, int16_t cy, uint8_t count, uint8_t index) {
  const int16_t gap = 14;
  int16_t x = LCD_WIDTH / 2 - (count - 1) * gap / 2;
  for (uint8_t i = 0; i < count; i++, x += gap) {
    if (i == index) {
      gfx.fillCircle(x, cy, 4, UI_COLOR_TEXT);
    } else {
      gfx.fillCircle(x, cy, 3, UI_COLOR_DIVIDER);
    }
  }
}

uint16_t UI_unitWidth(Arduino_GFX &gfx, const char *unit, const GFXfont *font) {
  if (unit[0] != '^') {
    return UI_textWidth(gfx, unit, font);
  }
  const int16_t r = font == UI_FONT_LARGE ? 5 : font == UI_FONT_TITLE ? 4 : 3;
  return 2 * r + 2 + (unit[1] ? UI_textWidth(gfx, unit + 1, font) : 0);
}

void UI_drawValue(Arduino_GFX &gfx, int16_t cx, int16_t y, const char *value, const char *unit,
    const GFXfont *valueFont, const GFXfont *unitFont, uint16_t color) {
  const int16_t gap = 5;
  const int16_t wv = UI_textWidth(gfx, value, valueFont);
  const int16_t wu = UI_unitWidth(gfx, unit, unitFont);
  int16_t x = cx - (wv + gap + wu) / 2;
  gfx.setTextColor(color);
  gfx.setFont(valueFont);
  gfx.setCursor(x, y);
  gfx.print(value);
  x += wv + gap;

  if (unit[0] == '^') {
    // Ring at the top of the unit's capitals, like a degree sign
    int16_t x1, y1;
    uint16_t w, h;
    gfx.setFont(unitFont);
    gfx.getTextBounds("C", 0, y, &x1, &y1, &w, &h);
    const int16_t r = unitFont == UI_FONT_LARGE ? 5 : unitFont == UI_FONT_TITLE ? 4 : 3;
    gfx.drawCircle(x + r, y1 + r, r, UI_COLOR_MUTED);
    gfx.drawCircle(x + r, y1 + r, r - 1, UI_COLOR_MUTED);
    x += 2 * r + 2;
    unit++;
  }
  gfx.setTextColor(UI_COLOR_MUTED);
  gfx.setFont(unitFont);
  gfx.setCursor(x, y);
  gfx.print(unit);
}

void UI_drawGraph(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t count, uint8_t capacity,
    std::function<float(uint8_t)> value, float minSpan, uint16_t color) {
  gfx.fillRoundRect(x, y, w, h, 8, UI_COLOR_PANEL);
  if (count < 2 || capacity < 2) return;

  float lo = NAN, hi = NAN;
  for (uint8_t i = 0; i < count; i++) {
    float v = value(i);
    if (isnan(v)) continue;
    lo = isnan(lo) ? v : min(lo, v);
    hi = isnan(hi) ? v : max(hi, v);
  }
  if (isnan(lo)) return;
  if (hi - lo < minSpan) {
    float mid = (hi + lo) / 2;
    lo = mid - minSpan / 2;
    hi = mid + minSpan / 2;
  }

  const int16_t pad = 6;
  const int16_t gx = x + pad, gy = y + pad, gw = w - 2 * pad, gh = h - 2 * pad;
  if (lo < 0 && hi > 0) {
    int16_t zy = gy + gh - (int16_t)((0 - lo) / (hi - lo) * gh);
    gfx.drawFastHLine(gx, zy, gw, UI_COLOR_DIVIDER);
  }

  bool havePrev = false;
  int16_t px = 0, py = 0;
  for (uint8_t i = 0; i < count; i++) {
    float v = value(i);
    if (isnan(v)) {
      havePrev = false;
      continue;
    }
    int16_t sx = gx + gw - (int16_t)((count - 1 - i) * (int32_t)gw / (capacity - 1));
    int16_t sy = gy + gh - (int16_t)((v - lo) / (hi - lo) * gh);
    if (havePrev) {
      gfx.drawLine(px, py, sx, sy, color);
      gfx.drawLine(px, py + 1, sx, sy + 1, color);
    } else {
      gfx.fillRect(sx, sy, 2, 2, color);  // A lone point stays visible
    }
    havePrev = true;
    px = sx;
    py = sy;
  }
}

void UI_drawLevelBar(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t percent, uint16_t color) {
  gfx.fillRoundRect(x, y, w, h, h / 2, UI_COLOR_DIVIDER);
  int16_t fill = w * min<uint8_t>(percent, 100) / 100;
  if (fill >= h) {
    gfx.fillRoundRect(x, y, fill, h, h / 2, color);
  } else if (fill > 0) {
    gfx.fillCircle(x + h / 2, y + h / 2, h / 2, color);
  }
}

#endif
