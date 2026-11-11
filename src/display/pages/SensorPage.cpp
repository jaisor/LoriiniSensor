#include "display/pages/SensorPage.h"

#if defined(SCREEN) && defined(SENSORS)

#include "Device.h"
#include "display/UI.h"

CSensorPage::CSensorPage(CDevice &device, const CSensor &sensor) : CPage(device), sensorId(sensor.getId()) {
  snprintf(title, sizeof(title), "%s 0x%02X", sensor.getName(), sensor.getAddress());
}

const CSensor* CSensorPage::sensor() const {
  return device.getSensors().find(sensorId);
}

bool CSensorPage::update() {
  const CSensor *s = sensor();
  const uint32_t sample = s ? s->getSample() : 0;
  if (sample != drawnSample) {
    drawnSample = sample;
    return true;
  }
  return false;
}

void CSensorPage::statusText(const char **line1, const char **line2) const {
  const CSensor *s = sensor();
  if (!s) {
    *line1 = "Disconnected";
    *line2 = "";
  } else if (!s->isOnline()) {
    *line1 = "Not responding";
    *line2 = "Retrying...";
  } else {
    *line1 = "Reading...";
    *line2 = "";
  }
}

void CSensorPage::drawStatusCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h,
    void (*icon)(Arduino_GFX &gfx, int16_t cx, int16_t cy)) {
  const char *line1, *line2;
  statusText(&line1, &line2);
  icon(gfx, x + w / 2, y + 66);
  UI_drawCentered(gfx, getTitle(), y + 134, UI_FONT_TITLE, UI_COLOR_TEXT, x, w);
  UI_drawCentered(gfx, line1, y + 168, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
  UI_drawCentered(gfx, line2, y + 190, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
}

void CSensorPage::drawStatusContent(Arduino_GFX &gfx) {
  const char *line1, *line2;
  statusText(&line1, &line2);
  UI_drawCentered(gfx, "--", UI_BODY_TOP + 86, UI_FONT_LARGE, UI_COLOR_MUTED);
  UI_drawCentered(gfx, line1, UI_BODY_TOP + 126, UI_FONT_LABEL, UI_COLOR_TEXT);
  UI_drawCentered(gfx, line2, UI_BODY_TOP + 152, UI_FONT_SMALL, UI_COLOR_MUTED);
}

#endif
