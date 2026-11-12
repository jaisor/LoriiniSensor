#pragma once

#include "Configuration.h"

#ifdef TOUCH

#include <Arduino.h>

enum class TouchEvent : uint8_t {
  NONE = 0,
  TAP,           // Reported once the double tap window has passed without a second tap (on lift
                 // when double taps are off)
  DOUBLE_TAP,
  LONG_PRESS,    // Reported while still touching, once TOUCH_LONG_PRESS_MS is reached without moving
  SWIPE_LEFT,    // Finger moved right to left
  SWIPE_RIGHT,
  SWIPE_UP,
  SWIPE_DOWN
};

const char* touchEventToString(TouchEvent event);

#define TOUCH_POLL_MS        15
#define TOUCH_SWIPE_MIN_PX   50    // Travel along the swipe axis
#define TOUCH_SWIPE_RATIO    2     // ... and at least this many times the travel across it
#define TOUCH_TAP_MAX_PX     15    // Movement still counted as a tap / long press
#define TOUCH_TAP_MAX_MS     500
#define TOUCH_DOUBLE_TAP_MS  350   // Max gap from the first tap's lift to the second tap's lift
#define TOUCH_LONG_PRESS_MS  700

// CST816T capacitive touch on the shared I2C bus. Polled from the loop: the chip's auto-sleep is
// turned off so its registers can be read any time. Gestures are worked out from the start and end
// points of each touch rather than the chip's own gesture codes, so the thresholds above apply.
class CTouch {

public:
  CTouch();

  bool begin();   // Wire must already be started on I2C_SDA / I2C_SCL
  bool isReady() const { return ready; }

  // At most one event per call, reported when the finger lifts
  TouchEvent loop();

  bool isTouched() const { return touching; }
  // Last point of the current / previous touch, in screen coordinates (TOUCH_MIRROR_X/Y applied)
  int16_t getX() const { return TOUCH_MIRROR_X ? LCD_WIDTH - 1 - lastX : lastX; }
  int16_t getY() const { return TOUCH_MIRROR_Y ? LCD_HEIGHT - 1 - lastY : lastY; }

  // Off: taps are reported as soon as the finger lifts and DOUBLE_TAP never is - for screens with
  // buttons to tap quickly, which would otherwise wait out the double tap window (and pair up)
  void setDoubleTap(bool enabled) { doubleTap = enabled; }

private:
  bool ready;
  unsigned long tsPoll;
  uint8_t readErrors;

  bool touching;
  int16_t startX, startY, lastX, lastY;
  unsigned long tsTouchStart;
  bool moved;               // Went past TOUCH_TAP_MAX_PX at some point during this touch
  bool longFired;           // LONG_PRESS already reported for this touch
  bool doubleTap = true;
  bool tapPending;          // A tap waiting to see if a second one follows
  unsigned long tsTap;

  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegisters(uint8_t reg, uint8_t *buf, uint8_t len);
  TouchEvent classify() const;
};

#endif
