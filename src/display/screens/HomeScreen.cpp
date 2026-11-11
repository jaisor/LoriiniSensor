#include "display/screens/HomeScreen.h"

#include "Configuration.h"
#ifdef SCREEN

#include <ArduinoLog.h>
#include "Device.h"
#include "display/UI.h"
#include "display/pages/NoSensorPage.h"
#include "display/pages/SettingsPage.h"
#include "display/pages/HelpPage.h"
#ifdef SENSORS
  #include "display/pages/PowerPage.h"
  #include "display/pages/EnvPage.h"
#endif

#define SLIDE_MS 240

// Card box in the carousel, with the page dots under it
#define CARD_X  SCREEN_SAFE_INSET
#define CARD_Y  UI_CONTENT_TOP
#define CARD_W  (LCD_WIDTH - 2 * SCREEN_SAFE_INSET)
#define CARD_H  208
#define DOTS_Y  (CARD_Y + CARD_H + 13)

CHomeScreen::CHomeScreen(CDevice &device) : CScreen(device), pages{} {
  noSensors = new CNoSensorPage(device);
  settings = new CSettingsPage(device);
  help = new CHelpPage(device);
  #ifdef SENSORS
  for (CSensorPage *&p : sensorPages) {
    p = nullptr;
  }
  #endif
  syncSensorPages();
}

CHomeScreen::~CHomeScreen() {
  #ifdef SENSORS
  for (uint8_t i = 0; i < sensorPageCount; i++) {
    delete sensorPages[i];
  }
  #endif
  delete noSensors;
  delete settings;
  delete help;
}

void CHomeScreen::syncSensorPages() {
  CPage *shown = pageCount ? pages[index] : nullptr;

  #ifdef SENSORS
  const CSensors &sensors = device.getSensors();
  sensorListRevision = sensors.getListRevision();

  // Keep the pages of sensors still there (in the sensors' order), make pages for new ones
  CSensorPage *next[HOME_SENSOR_PAGES] = {};
  uint8_t n = 0;
  for (uint8_t i = 0; i < sensors.getCount() && n < HOME_SENSOR_PAGES; i++) {
    const CSensor *s = sensors.get(i);
    CSensorPage *page = nullptr;
    for (uint8_t j = 0; j < sensorPageCount; j++) {
      if (sensorPages[j] && sensorPages[j]->getSensorId() == s->getId()) {
        page = sensorPages[j];
        sensorPages[j] = nullptr;
        break;
      }
    }
    if (!page) {
      if (s->getKind() == SensorKind::POWER) {
        page = new CPowerPage(device, *static_cast<const CPowerSensor*>(s));
      } else {
        page = new CEnvPage(device, *static_cast<const CEnvSensor*>(s));
      }
    }
    next[n++] = page;
  }
  // What's left belongs to dropped sensors
  for (uint8_t j = 0; j < sensorPageCount; j++) {
    if (sensorPages[j]) {
      if (opened && shown == sensorPages[j]) {
        opened = false;   // Its sensor is gone
      }
      if (shown == sensorPages[j]) {
        shown = nullptr;
      }
      delete sensorPages[j];
    }
  }
  for (uint8_t j = 0; j < HOME_SENSOR_PAGES; j++) {
    sensorPages[j] = next[j];
  }
  sensorPageCount = n;
  #else
  const uint8_t n = 0;
  #endif

  pageCount = 0;
  #ifdef SENSORS
  for (uint8_t i = 0; i < n; i++) {
    pages[pageCount++] = sensorPages[i];
  }
  #endif
  if (n == 0) {
    pages[pageCount++] = noSensors;
  }
  pages[pageCount++] = settings;
  pages[pageCount++] = help;

  // Stay on the page that was showing; if it went away, show the first one
  index = 0;
  for (uint8_t i = 0; i < pageCount; i++) {
    if (pages[i] == shown) {
      index = i;
      break;
    }
  }
  if (opened && pages[index] != shown) {
    opened = false;   // The "none found" page gave way to sensor pages
  }
  sliding = false;
  invalidate();
}

bool CHomeScreen::update() {
  #ifdef SENSORS
  if (device.getSensors().getListRevision() != sensorListRevision) {
    syncSensorPages();
  }
  #endif
  bool redraw = CScreen::update();
  if (sliding) {
    if (millis() - tsSlide >= SLIDE_MS) {
      sliding = false;
    }
    redraw = true;  // Every frame of the slide, and once more at rest
  }
  return pages[index]->update() || redraw;
}

void CHomeScreen::open() {
  sliding = false;
  opened = true;
  pages[index]->onOpen();
}

void CHomeScreen::close() {
  pages[index]->onClose();
  opened = false;
}

void CHomeScreen::slide(int8_t dir) {
  slideFrom = index;
  index = (index + pageCount + dir) % pageCount;
  slideDir = dir;
  sliding = true;
  tsSlide = millis();
  Log.verboseln("Home: page %s", pages[index]->getTitle());
}

bool CHomeScreen::onButton(ButtonId id, ButtonEvent event) {
  if (opened) {
    switch (pages[index]->onButton(id, event)) {
      case PageAction::EXIT:
        close();
        return true;
      case PageAction::REDRAW:
        return true;
      default:
        return false;
    }
  }

  if (id == ButtonId::POWER && event == ButtonEvent::CLICK) {
    slide(1);
    return true;
  }
  if (id == ButtonId::POWER && event == ButtonEvent::DOUBLE_CLICK) {
    slide(-1);
    return true;
  }
  if (id == ButtonId::BOOT && event == ButtonEvent::CLICK) {
    open();
    return true;
  }
  return false;
}

#ifdef TOUCH
bool CHomeScreen::onTouch(TouchEvent event, int16_t x, int16_t y) {
  if (opened) {
    // Long press leaves the page from wherever it is (submenus included)
    if (event == TouchEvent::LONG_PRESS) {
      close();
      return true;
    }
    return false;
  }
  if (event == TouchEvent::DOUBLE_TAP) {
    open();
    return true;
  }
  // Content follows the finger: swiping left pulls in the next card from the right
  if (event == TouchEvent::SWIPE_LEFT) {
    slide(1);
    return true;
  }
  if (event == TouchEvent::SWIPE_RIGHT) {
    slide(-1);
    return true;
  }
  return false;
}
#endif

void CHomeScreen::drawCard(Arduino_GFX &gfx, uint8_t page, int16_t dx) {
  const int16_t x = CARD_X + dx;
  gfx.fillRoundRect(x, CARD_Y, CARD_W, CARD_H, 18, UI_COLOR_PANEL);
  pages[page]->drawCard(gfx, x, CARD_Y, CARD_W, CARD_H);
}

void CHomeScreen::draw(Arduino_GFX &gfx) {
  UI_drawStatusBar(gfx, device);

  if (opened) {
    pages[index]->drawContent(gfx);
    return;
  }

  if (sliding) {
    // Ease out: fast start, settles into place
    float t = min(1.0f, (millis() - tsSlide) / (float)SLIDE_MS);
    float e = 1 - (1 - t) * (1 - t) * (1 - t);
    int16_t travel = LCD_WIDTH;
    drawCard(gfx, slideFrom, -slideDir * travel * e);
    drawCard(gfx, index, slideDir * travel * (1 - e));
  } else {
    drawCard(gfx, index, 0);
  }

  UI_drawPageDots(gfx, DOTS_Y, pageCount, index);
}

#endif
