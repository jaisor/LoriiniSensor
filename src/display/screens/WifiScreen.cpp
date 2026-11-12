#include "display/screens/WifiScreen.h"

#include "Configuration.h"
#ifdef SCREEN

#include "Device.h"
#include "display/UI.h"
#include "display/Display.h"

#define ANIMATION_FRAME_MS 400

void CWifiScreen::onEnter() {
  CScreen::onEnter();
  animationFrame = 0;
  tsAnimation = millis();
}

bool CWifiScreen::isAnimating() const {
  DeviceState s = device.getState();
  return s == DeviceState::WIFI_CONNECTING || s == DeviceState::WIFI_OFFLINE;
}

bool CWifiScreen::update() {
  bool redraw = CScreen::update();
  if (isAnimating() && millis() - tsAnimation >= ANIMATION_FRAME_MS) {
    tsAnimation = millis();
    animationFrame = (animationFrame + 1) % 4;
    redraw = true;
  }
  return redraw;
}

// Three arcs over a dot, `lit` of them (0-3) in color, the rest dimmed
void CWifiScreen::drawWifiIcon(Arduino_GFX &gfx, int16_t cx, int16_t cy, uint8_t lit, uint16_t color) {
  gfx.fillCircle(cx, cy, 6, color);
  for (uint8_t i = 0; i < 3; i++) {
    int16_t r1 = 18 + i * 16;
    gfx.fillArc(cx, cy, r1 + 8, r1, 225, 315, i < lit ? color : UI_COLOR_DIVIDER);
  }
}

bool CWifiScreen::onButton(ButtonId id, ButtonEvent event) {
  // OK (BOOT long press) or home (PWR long press) continue to the pages
  if (event == ButtonEvent::LONG_PRESS && device.getDisplay()) {
    device.getDisplay()->show(ScreenId::HOME);
  }
  return false;
}

#ifdef TOUCH
bool CWifiScreen::onTouch(TouchEvent event, int16_t x, int16_t y) {
  if (event == TouchEvent::DOUBLE_TAP && device.getDisplay()) {
    device.getDisplay()->show(ScreenId::HOME);
    return true;
  }
  return false;
}
#endif

void CWifiScreen::draw(Arduino_GFX &gfx) {
  UI_drawStatusBar(gfx, device);

  const network_info_t &net = device.getNetworkInfo();
  const int16_t iconY = 116;
  char line[64];

  switch (device.getState()) {

    case DeviceState::WIFI_CONNECTED: {
      uint8_t bars = CDevice::signalBars(net.rssi);
      drawWifiIcon(gfx, LCD_WIDTH / 2, iconY, bars > 0 ? max<uint8_t>(1, bars - 1) : 0, UI_COLOR_ACCENT);
      UI_drawCentered(gfx, "Connected", 150, UI_FONT_TITLE, UI_COLOR_ACCENT);
      snprintf(line, sizeof(line), "%s  %d dBm", net.ssid, net.rssi);
      UI_drawCentered(gfx, line, 176, UI_FONT_SMALL, UI_COLOR_MUTED);

      gfx.fillRoundRect(SCREEN_SAFE_INSET, 188, LCD_WIDTH - 2 * SCREEN_SAFE_INSET, 56, 10, UI_COLOR_PANEL);
      UI_drawCentered(gfx, "Web server", 207, UI_FONT_SMALL, UI_COLOR_MUTED);
      UI_drawCentered(gfx, net.ip, 234, UI_FONT_TITLE, UI_COLOR_TEXT);
    } break;

    case DeviceState::WIFI_AP_CREATED: {
      drawWifiIcon(gfx, LCD_WIDTH / 2, iconY, 3, UI_COLOR_BLUE);
      UI_drawCentered(gfx, "Setup mode", 150, UI_FONT_TITLE, UI_COLOR_BLUE);

      // Fills the space the footer would use
      gfx.fillRoundRect(SCREEN_SAFE_INSET, 160, LCD_WIDTH - 2 * SCREEN_SAFE_INSET, 90, 10, UI_COLOR_PANEL);
      UI_drawCentered(gfx, "Join WiFi", 179, UI_FONT_SMALL, UI_COLOR_MUTED);
      UI_drawCentered(gfx, net.ssid, 199, UI_FONT_LABEL, UI_COLOR_TEXT);
      snprintf(line, sizeof(line), "Password: %s", net.password);
      UI_drawCentered(gfx, line, 219, UI_FONT_SMALL, UI_COLOR_TEXT);
      snprintf(line, sizeof(line), "Open http://%s", net.ip);
      UI_drawCentered(gfx, line, 240, UI_FONT_LABEL, UI_COLOR_YELLOW);
    } return;

    case DeviceState::WIFI_OFFLINE:
    case DeviceState::WIFI_CONNECTING:
    default: {
      bool offline = device.getState() == DeviceState::WIFI_OFFLINE;
      uint16_t color = offline ? UI_COLOR_ORANGE : UI_COLOR_YELLOW;
      drawWifiIcon(gfx, LCD_WIDTH / 2, iconY, animationFrame, color);
      snprintf(line, sizeof(line), "%s%.*s", offline ? "Reconnecting" : "Connecting", animationFrame, "...");
      UI_drawCentered(gfx, line, 150, UI_FONT_TITLE, color);
      if (strlen(net.ssid)) {
        UI_drawCentered(gfx, net.ssid, 176, UI_FONT_SMALL, UI_COLOR_MUTED);
      }
      if (offline) {
        UI_drawCentered(gfx, "Network lost", 230, UI_FONT_SMALL, UI_COLOR_MUTED);
      }
    } break;
  }

  // Footer
  UI_drawCentered(gfx, configuration.name, LCD_HEIGHT - 16, UI_FONT_SMALL, UI_COLOR_MUTED);
}

#endif
