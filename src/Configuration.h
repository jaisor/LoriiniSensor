#pragma once

#include <Arduino.h>
#include <functional>
#include <ArduinoLog.h>
#include <StreamUtils.h>

#include "Board.h"

#define WIFI        // 2.4Ghz WiFi, web server and OTA
#define SCREEN      // On-board LCD
#define BUZZER      // On-board passive buzzer, button sounds
#define TOUCH       // CST816T touch panel (swipes in the carousel)
#define SENSORS     // External sensors on the I2C bus (INA219)

//#define DISABLE_LOGGING
#ifndef DISABLE_LOGGING
  #define LOG_LEVEL LOG_LEVEL_VERBOSE
#endif

#define WEB_LOGGING // When enabled log is available at http://<device_ip>/log
#ifdef WEB_LOGGING
  #define WEB_LOG_LEVEL LOG_LEVEL_VERBOSE
  #define WEB_LOG_MAX_SIZE 8192  // Cap log buffer to 8KB to prevent heap exhaustion
#endif

#define SERIAL_MONITOR_BAUD 115200  // Native USB CDC ignores it, kept for a UART bridge

#define EEPROM_FACTORY_RESET 0           // Byte used to count boots that fail to reach FACTORY_RESET_CLEAR_TIMER_MS
#define EEPROM_CONFIGURATION_START 1     // First EEPROM byte used for storing the configuration
#define FACTORY_RESET_CLEAR_TIMER_MS 2000   // Clear factory reset counter when elapsed, considered smooth boot

#define DEVICE_NAME "LoriiniSensor"

#ifdef WIFI
  // If unable to connect, it will create a soft access point
  #define WIFI_FALLBACK_SSID DEVICE_NAME // device chip id will be suffixed
  #define WIFI_FALLBACK_PASS "password123"
  #define WIFI_DEFAULT_POWER 78          // WIFI_POWER_19_5dBm, see CWifiManager::connect()

  #define NTP_SERVER "pool.ntp.org"
  #define NTP_GMT_OFFSET_SEC 0  // GMT (default on factory reset)
  #define NTP_DAYLIGHT_OFFSET_SEC 0  // Deprecated, POSIX TZ strings handle DST

  #define WEB_SERVER_PORT 80
#endif

#ifdef SCREEN
  #define SCREEN_BRIGHTNESS 80      // Backlight percent on factory reset
  #define SCREEN_MIN_BRIGHTNESS 5   // Lowest value the web UI allows (0 would look like a dead device)
  #define SCREEN_SAFE_INSET 14      // Side margin for panels; corners are rounded (~43px radius) so keep text centered
#endif

#define BUZZER_VOLUME 50            // Percent on factory reset, 0 mutes

// Sensor types, persisted (configuration.sensorType) - append only
#define SENSOR_NONE   0
#define SENSOR_INA219 1             // Current / voltage / power monitor
#define SENSOR_TYPE_COUNT 2

#define INA219_ADDRESS_MIN 0x40     // A0/A1 straps select 0x40-0x4F
#define INA219_ADDRESS_MAX 0x4F
#define INA219_DEFAULT_ADDRESS 0x40
#define INA219_DEFAULT_SHUNT_MOHM 100   // The common breakout boards use an R100 (0.1 ohm) shunt
#define INA219_MAX_SHUNT_MOHM 10000

// Persisted with a raw EEPROM.put - see "Conventions" in CLAUDE.md before changing it.
// Fields are only ever appended, in blocks after _loaded with their own marker.
struct configuration_t {

  #ifdef WIFI
    char wifiSsid[32];
    char wifiPassword[63];
    int8_t wifiPower;

    // ntp
    char ntpServer[128];
    long gmtOffset_sec;
    int daylightOffset_sec;
  #endif

  char name[128];

  uint8_t screenBrightness;  // Backlight percent, SCREEN_MIN_BRIGHTNESS..100

  char _loaded[7]; // "lsens1" once the configuration is initialized

  // Blocks added later, each with its own marker; EEPROM_loadConfig() sets defaults when it's missing
  uint8_t buzzerVolume;      // Button sounds, 0 (off) - 100
  char _buzzerLoaded[4];     // "bz1"

  uint8_t sensorType;            // SENSOR_*
  uint8_t ina219Address;         // INA219_ADDRESS_MIN..MAX
  uint16_t ina219ShuntMilliOhm;  // Shunt resistor, 1..INA219_MAX_SHUNT_MOHM
  char _sensorLoaded[4];         // "sn1"
};

extern const char * const SENSOR_TYPE_NAMES[];  // Indexed by SENSOR_*, shown in the UI
extern const char * const SENSOR_TYPE_IDS[];    // Indexed by SENSOR_*, used in JSON
int CONFIG_sensorTypeFromId(const char *id);    // -1 if unknown

extern configuration_t configuration;
#ifdef WEB_LOGGING
  extern StringPrint logStream;
  void trimLogStream();
#endif

uint8_t EEPROM_initAndCheckFactoryReset();
void EEPROM_clearFactoryReset();

void EEPROM_saveConfig();
void EEPROM_loadConfig();
void EEPROM_wipe();

uint32_t CONFIG_getDeviceId();
unsigned long CONFIG_getUpTime();

String CONFIG_getChipModel();
uint8_t CONFIG_getChipRevision();
uint32_t CONFIG_getFlashChipSize();

bool CONFIG_isTimeSynced();  // The clock holds real time, i.e. NTP has answered since boot
const char* CONFIG_getTzString(long gmtOffset_sec);  // POSIX TZ for a UTC offset, see Configuration.cpp

struct timezone_entry_t {
  long offset;
  const char* posix;
  const char* label;
};
extern const timezone_entry_t TIMEZONES[];
extern const int TIMEZONE_COUNT;
