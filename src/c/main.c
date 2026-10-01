#include <pebble.h>
#include "clock_area.h"
#include "messaging.h"
#include "settings.h"
#include "weather.h"
#include "sidebar.h"
#include "util.h"

// windows and layers
static Window* mainWindow;
static Layer* windowLayer;

// current bluetooth state
static bool isPhoneConnected;

// current time service subscription
static bool updatingEverySecond;

// seconds display state ("after shake" mode)
static AppTimer* shakeTimer;
static bool shakeActive;
static bool tapSubscribed;

// try to randomize when watches call the weather API
static uint8_t weatherRefreshMinute;

struct tm* get_current_time();
void update_clock();
void redrawScreen();
void tick_handler(struct tm *tick_time, TimeUnits units_changed);
void bluetoothStateChanged(bool newConnectionState);
void updateTickTimerSubscription(struct tm* currentTime);
static bool secondsRowVisible(struct tm* currentTime);
static void refreshSecondsState();


struct tm* get_current_time() {
  time_t rawTime;
  struct tm* timeInfo;

#ifdef USE_FAKE_TIME
  static struct tm fakeTime = {
    .tm_hour = 6,
    .tm_min  = 23,
    .tm_sec  = 0,
    .tm_mday = 24,
    .tm_mon  = 3,
    .tm_year = 126,
    .tm_wday = 5,
  };
  timeInfo = &fakeTime;
#else
  time(&rawTime);
  timeInfo = localtime(&rawTime);
#endif

  return timeInfo;
}

void update_clock() {
  struct tm* timeInfo = get_current_time();

  ClockArea_setShowSeconds(secondsRowVisible(timeInfo));
  ClockArea_update_time(timeInfo);
  Sidebar_updateTime(timeInfo);
}

/* true if a hide override (low battery, quiet time) is in effect */
static bool secondsHiddenByOverride() {
  if(settings.hideSecondsInQuietTime && quiet_time_is_active()) {
    return true;
  }

  if(settings.secondsBatteryMin > 0) {
    BatteryChargeState charge = battery_state_service_peek();
    if(!charge.is_charging && !charge.is_plugged &&
       charge.charge_percent <= settings.secondsBatteryMin) {
      return true;
    }
  }

  return false;
}

/* true if the current time is inside the user's active hours (the window
   may cross midnight, e.g. 22:00-06:00) */
static bool inActiveHours(struct tm* t) {
  int start, end;

  switch(settings.activeHoursMode) {
    case HOURS_DAYTIME:
      start = DAYTIME_START_MIN;
      end = DAYTIME_END_MIN;
      break;
    case HOURS_CUSTOM:
      start = settings.activeStartMin;
      end = settings.activeEndMin;
      break;
    default:
      return true;
  }

  int now = t->tm_hour * 60 + t->tm_min;

  if(start == end) {
    return true; // empty window is treated as all day
  } else if(start < end) {
    return now >= start && now < end;
  } else {
    return now >= start || now < end;
  }
}

/* true if the pattern calls for seconds in the current minute. "last minute"
   patterns match on the tick for :14:00 etc., so the per-second subscription
   is in place from the first second of that minute */
static bool patternMatches(struct tm* t) {
  switch(settings.secondsMode) {
    case SECONDS_ALWAYS:         return true;
    case SECONDS_HOURLY:         return t->tm_min == 59;
    case SECONDS_HALF_HOURLY:    return t->tm_min == 29 || t->tm_min == 59;
    case SECONDS_QUARTER_HOURLY: return t->tm_min % 15 == 14;
    default:                     return false;
  }
}

/* true if the seconds row on the main clock should be shown right now.
   A shake beats everything else (active hours, battery, quiet time) */
static bool secondsRowVisible(struct tm* currentTime) {
  if(settings.shakeToShow && shakeActive) {
    return true;
  }

  return patternMatches(currentTime) && inActiveHours(currentTime) &&
         !secondsHiddenByOverride();
}

/* subscribes to the tick timer service at the frequency currently needed --
   every second if the seconds row (or the seconds sidebar widget) is being
   shown, and every minute otherwise, so the watch can sleep between minutes */
void updateTickTimerSubscription(struct tm* currentTime) {
  bool needsEverySecond = dynamicSettings.updateScreenEverySecond ||
                           secondsRowVisible(currentTime);

  if(needsEverySecond != updatingEverySecond) {
    tick_timer_service_unsubscribe();

    if(needsEverySecond) {
      tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
      updatingEverySecond = true;
    } else {
      tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
      updatingEverySecond = false;
    }
  }
}

/* re-evaluates the seconds state after something affecting it changed (shake
   started/ended, battery state changed) and redraws just the clock */
static void refreshSecondsState() {
  updateTickTimerSubscription(get_current_time());
  update_clock();
  ClockArea_redraw();
}

static void shakeTimeoutHandler(void* data) {
  shakeTimer = NULL;
  shakeActive = false;
  refreshSecondsState();
}

static void tapHandler(AccelAxisType axis, int32_t direction) {
  if(!settings.shakeToShow) {
    return;
  }

  // (re)start the countdown
  if(shakeTimer != NULL) {
    app_timer_cancel(shakeTimer);
  }
  shakeTimer = app_timer_register((uint32_t)settings.shakeDurationSec * 1000,
                                  shakeTimeoutHandler, NULL);

  if(!shakeActive) {
    shakeActive = true;
    refreshSecondsState();
  }
}

/* only listen for shakes when the "after shake" mode is selected, since the
   accelerometer tap service has a (small) battery cost */
static void syncTapSubscription() {
  bool wantTaps = settings.shakeToShow;

  if(wantTaps && !tapSubscribed) {
    accel_tap_service_subscribe(tapHandler);
    tapSubscribed = true;
  } else if(!wantTaps) {
    if(tapSubscribed) {
      accel_tap_service_unsubscribe();
      tapSubscribed = false;
    }
    if(shakeTimer != NULL) {
      app_timer_cancel(shakeTimer);
      shakeTimer = NULL;
    }
    shakeActive = false;
  }
}

/* forces everything on screen to be redrawn -- perfect for keeping track of settings! */
void redrawScreen() {

  // the seconds mode may have changed
  syncTapSubscription();

  // check if the tick handler frequency should be changed
  updateTickTimerSubscription(get_current_time());

  window_set_background_color(mainWindow, settings.timeBgColor);

  // maybe the language changed!
  update_clock();

  // update the sidebar
  Sidebar_redraw();

  ClockArea_redraw();
}

static void main_window_load(Window *window) {
  window_set_background_color(window, settings.timeBgColor);

  // create the sidebar
  Sidebar_init(window);

  ClockArea_init(window);

  // Make sure the time is displayed from the start
  redrawScreen();
}

static void main_window_unload(Window *window) {
  ClockArea_deinit();
  Sidebar_deinit();
}


void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // check if the tick handler frequency should be changed (e.g. quiet time
  // started, or entering/leaving the last minute of a 30-minute block)
  updateTickTimerSubscription(tick_time);

  // every 30 minutes, request new weather data
  if(!dynamicSettings.disableWeather) {
    if(tick_time->tm_min == weatherRefreshMinute && tick_time->tm_sec == 0) {
      messaging_requestNewWeatherData();
    }
  }

  // every hour, if requested, vibrate
  if(!quiet_time_is_active() && tick_time->tm_sec == 0) {
    if(settings.hourlyVibe == VIBE_EVERY_HOUR) { // hourly vibes only
      if(tick_time->tm_min == 0) {
        vibes_double_pulse();
      }
    } else if(settings.hourlyVibe == VIBE_EVERY_HALF_HOUR) {  // hourly and half-hourly
      if(tick_time->tm_min == 0) {
        vibes_double_pulse();
      } else if(tick_time->tm_min == 30) {
        vibes_short_pulse();
      }
    }
  }

  update_clock();

  // redraw all screen
  Sidebar_redraw();
  ClockArea_redraw();
}

void bluetoothStateChanged(bool newConnectionState) {
  // if the phone was connected but isn't anymore and the user has opted in,
  // trigger a vibration
  if(!quiet_time_is_active() && isPhoneConnected && !newConnectionState && settings.btVibe) {
    static uint32_t const segments[] = { 200, 100, 100, 100, 500 };
    VibePattern pat = {
      .durations = segments,
      .num_segments = ARRAY_LENGTH(segments),
      };
    vibes_enqueue_custom_pattern(pat);
  }

  // if the phone was disconnected and isn't anymore, update the data
  if(!isPhoneConnected && newConnectionState && !dynamicSettings.disableWeather) {
    messaging_requestNewWeatherData();
  }

  isPhoneConnected = newConnectionState;

  Sidebar_redraw();
}

// force the sidebar to redraw any time the battery state changes
void batteryStateChanged(BatteryChargeState charge_state) {
  // the battery saver / charging state may change whether seconds are shown
  refreshSecondsState();
  Sidebar_redraw();
}

// fixes for disappearing elements after notifications
// (from http://codecorner.galanter.net/2016/01/08/solved-issue-with-pebble-framebuffer-after-notification-is-dismissed/)
static void app_focus_changing(bool focusing) {
  if (focusing) {
     layer_set_hidden(windowLayer, true);
  }
}

static void app_focus_changed(bool focused) {
  if (focused) {
     layer_set_hidden(windowLayer, false);
     layer_mark_dirty(windowLayer);
  }
}

static void init() {
  setlocale(LC_ALL, "");

  srand(time(NULL));

  weatherRefreshMinute = rand() % 60;

  // init settings
  Settings_init();

  // init weather system
  Weather_init();

  // init the messaging thing
  messaging_init(redrawScreen);

  // Create main Window element and assign to pointer
  mainWindow = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(mainWindow, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(mainWindow, true);

  windowLayer = window_get_root_layer(mainWindow);

  // Register with TickTimerService
  shakeActive = false;
  shakeTimer = NULL;
  tapSubscribed = false;
  syncTapSubscription();
  updatingEverySecond = false;
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  updateTickTimerSubscription(get_current_time());

  bool connected = bluetooth_connection_service_peek();
  bluetoothStateChanged(connected);
  bluetooth_connection_service_subscribe(bluetoothStateChanged);

  // register with battery service
  battery_state_service_subscribe(batteryStateChanged);

  // set up focus change handlers
  app_focus_service_subscribe_handlers((AppFocusHandlers){
    .did_focus = app_focus_changed,
    .will_focus = app_focus_changing
  });
}

static void deinit() {
  // Destroy Window
  window_destroy(mainWindow);

  // unload weather stuff
  Weather_deinit();
  Settings_deinit();

  if(shakeTimer != NULL) {
    app_timer_cancel(shakeTimer);
    shakeTimer = NULL;
  }
  if(tapSubscribed) {
    accel_tap_service_unsubscribe();
    tapSubscribed = false;
  }

  tick_timer_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();
  battery_state_service_unsubscribe();
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
