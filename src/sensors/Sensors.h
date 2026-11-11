#pragma once

#include "Configuration.h"

#ifdef SENSORS

#include <functional>
#include <mutex>
#include "sensors/Sensor.h"
#include "sensors/PowerSensor.h"
#include "sensors/EnvSensor.h"

#define SENSOR_MAX          8       // Sensors running at once (each gets a page)
#define SENSOR_SCAN_MS      5000    // Look for newly connected sensors this often
#define SENSOR_REMOVE_MS    60000   // Drop a sensor that has been offline this long
#define SENSOR_IDENTIFY_TRIES 3     // Scans that try to identify a device before it's left alone
#define SENSOR_SCAN_ADDRESSES 20

// Finds the supported sensors on the I2C bus and runs them, any number of each up to SENSOR_MAX.
//
// The bus is scanned at boot and every SENSOR_SCAN_MS after, so sensors can be plugged in while
// running. Only the addresses the supported chips can have are probed (0x38-0x39, 0x40-0x4F,
// 0x76-0x77). Where the chips have ID registers, they are read before anything is written:
//   0x38-0x39  AHT10 / AHT20: soft reset, then the calibration status; the CRC only the AHT20 sends
//              tells them apart
//   0x40-0x43  INA3221 (TI manufacturer and die ID), else INA219
//   0x40-0x4F  INA219: no ID register, so a device without TI's IDs is reset and has to take and
//              read back the INA219 configuration (other chips at 0x40, e.g. a PCA9685, get written to)
//   0x76-0x77  BME280 / BMP280 / BME680 / BME688 (chip ID, BME68x variant ID)
// A device that answers but isn't identified in SENSOR_IDENTIFY_TRIES scans is left alone until it
// disappears from the bus.
// A sensor that stops answering stays (offline, retried) for SENSOR_REMOVE_MS, then is dropped.
//
// Sensors are kept sorted by address. The list only changes in loop(); other tasks go through
// forEach(), which holds the list lock.
class CSensors {

public:
  CSensors();
  ~CSensors();

  void begin();   // Wire must be started: scans the bus
  // True when a sensor was added, dropped, went offline or came back, for the device to redraw
  bool loop();

  // loop() task only (the display)
  uint8_t getCount() const { return count; }
  const CSensor* get(uint8_t i) const { return i < count ? sensors[i] : nullptr; }
  const CSensor* find(uint16_t id) const;   // nullptr once the sensor is gone
  uint32_t getListRevision() const { return listRevision; }   // Bumped when sensors are added or dropped

  // Any task: calls f for each sensor, in order, with the list locked
  void forEach(std::function<void(const CSensor&)> f) const;

private:
  CSensor *sensors[SENSOR_MAX];
  uint8_t count;
  uint32_t listRevision;
  uint8_t failures[SENSOR_SCAN_ADDRESSES];   // Failed identifications per scanned address, while it answers
  unsigned long tsScan;
  mutable std::mutex lock;

  bool scan();               // True when sensors were added
  bool owned(uint8_t address) const;
  CSensor* identify(uint8_t address);
  CSensor* tryInit(CSensor *candidate);   // Deletes it when init() fails
  void add(CSensor *sensor);
  void remove(uint8_t index);
};

// Value and unit for the UI, scaled (mA / A, mW / W, ...). Units starting with '^' are degrees: the
// fonts have no degree sign, so the UI draws a ring for it (UI_drawValue()).
void SENSOR_formatCurrent(float amps, char *value, size_t valueSize, const char **unit);
void SENSOR_formatVoltage(float volts, char *value, size_t valueSize, const char **unit);
void SENSOR_formatPower(float watts, char *value, size_t valueSize, const char **unit);
void SENSOR_formatEnv(EnvValue v, float x, char *value, size_t valueSize, const char **unit);

#endif
