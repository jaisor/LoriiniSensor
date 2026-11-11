#pragma once

#include "Configuration.h"

#ifdef SENSORS

#include "sensors/INA219.h"

#define SENSOR_READ_MS      50      // Poll the chip (its own averaging covers ~35ms)
#define SENSOR_PUBLISH_MS   200     // Average the polls into a published value 5x a second
#define SENSOR_RETRY_MS     5000    // Look for a missing sensor again
#define SENSOR_MAX_ERRORS   5       // Failed reads in a row before the sensor counts as lost
#define SENSOR_HISTORY      100     // Published current values kept for the graph (20s)

enum class SensorStatus : uint8_t {
  OFF = 0,      // configuration.sensorType is SENSOR_NONE
  NOT_FOUND,    // Configured but not answering, retried every SENSOR_RETRY_MS
  OK
};

// Latest published values. Written from loop() only; the web server reads them without locking
// (single floats, a torn read at worst mixes two consecutive samples).
struct sensor_readings_t {
  bool valid;           // A value has been published since the sensor was found
  float voltage;        // V
  float current;        // A
  float power;          // W
  bool overRange;       // Some poll in this sample hit the end of the shunt range
  uint32_t sample;      // Increments with every published value
};

// Runs the sensor chosen in the configuration: finds it, polls it, averages and publishes readings
// and keeps a short history. Follows configuration changes (type, address, shunt) live.
class CSensors {

public:
  CSensors();

  void begin();   // Wire must be started
  // Returns true when the status changed (found, lost, switched off), for the device to redraw
  bool loop();

  SensorStatus getStatus() const { return status; }
  uint8_t getType() const { return type; }
  const char* getTypeName() const { return SENSOR_TYPE_NAMES[type]; }
  const sensor_readings_t& getReadings() const { return readings; }

  // Current history, oldest first: count values, i = 0..count-1
  uint8_t getHistoryCount() const { return historyCount; }
  float getHistory(uint8_t i) const;

private:
  uint8_t type;           // Applied configuration, compared to configuration each loop
  uint8_t address;
  uint16_t shuntMilliOhm;

  SensorStatus status;
  CINA219 ina219;
  uint8_t errors;
  unsigned long tsRead, tsPublish, tsRetry;

  // Polls since the last publish
  float sumVoltage, sumCurrent;
  uint8_t polls;
  bool pollOverRange;

  sensor_readings_t readings;
  float history[SENSOR_HISTORY];
  uint8_t historyStart, historyCount;

  bool configChanged() const;
  void apply();             // Take the configuration and (re)start the sensor
  bool probe();
  void publish();
  void resetSamples();
};

// Value and unit for the UI, scaled (mA / A, mW / W)
void SENSOR_formatCurrent(float amps, char *value, size_t valueSize, const char **unit);
void SENSOR_formatVoltage(float volts, char *value, size_t valueSize, const char **unit);
void SENSOR_formatPower(float watts, char *value, size_t valueSize, const char **unit);

#endif
