#include <time.h>
#include "display/Screen.h"
#include "Device.h"

static int currentMinute() {
  struct tm timeinfo;
  if (!CONFIG_isTimeSynced() || !getLocalTime(&timeinfo, 0)) {
    return -1;
  }
  return timeinfo.tm_hour * 60 + timeinfo.tm_min;
}

bool CScreen::update() {
  return device.getRevision() != drawnRevision || currentMinute() != drawnMinute;
}

void CScreen::drawn() {
  dirty = false;
  drawnRevision = device.getRevision();
  drawnMinute = currentMinute();
}
