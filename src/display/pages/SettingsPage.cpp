#include "display/pages/SettingsPage.h"

#include "Configuration.h"
#ifdef SCREEN

#include <version.h>
#include "Device.h"
#include "display/UI.h"

#define LEVEL_STEP 10

const char* const CSettingsPage::ITEM_LABELS[ITEM_COUNT] = { "WiFi", "Sensors", "Brightness", "Screen saver", "Sound", "About", "Back" };

// Gear icon centered on cx,cy; hole is drawn in the card's color
static void drawGearIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy, uint16_t color, uint16_t holeColor) {
  for (uint8_t i = 0; i < 8; i++) {
    float a = i * PI / 4;
    gfx.fillCircle(cx + cosf(a) * 24, cy + sinf(a) * 24, 6, color);
  }
  gfx.fillCircle(cx, cy, 22, color);
  gfx.fillCircle(cx, cy, 9, holeColor);
}

// Label above a value, both centered
static void drawField(Arduino_GFX &gfx, int16_t y, const char *label, const char *value, uint16_t valueColor = UI_COLOR_TEXT) {
  UI_drawCentered(gfx, label, y, UI_FONT_SMALL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, value, y + 20, UI_FONT_LABEL, valueColor);
}

// Next / previous step of a 0-100 level, landing on multiples of LEVEL_STEP
static uint8_t stepLevel(uint8_t value, int8_t dir, uint8_t minValue) {
  int v = dir > 0 ? (value / LEVEL_STEP + 1) * LEVEL_STEP : ((value + LEVEL_STEP - 1) / LEVEL_STEP - 1) * LEVEL_STEP;
  return constrain(v, minValue, 100);
}

// Index of the screen saver preset at or below a duration (one set on the web may be in between)
static uint8_t saverPresetIndex(uint16_t sec) {
  uint8_t i = 0;
  while (i + 1 < SCREEN_SAVER_PRESET_COUNT && SCREEN_SAVER_PRESETS[i + 1] <= sec) i++;
  return i;
}

// The next longer / shorter preset, stopping at the ends
static uint16_t stepScreenSaver(uint16_t sec, int8_t dir) {
  uint8_t i = saverPresetIndex(sec);
  if (dir > 0) {
    return SCREEN_SAVER_PRESETS[min<uint8_t>(i + 1, SCREEN_SAVER_PRESET_COUNT - 1)];
  }
  if (SCREEN_SAVER_PRESETS[i] < sec) return SCREEN_SAVER_PRESETS[i];   // Down to the preset below
  return SCREEN_SAVER_PRESETS[i > 0 ? i - 1 : 0];
}

void CSettingsPage::drawCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h) {
  drawGearIcon(gfx, x + w / 2, y + 66, UI_COLOR_BLUE, UI_COLOR_PANEL);
  UI_drawCentered(gfx, getTitle(), y + 134, UI_FONT_TITLE, UI_COLOR_TEXT, x, w);

  const network_info_t &net = device.getNetworkInfo();
  const char *status;
  uint16_t color = UI_COLOR_MUTED;
  switch (device.getState()) {
    case DeviceState::WIFI_CONNECTED:  status = net.ip; color = UI_COLOR_TEXT; break;
    case DeviceState::WIFI_AP_CREATED: status = "Setup mode"; color = UI_COLOR_BLUE; break;
    case DeviceState::WIFI_OFFLINE:    status = "WiFi offline"; color = UI_COLOR_ORANGE; break;
    default:                           status = "Connecting..."; color = UI_COLOR_YELLOW; break;
  }
  UI_drawCentered(gfx, status, y + 168, UI_FONT_LABEL, color, x, w);
  UI_drawCentered(gfx, net.ssid, y + 192, UI_FONT_SMALL, UI_COLOR_MUTED, x, w);
}

void CSettingsPage::onOpen() {
  view = View::MENU;
  selected = 0;
  levelChanged = false;
}

void CSettingsPage::onClose() {
  leaveLevelView();
}

bool CSettingsPage::canForgetWifi() const {
  #ifdef WIFI
  return strlen(configuration.wifiSsid) > 0;
  #else
  return false;
  #endif
}

void CSettingsPage::leaveLevelView() {
  if (levelChanged) {
    levelChanged = false;
    EEPROM_saveConfig();
  }
}

PageAction CSettingsPage::onButton(ButtonId id, ButtonEvent event) {
  const bool next = id == ButtonId::POWER && event == ButtonEvent::CLICK;
  const bool prev = id == ButtonId::POWER && event == ButtonEvent::DOUBLE_CLICK;
  const bool select = id == ButtonId::BOOT && event == ButtonEvent::CLICK;
  const bool back = id == ButtonId::BOOT && event == ButtonEvent::LONG_PRESS;

  switch (view) {

    case View::MENU:
      if (next) { selected = (selected + 1) % ITEM_COUNT; return PageAction::REDRAW; }
      if (prev) { selected = (selected + ITEM_COUNT - 1) % ITEM_COUNT; return PageAction::REDRAW; }
      if (back) return PageAction::EXIT;
      if (select) {
        switch (selected) {
          case ITEM_WIFI:       view = View::NETWORK; break;
          case ITEM_SENSOR:     view = View::SENSOR; break;
          case ITEM_BRIGHTNESS: view = View::BRIGHTNESS; break;
          case ITEM_SCREEN_SAVER: view = View::SCREEN_SAVER; break;
          case ITEM_VOLUME:     view = View::VOLUME; break;
          case ITEM_ABOUT:      view = View::ABOUT; break;
          default:              return PageAction::EXIT;  // Back
        }
        return PageAction::REDRAW;
      }
      break;

    case View::NETWORK:
      if (back) { view = View::MENU; return PageAction::REDRAW; }
      if (select && canForgetWifi()) { view = View::FORGET_WIFI; return PageAction::REDRAW; }
      break;

    case View::FORGET_WIFI:
      if (select) {
        #ifdef WIFI
        device.forgetWifi();  // Saves and restarts in setup mode
        #endif
        view = View::NETWORK;
        return PageAction::REDRAW;
      }
      if (back || next || prev) { view = View::NETWORK; return PageAction::REDRAW; }
      break;

    case View::BRIGHTNESS:
    case View::SCREEN_SAVER:
    case View::VOLUME: {
      if (select || back) {
        leaveLevelView();
        view = View::MENU;
        return PageAction::REDRAW;
      }
      if (next || prev) {
        int8_t dir = next ? 1 : -1;
        if (view == View::BRIGHTNESS) {
          #ifdef SCREEN
          configuration.screenBrightness = stepLevel(configuration.screenBrightness, dir, SCREEN_MIN_BRIGHTNESS);  // Applied by the display loop
          #endif
        } else if (view == View::SCREEN_SAVER) {
          configuration.screenSaverSec = stepScreenSaver(configuration.screenSaverSec, dir);  // Counts from now
        } else {
          configuration.buzzerVolume = stepLevel(configuration.buzzerVolume, dir, 0);  // The button sound plays at the new level
        }
        levelChanged = true;
        return PageAction::REDRAW;
      }
    } break;

    case View::SENSOR:
      if (select || back) { view = View::MENU; return PageAction::REDRAW; }
      break;

    default:  // ABOUT
      if (select || back) { view = View::MENU; return PageAction::REDRAW; }
      break;
  }
  return PageAction::NONE;
}

void CSettingsPage::drawContent(Arduino_GFX &gfx) {
  switch (view) {
    case View::MENU:        drawMenu(gfx); break;
    case View::NETWORK:        drawWifi(gfx); break;
    case View::FORGET_WIFI: drawForgetWifi(gfx); break;
    case View::SENSOR:      drawSensors(gfx); break;
    case View::BRIGHTNESS:  drawLevel(gfx, "Screen brightness", configuration.screenBrightness, nullptr); break;
    case View::SCREEN_SAVER: drawScreenSaver(gfx); break;
    case View::VOLUME:      drawLevel(gfx, "Button sounds", configuration.buzzerVolume, "Off"); break;
    case View::ABOUT:       drawAbout(gfx); break;
  }
}

void CSettingsPage::drawMenu(Arduino_GFX &gfx) {
  const int16_t rowH = 30;   // ITEM_COUNT rows end above UI_BODY_BOTTOM
  const int16_t left = SCREEN_SAFE_INSET + 6;
  const int16_t width = LCD_WIDTH - 2 * left;
  for (uint8_t i = 0; i < ITEM_COUNT; i++) {
    int16_t y = UI_BODY_TOP + 4 + i * rowH;
    bool sel = i == selected;
    if (sel) {
      gfx.fillRoundRect(left, y, width, rowH - 2, 8, UI_COLOR_PANEL);
      gfx.fillRoundRect(left, y + 5, 3, rowH - 12, 1, UI_COLOR_ACCENT);
    }
    gfx.setFont(UI_FONT_LABEL);
    gfx.setTextColor(sel ? UI_COLOR_TEXT : UI_COLOR_MUTED);
    gfx.setCursor(left + 14, y + 20);
    gfx.print(ITEM_LABELS[i]);

    // Current value or a chevron on the right
    char value[8] = "";
    if (i == ITEM_BRIGHTNESS) {
      snprintf(value, sizeof(value), "%u%%", configuration.screenBrightness);
    } else if (i == ITEM_SENSOR) {
      #ifdef SENSORS
      snprintf(value, sizeof(value), "%u", device.getSensors().getCount());
      #endif
    } else if (i == ITEM_SCREEN_SAVER) {
      CONFIG_formatDuration(configuration.screenSaverSec, value, sizeof(value));
    } else if (i == ITEM_VOLUME) {
      if (configuration.buzzerVolume) snprintf(value, sizeof(value), "%u%%", configuration.buzzerVolume);
      else strcpy(value, "Off");
    }
    if (value[0]) {
      uint16_t w = UI_textWidth(gfx, value, UI_FONT_SMALL);
      gfx.setTextColor(UI_COLOR_MUTED);
      gfx.setCursor(left + width - 12 - w, y + 20);
      gfx.print(value);
    } else if (i != ITEM_BACK) {
      UI_drawChevron(gfx, left + width - 16, y + rowH / 2 - 1, 5, 1, sel ? UI_COLOR_TEXT : UI_COLOR_DIVIDER);
    }
  }
}

void CSettingsPage::drawWifi(Arduino_GFX &gfx) {
  const network_info_t &net = device.getNetworkInfo();
  char line[48];
  switch (device.getState()) {
    case DeviceState::WIFI_CONNECTED:
      drawField(gfx, UI_BODY_TOP + 26, "Network", net.ssid);
      drawField(gfx, UI_BODY_TOP + 82, "Web server", net.ip, UI_COLOR_ACCENT);
      snprintf(line, sizeof(line), "%d dBm", net.rssi);
      drawField(gfx, UI_BODY_TOP + 138, "Signal", line);
      break;
    case DeviceState::WIFI_AP_CREATED:
      drawField(gfx, UI_BODY_TOP + 26, "Setup network", net.ssid, UI_COLOR_BLUE);
      drawField(gfx, UI_BODY_TOP + 82, "Password", net.password);
      snprintf(line, sizeof(line), "http://%s", net.ip);
      drawField(gfx, UI_BODY_TOP + 138, "Then open", line, UI_COLOR_YELLOW);
      break;
    default:
      drawField(gfx, UI_BODY_TOP + 26, device.getState() == DeviceState::WIFI_OFFLINE ? "Reconnecting to" : "Connecting to",
        net.ssid, UI_COLOR_YELLOW);
      UI_drawCentered(gfx, "Configure WiFi from the", UI_BODY_TOP + 100, UI_FONT_SMALL, UI_COLOR_MUTED);
      UI_drawCentered(gfx, "web page in setup mode", UI_BODY_TOP + 120, UI_FONT_SMALL, UI_COLOR_MUTED);
      break;
  }
}

void CSettingsPage::drawForgetWifi(Arduino_GFX &gfx) {
  #ifdef WIFI
  UI_drawCentered(gfx, "Forget network?", UI_BODY_TOP + 40, UI_FONT_TITLE, UI_COLOR_TEXT);
  UI_drawCentered(gfx, configuration.wifiSsid, UI_BODY_TOP + 70, UI_FONT_LABEL, UI_COLOR_ORANGE);
  #endif
  UI_drawCentered(gfx, "The device restarts in", UI_BODY_TOP + 112, UI_FONT_SMALL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, "setup mode, with its own", UI_BODY_TOP + 132, UI_FONT_SMALL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, "WiFi to set up a new one", UI_BODY_TOP + 152, UI_FONT_SMALL, UI_COLOR_MUTED);
}

void CSettingsPage::drawSensors(Arduino_GFX &gfx) {
  #ifdef SENSORS
  const CSensors &sensors = device.getSensors();
  const uint8_t n = sensors.getCount();
  if (n == 0) {
    UI_drawCentered(gfx, "No sensors found", UI_BODY_TOP + 60, UI_FONT_LABEL, UI_COLOR_TEXT);
    UI_drawCentered(gfx, "SDA 11, SCL 10, 3.3V", UI_BODY_TOP + 90, UI_FONT_SMALL, UI_COLOR_MUTED);
    UI_drawCentered(gfx, "Looked for every 5 s", UI_BODY_TOP + 112, UI_FONT_SMALL, UI_COLOR_MUTED);
    return;
  }
  // One row each: name, address, and whether it answers (SENSOR_MAX rows fit)
  const int16_t left = 30, right = LCD_WIDTH - 30;
  const int16_t rowH = 24;
  int16_t y = UI_BODY_TOP + 26;
  UI_drawCentered(gfx, "Found on I2C", y, UI_FONT_SMALL, UI_COLOR_MUTED);
  for (uint8_t i = 0; i < n; i++) {
    const CSensor *s = sensors.get(i);
    char address[8];
    y += rowH;
    gfx.setFont(UI_FONT_LABEL);
    gfx.setTextColor(UI_COLOR_TEXT);
    gfx.setCursor(left, y);
    gfx.print(s->getName());
    snprintf(address, sizeof(address), "0x%02X", s->getAddress());
    gfx.setFont(UI_FONT_SMALL);
    gfx.setTextColor(UI_COLOR_MUTED);
    gfx.setCursor(left + 88, y);
    gfx.print(address);
    const bool ok = s->isOnline();
    const char *status = ok ? "OK" : "Lost";
    gfx.setFont(UI_FONT_LABEL);
    gfx.setTextColor(ok ? UI_COLOR_ACCENT : UI_COLOR_ORANGE);
    gfx.setCursor(right - UI_textWidth(gfx, status, UI_FONT_LABEL), y);
    gfx.print(status);
  }
  #else
  UI_drawCentered(gfx, "No sensor support", UI_BODY_TOP + 60, UI_FONT_LABEL, UI_COLOR_MUTED);
  #endif
}

void CSettingsPage::drawLevel(Arduino_GFX &gfx, const char *label, uint8_t value, const char *offText) {
  char text[8];
  if (value == 0 && offText) {
    strlcpy(text, offText, sizeof(text));
  } else {
    snprintf(text, sizeof(text), "%u%%", value);
  }
  UI_drawCentered(gfx, label, UI_BODY_TOP + 50, UI_FONT_LABEL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, text, UI_BODY_TOP + 104, UI_FONT_LARGE, value ? UI_COLOR_TEXT : UI_COLOR_MUTED);
  const int16_t w = 160;
  const int16_t barY = UI_BODY_TOP + 130;
  UI_drawLevelBar(gfx, (LCD_WIDTH - w) / 2, barY, w, 12, value, UI_COLOR_ACCENT);
  UI_drawChevron(gfx, (LCD_WIDTH - w) / 2 - 16, barY + 6, 5, -1, UI_COLOR_MUTED);
  UI_drawChevron(gfx, (LCD_WIDTH + w) / 2 + 16, barY + 6, 5, 1, UI_COLOR_MUTED);
}

void CSettingsPage::drawScreenSaver(Arduino_GFX &gfx) {
  char text[12];
  const uint16_t sec = configuration.screenSaverSec;
  CONFIG_formatDuration(sec, text, sizeof(text));
  UI_drawCentered(gfx, "Screen saver", UI_BODY_TOP + 50, UI_FONT_LABEL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, text, UI_BODY_TOP + 104, UI_FONT_LARGE, sec ? UI_COLOR_TEXT : UI_COLOR_MUTED);
  const int16_t w = 160;
  const int16_t barY = UI_BODY_TOP + 130;
  const uint8_t level = saverPresetIndex(sec) * 100 / (SCREEN_SAVER_PRESET_COUNT - 1);
  UI_drawLevelBar(gfx, (LCD_WIDTH - w) / 2, barY, w, 12, level, UI_COLOR_ACCENT);
  UI_drawChevron(gfx, (LCD_WIDTH - w) / 2 - 16, barY + 6, 5, -1, UI_COLOR_MUTED);
  UI_drawChevron(gfx, (LCD_WIDTH + w) / 2 + 16, barY + 6, 5, 1, UI_COLOR_MUTED);
  if (sec) {
    UI_drawCentered(gfx, "Idle that long: dark,", UI_BODY_TOP + 172, UI_FONT_SMALL, UI_COLOR_MUTED);
    UI_drawCentered(gfx, "half as long: dimmed", UI_BODY_TOP + 192, UI_FONT_SMALL, UI_COLOR_MUTED);
  } else {
    UI_drawCentered(gfx, "The screen stays on", UI_BODY_TOP + 172, UI_FONT_SMALL, UI_COLOR_MUTED);
  }
}

void CSettingsPage::drawAbout(Arduino_GFX &gfx) {
  char line[48];
  drawField(gfx, UI_BODY_TOP + 26, "Name", configuration.name);
  drawField(gfx, UI_BODY_TOP + 78, "Version", VERSION_SHORT);
  unsigned long sec = CONFIG_getUpTime() / 1000;
  snprintf(line, sizeof(line), "%lud %02lu:%02lu", sec / 86400, (sec / 3600) % 24, (sec / 60) % 60);
  drawField(gfx, UI_BODY_TOP + 130, "Uptime", line);
  snprintf(line, sizeof(line), "%u KB / %u KB", (unsigned)(ESP.getFreeHeap() / 1024), (unsigned)(ESP.getFreePsram() / 1024));
  drawField(gfx, UI_BODY_TOP + 182, "Free heap / PSRAM", line);
}

#endif
