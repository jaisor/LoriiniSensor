#include "display/Display.h"

#ifdef SCREEN

#include <ArduinoLog.h>
#include "display/UI.h"
#include "display/screens/BootScreen.h"
#include "display/screens/WifiScreen.h"
#include "display/screens/HomeScreen.h"

#define DISPLAY_MIN_FRAME_MS 33      // At most ~30 redraws per second
#define BACKLIGHT_PWM_HZ 5000
#define BACKLIGHT_PWM_BITS 10

CDisplay::CDisplay(CDevice &device)
: device(device), bus(nullptr), panel(nullptr), canvas(nullptr), ready(false), screens{}, current(nullptr),
  appliedBrightness(0), tsLastFlush(0), saver(SaverState::AWAKE), tsActivity(0), appliedSaverSec(0),
  message{}, tsMessageEnd(0), messageShowing(false) {
}

CDisplay::~CDisplay() {
  for (CScreen *s : screens) {
    delete s;
  }
  delete canvas;
  delete panel;
  delete bus;
}

void CDisplay::begin() {
  // Backlight off until the first frame is drawn, so the panel's power-on noise isn't visible
  ledcAttach(LCD_BL, BACKLIGHT_PWM_HZ, BACKLIGHT_PWM_BITS);
  ledcWrite(LCD_BL, 0);

  bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI, GFX_NOT_DEFINED);
  panel = new Arduino_ST7789(bus, LCD_RST, 0 /* rotation */, true /* IPS */, LCD_WIDTH, LCD_HEIGHT,
    LCD_COL_OFFSET, LCD_ROW_OFFSET, LCD_COL_OFFSET, LCD_ROW_OFFSET);
  canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

  if (!canvas->begin(LCD_SPI_HZ)) {
    Log.errorln("Display initialization failed (framebuffer of %i bytes, %i free PSRAM)",
      LCD_WIDTH * LCD_HEIGHT * 2, ESP.getFreePsram());
    return;
  }
  ready = true;
  Log.infoln("Display %ix%i initialized", LCD_WIDTH, LCD_HEIGHT);

  screens[(uint8_t)ScreenId::BOOT] = new CBootScreen(device);
  screens[(uint8_t)ScreenId::WIFI_STATUS] = new CWifiScreen(device);
  screens[(uint8_t)ScreenId::HOME] = new CHomeScreen(device);

  show(screenForState(device.getState()));
  render();
  applyBrightness();
  tsActivity = millis();
  appliedSaverSec = configuration.screenSaverSec;
}

// The extension point for state driven UI: which screen each device state shows
ScreenId CDisplay::screenForState(DeviceState state) {
  switch (state) {
    case DeviceState::INITIALIZING:
      return ScreenId::BOOT;
    case DeviceState::WIFI_AP_CREATED:
      return ScreenId::WIFI_STATUS;   // How to join and configure; BOOT continues to HOME
    case DeviceState::WIFI_CONNECTING:
    case DeviceState::WIFI_CONNECTED:
    case DeviceState::WIFI_OFFLINE:
    default:
      return ScreenId::HOME;          // WiFi shows in the status bar
  }
}

void CDisplay::onStateChanged(DeviceState state) {
  if (!ready) return;
  show(screenForState(state));
  // The home screen only shows WiFi as an icon, so announce the changes
  if (state == DeviceState::WIFI_CONNECTED) {
    showMessage(device.getNetworkInfo().ip, 3000);
  } else if (state == DeviceState::WIFI_OFFLINE) {
    showMessage("WiFi lost", 3000);
  }
  if (current) {
    current->invalidate();  // Same screen can look different per state
  }
}

void CDisplay::show(ScreenId id) {
  if (!ready || id >= ScreenId::COUNT) return;
  CScreen *next = screens[(uint8_t)id];
  if (!next || next == current) return;
  if (current) {
    current->onExit();
  }
  Log.verboseln("Screen: %s", next->getName());
  current = next;
  current->onEnter();
}

void CDisplay::onButton(ButtonId id, ButtonEvent event) {
  if (!ready || !current) return;
  if (current->onButton(id, event)) {
    current->invalidate();
  }
}

#ifdef TOUCH
bool CDisplay::onTouch(TouchEvent event, int16_t x, int16_t y) {
  if (!ready || !current) return false;
  if (current->onTouch(event, x, y)) {
    current->invalidate();
    return true;
  }
  return false;
}
#endif

void CDisplay::showMessage(const char *text, unsigned long durationMs) {
  strlcpy(message, text, sizeof(message));
  tsMessageEnd = millis() + durationMs;
  messageShowing = true;
  if (current) {
    current->invalidate();
  }
}

void CDisplay::applyBrightness() {
  appliedBrightness = configuration.screenBrightness;
  uint32_t level = appliedBrightness;
  if (saver == SaverState::DIM) {
    level = (level + 1) / 2;
  } else if (saver == SaverState::DARK) {
    level = 0;
  }
  // Perceived brightness is roughly quadratic in duty cycle
  uint32_t max = (1 << BACKLIGHT_PWM_BITS) - 1;
  uint32_t duty = level * level * max / 10000;
  ledcWrite(LCD_BL, duty);
}

CDisplay::SaverState CDisplay::saverStateFor(unsigned long idleMs) const {
  if (configuration.screenSaverSec == 0) return SaverState::AWAKE;
  unsigned long timeoutMs = (unsigned long)configuration.screenSaverSec * 1000;
  if (idleMs >= timeoutMs) return SaverState::DARK;
  if (idleMs >= timeoutMs / 2) return SaverState::DIM;
  return SaverState::AWAKE;
}

void CDisplay::setSaver(SaverState state) {
  const SaverState was = saver;
  saver = state;
  if (state == SaverState::DARK) {
    Log.verboseln("Screen saver: dark");
    applyBrightness();
    panel->displayOff();   // Panel sleep; its memory keeps the frame
  } else {
    if (was == SaverState::DARK) {
      Log.verboseln("Screen saver: awake");
      panel->displayOn();
      current->invalidate();
      render();            // Up to date before the backlight comes on
    } else if (state == SaverState::DIM) {
      Log.verboseln("Screen saver: dimmed");
    }
    applyBrightness();
  }
}

bool CDisplay::wake() {
  tsActivity = millis();
  if (!ready || !current || saver == SaverState::AWAKE) return false;
  const bool wasDark = saver == SaverState::DARK;
  setSaver(SaverState::AWAKE);
  return wasDark;
}

void CDisplay::loop() {
  if (!ready || !current) return;

  // Screen saver; a new duration (Settings or the web page) counts from now
  if (appliedSaverSec != configuration.screenSaverSec) {
    appliedSaverSec = configuration.screenSaverSec;
    tsActivity = millis();
  }
  const SaverState state = saverStateFor(millis() - tsActivity);
  if (state != saver) {
    setSaver(state);
  }

  // Changed from the web UI (another task); applied here so it takes effect live
  if (appliedBrightness != configuration.screenBrightness) {
    applyBrightness();
  }

  if (saver == SaverState::DARK) {
    return;   // Nothing to draw; wake() renders a fresh frame
  }

  if (messageShowing && (long)(millis() - tsMessageEnd) >= 0) {
    messageShowing = false;
    current->invalidate();
  }

  if (current->update()) {
    current->invalidate();
  }

  if (current->isDirty() && millis() - tsLastFlush >= DISPLAY_MIN_FRAME_MS) {
    render();
  }
}

void CDisplay::render() {
  if (!current) return;
  canvas->fillScreen(UI_COLOR_BG);
  canvas->setFont(nullptr);
  canvas->setTextSize(1);
  canvas->setTextWrap(false);
  current->draw(*canvas);
  if (messageShowing) {
    drawMessage();
  }
  canvas->flush();
  current->drawn();
  tsLastFlush = millis();
}

void CDisplay::drawMessage() {
  const int16_t w = LCD_WIDTH - 2 * SCREEN_SAFE_INSET;
  const int16_t h = 56;
  const int16_t x = SCREEN_SAFE_INSET;
  const int16_t y = (LCD_HEIGHT - h) / 2;
  canvas->fillRoundRect(x, y, w, h, 10, UI_COLOR_PANEL);
  canvas->drawRoundRect(x, y, w, h, 10, UI_COLOR_ACCENT);
  UI_drawCentered(*canvas, message, y + h / 2 + 6, UI_FONT_LABEL, UI_COLOR_TEXT);
}

#endif
