#include "display/pages/MainPage.h"

#include "Configuration.h"
#ifdef SCREEN

#include "Device.h"
#include "display/UI.h"

// Dial icon centered on cx,cy
static void drawGaugeIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy, uint16_t color) {
  // 270 degree dial open at the bottom (0 degrees = 3 o'clock, clockwise)
  gfx.fillArc(cx, cy, 32, 25, 135, 359.9, color);
  gfx.fillArc(cx, cy, 32, 25, 0, 45, color);
  // Needle
  for (int8_t t = -1; t <= 1; t++) {
    gfx.drawLine(cx + t, cy, cx + 15 + t, cy - 15, UI_COLOR_TEXT);
  }
  gfx.fillCircle(cx, cy, 5, UI_COLOR_TEXT);
}

// Value and its unit on one baseline, centered on cx
static void drawValue(Arduino_GFX &gfx, int16_t cx, int16_t y, const char *value, const char *unit,
    const GFXfont *valueFont, const GFXfont *unitFont, uint16_t color) {
  const int16_t gap = 5;
  int16_t wv = UI_textWidth(gfx, value, valueFont);
  int16_t wu = UI_textWidth(gfx, unit, unitFont);
  int16_t x = cx - (wv + gap + wu) / 2;
  gfx.setTextColor(color);
  gfx.setFont(valueFont);
  gfx.setCursor(x, y);
  gfx.print(value);
  gfx.setTextColor(UI_COLOR_MUTED);
  gfx.setFont(unitFont);
  gfx.setCursor(x + wv + gap, y);
  gfx.print(unit);
}

bool CMainPage::hasReadings() const {
  #ifdef SENSORS
  const CSensors &s = device.getSensors();
  return s.getStatus() == SensorStatus::OK && s.getReadings().valid;
  #else
  return false;
  #endif
}

void CMainPage::statusText(const char **line1, const char **line2) const {
  #ifdef SENSORS
  const CSensors &s = device.getSensors();
  switch (s.getStatus()) {
    case SensorStatus::OFF:       *line1 = "No sensor selected"; *line2 = "Pick one in Settings"; return;
    case SensorStatus::NOT_FOUND: *line1 = "INA219 not found";   *line2 = "SDA 11, SCL 10";       return;
    default:                      *line1 = "Reading...";         *line2 = "";                     return;
  }
  #else
  *line1 = "No sensor support";
  *line2 = "";
  #endif
}

bool CMainPage::update() {
  #ifdef SENSORS
  uint32_t sample = device.getSensors().getReadings().sample;
  if (sample != drawnSample) {
    drawnSample = sample;
    return true;
  }
  #endif
  return false;
}

void CMainPage::drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  const int16_t cx = x + w / 2;
  #ifdef SENSORS
  if (hasReadings()) {
    const sensor_readings_t &r = device.getSensors().getReadings();
    char value[16];
    const char *unit;
    UI_drawCentered(gfx, getTitle(), y + 32, UI_FONT_LABEL, UI_COLOR_MUTED, x, w);
    SENSOR_formatCurrent(r.current, value, sizeof(value), &unit);
    drawValue(gfx, cx, y + 92, value, unit, UI_FONT_LARGE, UI_FONT_TITLE, r.overRange ? UI_COLOR_ORANGE : UI_COLOR_TEXT);
    SENSOR_formatVoltage(r.voltage, value, sizeof(value), &unit);
    drawValue(gfx, cx, y + 140, value, unit, UI_FONT_TITLE, UI_FONT_LABEL, UI_COLOR_ACCENT);
    SENSOR_formatPower(r.power, value, sizeof(value), &unit);
    drawValue(gfx, cx, y + 176, value, unit, UI_FONT_LABEL, UI_FONT_SMALL, UI_COLOR_MUTED);
    return;
  }
  #endif
  const char *line1, *line2;
  statusText(&line1, &line2);
  drawGaugeIcon(gfx, cx, y + 66, UI_COLOR_ACCENT);
  UI_drawCentered(gfx, getTitle(), y + 134, UI_FONT_TITLE, UI_COLOR_TEXT, x, w);
  UI_drawCentered(gfx, line1, y + 168, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
  UI_drawCentered(gfx, line2, y + 190, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
}

void CMainPage::drawContent(Arduino_GFX &gfx) {
  #ifdef SENSORS
  if (hasReadings()) {
    const sensor_readings_t &r = device.getSensors().getReadings();
    char value[16];
    const char *unit;

    UI_drawCentered(gfx, r.overRange ? "Current - over range" : "Current", UI_BODY_TOP + 24, UI_FONT_SMALL,
      r.overRange ? UI_COLOR_ORANGE : UI_COLOR_MUTED);
    SENSOR_formatCurrent(r.current, value, sizeof(value), &unit);
    drawValue(gfx, LCD_WIDTH / 2, UI_BODY_TOP + 60, value, unit, UI_FONT_LARGE, UI_FONT_TITLE,
      r.overRange ? UI_COLOR_ORANGE : UI_COLOR_TEXT);

    // Voltage and power side by side
    const int16_t left = LCD_WIDTH / 2 - 52, right = LCD_WIDTH / 2 + 52;
    UI_drawCentered(gfx, "Voltage", UI_BODY_TOP + 92, UI_FONT_SMALL, UI_COLOR_MUTED, left - 52, 104);
    UI_drawCentered(gfx, "Power", UI_BODY_TOP + 92, UI_FONT_SMALL, UI_COLOR_MUTED, right - 52, 104);
    SENSOR_formatVoltage(r.voltage, value, sizeof(value), &unit);
    drawValue(gfx, left, UI_BODY_TOP + 118, value, unit, UI_FONT_TITLE, UI_FONT_LABEL, UI_COLOR_ACCENT);
    SENSOR_formatPower(r.power, value, sizeof(value), &unit);
    drawValue(gfx, right, UI_BODY_TOP + 118, value, unit, UI_FONT_TITLE, UI_FONT_LABEL, UI_COLOR_TEXT);

    drawGraph(gfx, SCREEN_SAFE_INSET + 10, UI_BODY_TOP + 136, LCD_WIDTH - 2 * (SCREEN_SAFE_INSET + 10), 58);
    UI_drawCentered(gfx, "Current, last 20 s", UI_BODY_TOP + 214, UI_FONT_SMALL, UI_COLOR_MUTED);
    return;
  }
  #endif
  const char *line1, *line2;
  statusText(&line1, &line2);
  UI_drawCentered(gfx, "--", UI_BODY_TOP + 86, UI_FONT_LARGE, UI_COLOR_MUTED);
  UI_drawCentered(gfx, line1, UI_BODY_TOP + 126, UI_FONT_LABEL, UI_COLOR_TEXT);
  UI_drawCentered(gfx, line2, UI_BODY_TOP + 152, UI_FONT_SMALL, UI_COLOR_MUTED);
}

// Current history as a line, scaled to its own range (at least 1mA tall so noise stays flat)
void CMainPage::drawGraph(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  gfx.fillRoundRect(x, y, w, h, 8, UI_COLOR_PANEL);
  #ifdef SENSORS
  const CSensors &s = device.getSensors();
  const uint8_t n = s.getHistoryCount();
  if (n < 2) return;

  float lo = s.getHistory(0), hi = lo;
  for (uint8_t i = 1; i < n; i++) {
    float v = s.getHistory(i);
    lo = min(lo, v);
    hi = max(hi, v);
  }
  const float minSpan = 0.001f;
  if (hi - lo < minSpan) {
    float mid = (hi + lo) / 2;
    lo = mid - minSpan / 2;
    hi = mid + minSpan / 2;
  }

  const int16_t pad = 6;
  const int16_t gx = x + pad, gy = y + pad, gw = w - 2 * pad, gh = h - 2 * pad;
  // Zero line when it falls inside the range (current changing direction)
  if (lo < 0 && hi > 0) {
    int16_t zy = gy + gh - (int16_t)((0 - lo) / (hi - lo) * gh);
    gfx.drawFastHLine(gx, zy, gw, UI_COLOR_DIVIDER);
  }

  // Newest value at the right edge; a partial history starts part way in
  int16_t px = 0, py = 0;
  for (uint8_t i = 0; i < n; i++) {
    int16_t sx = gx + gw - (int16_t)((n - 1 - i) * (int32_t)gw / (SENSOR_HISTORY - 1));
    int16_t sy = gy + gh - (int16_t)((s.getHistory(i) - lo) / (hi - lo) * gh);
    if (i > 0) {
      gfx.drawLine(px, py, sx, sy, UI_COLOR_ACCENT);
      gfx.drawLine(px, py + 1, sx, sy + 1, UI_COLOR_ACCENT);
    }
    px = sx;
    py = sy;
  }
  #endif
}

PageAction CMainPage::onButton(ButtonId id, ButtonEvent event) {
  if (id == ButtonId::BOOT && (event == ButtonEvent::LONG_PRESS || event == ButtonEvent::CLICK)) {
    return PageAction::EXIT;
  }
  return PageAction::NONE;
}

#endif
