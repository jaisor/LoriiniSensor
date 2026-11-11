#include "display/pages/SettingsPage.h"

#include "Configuration.h"
#ifdef SCREEN

#include <version.h>
#include "Device.h"
#include "display/UI.h"

#define LEVEL_STEP 10

const char* const CSettingsPage::ITEM_LABELS[ITEM_COUNT] = { "WiFi", "Sensor", "Brightness", "Sound", "About", "Back" };

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
        } else {
          configuration.buzzerVolume = stepLevel(configuration.buzzerVolume, dir, 0);  // The button sound plays at the new level
        }
        levelChanged = true;
        return PageAction::REDRAW;
      }
    } break;

    default:  // SENSOR, ABOUT
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
    case View::SENSOR:      drawSensor(gfx); break;
    case View::BRIGHTNESS:  drawLevel(gfx, "Screen brightness", configuration.screenBrightness, nullptr); break;
    case View::VOLUME:      drawLevel(gfx, "Button sounds", configuration.buzzerVolume, "Off"); break;
    case View::ABOUT:       drawAbout(gfx); break;
  }
}

void CSettingsPage::drawMenu(Arduino_GFX &gfx) {
  const int16_t rowH = 34;
  const int16_t left = SCREEN_SAFE_INSET + 6;
  const int16_t width = LCD_WIDTH - 2 * left;
  for (uint8_t i = 0; i < ITEM_COUNT; i++) {
    int16_t y = UI_BODY_TOP + 6 + i * rowH;
    bool sel = i == selected;
    if (sel) {
      gfx.fillRoundRect(left, y, width, rowH - 2, 8, UI_COLOR_PANEL);
      gfx.fillRoundRect(left, y + 5, 3, rowH - 12, 1, UI_COLOR_ACCENT);
    }
    gfx.setFont(UI_FONT_LABEL);
    gfx.setTextColor(sel ? UI_COLOR_TEXT : UI_COLOR_MUTED);
    gfx.setCursor(left + 14, y + 22);
    gfx.print(ITEM_LABELS[i]);

    // Current value or a chevron on the right
    char value[8] = "";
    if (i == ITEM_BRIGHTNESS) {
      snprintf(value, sizeof(value), "%u%%", configuration.screenBrightness);
    } else if (i == ITEM_VOLUME) {
      if (configuration.buzzerVolume) snprintf(value, sizeof(value), "%u%%", configuration.buzzerVolume);
      else strcpy(value, "Off");
    }
    if (value[0]) {
      uint16_t w = UI_textWidth(gfx, value, UI_FONT_SMALL);
      gfx.setTextColor(UI_COLOR_MUTED);
      gfx.setCursor(left + width - 12 - w, y + 22);
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

void CSettingsPage::drawSensor(Arduino_GFX &gfx) {
  // TODO: list the supported sensors to choose from, and the chosen one's settings (interval, calibration)
  UI_drawCentered(gfx, "Sensor", UI_BODY_TOP + 60, UI_FONT_TITLE, UI_COLOR_TEXT);
  UI_drawCentered(gfx, "No sensors yet", UI_BODY_TOP + 100, UI_FONT_LABEL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, "Sensor selection and", UI_BODY_TOP + 140, UI_FONT_SMALL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, "configuration will be", UI_BODY_TOP + 160, UI_FONT_SMALL, UI_COLOR_MUTED);
  UI_drawCentered(gfx, "here", UI_BODY_TOP + 180, UI_FONT_SMALL, UI_COLOR_MUTED);
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
