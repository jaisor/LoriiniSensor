#pragma once

#include "Configuration.h"

#if defined(SCREEN) && defined(SENSORS)

#include "display/pages/Page.h"
#include "sensors/Sensors.h"

// Page of one detected sensor, created by CHomeScreen when CSensors finds it. It refers to the sensor
// by id rather than pointer: the sensor can be dropped (unplugged) before the carousel catches up, and
// sensor() then returns nullptr. Redraws whenever the sensor publishes a value.
class CSensorPage : public CPage {

public:
  CSensorPage(CDevice &device, const CSensor &sensor);

  uint16_t getSensorId() const { return sensorId; }
  const char* getTitle() const override { return title; }   // "INA219 0x40", shown on the card
  bool update() override;

protected:
  const CSensor* sensor() const;

  // The sensor has no values to show: icon, name and why, as the card
  void drawStatusCard(Arduino_GFX &gfx, int16_t x, int16_t y, int16_t w, int16_t h,
    void (*icon)(Arduino_GFX &gfx, int16_t cx, int16_t cy));
  // ... and opened
  void drawStatusContent(Arduino_GFX &gfx);

private:
  uint16_t sensorId;
  char title[16];
  uint32_t drawnSample = 0;

  void statusText(const char **line1, const char **line2) const;
};

#endif
