#include "display/pages/EnvPage.h"

#if defined(SCREEN) && defined(SENSORS)

#include "Device.h"
#include "display/UI.h"

// Smallest graph height per value, so sensor noise stays flat
static float graphMinSpan(EnvValue v) {
  switch (v) {
    case EnvValue::TEMPERATURE: return 0.5f;    // C
    case EnvValue::HUMIDITY:    return 2.0f;    // %
    case EnvValue::PRESSURE:    return 0.5f;    // hPa
    case EnvValue::GAS:         return 2000.0f; // Ohm
    default:                    return 1.0f;
  }
}

static uint16_t valueColor(EnvValue v) {
  switch (v) {
    case EnvValue::TEMPERATURE: return UI_COLOR_ORANGE;
    case EnvValue::HUMIDITY:    return UI_COLOR_BLUE;
    case EnvValue::PRESSURE:    return UI_COLOR_ACCENT;
    default:                    return UI_COLOR_YELLOW;
  }
}

void CEnvPage::drawIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy) {
  // Bulb and stem, with the "mercury" inside
  gfx.fillRoundRect(cx - 8, cy - 34, 16, 50, 8, UI_COLOR_ORANGE);
  gfx.fillCircle(cx, cy + 18, 15, UI_COLOR_ORANGE);
  gfx.fillRoundRect(cx - 3, cy - 26, 6, 42, 3, UI_COLOR_PANEL);
  gfx.fillCircle(cx, cy + 18, 8, UI_COLOR_PANEL);
  gfx.fillRect(cx - 2, cy - 6, 4, 22, UI_COLOR_TEXT);
  gfx.fillCircle(cx, cy + 18, 6, UI_COLOR_TEXT);
  // Scale marks
  for (int8_t i = 0; i < 3; i++) {
    gfx.fillRect(cx + 12, cy - 24 + i * 12, 7, 2, UI_COLOR_MUTED);
  }
}

uint8_t CEnvPage::valuesOf(const CEnvSensor &s, EnvValue *out) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < ENV_VALUE_COUNT; i++) {
    if (s.has((EnvValue)i)) out[n++] = (EnvValue)i;
  }
  return n;
}

void CEnvPage::drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  const CEnvSensor *s = env();
  if (!s || !s->hasValues()) {
    drawStatusCard(gfx, x, y, w, h, drawIcon);
    return;
  }
  const int16_t cx = x + w / 2;
  char value[16];
  const char *unit;

  UI_drawCentered(gfx, getTitle(), y + 28, UI_FONT_LABEL, UI_COLOR_MUTED, x, w);
  SENSOR_formatEnv(EnvValue::TEMPERATURE, s->get(EnvValue::TEMPERATURE), value, sizeof(value), &unit);
  UI_drawValue(gfx, cx, y + 82, value, unit, UI_FONT_LARGE, UI_FONT_TITLE, UI_COLOR_TEXT);

  // The rest under it: the first a size larger
  EnvValue values[ENV_VALUE_COUNT];
  const uint8_t n = valuesOf(*s, values);
  int16_t row = y + 126;
  for (uint8_t i = 1; i < n; i++) {
    SENSOR_formatEnv(values[i], s->get(values[i]), value, sizeof(value), &unit);
    if (i == 1) {
      UI_drawValue(gfx, cx, row, value, unit, UI_FONT_TITLE, UI_FONT_LABEL, valueColor(values[i]));
      row += 34;
    } else {
      UI_drawValue(gfx, cx, row, value, unit, UI_FONT_LABEL, UI_FONT_SMALL, UI_COLOR_TEXT);
      row += 28;
    }
  }
}

void CEnvPage::drawContent(Arduino_GFX &gfx) {
  const CEnvSensor *s = env();
  if (!s || !s->hasValues()) {
    drawStatusContent(gfx);
    return;
  }
  EnvValue values[ENV_VALUE_COUNT];
  const uint8_t n = valuesOf(*s, values);
  const EnvValue v = values[min<uint8_t>(selected, n - 1)];
  char value[16], lo[16], hi[16], line[48];
  const char *unit;

  UI_drawCentered(gfx, SENSOR_envValueName(v), UI_BODY_TOP + 24, UI_FONT_SMALL, UI_COLOR_MUTED);
  SENSOR_formatEnv(v, s->get(v), value, sizeof(value), &unit);
  UI_drawValue(gfx, LCD_WIDTH / 2, UI_BODY_TOP + 62, value, unit, UI_FONT_LARGE, UI_FONT_TITLE, UI_COLOR_TEXT);

  // Range over the graph's span
  float vMin = NAN, vMax = NAN;
  const uint8_t count = s->getHistoryCount();
  for (uint8_t i = 0; i < count; i++) {
    float x = s->getHistory(v, i);
    if (isnan(x)) continue;
    vMin = isnan(vMin) ? x : min(vMin, x);
    vMax = isnan(vMax) ? x : max(vMax, x);
  }
  if (!isnan(vMin)) {
    const char *u;
    SENSOR_formatEnv(v, vMin, lo, sizeof(lo), &u);
    SENSOR_formatEnv(v, vMax, hi, sizeof(hi), &u);
    snprintf(line, sizeof(line), "min %s   max %s", lo, hi);
    UI_drawCentered(gfx, line, UI_BODY_TOP + 92, UI_FONT_SMALL, UI_COLOR_MUTED);
  }

  UI_drawGraph(gfx, SCREEN_SAFE_INSET + 10, UI_BODY_TOP + 104, LCD_WIDTH - 2 * (SCREEN_SAFE_INSET + 10), 80,
    count, SENSOR_HISTORY, [s, v](uint8_t i) { return s->getHistory(v, i); }, graphMinSpan(v), valueColor(v));
  snprintf(line, sizeof(line), "Last %lu min", ENV_HISTORY_SPAN_MS / 60000);
  UI_drawCentered(gfx, line, UI_BODY_TOP + 202, UI_FONT_SMALL, UI_COLOR_MUTED);
  if (n > 1) {
    UI_drawPageDots(gfx, UI_BODY_BOTTOM - 8, n, min<uint8_t>(selected, n - 1));
  }
}

PageAction CEnvPage::onButton(ButtonId id, ButtonEvent event) {
  const CEnvSensor *s = env();
  EnvValue values[ENV_VALUE_COUNT];
  const uint8_t n = s ? valuesOf(*s, values) : 1;
  const int8_t dir = PAGE_step(id, event);
  if (dir && n > 1) {
    selected = (selected + n + dir) % n;
    return PageAction::REDRAW;
  }
  if (PAGE_isUp(id, event) || PAGE_isCancel(id, event)) {
    return PageAction::EXIT;
  }
  return PageAction::NONE;
}

#endif
