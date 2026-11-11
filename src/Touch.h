#pragma once

#include "Configuration.h"

#ifdef TOUCH

#include <Arduino.h>

enum class TouchEvent : uint8_t {
  NONE = 0,
  TAP,
  SWIPE_LEFT,    // Finger moved right to left
  SWIPE_RIGHT,
  SWIPE_UP,
  SWIPE_DOWN
};

const char* touchEventToString(TouchEvent event);

#define TOUCH_POLL_MS        15
#define TOUCH_SWIPE_MIN_PX   50    // Travel along the swipe axis
#define TOUCH_SWIPE_RATIO    2     // ... and at least this many times the travel across it
#define TOUCH_TAP_MAX_PX     15
#define TOUCH_TAP_MAX_MS     500

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
  int16_t getX() const { return lastX; }  // Last point of the current / previous touch
  int16_t getY() const { return lastY; }

private:
  bool ready;
  unsigned long tsPoll;
  uint8_t readErrors;

  bool touching;
  int16_t startX, startY, lastX, lastY;
  unsigned long tsTouchStart;

  bool writeRegister(uint8_t reg, uint8_t value);
  bool readRegisters(uint8_t reg, uint8_t *buf, uint8_t len);
  TouchEvent classify() const;
};

#endif
