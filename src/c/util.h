#pragma once
#include <pebble.h>
#include "time.h"


/*
 * For the specified GDrawCommandImage, recolors it with
 * the specified fill and stroke colors
 */
extern void gdraw_command_image_recolor(GDrawCommandImage *img, GColor fill_color, GColor stroke_color);

/*
 * Returns the current time in Swatch Internet Time "beats"
 */
extern int time_get_beats(const struct tm *tm);

/*
 * Returns true during the last minute of every 30-minute block
 * (i.e. minutes 29 and 59), when seconds should be displayed
 */
extern bool time_is_last_minute_of_half_hour(const struct tm *tm);

#ifdef PBL_HEALTH
  /*
   * Checks if any of the specified health activites exist in the specified time range
   */
  extern bool is_health_metric_accessible(HealthMetric metric);

  /*
   * Returns true if the user is sleeping, false otherwise
   */
  extern bool is_user_sleeping();
#endif
