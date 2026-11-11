#include "Button.h"

const char* buttonIdToString(ButtonId id) {
  switch (id) {
    case ButtonId::BOOT:  return "BOOT";
    case ButtonId::POWER: return "POWER";
    default:              return "UNKNOWN";
  }
}

const char* buttonEventToString(ButtonEvent event) {
  switch (event) {
    case ButtonEvent::NONE:         return "NONE";
    case ButtonEvent::CLICK:        return "CLICK";
    case ButtonEvent::DOUBLE_CLICK: return "DOUBLE_CLICK";
    case ButtonEvent::LONG_PRESS:   return "LONG_PRESS";
    default:                        return "UNKNOWN";
  }
}

CButton::CButton(uint8_t pin, uint8_t mode, bool activeLow)
: pin(pin), mode(mode), activeLow(activeLow), raw(false), tsRawChange(0), stable(false), ignoreUntilReleased(false),
  tsPress(0), tsRelease(0), clicks(0), longFired(false) {
}

void CButton::begin() {
  pinMode(pin, mode);
  raw = stable = read();
  tsRawChange = millis();
  ignoreUntilReleased = stable;
}

unsigned long CButton::pressedFor() const {
  return isPressed() ? millis() - tsPress : 0;
}

ButtonEvent CButton::loop() {
  const unsigned long now = millis();

  bool r = read();
  if (r != raw) {
    raw = r;
    tsRawChange = now;
  }

  if (raw != stable && now - tsRawChange >= BUTTON_DEBOUNCE_MS) {
    stable = raw;
    if (stable) {
      // Pressed
      if (!ignoreUntilReleased) {
        tsPress = now;
        longFired = false;
      }
    } else {
      // Released
      if (ignoreUntilReleased) {
        ignoreUntilReleased = false;
        clicks = 0;
      } else if (!longFired) {
        tsRelease = now;
        if (++clicks >= 2) {
          clicks = 0;
          return ButtonEvent::DOUBLE_CLICK;
        }
      }
    }
  }

  if (isPressed() && !longFired && now - tsPress >= BUTTON_LONG_PRESS_MS) {
    longFired = true;
    clicks = 0;  // A click just before the hold is dropped
    return ButtonEvent::LONG_PRESS;
  }

  if (!stable && clicks == 1 && now - tsRelease >= BUTTON_DOUBLE_CLICK_MS) {
    clicks = 0;
    return ButtonEvent::CLICK;
  }

  return ButtonEvent::NONE;
}
