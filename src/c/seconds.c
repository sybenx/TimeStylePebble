#include "seconds.h"
#include "settings.h"
#include <pebble.h>
#include <stddef.h>

#define SECONDS_PERSIST_KEY 310
#define SECONDS_SETTINGS_VERSION 1

#define DAYTIME_START_MIN (7 * 60)
#define DAYTIME_END_MIN (22 * 60)

#define MIN_SHAKE_DURATION_SEC 5
#define MAX_SHAKE_DURATION_SEC 600

// defaults = the "Classic" preset (must match src/pkjs/seconds_config.js)
#define DEFAULT_SECONDS_PATTERN SECONDS_HALF_HOURLY
#define DEFAULT_SHAKE_DURATION_SEC 30
#define DEFAULT_SECONDS_BATTERY_MIN 0

// persisted raw, so new fields go at the bottom and bump the version
typedef struct {
  uint8_t version;
  uint8_t secondsMode;       // SecondsPatternType
  uint8_t activeHoursMode;   // ActiveHoursType
  uint8_t secondsBatteryMin; // hide seconds at or below this % (0 = off)
  uint16_t activeStartMin;   // custom window start, minutes since midnight
  uint16_t activeEndMin;     // custom window end (may be before start:
                             // the window then crosses midnight)
  uint16_t shakeDurationSec; // how long seconds stay visible after a shake
  bool hideSecondsInQuietTime;
  bool shakeToShow; // a shake shows seconds, overriding everything else
} SecondsSettings;

static SecondsSettings s_settings;

// tick subscription state
static bool s_everySecond; // what the tick timer is subscribed to right now
static bool s_lastBase;    // the caller's own subscription, as last seen

// shake state
static AppTimer *s_shakeTimer;
static bool s_shakeActive;
static bool s_tapSubscribed;
static void (*s_refresh)(void);

/*
 * TimeStyle Seconds 7.13 saved its settings as upstream's 7.x settings
 * struct (36 bytes, settings version 7) with the seconds fields appended,
 * as settings version 8. These are those appended fields.
 */
#define LEGACY_SETTINGS_VERSION 8
#define LEGACY_SETTINGS_SIZE 48
#define LEGACY_SECONDS_OFFSET 36

typedef struct {
  uint8_t secondsMode;
  uint8_t activeHoursMode;
  uint16_t activeStartMin;
  uint16_t activeEndMin;
  uint8_t secondsBatteryMin;
  bool hideSecondsInQuietTime;
  bool shakeToShow;
  uint16_t shakeDurationSec;
} LegacySecondsFields;

_Static_assert(LEGACY_SECONDS_OFFSET + sizeof(LegacySecondsFields) ==
                   LEGACY_SETTINGS_SIZE,
               "LegacySecondsFields must match the 7.13 persisted layout");

static void set_defaults(void) {
  s_settings.version = SECONDS_SETTINGS_VERSION;
  s_settings.secondsMode = DEFAULT_SECONDS_PATTERN;
  s_settings.activeHoursMode = HOURS_ALL_DAY;
  s_settings.activeStartMin = DAYTIME_START_MIN;
  s_settings.activeEndMin = DAYTIME_END_MIN;
  s_settings.secondsBatteryMin = DEFAULT_SECONDS_BATTERY_MIN;
  s_settings.hideSecondsInQuietTime = true;
  s_settings.shakeToShow = false;
  s_settings.shakeDurationSec = DEFAULT_SHAKE_DURATION_SEC;
}

// in case of corrupt or unexpected values
static void sanitize(void) {
  if (s_settings.secondsMode > SECONDS_NEVER) {
    s_settings.secondsMode = DEFAULT_SECONDS_PATTERN;
  }
  if (s_settings.activeHoursMode > HOURS_CUSTOM) {
    s_settings.activeHoursMode = HOURS_ALL_DAY;
  }
  if (s_settings.activeStartMin >= 24 * 60 ||
      s_settings.activeEndMin >= 24 * 60) {
    s_settings.activeStartMin = DAYTIME_START_MIN;
    s_settings.activeEndMin = DAYTIME_END_MIN;
  }
  if (s_settings.secondsBatteryMin > 100) {
    s_settings.secondsBatteryMin = DEFAULT_SECONDS_BATTERY_MIN;
  }
  if (s_settings.shakeDurationSec < MIN_SHAKE_DURATION_SEC ||
      s_settings.shakeDurationSec > MAX_SHAKE_DURATION_SEC) {
    s_settings.shakeDurationSec = DEFAULT_SHAKE_DURATION_SEC;
  }
  s_settings.hideSecondsInQuietTime = !!s_settings.hideSecondsInQuietTime;
  s_settings.shakeToShow = !!s_settings.shakeToShow;
}

static void save(void) {
  s_settings.version = SECONDS_SETTINGS_VERSION;
  persist_write_data(SECONDS_PERSIST_KEY, &s_settings, sizeof(s_settings));
}

// pulls the seconds fields out of a 7.13 settings blob, then relabels the
// blob as settings version 7: its first 36 bytes are exactly that, and
// version 7 is what the upstream migration in settings.c picks up
static void migrate_from_legacy(void) {
  uint8_t blob[LEGACY_SETTINGS_SIZE];

  if (persist_read_data(SETTINGS_PERSIST_KEY, blob, sizeof(blob)) ==
      (int)sizeof(blob)) {
    LegacySecondsFields old;
    memcpy(&old, blob + LEGACY_SECONDS_OFFSET, sizeof(old));

    s_settings.secondsMode = old.secondsMode;
    s_settings.activeHoursMode = old.activeHoursMode;
    s_settings.activeStartMin = old.activeStartMin;
    s_settings.activeEndMin = old.activeEndMin;
    s_settings.secondsBatteryMin = old.secondsBatteryMin;
    s_settings.hideSecondsInQuietTime = old.hideSecondsInQuietTime;
    s_settings.shakeToShow = old.shakeToShow;
    s_settings.shakeDurationSec = old.shakeDurationSec;
    sanitize();
  }

  // the seconds settings first, so nothing is lost if this is interrupted
  save();
  persist_write_int(SETTINGS_VERSION_PERSIST_KEY, SETTINGS_VERSION_MASTER);
}

void Seconds_init(void) {
  set_defaults();

  if (persist_exists(SECONDS_PERSIST_KEY)) {
    SecondsSettings stored;
    if (persist_read_data(SECONDS_PERSIST_KEY, &stored, sizeof(stored)) ==
            (int)sizeof(stored) &&
        stored.version == SECONDS_SETTINGS_VERSION) {
      s_settings = stored;
      sanitize();
    }
  }

  // checked even when the seconds settings already exist, in case an
  // earlier migration was cut short before relabeling the blob
  if (persist_exists(SETTINGS_PERSIST_KEY) &&
      persist_read_int(SETTINGS_VERSION_PERSIST_KEY) ==
          LEGACY_SETTINGS_VERSION) {
    migrate_from_legacy();
  }
}

/* true if a hide override (low battery, quiet time) is in effect */
static bool hidden_by_override(void) {
  if (s_settings.hideSecondsInQuietTime && quiet_time_is_active()) {
    return true;
  }

  if (s_settings.secondsBatteryMin > 0) {
    BatteryChargeState charge = battery_state_service_peek();
    if (!charge.is_charging && !charge.is_plugged &&
        charge.charge_percent <= s_settings.secondsBatteryMin) {
      return true;
    }
  }

  return false;
}

/* true if the time is inside the user's active hours (the window may cross
   midnight, e.g. 22:00-06:00) */
static bool in_active_hours(const struct tm *t) {
  int start, end;

  switch (s_settings.activeHoursMode) {
  case HOURS_DAYTIME:
    start = DAYTIME_START_MIN;
    end = DAYTIME_END_MIN;
    break;
  case HOURS_CUSTOM:
    start = s_settings.activeStartMin;
    end = s_settings.activeEndMin;
    break;
  default:
    return true;
  }

  int now = t->tm_hour * 60 + t->tm_min;

  if (start == end) {
    return true; // an empty window is treated as all day
  } else if (start < end) {
    return now >= start && now < end;
  } else {
    return now >= start || now < end;
  }
}

/* true if the pattern calls for seconds in this minute. "last minute"
   patterns match on the tick for :14:00 etc., so the per-second
   subscription is in place from the first second of that minute */
static bool pattern_matches(const struct tm *t) {
  switch (s_settings.secondsMode) {
  case SECONDS_ALWAYS:
    return true;
  case SECONDS_HOURLY:
    return t->tm_min == 59;
  case SECONDS_HALF_HOURLY:
    return t->tm_min == 29 || t->tm_min == 59;
  case SECONDS_QUARTER_HOURLY:
    return t->tm_min % 15 == 14;
  default:
    return false;
  }
}

/* a shake beats everything else (active hours, battery, quiet time) */
bool Seconds_rowVisible(const struct tm *t) {
  if (s_settings.shakeToShow && s_shakeActive) {
    return true;
  }

  return pattern_matches(t) && in_active_hours(t) && !hidden_by_override();
}

static void shake_timeout_handler(void *data) {
  s_shakeTimer = NULL;
  s_shakeActive = false;
  if (s_refresh) {
    s_refresh();
  }
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  if (!s_settings.shakeToShow) {
    return;
  }

  // (re)start the countdown
  if (s_shakeTimer != NULL) {
    app_timer_cancel(s_shakeTimer);
  }
  s_shakeTimer =
      app_timer_register((uint32_t)s_settings.shakeDurationSec * 1000,
                         shake_timeout_handler, NULL);

  if (!s_shakeActive) {
    s_shakeActive = true;
    if (s_refresh) {
      s_refresh();
    }
  }
}

static void stop_shake(void) {
  if (s_shakeTimer != NULL) {
    app_timer_cancel(s_shakeTimer);
    s_shakeTimer = NULL;
  }
  s_shakeActive = false;
}

/* only listen for shakes when "shake to show" is on, since the
   accelerometer tap service has a (small) battery cost */
static void sync_tap_subscription(void) {
  if (s_settings.shakeToShow && !s_tapSubscribed) {
    accel_tap_service_subscribe(tap_handler);
    s_tapSubscribed = true;
  } else if (!s_settings.shakeToShow) {
    if (s_tapSubscribed) {
      accel_tap_service_unsubscribe();
      s_tapSubscribed = false;
    }
    stop_shake();
  }
}

void Seconds_sync(TickHandler handler, bool baseEverySecond) {
  sync_tap_subscription();

  // the caller resubscribes on its own whenever its base changes, which
  // replaces whatever was set up here
  if (baseEverySecond != s_lastBase) {
    s_lastBase = baseEverySecond;
    s_everySecond = baseEverySecond;
  }

  time_t now = time(NULL);
  bool wanted = baseEverySecond || Seconds_rowVisible(localtime(&now));

  if (wanted != s_everySecond) {
    tick_timer_service_unsubscribe();
    tick_timer_service_subscribe(wanted ? SECOND_UNIT : MINUTE_UNIT, handler);
    s_everySecond = wanted;
  }
}

void Seconds_start(TickHandler handler, bool baseEverySecond,
                   void (*refresh)(void)) {
  s_refresh = refresh;

  // init() has just subscribed afresh
  s_lastBase = baseEverySecond;
  s_everySecond = baseEverySecond;

  Seconds_sync(handler, baseEverySecond);
}

void Seconds_deinit(void) {
  stop_shake();
  if (s_tapSubscribed) {
    accel_tap_service_unsubscribe();
    s_tapSubscribed = false;
  }
}

// the value of an integer tuple, whatever width the phone sent it in
static int32_t tuple_value(const Tuple *t) {
  if (t->type == TUPLE_UINT) {
    switch (t->length) {
    case 1:
      return t->value->uint8;
    case 2:
      return t->value->uint16;
    default:
      return (int32_t)t->value->uint32;
    }
  }

  switch (t->length) {
  case 1:
    return t->value->int8;
  case 2:
    return t->value->int16;
  default:
    return t->value->int32;
  }
}

// finds an integer setting; false if it's absent or out of range
static bool read_setting(DictionaryIterator *iterator, uint32_t key,
                         int32_t min, int32_t max, int32_t *value) {
  Tuple *t = dict_find(iterator, key);
  if (t == NULL || (t->type != TUPLE_INT && t->type != TUPLE_UINT)) {
    return false;
  }

  *value = tuple_value(t);
  return *value >= min && *value <= max;
}

bool Seconds_handleMessage(DictionaryIterator *iterator) {
  static const uint32_t *const keys[] = {
      &MESSAGE_KEY_SettingSecondsMode,     &MESSAGE_KEY_SettingActiveHoursMode,
      &MESSAGE_KEY_SettingActiveStartMin,  &MESSAGE_KEY_SettingActiveEndMin,
      &MESSAGE_KEY_SettingShakeToShow,     &MESSAGE_KEY_SettingShakeDuration,
      &MESSAGE_KEY_SettingSecondsBatteryMin,
      &MESSAGE_KEY_SettingSecondsQuietHide};

  bool found = false;
  for (size_t i = 0; i < ARRAY_LENGTH(keys) && !found; i++) {
    found = dict_find(iterator, *keys[i]) != NULL;
  }
  if (!found) {
    return false;
  }

  int32_t value;

  if (read_setting(iterator, MESSAGE_KEY_SettingSecondsMode, SECONDS_ALWAYS,
                   SECONDS_NEVER, &value)) {
    s_settings.secondsMode = (uint8_t)value;
  }
  if (read_setting(iterator, MESSAGE_KEY_SettingActiveHoursMode,
                   HOURS_ALL_DAY, HOURS_CUSTOM, &value)) {
    s_settings.activeHoursMode = (uint8_t)value;
  }
  if (read_setting(iterator, MESSAGE_KEY_SettingActiveStartMin, 0,
                   24 * 60 - 1, &value)) {
    s_settings.activeStartMin = (uint16_t)value;
  }
  if (read_setting(iterator, MESSAGE_KEY_SettingActiveEndMin, 0, 24 * 60 - 1,
                   &value)) {
    s_settings.activeEndMin = (uint16_t)value;
  }
  if (read_setting(iterator, MESSAGE_KEY_SettingShakeToShow, 0, 1, &value)) {
    s_settings.shakeToShow = (bool)value;
  }
  if (read_setting(iterator, MESSAGE_KEY_SettingShakeDuration,
                   MIN_SHAKE_DURATION_SEC, MAX_SHAKE_DURATION_SEC, &value)) {
    s_settings.shakeDurationSec = (uint16_t)value;
  }
  if (read_setting(iterator, MESSAGE_KEY_SettingSecondsBatteryMin, 0, 100,
                   &value)) {
    s_settings.secondsBatteryMin = (uint8_t)value;
  }
  if (read_setting(iterator, MESSAGE_KEY_SettingSecondsQuietHide, 0, 1,
                   &value)) {
    s_settings.hideSecondsInQuietTime = (bool)value;
  }

  save();
  return true;
}
