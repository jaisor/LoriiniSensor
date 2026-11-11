#pragma once

#include "display/Screen.h"
#include "display/pages/Page.h"
#ifdef SENSORS
  #include "sensors/Sensors.h"
  #define HOME_SENSOR_PAGES SENSOR_MAX
#else
  #define HOME_SENSOR_PAGES 0
#endif

#define HOME_MAX_PAGES (HOME_SENSOR_PAGES + 3)   // Sensors (or the "none found" page), Settings, Help

class CSensorPage;

// Status bar over a carousel of pages: one per detected sensor (or a "none found" page), Settings,
// Help. The sensor pages follow CSensors: added and dropped as sensors come and go, while the page
// being shown stays in place.
//   Browsing: PWR click or swipe left = next page (slides in from the right), PWR double click or swipe
//             right = previous, BOOT click or double tap = open.
//   Opened:   buttons go to the page; it returns PageAction::EXIT (BOOT long press by convention) to browse
//             again. A long press on the screen leaves the page from anywhere in it.
class CHomeScreen : public CScreen {

public:
  explicit CHomeScreen(CDevice &device);
  ~CHomeScreen();

  const char* getName() const override { return "Home"; }
  bool update() override;
  void draw(Arduino_GFX &gfx) override;
  bool onButton(ButtonId id, ButtonEvent event) override;
  #ifdef TOUCH
  bool onTouch(TouchEvent event, int16_t x, int16_t y) override;
  #endif

private:
  CPage *pages[HOME_MAX_PAGES];   // The carousel, in order
  uint8_t pageCount = 0;
  uint8_t index = 0;
  bool opened = false;

  CPage *noSensors;
  CPage *settings;
  CPage *help;
  #ifdef SENSORS
  CSensorPage *sensorPages[HOME_SENSOR_PAGES];
  uint8_t sensorPageCount = 0;
  uint32_t sensorListRevision = UINT32_MAX;
  #endif

  // Slide between cards
  bool sliding = false;
  int8_t slideDir = 0;          // 1 = new card enters from the right
  uint8_t slideFrom = 0;
  unsigned long tsSlide = 0;

  void syncSensorPages();       // Pages for the sensors CSensors has now
  void open();
  void close();
  void slide(int8_t dir);
  void drawCard(Arduino_GFX &gfx, uint8_t page, int16_t dx);
};
