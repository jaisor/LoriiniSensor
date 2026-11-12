#include "Touch.h"

#ifdef TOUCH

#include <Wire.h>
#include <ArduinoLog.h>

// CST816 registers
#define REG_FINGER_NUM    0x02  // Followed by XposH, XposL, YposH, YposL
#define REG_CHIP_ID       0xA7
#define REG_FW_VERSION    0xA9
#define REG_DIS_AUTO_SLEEP 0xFE // Non-zero keeps the chip awake (it doesn't answer I2C while asleep)

const char* touchEventToString(TouchEvent event) {
  switch (event) {
    case TouchEvent::NONE:        return "NONE";
    case TouchEvent::TAP:         return "TAP";
    case TouchEvent::DOUBLE_TAP:  return "DOUBLE_TAP";
    case TouchEvent::LONG_PRESS:  return "LONG_PRESS";
    case TouchEvent::SWIPE_LEFT:  return "SWIPE_LEFT";
    case TouchEvent::SWIPE_RIGHT: return "SWIPE_RIGHT";
    case TouchEvent::SWIPE_UP:    return "SWIPE_UP";
    case TouchEvent::SWIPE_DOWN:  return "SWIPE_DOWN";
    default:                      return "UNKNOWN";
  }
}

CTouch::CTouch()
: ready(false), tsPoll(0), readErrors(0), touching(false), startX(0), startY(0), lastX(0), lastY(0), tsTouchStart(0),
  moved(false), longFired(false), tapPending(false), tsTap(0) {
}

bool CTouch::writeRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool CTouch::readRegisters(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, len) != len) return false;
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

bool CTouch::begin() {
  pinMode(TP_INT, INPUT);
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(10);
  digitalWrite(TP_RST, HIGH);
  delay(60);  // Boot time after reset

  uint8_t id[3];
  if (!readRegisters(REG_CHIP_ID, id, 3)) {
    Log.errorln("Touch controller not found at 0x%x", TOUCH_I2C_ADDR);
    return false;
  }
  Log.infoln("Touch controller chip id 0x%x, firmware %i", id[0], id[2]);

  if (!writeRegister(REG_DIS_AUTO_SLEEP, 0x01)) {
    Log.warningln("Touch controller: couldn't disable auto sleep");
  }
  ready = true;
  return true;
}

TouchEvent CTouch::classify() const {
  int16_t dx = lastX - startX;
  int16_t dy = lastY - startY;
  #if TOUCH_MIRROR_X
    dx = -dx;
  #endif
  #if TOUCH_MIRROR_Y
    dy = -dy;
  #endif
  int16_t ax = abs(dx), ay = abs(dy);

  if (ax >= TOUCH_SWIPE_MIN_PX && ax >= TOUCH_SWIPE_RATIO * ay) {
    return dx < 0 ? TouchEvent::SWIPE_LEFT : TouchEvent::SWIPE_RIGHT;
  }
  if (ay >= TOUCH_SWIPE_MIN_PX && ay >= TOUCH_SWIPE_RATIO * ax) {
    return dy < 0 ? TouchEvent::SWIPE_UP : TouchEvent::SWIPE_DOWN;
  }
  if (!moved && millis() - tsTouchStart <= TOUCH_TAP_MAX_MS) {
    return TouchEvent::TAP;
  }
  return TouchEvent::NONE;  // Wandering or diagonal
}

TouchEvent CTouch::loop() {
  if (!ready || millis() - tsPoll < TOUCH_POLL_MS) return TouchEvent::NONE;
  tsPoll = millis();

  // A single tap is only certain once the double tap window has passed
  if (tapPending && !touching && (!doubleTap || millis() - tsTap > TOUCH_DOUBLE_TAP_MS)) {
    tapPending = false;
    return TouchEvent::TAP;
  }

  uint8_t buf[5];
  if (!readRegisters(REG_FINGER_NUM, buf, sizeof(buf))) {
    if (readErrors < 255 && ++readErrors == 10) {
      Log.warningln("Touch controller stopped answering");
    }
    return TouchEvent::NONE;
  }
  readErrors = 0;

  bool down = buf[0] > 0;
  if (down) {
    int16_t x = ((buf[1] & 0x0F) << 8) | buf[2];
    int16_t y = ((buf[3] & 0x0F) << 8) | buf[4];
    if (!touching) {
      touching = true;
      startX = x;
      startY = y;
      tsTouchStart = millis();
      moved = false;
      longFired = false;
    }
    lastX = x;
    lastY = y;
    if (abs(x - startX) > TOUCH_TAP_MAX_PX || abs(y - startY) > TOUCH_TAP_MAX_PX) {
      moved = true;
    }
    if (!moved && !longFired && millis() - tsTouchStart >= TOUCH_LONG_PRESS_MS) {
      longFired = true;
      tapPending = false;
      return TouchEvent::LONG_PRESS;
    }
    return TouchEvent::NONE;
  }

  if (!touching) return TouchEvent::NONE;

  // Lifted: the last reported point while down is the end of the gesture
  touching = false;
  if (longFired) return TouchEvent::NONE;  // Already reported
  TouchEvent event = classify();
  if (event != TouchEvent::TAP || !doubleTap) {
    tapPending = false;
    return event;
  }
  if (tapPending && millis() - tsTap <= TOUCH_DOUBLE_TAP_MS) {
    tapPending = false;
    return TouchEvent::DOUBLE_TAP;
  }
  tapPending = true;
  tsTap = millis();
  return TouchEvent::NONE;
}

#endif
