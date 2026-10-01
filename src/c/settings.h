#pragma once
#include <pebble.h>
#include "sidebar_widgets.h"

#define CURRENT_SETTINGS_VERSION 8

// persistent storage keys
#define SETTINGS_PERSIST_KEY 100
#define SETTINGS_VERSION_PERSIST_KEY 4
#define MIGRATION_EMERY_FONT_PERSIST_KEY 200

#define FONT_SETTING_DEFAULT 0
#define FONT_SETTING_LECO    1
#define FONT_SETTING_BOLD    2
#define FONT_SETTING_BOLD_H  3
#define FONT_SETTING_BOLD_M  4

typedef enum {
  NO_VIBE = 0,
  VIBE_EVERY_HOUR = 1,
  VIBE_EVERY_HALF_HOUR = 2
} VibeIntervalType;

// which minutes the seconds row is shown in (the pattern)
typedef enum {
  SECONDS_ALWAYS = 0,
  SECONDS_HOURLY = 1,        // :59
  SECONDS_HALF_HOURLY = 2,   // :29 and :59 (original behavior)
  SECONDS_QUARTER_HOURLY = 3,// :14, :29, :44, :59
  SECONDS_NEVER = 4
} SecondsPatternType;

// when (during the day) the seconds row may be shown
typedef enum {
  HOURS_ALL_DAY = 0,
  HOURS_DAYTIME = 1,   // 07:00 - 22:00
  HOURS_CUSTOM = 2
} ActiveHoursType;

#define DAYTIME_START_MIN (7 * 60)
#define DAYTIME_END_MIN (22 * 60)

// defaults = the "Classic" preset
#define DEFAULT_SECONDS_PATTERN SECONDS_HALF_HOURLY
#define DEFAULT_SHAKE_DURATION_SEC 30
#define DEFAULT_SECONDS_BATTERY_MIN 0

// Settings struct -- note, all new settings should ALWAYS be added to the bottom
typedef struct {
  // color settings
  GColor timeColor;
  GColor timeBgColor;
  GColor sidebarColor;
  GColor sidebarTextColor;

  // general settings
  uint8_t languageId;
  bool showLeadingZero;
  uint8_t clockFontId;

  // vibration settings
  bool btVibe;
  VibeIntervalType hourlyVibe;

  // sidebar settings
  SidebarWidgetType widgets[3];
  bool sidebarOnLeft;
  bool useLargeFonts;
  bool activateDisconnectIcon;
  
  // weather widget settings
  bool useMetric;

  // battery meter widget settings
  bool showBatteryPct;
  bool disableAutobattery;

  // alt tz widget settings
  char altclockName[8];
  int altclockOffset;

  // health widget Settings
  bool healthUseDistance;
  bool healthUseRestfulSleep;
  char decimalSeparator;

  // seconds settings (added in settings v8)
  uint8_t secondsMode;             // SecondsPatternType
  uint8_t activeHoursMode;         // ActiveHoursType
  uint16_t activeStartMin;         // custom window start, minutes since midnight
  uint16_t activeEndMin;           // custom window end (may be before start: crosses midnight)
  uint8_t secondsBatteryMin;       // hide seconds at or below this % (0 = off)
  bool hideSecondsInQuietTime;
  bool shakeToShow;                // a shake shows seconds, overriding everything else
  uint16_t shakeDurationSec;       // how long seconds stay visible after a shake
} Settings;

// Dynamic settings (calculated at runtime based on currently-selected widgets)
typedef struct {
  bool disableWeather;
  bool updateScreenEverySecond;
  bool enableAutoBatteryWidget;
  bool enableBeats;
  bool enableAltTimeZone;
  
  GColor iconFillColor;
  GColor iconStrokeColor;
} DynamicSettings;

// Legacy packed settings struct
// this is now deprecated, and will be removed in the next version
typedef struct {
  GColor timeColor;
  GColor timeBgColor;
  GColor sidebarColor;
  GColor sidebarTextColor;

  // general settings
  uint8_t languageId;
  uint8_t showLeadingZero:1;
  uint8_t clockFontId:7;

  // vibration settings
  uint8_t btVibe:1;
  int8_t hourlyVibe:7;

  // sidebar settings
  uint8_t widgets[3];
  uint8_t sidebarOnLeft:1;
  uint8_t useLargeFonts:1;

  // weather widget settings
  uint8_t useMetric:1;

  // battery meter widget settings
  uint8_t showBatteryPct:1;
  uint8_t disableAutobattery:1;

  // health widget Settings
  uint8_t healthUseDistance:1;
  uint8_t healthUseRestfulSleep:1;
  char decimalSeparator;

  // alt tz widget settings
  char altclockName[8];
  int8_t altclockOffset;

  // bluetooth disconnection icon
  int8_t activateDisconnectIcon:1;
} LegacyStoredSettings;

extern Settings settings;
extern DynamicSettings dynamicSettings;

void Settings_init();
void Settings_deinit();
void Settings_loadFromStorage();
void Settings_saveToStorage();
void Settings_updateDynamicSettings();
