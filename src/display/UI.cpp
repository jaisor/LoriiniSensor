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
  const int16_t w = 24, h = 12;
  uint16_t color = percent <= 10 ? UI_COLOR_RED : percent <= 20 ? UI_COLOR_ORANGE : UI_COLOR_ACCENT;
  gfx.drawRoundRect(x, y, w, h, 3, UI_COLOR_MUTED);
  gfx.fillRect(x + w, y + 3, 2, h - 6, UI_COLOR_MUTED);
  int16_t fill = (w - 4) * min<uint8_t>(percent, 100) / 100;
  if (fill > 0) {
    gfx.fillRect(x + 2, y + 2, fill, h - 4, color);
  }
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
  const int16_t side = 40;  // Rounded corners are ~43px in radius

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

  // Battery
  if (device.getBatteryVoltage() > 0.5f) {
    uint8_t percent = device.getBatteryPercent();
    const int16_t iconX = LCD_WIDTH - side - 26;
    UI_drawBattery(gfx, iconX, 14, percent);
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", percent);
    uint16_t w = UI_textWidth(gfx, buf, UI_FONT_SMALL);
    gfx.setTextColor(UI_COLOR_MUTED);
    gfx.setCursor(iconX - 4 - w, baseline);
    gfx.print(buf);
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
