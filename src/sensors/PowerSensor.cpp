#include "sensors/PowerSensor.h"

CPowerSensor::CPowerSensor(SensorModel model, uint8_t address, uint8_t channels)
: CSensor(model, address), channels(min<uint8_t>(channels, POWER_MAX_CHANNELS)), readings{}, history{} {
  resetAverages();
}

float CPowerSensor::getHistory(uint8_t channel, uint8_t i) const {
  if (channel >= channels || i >= historyCount) return 0;
  return history[channel][(historyStart + i) % SENSOR_HISTORY];
}

void CPowerSensor::resetAverages() {
  for (uint8_t c = 0; c < POWER_MAX_CHANNELS; c++) {
    sumVoltage[c] = sumCurrent[c] = 0;
    pollOverRange[c] = false;
  }
  polls = 0;
}

void CPowerSensor::restart() {
  resetAverages();
  historyStart = historyCount = 0;
  tsRead = tsPublish = millis();
}

void CPowerSensor::publish() {
  if (polls == 0) return;
  const uint8_t slot = (historyStart + historyCount) % SENSOR_HISTORY;
  for (uint8_t c = 0; c < channels; c++) {
    power_reading_t &r = readings[c];
    r.voltage = sumVoltage[c] / polls;
    r.current = sumCurrent[c] / polls;
    r.power = r.voltage * r.current;
    r.overRange = pollOverRange[c];
    history[c][slot] = r.current;
  }
  if (historyCount < SENSOR_HISTORY) {
    historyCount++;
  } else {
    historyStart = (historyStart + 1) % SENSOR_HISTORY;
  }
  resetAverages();
  published();
}

bool CPowerSensor::run(unsigned long now) {
  if (now - tsRead >= POWER_READ_MS) {
    tsRead = now;
    power_reading_t r[POWER_MAX_CHANNELS] = {};
    if (!readChannels(r)) {
      return false;
    }
    for (uint8_t c = 0; c < channels; c++) {
      sumVoltage[c] += r[c].voltage;
      sumCurrent[c] += r[c].current;
      pollOverRange[c] |= r[c].overRange;
    }
    polls++;
  }
  if (now - tsPublish >= POWER_PUBLISH_MS) {
    tsPublish = now;
    publish();
  }
  return true;
}
