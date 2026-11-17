#pragma once
#include <pebble.h>

// "public" functions
void ClockArea_init(Window* window);
void ClockArea_deinit();
void ClockArea_redraw();
void ClockArea_setShowSeconds(bool show);
void ClockArea_setShowSeconds(bool show);
void ClockArea_update_time(struct tm* time_info);
