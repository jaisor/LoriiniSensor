#pragma once

#include <Arduino.h>

enum class ButtonId : uint8_t {
  BOOT = 0,
  POWER,
  COUNT
};

enum class ButtonEvent : uint8_t {
  NONE = 0,
  CLICK,          // Reported once the double click window has passed without a second press
  DOUBLE_CLICK,
  LONG_PRESS      // Reported while still held, once BUTTON_LONG_PRESS_MS is reached
};

const char* buttonIdToString(ButtonId id);
const char* buttonEventToString(ButtonEvent event);

#define BUTTON_DEBOUNCE_MS      25
#define BUTTON_DOUBLE_CLICK_MS  300   // Max gap between the two presses of a double click
#define BUTTON_LONG_PRESS_MS    700

// Debounced push button with click / double click / long press detection. Non-blocking: call loop()
// every main loop iteration.
class CButton {

public:
  CButton(uint8_t pin, uint8_t mode = INPUT_PULLUP, bool activeLow = true);

  // A button that is already down here (e.g. PWR, which powers the board on) is ignored until released
  void begin();

  // At most one event per call
  ButtonEvent loop();

  bool isPressed() const { return stable && !ignoreUntilReleased; }
  unsigned long pressedFor() const;   // ms the current press has lasted, 0 when up

private:
  uint8_t pin;
  uint8_t mode;
  bool activeLow;

  bool raw;                 // Last reading
  unsigned long tsRawChange;
  bool stable;              // Debounced state, true = pressed
  bool ignoreUntilReleased;

  unsigned long tsPress;
  unsigned long tsRelease;
  uint8_t clicks;           // Short presses waiting for the double click window
  bool longFired;

  bool read() const { return digitalRead(pin) == (activeLow ? LOW : HIGH); }
};
