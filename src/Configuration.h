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
#define SENSORS     // External sensors on the I2C bus, detected (INA219, INA3221, BME280, BME68x, AHT10/20)
#define MQTT        // Publish to an MQTT broker, with Home Assistant discovery (needs WIFI)

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
#else
  #undef MQTT
#endif

#ifdef MQTT
  // Off until a server is set on the MQTT web page. Topics live under <mqttTopic>/<device id>.
  #define MQTT_PORT 1883
  #define MQTT_TOPIC "loriini_sensor"
  #define MQTT_DISCOVERY_PREFIX "homeassistant"   // Home Assistant's default discovery prefix
  #define MQTT_INTERVAL_SEC 10        // Sensor values, averaged over this long, on factory reset
  #define MQTT_MAX_INTERVAL_SEC 3600
  #define MQTT_BUFFER_SIZE 2048       // Largest incoming message: configuration JSON, our own discovery
#endif

#ifdef SCREEN
  #define SCREEN_BRIGHTNESS 80      // Backlight percent on factory reset
  #define SCREEN_MIN_BRIGHTNESS 5   // Lowest value the web UI allows (0 would look like a dead device)
  #define SCREEN_SAFE_INSET 14      // Side margin for panels; corners are rounded (~43px radius) so keep text centered
#endif

#define BUZZER_VOLUME 50            // Percent on factory reset, 0 mutes

// Screen saver: without a button press or touch for this long the screen goes dark, dimming to half
// brightness halfway there. 0 turns it off.
#define SCREEN_SAVER_SEC 300        // On factory reset
#define SCREEN_SAVER_MIN_SEC 10
#define SCREEN_SAVER_MAX_SEC 3600

// Power monitor shunt resistors, the same for every chip of a model (and every INA3221 channel)
#define INA219_DEFAULT_SHUNT_MOHM 100   // The common breakout boards use an R100 (0.1 ohm) shunt
#define INA3221_DEFAULT_SHUNT_MOHM 100
#define INA_MAX_SHUNT_MOHM 10000

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

  uint8_t _unusedSensorType;     // Sensor type and INA219 address before sensors were detected,
  uint8_t _unusedIna219Address;  // kept for the layout
  uint16_t ina219ShuntMilliOhm;  // Shunt resistor, 1..INA_MAX_SHUNT_MOHM
  char _sensorLoaded[4];         // "sn1"

  uint16_t ina3221ShuntMilliOhm; // Shunt resistors of all channels, 1..INA_MAX_SHUNT_MOHM
  char _sensor2Loaded[4];        // "sn2"

  // MQTT, in the layout whether or not MQTT is compiled in
  char mqttServer[128];          // Blank turns MQTT off
  uint16_t mqttPort;
  char mqttUser[64];             // Blank connects anonymously
  char mqttPassword[64];
  char mqttTopic[64];            // Base topic, the device id is appended
  uint8_t mqttDiscovery;         // Home Assistant discovery
  uint16_t mqttIntervalSec;      // Sensor values every, 1..MQTT_MAX_INTERVAL_SEC
  char _mqttLoaded[4];           // "mq1"

  uint16_t screenSaverSec;       // 0 (never) or SCREEN_SAVER_MIN_SEC..SCREEN_SAVER_MAX_SEC
  char _screenSaverLoaded[4];    // "ss1"
};

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

// Screen saver durations offered by the Settings screen and the web page, ascending, 0 = off
extern const uint16_t SCREEN_SAVER_PRESETS[];
extern const uint8_t SCREEN_SAVER_PRESET_COUNT;
bool CONFIG_isValidScreenSaver(long sec);
void CONFIG_formatDuration(uint16_t sec, char *out, size_t size);   // "Off", "30 s", "5 min", "1 h"

bool CONFIG_isTimeSynced();  // The clock holds real time, i.e. NTP has answered since boot
const char* CONFIG_getTzString(long gmtOffset_sec);  // POSIX TZ for a UTC offset, see Configuration.cpp

struct timezone_entry_t {
  long offset;
  const char* posix;
  const char* label;
};
extern const timezone_entry_t TIMEZONES[];
extern const int TIMEZONE_COUNT;
