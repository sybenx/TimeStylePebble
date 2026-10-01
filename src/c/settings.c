#include "settings.h"
#include <pebble.h>

Settings settings;
DynamicSettings dynamicSettings;

void Settings_init() {
  Settings_loadFromStorage();

  // for anyone who already installed timestyle on an emery, reset
  // their large font setting, since the old "large" setting is
  // roughly equivalent to the new default, and the new "large"
  // setting is EVEN LARGER
#if defined(PBL_PLATFORM_EMERY) || defined(PBL_PLATFORM_GABBRO)
  if (!persist_exists(MIGRATION_EMERY_FONT_PERSIST_KEY)) {
    if (settings.useLargeFonts) {
      settings.useLargeFonts = false;
      Settings_saveToStorage();
    }
    persist_write_bool(MIGRATION_EMERY_FONT_PERSIST_KEY, true);
  }
#endif
}

void Settings_deinit() { Settings_saveToStorage(); }

void Settings_loadFromStorage() {
  // Set defaults
  settings.timeBgColor = GColorBlack;
  settings.sidebarTextColor = GColorBlack;

#ifdef PBL_COLOR
  settings.timeColor = GColorOrange;
  settings.sidebarColor = GColorOrange;
#else
  settings.timeColor = GColorWhite;
  settings.sidebarColor = GColorWhite;
#endif

  settings.widgets[0] = WEATHER_CURRENT;
  settings.widgets[1] = EMPTY;
  settings.widgets[2] = DATE;

  settings.activateDisconnectIcon = true;
  strncpy(settings.altclockName, "ALT", sizeof(settings.altclockName));
  settings.altclockOffset = 0;
  settings.decimalSeparator = '.';
  settings.showBatteryPct = true;

  settings.secondsMode = DEFAULT_SECONDS_PATTERN;
  settings.activeHoursMode = HOURS_ALL_DAY;
  settings.activeStartMin = DAYTIME_START_MIN;
  settings.activeEndMin = DAYTIME_END_MIN;
  settings.secondsBatteryMin = DEFAULT_SECONDS_BATTERY_MIN;
  settings.hideSecondsInQuietTime = true;
  settings.shakeToShow = false;
  settings.shakeDurationSec = DEFAULT_SHAKE_DURATION_SEC;

  // to correct settings migration bug (settings key v6), we must do another
  // migration (nooooooooooo)
  if (persist_exists(SETTINGS_PERSIST_KEY)) {
    int version = 0;

    if (persist_exists(SETTINGS_VERSION_PERSIST_KEY)) {
      version = persist_read_int(SETTINGS_VERSION_PERSIST_KEY);

      if (version == 7) {
        // v7 settings: no seconds fields yet. only read the bytes that existed,
        // so the new fields keep the defaults set above
        persist_read_data(SETTINGS_PERSIST_KEY, &settings, offsetof(Settings, secondsMode));
        settings.altclockName[sizeof(settings.altclockName) - 1] = '\0';

        // re-save in new v8 format
        Settings_saveToStorage();
      } else if (version < CURRENT_SETTINGS_VERSION) {
        // v6 settings: load via memcpy
        LegacyStoredSettings s;
        persist_read_data(SETTINGS_PERSIST_KEY, &s, sizeof(s));
        memcpy(&settings, &s, sizeof(LegacyStoredSettings));

        // re-save in the current format
        Settings_saveToStorage();
      } else {
        // v8 settings: load directly via single persist read
        persist_read_data(SETTINGS_PERSIST_KEY, &settings, sizeof(settings));
        settings.altclockName[sizeof(settings.altclockName) - 1] = '\0';
      }
    }
  }

  // sanitize the seconds settings in case of corrupt/unexpected values
  if (settings.secondsMode > SECONDS_NEVER) {
    settings.secondsMode = DEFAULT_SECONDS_PATTERN;
  }
  if (settings.activeHoursMode > HOURS_CUSTOM) {
    settings.activeHoursMode = HOURS_ALL_DAY;
  }
  if (settings.activeStartMin >= 24 * 60 || settings.activeEndMin >= 24 * 60) {
    settings.activeStartMin = DAYTIME_START_MIN;
    settings.activeEndMin = DAYTIME_END_MIN;
  }
  if (settings.secondsBatteryMin > 100) {
    settings.secondsBatteryMin = DEFAULT_SECONDS_BATTERY_MIN;
  }
  if (settings.shakeDurationSec < 5 || settings.shakeDurationSec > 600) {
    settings.shakeDurationSec = DEFAULT_SHAKE_DURATION_SEC;
  }

  Settings_updateDynamicSettings();
}

void Settings_saveToStorage() {
  Settings_updateDynamicSettings();

  // We're limited to 256 bytes, so make sure that settings fits
  _Static_assert(sizeof(Settings) <= PERSIST_DATA_MAX_LENGTH,
                 "Warning: settings struct is too large!");
  // APP_LOG(APP_LOG_LEVEL_DEBUG, "Current settings size %d", sizeof(settings));

  // Write the data
  persist_write_data(SETTINGS_PERSIST_KEY, &settings, sizeof(settings));
  persist_write_int(SETTINGS_VERSION_PERSIST_KEY, CURRENT_SETTINGS_VERSION);
}

void Settings_updateDynamicSettings() {
  dynamicSettings.disableWeather = true;
  dynamicSettings.updateScreenEverySecond = false;
  dynamicSettings.enableAutoBatteryWidget = true;
  dynamicSettings.enableBeats = false;
  dynamicSettings.enableAltTimeZone = false;

  for (int i = 0; i < 3; i++) {
    // if there are any weather widgets, enable weather checking
    if (settings.widgets[i] == WEATHER_CURRENT ||
        settings.widgets[i] == WEATHER_FORECAST_TODAY ||
        settings.widgets[i] == WEATHER_UV_INDEX) {
      dynamicSettings.disableWeather = false;
    }

    // if any widget is "seconds", we'll need to update the sidebar every second
    if (settings.widgets[i] == SECONDS) {
      dynamicSettings.updateScreenEverySecond = true;
    }

    // if any widget is "battery", disable the automatic battery indication
    if (settings.widgets[i] == BATTERY_METER) {
      dynamicSettings.enableAutoBatteryWidget = false;
    }

    // if any widget is "beats", enable the beats calculation
    if (settings.widgets[i] == BEATS) {
      dynamicSettings.enableBeats = true;
    }

    // if any widget is "alt_time_zone", enable the alternative time calculation
    if (settings.widgets[i] == ALT_TIME_ZONE) {
      dynamicSettings.enableAltTimeZone = true;
    }
  }

  // if the sidebar is black, use inverted colors for icons
  if (gcolor_equal(settings.sidebarColor, GColorBlack)) {
    dynamicSettings.iconFillColor = GColorBlack;
    dynamicSettings.iconStrokeColor = settings.sidebarTextColor;
  } else {
    dynamicSettings.iconFillColor = GColorWhite;
    dynamicSettings.iconStrokeColor = GColorBlack;
  }
}
