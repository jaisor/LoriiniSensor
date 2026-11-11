#include <Arduino.h>
#include <ArduinoLog.h>

#include "Device.h"
#ifdef SCREEN
  #include "display/Display.h"
#endif

#define BATTERY_READ_INTERVAL_MS 10000
#define POWER_OFF_HOLD_MS 3000        // Hold PWR this long to switch off (only cuts power when on battery)
#define POWER_OFF_DELAY_MS 1500       // Time to read the message before power is cut

const char* deviceStateToString(DeviceState state) {
  switch (state) {
    case DeviceState::INITIALIZING:    return "INITIALIZING";
    case DeviceState::WIFI_CONNECTING: return "WIFI_CONNECTING";
    case DeviceState::WIFI_AP_CREATED: return "WIFI_AP_CREATED";
    case DeviceState::WIFI_CONNECTED:  return "WIFI_CONNECTED";
    case DeviceState::WIFI_OFFLINE:    return "WIFI_OFFLINE";
    default:                           return "UNKNOWN";
  }
}

void CDevice::initHardware() {
  // On battery the board is only powered while PWR is held, until SYS_EN latches it on
  pinMode(PWR_SYS_EN, OUTPUT);
  digitalWrite(PWR_SYS_EN, HIGH);
  pinMode(PWR_SYS_OUT, INPUT);
  // Passive buzzer: driving it high continuously heats it up
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

CDevice::CDevice()
: _state(DeviceState::INITIALIZING), bootProgress(0), network{}, batteryVoltage(0), revision(0),
  tsBattery(0), powerOffRequested(false), powerHoldHandled(false), tsPowerOff(0),
  // Both have external pull-ups and read low when pressed. BOOT is a strapping pin, which only matters at reset.
  buttons{ CButton(BOOT_BUTTON_PIN, INPUT_PULLUP), CButton(PWR_SYS_OUT, INPUT) } {
  #ifdef SCREEN
  display = new CDisplay(*this);
  #endif
}

CDevice::~CDevice() {
  #ifdef SCREEN
  delete display;
  #endif
  Log.noticeln(F("Device destroyed"));
}

void CDevice::begin() {
  analogReadResolution(12);
  readBattery();
  #ifdef SCREEN
  display->begin();
  #endif
  #ifdef BUZZER
  buzzer.begin();  // After the backlight PWM, see CBuzzer
  #endif
  // PWR is usually still held from switching the board on; CButton ignores that press
  for (CButton &b : buttons) {
    b.begin();
  }
  Log.infoln(F("Device initialized"));
}

void CDevice::loop() {
  if (millis() - tsBattery > BATTERY_READ_INTERVAL_MS) {
    readBattery();
  }
  handleButtons();
  handlePowerOff();
  #ifdef SCREEN
  display->loop();
  #endif
}

void CDevice::setState(DeviceState state) {
  if (_state != state) {
    Log.infoln("Device state: %s -> %s", deviceStateToString(_state), deviceStateToString(state));
    _state = state;
    changed();
    #ifdef SCREEN
    display->onStateChanged(state);
    #endif
  }
}

void CDevice::setBootProgress(uint8_t percent) {
  percent = min<uint8_t>(percent, 100);
  if (percent != bootProgress) {
    bootProgress = percent;
    changed();
    #ifdef SCREEN
    // setup() blocks the loop, so draw right away to show progress
    display->loop();
    #endif
  }
}

void CDevice::setNetworkInfo(const char *ssid, const char *ip, const char *apPassword) {
  strlcpy(network.ssid, ssid ? ssid : "", sizeof(network.ssid));
  strlcpy(network.ip, ip ? ip : "", sizeof(network.ip));
  strlcpy(network.password, apPassword ? apPassword : "", sizeof(network.password));
  changed();
}

// Signal strength in the four steps the screen shows, so small fluctuations don't cause redraws
uint8_t CDevice::signalBars(int8_t rssi) {
  if (rssi == 0) return 0;
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -78) return 2;
  return 1;
}

void CDevice::setRssi(int8_t rssi) {
  bool barsChanged = signalBars(rssi) != signalBars(network.rssi);
  network.rssi = rssi;
  if (barsChanged) {
    changed();
  }
}

void CDevice::showMessage(const char *message, unsigned long durationMs) {
  Log.infoln("Message: %s", message);
  #ifdef SCREEN
  display->showMessage(message, durationMs);
  #endif
}

void CDevice::readBattery() {
  tsBattery = millis();
  uint32_t mv = 0;
  for (uint8_t i = 0; i < 8; i++) {
    mv += analogReadMilliVolts(BAT_ADC_PIN);
  }
  float v = (mv / 8.0f) * BAT_ADC_MULTIPLIER / 1000.0f;
  if (fabsf(v - batteryVoltage) >= 0.05f) {
    changed();
  }
  batteryVoltage = v;
}

void CDevice::handleButtons() {
  for (uint8_t i = 0; i < (uint8_t)ButtonId::COUNT; i++) {
    ButtonEvent event = buttons[i].loop();
    if (event != ButtonEvent::NONE) {
      onButton((ButtonId)i, event);
    }
  }

  // Holding PWR switches off, on top of the long press event it reports first
  CButton &power = buttons[(uint8_t)ButtonId::POWER];
  if (!power.isPressed()) {
    powerHoldHandled = false;
  } else if (!powerHoldHandled && power.pressedFor() >= POWER_OFF_HOLD_MS) {
    powerHoldHandled = true;
    powerOff();
  }
}

void CDevice::onButton(ButtonId id, ButtonEvent event) {
  Log.infoln("Button %s: %s", buttonIdToString(id), buttonEventToString(event));

  #ifdef BUZZER
  switch (event) {
    case ButtonEvent::CLICK:        buzzer.play(BuzzerSound::CLICK); break;
    case ButtonEvent::DOUBLE_CLICK: buzzer.play(BuzzerSound::DOUBLE_CLICK); break;
    case ButtonEvent::LONG_PRESS:   buzzer.play(BuzzerSound::LONG_PRESS); break;
    default: break;
  }
  #endif

  #ifdef SCREEN
  display->onButton(id, event);
  #endif
}

void CDevice::handlePowerOff() {
  if (powerOffRequested && millis() - tsPowerOff > POWER_OFF_DELAY_MS) {
    Log.noticeln("Releasing power latch");
    digitalWrite(PWR_SYS_EN, LOW);
    // On USB power the board keeps running; let it be switched off again later
    powerOffRequested = false;
    delay(500);
    digitalWrite(PWR_SYS_EN, HIGH);
  }
}

void CDevice::powerOff() {
  Log.noticeln("Powering off");
  showMessage("Powering off", POWER_OFF_DELAY_MS + 1000);
  #ifdef BUZZER
  buzzer.play(BuzzerSound::POWER_OFF);
  #endif
  powerOffRequested = true;
  tsPowerOff = millis();
}
