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

void UI_drawStatusBar(Arduino_GFX &gfx, CDevice &device) {
  const int16_t baseline = 26;
  const int16_t side = 40;  // Rounded corners are ~43px in radius

  // Signal
  const network_info_t &net = device.getNetworkInfo();
  if (device.getState() == DeviceState::WIFI_CONNECTED) {
    UI_drawSignalBars(gfx, side, 13, 22, 14, CDevice::signalBars(net.rssi), UI_COLOR_TEXT);
  } else if (device.getState() == DeviceState::WIFI_AP_CREATED) {
    gfx.setFont(UI_FONT_LABEL);
    gfx.setTextColor(UI_COLOR_BLUE);
    gfx.setCursor(side, baseline);
    gfx.print("AP");
  }

  // Clock
  struct tm timeinfo;
  if (CONFIG_isTimeSynced() && getLocalTime(&timeinfo, 0)) {
    char buf[8];
    strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
    UI_drawCentered(gfx, buf, baseline, UI_FONT_LABEL, UI_COLOR_TEXT);
  }

  // Battery
  float v = device.getBatteryVoltage();
  if (v > 0.5f) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%.1fV", v);
    uint16_t w = UI_textWidth(gfx, buf, UI_FONT_SMALL);
    gfx.setTextColor(v < 3.5f ? UI_COLOR_ORANGE : UI_COLOR_MUTED);
    gfx.setCursor(LCD_WIDTH - side - w, baseline);
    gfx.print(buf);
  }

  gfx.drawFastHLine(SCREEN_SAFE_INSET, UI_STATUS_BAR_HEIGHT, LCD_WIDTH - 2 * SCREEN_SAFE_INSET, UI_COLOR_DIVIDER);
}

#endif
