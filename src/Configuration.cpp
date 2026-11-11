#include <Arduino.h>
#include <EEPROM.h>
#include <time.h>
#include <version.h>
#include "Configuration.h"

configuration_t configuration;
static_assert(EEPROM_CONFIGURATION_START + sizeof(configuration_t) <= 4096, "configuration_t no longer fits in EEPROM");

#ifdef WEB_LOGGING
  StringPrint logStream;

  void trimLogStream() {
    if (logStream.str().length() > WEB_LOG_MAX_SIZE) {
      // Keep only the most recent half of the buffer
      String s = logStream.str().c_str();
      logStream.str("");
      logStream.print(s.substring(s.length() - WEB_LOG_MAX_SIZE / 2));
    }
  }
#endif

uint8_t EEPROM_initAndCheckFactoryReset() {
  Log.noticeln("Configuration size: %i", sizeof(configuration_t));

  EEPROM.begin(sizeof(configuration_t) + EEPROM_CONFIGURATION_START);
  uint8_t resetCounter = EEPROM.read(EEPROM_FACTORY_RESET);

  Log.noticeln("Factory reset counter: %i", resetCounter);
  Log.noticeln("EEPROM length: %i", EEPROM.length());

  // Bump reset counter
  EEPROM.write(EEPROM_FACTORY_RESET, resetCounter + 1);
  EEPROM.commit();

  return resetCounter;
}

void EEPROM_clearFactoryReset() {
  EEPROM.write(EEPROM_FACTORY_RESET, 0);
  EEPROM.commit();
}

void EEPROM_saveConfig() {
  Log.info("Saving configuration to EEPROM ... ");
  EEPROM.put(EEPROM_CONFIGURATION_START, configuration);
  EEPROM.commit();
  Log.verboseln("Saved");
}

void EEPROM_loadConfig() {

  configuration = {};
  EEPROM.get(EEPROM_CONFIGURATION_START, configuration);

  Log.noticeln("Configuration loaded: %s", configuration._loaded);

  if (strncmp(configuration._loaded, "lsens1", sizeof(configuration._loaded))) {
    // Blank, or written by other firmware - start from defaults
    Log.infoln("Blank configuration, loading defaults");
    configuration = {};
    strcpy(configuration._loaded, "lsens1");
    strcpy(configuration.name, DEVICE_NAME);
    #ifdef WIFI
      strcpy(configuration.ntpServer, NTP_SERVER);
      configuration.gmtOffset_sec = NTP_GMT_OFFSET_SEC;
      configuration.daylightOffset_sec = NTP_DAYLIGHT_OFFSET_SEC;
      configuration.wifiPower = WIFI_DEFAULT_POWER;
    #endif
    #ifdef SCREEN
      configuration.screenBrightness = SCREEN_BRIGHTNESS;
    #endif
  }

  if (strncmp(configuration._buzzerLoaded, "bz1", sizeof(configuration._buzzerLoaded))) {
    Log.infoln("No buzzer configuration, loading defaults");
    strcpy(configuration._buzzerLoaded, "bz1");
    configuration.buzzerVolume = BUZZER_VOLUME;
  }
  if (configuration.buzzerVolume > 100) {
    Log.verboseln("Invalid buzzerVolume");
    configuration.buzzerVolume = BUZZER_VOLUME;
  }

  if (strncmp(configuration._sensorLoaded, "sn1", sizeof(configuration._sensorLoaded))) {
    Log.infoln("No sensor configuration, loading defaults");
    strcpy(configuration._sensorLoaded, "sn1");
    configuration.ina219ShuntMilliOhm = INA219_DEFAULT_SHUNT_MOHM;
  }
  if (configuration.ina219ShuntMilliOhm == 0 || configuration.ina219ShuntMilliOhm > INA_MAX_SHUNT_MOHM) {
    Log.verboseln("Invalid ina219ShuntMilliOhm");
    configuration.ina219ShuntMilliOhm = INA219_DEFAULT_SHUNT_MOHM;
  }

  if (strncmp(configuration._sensor2Loaded, "sn2", sizeof(configuration._sensor2Loaded))) {
    Log.infoln("No INA3221 configuration, loading defaults");
    strcpy(configuration._sensor2Loaded, "sn2");
    configuration.ina3221ShuntMilliOhm = INA3221_DEFAULT_SHUNT_MOHM;
  }
  if (configuration.ina3221ShuntMilliOhm == 0 || configuration.ina3221ShuntMilliOhm > INA_MAX_SHUNT_MOHM) {
    Log.verboseln("Invalid ina3221ShuntMilliOhm");
    configuration.ina3221ShuntMilliOhm = INA3221_DEFAULT_SHUNT_MOHM;
  }

  // Strings come straight from flash, make sure they end
  configuration.name[sizeof(configuration.name) - 1] = '\0';
  if (!strlen(configuration.name)) {
    strcpy(configuration.name, DEVICE_NAME);
  }

#ifdef WIFI
  configuration.wifiSsid[sizeof(configuration.wifiSsid) - 1] = '\0';
  configuration.wifiPassword[sizeof(configuration.wifiPassword) - 1] = '\0';
  configuration.ntpServer[sizeof(configuration.ntpServer) - 1] = '\0';
  for (const char *p = configuration.wifiSsid; *p; p++) {
    if (!isAscii(*p)) {
      Log.verboseln("Bad SSID, clearing: %s", configuration.wifiSsid);
      strcpy(configuration.wifiSsid, "");
      break;
    }
  }
#endif

#ifdef SCREEN
  if (configuration.screenBrightness < SCREEN_MIN_BRIGHTNESS || configuration.screenBrightness > 100) {
    Log.verboseln("Invalid screenBrightness");
    configuration.screenBrightness = SCREEN_BRIGHTNESS;
  }
#endif

  Log.noticeln("Device name: %s", configuration.name);
  Log.noticeln("Version: %s", VERSION);
}

void EEPROM_wipe() {
  Log.warningln("Wiping configuration with size %i!", EEPROM.length());
  for (uint16_t i = 0; i < EEPROM.length(); i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

uint32_t CONFIG_getDeviceId() {
  uint32_t chipId = 0;
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  return chipId;
}

String CONFIG_getChipModel() {
  return ESP.getChipModel();
}

uint8_t CONFIG_getChipRevision() {
  return ESP.getChipRevision();
}

uint32_t CONFIG_getFlashChipSize() {
  return ESP.getFlashChipSize();
}

static unsigned long tMillisUp = millis();
unsigned long CONFIG_getUpTime() {
  return millis() - tMillisUp;
}

bool CONFIG_isTimeSynced() {
  // The clock starts at the epoch; anything after late 2023 came from an NTP answer
  return time(nullptr) > 1700000000;
}

// { gmtOffset_sec, POSIX TZ string, human-readable label }
const timezone_entry_t TIMEZONES[] = {
  { -43200, "UTC12",                              "(UTC-12:00) Baker Island" },
  { -39600, "SST11",                              "(UTC-11:00) American Samoa" },
  { -36000, "HST10",                              "(UTC-10:00) Hawaii" },
  { -32400, "AKST9AKDT,M3.2.0,M11.1.0",           "(UTC-09:00) Alaska" },
  { -28800, "PST8PDT,M3.2.0,M11.1.0",             "(UTC-08:00) Pacific Time (US & Canada)" },
  { -25200, "MST7MDT,M3.2.0,M11.1.0",             "(UTC-07:00) Mountain Time (US & Canada)" },
  { -21600, "CST6CDT,M3.2.0,M11.1.0",             "(UTC-06:00) Central Time (US & Canada)" },
  { -18000, "EST5EDT,M3.2.0,M11.1.0",             "(UTC-05:00) Eastern Time (US & Canada)" },
  { -14400, "AST4ADT,M3.2.0,M11.1.0",             "(UTC-04:00) Atlantic Time (Canada)" },
  { -12600, "NST3:30NDT,M3.2.0,M11.1.0",          "(UTC-03:30) Newfoundland" },
  { -10800, "BRT3",                               "(UTC-03:00) Buenos Aires, Brasilia" },
  {  -7200, "FNT2",                               "(UTC-02:00) Mid-Atlantic" },
  {  -3600, "AZOT1AZOST,M3.5.0/0,M10.5.0/1",      "(UTC-01:00) Azores" },
  {      0, "GMT0",                               "(UTC+00:00) GMT / London, Dublin" },
  {   3600, "CET-1CEST,M3.5.0,M10.5.0/3",         "(UTC+01:00) Central European Time" },
  {   7200, "EET-2EEST,M3.5.0/3,M10.5.0/4",       "(UTC+02:00) Eastern European Time" },
  {  10800, "MSK-3",                              "(UTC+03:00) Moscow, Istanbul" },
  {  12600, "IRST-3:30IRDT,J79/24,J263/24",       "(UTC+03:30) Tehran" },
  {  14400, "GST-4",                              "(UTC+04:00) Dubai, Baku" },
  {  16200, "AFT-4:30",                           "(UTC+04:30) Kabul" },
  {  18000, "PKT-5",                              "(UTC+05:00) Karachi, Tashkent" },
  {  19800, "IST-5:30",                           "(UTC+05:30) Mumbai, New Delhi" },
  {  20700, "NPT-5:45",                           "(UTC+05:45) Kathmandu" },
  {  21600, "BST-6",                              "(UTC+06:00) Dhaka, Almaty" },
  {  23400, "MMT-6:30",                           "(UTC+06:30) Yangon" },
  {  25200, "ICT-7",                              "(UTC+07:00) Bangkok, Jakarta" },
  {  28800, "CST-8",                              "(UTC+08:00) Beijing, Singapore, Perth" },
  {  32400, "JST-9",                              "(UTC+09:00) Tokyo, Seoul" },
  {  34200, "ACST-9:30ACDT,M10.1.0,M4.1.0/3",     "(UTC+09:30) Adelaide, Darwin" },
  {  36000, "AEST-10AEDT,M10.1.0,M4.1.0/3",       "(UTC+10:00) Sydney, Melbourne" },
  {  39600, "SBT-11",                             "(UTC+11:00) Solomon Islands" },
  {  43200, "NZST-12NZDT,M9.5.0,M4.1.0/3",        "(UTC+12:00) Auckland, Fiji" },
  {  46800, "TOT-13",                             "(UTC+13:00) Tonga, Samoa" },
};
const int TIMEZONE_COUNT = sizeof(TIMEZONES) / sizeof(TIMEZONES[0]);

const char* CONFIG_getTzString(long gmtOffset_sec) {
  for (int i = 0; i < TIMEZONE_COUNT; i++) {
    if (TIMEZONES[i].offset == gmtOffset_sec) {
      return TIMEZONES[i].posix;
    }
  }
  // Offsets not in the table: POSIX signs are inverted
  static char tzStr[16];
  snprintf(tzStr, sizeof(tzStr), "UTC%+ld", -gmtOffset_sec / 3600);
  return tzStr;
}
