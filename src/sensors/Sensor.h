#pragma once

#include <Arduino.h>

#define SENSOR_RETRY_MS     5000    // Re-initialize an offline sensor this often
#define SENSOR_MAX_ERRORS   5       // Failed reads in a row before a sensor counts as offline
#define SENSOR_HISTORY      100     // Values kept per graph

// Chips the firmware can identify, in no persisted order
enum class SensorModel : uint8_t {
  INA219 = 0,   // Current / bus voltage, 1 channel
  INA3221,      // Current / bus voltage, 3 channels
  BME280,       // Temperature, humidity, pressure
  BMP280,       // Temperature, pressure (BME280 without humidity, same driver)
  BME680,       // Temperature, humidity, pressure, gas resistance
  BME688,       // BME680 with the high range gas sensor, same driver
  AHT10,        // Temperature, humidity
  AHT20,        // Temperature, humidity (also AHT21 / AHT25)
  COUNT
};

const char* SENSOR_modelName(SensorModel model);  // "INA219", shown on screen
const char* SENSOR_modelId(SensorModel model);    // "ina219", in JSON

// What a sensor measures, and so which page shows it
enum class SensorKind : uint8_t {
  POWER = 0,     // CPowerSensor: voltage / current / power channels
  ENVIRONMENT    // CEnvSensor: temperature and whichever of humidity / pressure / gas it has
};

// One chip at one I2C address, found by CSensors. Subclasses talk to the chip; this keeps track of
// whether it answers: SENSOR_MAX_ERRORS failed reads in a row take it offline, then it is
// re-initialized every SENSOR_RETRY_MS until it answers again (or CSensors drops it).
//
// Values are written from loop() only. Other tasks (the web server) read them without locking:
// single floats, so a torn read at worst mixes two consecutive samples.
class CSensor {

public:
  CSensor(SensorModel model, uint8_t address);
  virtual ~CSensor() {}

  uint16_t getId() const { return id; }   // Unique while running; pages refer to their sensor by it
  SensorModel getModel() const { return model; }
  const char* getName() const { return SENSOR_modelName(model); }
  uint8_t getAddress() const { return address; }
  virtual SensorKind getKind() const = 0;

  bool isOnline() const { return online; }
  bool hasValues() const { return online && valid; }  // Online and published since (re)connecting
  uint32_t getSample() const { return sample; }       // Bumped with every published value
  unsigned long getOfflineSince() const { return tsOffline; }

  // (Re)configure the chip and restart measuring; false when it doesn't answer as this model.
  // CSensors calls it once to identify the chip, loop() again while the sensor is offline.
  virtual bool init() = 0;

  // Every loop. True when the sensor went offline or came back.
  bool loop(unsigned long now);

protected:
  SensorModel model;   // A driver for a chip family can refine it in init() (AHT10 / AHT20)

  // Read or trigger as the chip needs, called every loop while online; false on an I2C error
  virtual bool run(unsigned long now) = 0;

  void published() { sample++; valid = true; }

private:
  uint16_t id;
  uint8_t address;
  bool online = true;        // Created only once init() succeeded
  bool valid = false;
  uint8_t errors = 0;
  uint32_t sample = 0;
  unsigned long tsRetry = 0;
  unsigned long tsOffline = 0;
};
