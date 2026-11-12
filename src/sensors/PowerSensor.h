#pragma once

#include "sensors/Sensor.h"

#define POWER_READ_MS       50      // Poll the chip (its own averaging covers about as long)
#define POWER_PUBLISH_MS    200     // Average the polls into a published value 5x a second
#define POWER_MAX_CHANNELS  3
#define POWER_HISTORY_MS    (SENSOR_HISTORY * POWER_PUBLISH_MS)   // Current graph span, 20s

// With nothing attached the inputs float and the shunt reading is noise (amps of swing), while the bus
// voltage reads ~0. A channel counts as connected from POWER_CONNECT_V of bus voltage, and as
// disconnected below POWER_DISCONNECT_V (the gap keeps it from flickering); disconnected, its current
// is taken as 0. A pull-down (~100k from VIN- to GND) keeps the inputs from floating in the first place.
#define POWER_CONNECT_V     0.5f
#define POWER_DISCONNECT_V  0.3f

struct power_reading_t {
  float voltage;    // V, bus voltage on the load side
  float current;    // A, positive into the load
  float power;      // W
  bool overRange;   // Shunt voltage at the end of its range in some poll: current is at least this
  bool connected;   // Bus voltage present (see POWER_CONNECT_V); when false, current and power are 0
};

// Current / voltage monitor with one or more channels. Polls every POWER_READ_MS, publishes the
// average every POWER_PUBLISH_MS and keeps SENSOR_HISTORY published currents per channel.
class CPowerSensor : public CSensor {

public:
  SensorKind getKind() const override { return SensorKind::POWER; }

  uint8_t getChannelCount() const { return channels; }
  const power_reading_t& getReading(uint8_t channel) const { return readings[channel]; }

  // Current history of a channel, oldest first: i = 0..getHistoryCount()-1
  uint8_t getHistoryCount() const { return historyCount; }
  float getHistory(uint8_t channel, uint8_t i) const;

protected:
  CPowerSensor(SensorModel model, uint8_t address, uint8_t channels);

  // The latest conversion of every channel (voltage, current, overRange; power is computed here)
  virtual bool readChannels(power_reading_t *out) = 0;

  bool run(unsigned long now) override;
  void restart();   // From init(): drop averages and history

private:
  uint8_t channels;
  power_reading_t readings[POWER_MAX_CHANNELS];
  float sumVoltage[POWER_MAX_CHANNELS], sumCurrent[POWER_MAX_CHANNELS];
  bool pollOverRange[POWER_MAX_CHANNELS];
  bool connected[POWER_MAX_CHANNELS];
  uint8_t polls = 0;
  unsigned long tsRead = 0, tsPublish = 0;

  float history[POWER_MAX_CHANNELS][SENSOR_HISTORY];
  uint8_t historyStart = 0, historyCount = 0;

  void resetAverages();
  void publish();
};
