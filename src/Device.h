#pragma once

#include <Arduino.h>
#include <atomic>
#include "Configuration.h"
#include "Button.h"
#ifdef BUZZER
  #include "Buzzer.h"
#endif

#ifdef SCREEN
  class CDisplay;
#endif

// What the device is doing. The display picks its screen from this (CDisplay::screenForState), so new
// states are how new screens get shown automatically.
enum class DeviceState : uint8_t {
  INITIALIZING = 0,
  WIFI_CONNECTING,   // Joining the configured network
  WIFI_AP_CREATED,   // Running its own access point for setup
  WIFI_CONNECTED,    // Joined, web server reachable
  WIFI_OFFLINE       // Lost the network, reconnecting
};

const char* deviceStateToString(DeviceState state);

// Network details shown on screen and in the web UI, set by CWifiManager
struct network_info_t {
  char ssid[33];
  char password[64];  // Only for the soft AP (so it can be shown on screen), never the configured network's
  char ip[16];
  int8_t rssi;        // dBm, 0 when unknown
};

class CDevice {

public:
  CDevice();
  virtual ~CDevice();

  // First thing in setup(): latch battery power on and silence the buzzer
  static void initHardware();

  void begin();
  void loop();

  DeviceState getState() const { return _state; }
  void setState(DeviceState state);

  void setBootProgress(uint8_t percent); // 0-100
  uint8_t getBootProgress() const { return bootProgress; }

  void setNetworkInfo(const char *ssid, const char *ip, const char *apPassword = "");
  void setRssi(int8_t rssi);
  static uint8_t signalBars(int8_t rssi);  // 0 (unknown) to 4
  const network_info_t& getNetworkInfo() const { return network; }

  float getBatteryVoltage() const { return batteryVoltage; }

  // Bumped whenever anything a screen may show changes; screens redraw when it differs from theirs
  uint32_t getRevision() const { return revision.load(); }
  void configChanged() { changed(); }  // Settings shown on screen were edited (web UI)

  // Brief overlay message on the display, e.g. "Saved" or "Powering off"
  void showMessage(const char *message, unsigned long durationMs = 3000);

  void powerOff();

  #ifdef BUZZER
  void playSound(BuzzerSound sound) { buzzer.play(sound); }
  #endif

  #ifdef SCREEN
  CDisplay* getDisplay() const { return display; }
  #endif

private:
  DeviceState _state;
  uint8_t bootProgress;
  network_info_t network;
  float batteryVoltage;
  std::atomic<uint32_t> revision;  // Bumped from the web server task too

  unsigned long tsBattery;
  bool powerOffRequested;
  bool powerHoldHandled;          // Power off already triggered by the current PWR press
  unsigned long tsPowerOff;

  CButton buttons[(uint8_t)ButtonId::COUNT];
  #ifdef BUZZER
  CBuzzer buzzer;
  #endif

  #ifdef SCREEN
  CDisplay *display;
  #endif

  void readBattery();
  void handleButtons();
  void onButton(ButtonId id, ButtonEvent event);
  void handlePowerOff();
  void changed() { revision++; }
};
