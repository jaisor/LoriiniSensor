#pragma once

#include "sensors/Sensor.h"

#define ENV_HISTORY_MS          6000    // One graph point per this long, the average of the readings in it
#define ENV_HISTORY_SPAN_MS     ((unsigned long)SENSOR_HISTORY * ENV_HISTORY_MS)   // 10 min
#define ENV_CONVERSION_TIMEOUT_MS 1000  // A conversion still busy this long after it should be done fails

// What an environment sensor can measure
enum class EnvValue : uint8_t {
  TEMPERATURE = 0,  // C
  HUMIDITY,         // % relative
  PRESSURE,         // hPa
  GAS,              // Ohm, gas sensor (MOX) resistance: falls with VOCs in the air
  COUNT
};
#define ENV_VALUE_COUNT ((uint8_t)EnvValue::COUNT)
#define ENV_BIT(v) (1 << (uint8_t)(v))

const char* SENSOR_envValueName(EnvValue v);   // "Temperature"
const char* SENSOR_envValueId(EnvValue v);     // "temperature_c", in JSON

// Temperature and some of humidity / pressure / gas. Takes a reading every getInterval() ms (chips
// that convert on request are triggered, then read once the conversion is done) and keeps a graph
// history of SENSOR_HISTORY points, ENV_HISTORY_MS apart.
class CEnvSensor : public CSensor {

public:
  SensorKind getKind() const override { return SensorKind::ENVIRONMENT; }

  bool has(EnvValue v) const { return values & ENV_BIT(v); }
  float get(EnvValue v) const { return current[(uint8_t)v]; }   // NAN when unknown (or not measured)
  unsigned long getInterval() const { return interval; }

  // History of a value, oldest first: i = 0..getHistoryCount()-1, NAN where unknown
  uint8_t getHistoryCount() const { return historyCount; }
  float getHistory(EnvValue v, uint8_t i) const;

protected:
  CEnvSensor(SensorModel model, uint8_t address, uint8_t values, unsigned long intervalMs);

  enum class ReadResult : uint8_t { OK, BUSY, ERROR };

  // Start a conversion (chips measuring continuously have nothing to do), and how long it takes
  virtual bool startMeasurement() = 0;
  virtual unsigned long conversionMs() const = 0;
  // Fetch it into out[EnvValue], leaving NAN for what this reading doesn't have; BUSY to be asked again
  virtual ReadResult readMeasurement(float *out) = 0;

  bool run(unsigned long now) override;
  void restart();   // From init(): measure right away, drop the history
  void setValues(uint8_t mask) { values = mask; }

private:
  uint8_t values;
  unsigned long interval;
  float current[ENV_VALUE_COUNT];

  bool converting = false;
  bool first = true;
  unsigned long tsStart = 0;

  // Averages for the next history point
  float sum[ENV_VALUE_COUNT];
  uint8_t sumCount[ENV_VALUE_COUNT];
  unsigned long tsHistory = 0;
  float history[ENV_VALUE_COUNT][SENSOR_HISTORY];
  uint8_t historyStart = 0, historyCount = 0;

  void publish(const float *v, unsigned long now);
  void resetSums();
};
