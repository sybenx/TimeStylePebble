#pragma once
#include <pebble.h>

/*
 * TimeStyle Seconds: everything this fork adds on top of TimeStyle lives in
 * seconds.c / seconds.h (and src/pkjs/seconds_config.js on the phone), with
 * only one-line hooks in the upstream files. Keeping it that way is what
 * lets the daily upstream merge go through without conflicts.
 *
 * The seconds settings are persisted under their own key, separate from the
 * upstream Settings struct, for the same reason.
 */

// which minutes the seconds row is shown in (the pattern)
typedef enum {
  SECONDS_ALWAYS = 0,
  SECONDS_HOURLY = 1,         // :59
  SECONDS_HALF_HOURLY = 2,    // :29 and :59 (original behavior)
  SECONDS_QUARTER_HOURLY = 3, // :14, :29, :44, :59
  SECONDS_NEVER = 4
} SecondsPatternType;

// when (during the day) the seconds row may be shown
typedef enum {
  HOURS_ALL_DAY = 0,
  HOURS_DAYTIME = 1, // 07:00 - 22:00
  HOURS_CUSTOM = 2
} ActiveHoursType;

// loads the seconds settings. Must run before Settings_init(), because it
// also converts the settings saved by TimeStyle Seconds 7.13 into a shape
// the upstream settings migration understands
void Seconds_init(void);
void Seconds_deinit(void);

// call once the tick timer subscription is in place in init().
// baseEverySecond: whether that subscription is per-second
// refresh: called when the seconds row should appear or disappear outside
//          of a tick (a shake started or timed out)
void Seconds_start(TickHandler handler, bool baseEverySecond,
                   void (*refresh)(void));

// brings the tick timer and shake subscriptions in line with the settings
// and the current time: per-second ticks only while the seconds row shows.
// baseEverySecond: what the caller itself subscribed to
void Seconds_sync(TickHandler handler, bool baseEverySecond);

// true if the seconds row on the main clock should be shown at this time
bool Seconds_rowVisible(const struct tm *t);

// reads any seconds settings out of a message from the phone and saves
// them; returns true if there were any
bool Seconds_handleMessage(DictionaryIterator *iterator);
