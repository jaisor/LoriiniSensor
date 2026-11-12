#include "display/pages/PowerPage.h"

#if defined(SCREEN) && defined(SENSORS)

#include "Device.h"
#include "display/UI.h"

static const char NOT_CONNECTED[] = "Not connected";   // No bus voltage: the readings would be noise

void CPowerPage::drawIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy) {
  // 270 degree dial open at the bottom (0 degrees = 3 o'clock, clockwise)
  gfx.fillArc(cx, cy, 32, 25, 135, 359.9, UI_COLOR_ACCENT);
  gfx.fillArc(cx, cy, 32, 25, 0, 45, UI_COLOR_ACCENT);
  // Needle
  for (int8_t t = -1; t <= 1; t++) {
    gfx.drawLine(cx + t, cy, cx + 15 + t, cy - 15, UI_COLOR_TEXT);
  }
  gfx.fillCircle(cx, cy, 5, UI_COLOR_TEXT);
}

// Channel number in a disc, current, and voltage / power under it
void CPowerPage::drawChannelRow(Arduino_GFX &gfx, const CPowerSensor &s, uint8_t ch, int16_t x, int16_t y, int16_t w) {
  const power_reading_t &r = s.getReading(ch);
  char value[16], line[32];
  const char *unit, *unit2;

  char number[2] = { (char)('1' + ch), 0 };
  gfx.fillCircle(x + 30, y - 7, 11, UI_COLOR_DIVIDER);
  UI_drawCentered(gfx, number, y - 1, UI_FONT_LABEL, UI_COLOR_TEXT, x + 19, 23);

  const int16_t cx = x + w / 2 + 12;
  if (!r.connected) {
    UI_drawCentered(gfx, NOT_CONNECTED, y + 6, UI_FONT_LABEL, UI_COLOR_MUTED, cx - 70, 140);
    return;
  }
  SENSOR_formatCurrent(r.current, value, sizeof(value), &unit);
  UI_drawValue(gfx, cx, y, value, unit, UI_FONT_TITLE, UI_FONT_LABEL, r.overRange ? UI_COLOR_ORANGE : UI_COLOR_TEXT);

  char power[16];
  SENSOR_formatVoltage(r.voltage, value, sizeof(value), &unit);
  SENSOR_formatPower(r.power, power, sizeof(power), &unit2);
  snprintf(line, sizeof(line), "%s %s  %s %s", value, unit, power, unit2);
  UI_drawCentered(gfx, line, y + 20, UI_FONT_SMALL, UI_COLOR_MUTED, cx - 70, 140);
}

void CPowerPage::drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  const CPowerSensor *s = power();
  if (!s || !s->hasValues()) {
    drawStatusCard(gfx, x, y, w, h, drawIcon);
    return;
  }

  if (s->getChannelCount() > 1) {
    UI_drawCentered(gfx, getTitle(), y + 28, UI_FONT_LABEL, UI_COLOR_MUTED, x, w);
    for (uint8_t ch = 0; ch < s->getChannelCount(); ch++) {
      drawChannelRow(gfx, *s, ch, x, y + 66 + ch * 50, w);
    }
    return;
  }

  const power_reading_t &r = s->getReading(0);
  const int16_t cx = x + w / 2;
  char value[16];
  const char *unit;
  UI_drawCentered(gfx, getTitle(), y + 32, UI_FONT_LABEL, UI_COLOR_MUTED, x, w);
  if (!r.connected) {
    UI_drawCentered(gfx, NOT_CONNECTED, y + 104, UI_FONT_TITLE, UI_COLOR_MUTED, x, w);
    UI_drawCentered(gfx, "No voltage at VIN-", y + 140, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
    return;
  }
  SENSOR_formatCurrent(r.current, value, sizeof(value), &unit);
  UI_drawValue(gfx, cx, y + 92, value, unit, UI_FONT_LARGE, UI_FONT_TITLE, r.overRange ? UI_COLOR_ORANGE : UI_COLOR_TEXT);
  SENSOR_formatVoltage(r.voltage, value, sizeof(value), &unit);
  UI_drawValue(gfx, cx, y + 140, value, unit, UI_FONT_TITLE, UI_FONT_LABEL, UI_COLOR_ACCENT);
  SENSOR_formatPower(r.power, value, sizeof(value), &unit);
  UI_drawValue(gfx, cx, y + 176, value, unit, UI_FONT_LABEL, UI_FONT_SMALL, UI_COLOR_MUTED);
}

void CPowerPage::drawContent(Arduino_GFX &gfx) {
  const CPowerSensor *s = power();
  if (!s || !s->hasValues()) {
    drawStatusContent(gfx);
    return;
  }
  const uint8_t ch = min<uint8_t>(channel, s->getChannelCount() - 1);
  const power_reading_t &r = s->getReading(ch);
  char value[16], label[32];
  const char *unit;

  if (s->getChannelCount() > 1) {
    snprintf(label, sizeof(label), r.overRange ? "Channel %u - over range" : "Channel %u current", ch + 1);
  } else {
    strlcpy(label, r.overRange ? "Current - over range" : "Current", sizeof(label));
  }
  UI_drawCentered(gfx, label, UI_BODY_TOP + 24, UI_FONT_SMALL, r.overRange ? UI_COLOR_ORANGE : UI_COLOR_MUTED);
  if (r.connected) {
    SENSOR_formatCurrent(r.current, value, sizeof(value), &unit);
    UI_drawValue(gfx, LCD_WIDTH / 2, UI_BODY_TOP + 60, value, unit, UI_FONT_LARGE, UI_FONT_TITLE,
      r.overRange ? UI_COLOR_ORANGE : UI_COLOR_TEXT);
  } else {
    UI_drawCentered(gfx, NOT_CONNECTED, UI_BODY_TOP + 56, UI_FONT_TITLE, UI_COLOR_MUTED);
  }

  // Voltage and power side by side
  const int16_t left = LCD_WIDTH / 2 - 52, right = LCD_WIDTH / 2 + 52;
  UI_drawCentered(gfx, "Voltage", UI_BODY_TOP + 92, UI_FONT_SMALL, UI_COLOR_MUTED, left - 52, 104);
  UI_drawCentered(gfx, "Power", UI_BODY_TOP + 92, UI_FONT_SMALL, UI_COLOR_MUTED, right - 52, 104);
  SENSOR_formatVoltage(r.voltage, value, sizeof(value), &unit);
  UI_drawValue(gfx, left, UI_BODY_TOP + 118, value, unit, UI_FONT_TITLE, UI_FONT_LABEL, UI_COLOR_ACCENT);
  SENSOR_formatPower(r.power, value, sizeof(value), &unit);
  UI_drawValue(gfx, right, UI_BODY_TOP + 118, value, unit, UI_FONT_TITLE, UI_FONT_LABEL, UI_COLOR_TEXT);

  // Current graph, at least 1mA tall so noise stays flat
  UI_drawGraph(gfx, SCREEN_SAFE_INSET + 10, UI_BODY_TOP + 136, LCD_WIDTH - 2 * (SCREEN_SAFE_INSET + 10), 58,
    s->getHistoryCount(), SENSOR_HISTORY, [s, ch](uint8_t i) { return s->getHistory(ch, i); }, 0.001f, UI_COLOR_ACCENT);
  snprintf(label, sizeof(label), "Current, last %u s", (unsigned)(POWER_HISTORY_MS / 1000));
  UI_drawCentered(gfx, label, UI_BODY_TOP + 214, UI_FONT_SMALL, UI_COLOR_MUTED);
}

PageAction CPowerPage::onButton(ButtonId id, ButtonEvent event) {
  const CPowerSensor *s = power();
  const uint8_t channels = s ? s->getChannelCount() : 1;
  const int8_t dir = PAGE_step(id, event);
  if (dir && channels > 1) {
    channel = (channel + channels + dir) % channels;
    return PageAction::REDRAW;
  }
  if (PAGE_isUp(id, event) || PAGE_isCancel(id, event)) {
    return PageAction::EXIT;
  }
  return PageAction::NONE;
}

#endif
