#include <Arduino.h>
#include <ArduinoLog.h>

#if !defined(ESP32)
  #error This code is intended to run on the ESP32 platform! Please check your Tools->Board setting.
#endif

#include "Configuration.h"
#include "Device.h"
#include "wifi/WifiManager.h"

CDevice *device;
CWifiManager *wifiManager;

unsigned long tsSmoothBoot;
bool smoothBoot;

void setup() {
  // Before anything slow: on battery the board only stays powered once this latches
  CDevice::initHardware();

  #ifndef DISABLE_LOGGING
  Serial.begin(SERIAL_MONITOR_BAUD);
  // Bounded wait. With USB CDC, Serial only becomes true once a terminal attaches, so an unbounded
  // `while (!Serial)` would stop setup() from reaching loop() - where the factory reset counter is
  // cleared, so three headless boots in that state would wipe the stored configuration.
  unsigned long tsSerial = millis();
  while (!Serial && millis() - tsSerial < 2000) { delay(10); }
  Log.begin(LOG_LEVEL, &Serial);
  Log.infoln(F("\n\nInitializing..."));
    #ifdef WEB_LOGGING
    Log.addHandler(&logStream);
    Log.infoln(F("Initializing web log..."));
    #endif
  #elif defined(WEB_LOGGING)
    Log.begin(WEB_LOG_LEVEL, &logStream);
    Log.infoln(F("Initializing web log..."));
  #endif

  if (EEPROM_initAndCheckFactoryReset() >= 3) {
    Log.warningln("Factory reset conditions met!");
    EEPROM_wipe();
  }

  tsSmoothBoot = millis();
  smoothBoot = false;

  EEPROM_loadConfig();
  Log.infoln("Configuration loaded");

  device = new CDevice();
  device->begin();
  device->setBootProgress(40);

  wifiManager = new CWifiManager(device);
  device->setBootProgress(100);

  Log.noticeln("Setup completed!");
}

void loop() {
  if (!smoothBoot && millis() - tsSmoothBoot > FACTORY_RESET_CLEAR_TIMER_MS) {
    smoothBoot = true;
    EEPROM_clearFactoryReset();
    Log.noticeln("Device booted smoothly!");
  }

  device->loop();
  wifiManager->loop();

  #ifdef WEB_LOGGING
  trimLogStream();
  #endif

  if (wifiManager->isRebootNeeded()) {
    return;
  }

  delay(5);
  yield();
}
