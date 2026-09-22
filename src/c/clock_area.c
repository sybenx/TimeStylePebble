#ifndef PBL_PLATFORM_APLITE
#include <pebble.h>

#include "clock_area.h"
#include "settings.h"
#include "sidebar.h"
#include "util.h"

#include <pebble-fctx/fctx.h>
#include <pebble-fctx/fpath.h>
#include <pebble-fctx/ffont.h>

#define ROUND_VERTICAL_PADDING 15

char time_hours[3];
char time_minutes[3];
char time_seconds[3];
bool showingSeconds;

Layer* clock_area_layer;
FFont* hours_font;
FFont* minutes_font;

// just allocate all the fonts at startup because i don't feel like
// dealing with allocating and deallocating things
FFont* avenir;
FFont* avenir_bold;
FFont* leco;

GRect screen_rect;

// "private" functions
void update_fonts() {
  switch(settings.clockFontId) {
    case FONT_SETTING_DEFAULT:
        hours_font = avenir;
        minutes_font = avenir;
      break;
    case FONT_SETTING_BOLD:
        hours_font = avenir_bold;
        minutes_font = avenir_bold;
      break;
    case FONT_SETTING_BOLD_H:
        hours_font = avenir_bold;
        minutes_font = avenir;
      break;
    case FONT_SETTING_BOLD_M:
        hours_font = avenir;
        minutes_font = avenir_bold;
      break;
    case FONT_SETTING_LECO:
        hours_font = leco;
        minutes_font = leco;
      break;
  }
}

void update_clock_area_layer(Layer *l, GContext* ctx) {
  // check layer bounds
  GRect bounds = layer_get_unobstructed_bounds(l);

  #ifdef PBL_ROUND
    bounds = GRect(0, ROUND_VERTICAL_PADDING, screen_rect.size.w, screen_rect.size.h - ROUND_VERTICAL_PADDING * 2);
  #endif

  // initialize FCTX, the fancy 3rd party drawing library that all the cool kids use
  FContext fctx;

  fctx_init_context(&fctx, ctx);
  fctx_set_color_bias(&fctx, 0);
  fctx_set_fill_color(&fctx, settings.timeColor);


  // calculate font size
  int font_size = 4 * bounds.size.h / 7;

  // avenir + avenir bold metrics
  int v_padding = bounds.size.h / 16;
  int h_adjust = 0;
  int v_adjust = 0;

  // alternate metrics for LECO
  if(settings.clockFontId == FONT_SETTING_LECO) {
    font_size = 4 * bounds.size.h / 7 + 6;
    v_padding = bounds.size.h / 20;
    h_adjust = -4;
    v_adjust = 0;

    // leco looks awful with antialiasing
    #ifdef PBL_COLOR
      fctx_enable_aa(false);
    #endif
  } else {
    #ifdef PBL_COLOR
      fctx_enable_aa(true);
    #endif
  }

  // if it's a round watch, EVERYTHING CHANGES
  #ifdef PBL_ROUND
    v_adjust = ROUND_VERTICAL_PADDING;

    if(settings.clockFontId != FONT_SETTING_LECO) {
      h_adjust = -1;
    }
  #else
    // for rectangular watches, adjust X position based on sidebar position
    if(settings.sidebarOnLeft) {
      h_adjust += sidebarWidth / 2;
    } else {
      h_adjust -= sidebarWidth / 2 + 1;
    }
  #endif

  FPoint time_pos;
  fctx_begin_fill(&fctx);
  int center_x = bounds.size.w / 2 + h_adjust;

  if(showingSeconds) {
    // stack hours, minutes, and seconds as three even rows, since there's no
    // room to show seconds on the same line as the minutes
    int row_height = bounds.size.h / 3;
    int hours_size = row_height * 6 / 5;
    int minutes_size = row_height * 6 / 5;
    int seconds_size = row_height * 4 / 5;

    fctx_set_text_em_height(&fctx, hours_font, hours_size);
    time_pos.x = INT_TO_FIXED(center_x);
    time_pos.y = INT_TO_FIXED(row_height / 2 + v_adjust);
    fctx_set_offset(&fctx, time_pos);
    fctx_draw_string(&fctx, time_hours, hours_font, GTextAlignmentCenter, FTextAnchorMiddle);

    // set minutes' em height (and draw it) before touching it again for the
    // seconds row below, since minutes_font and hours_font may be the same font
    fctx_set_text_em_height(&fctx, minutes_font, minutes_size);
    time_pos.y = INT_TO_FIXED(row_height + row_height / 2 + v_adjust);
    fctx_set_offset(&fctx, time_pos);
    fctx_draw_string(&fctx, time_minutes, minutes_font, GTextAlignmentCenter, FTextAnchorMiddle);

    fctx_set_text_em_height(&fctx, minutes_font, seconds_size);
    time_pos.y = INT_TO_FIXED(row_height * 2 + row_height / 2 + v_adjust);
    fctx_set_offset(&fctx, time_pos);
    fctx_draw_string(&fctx, time_seconds, minutes_font, GTextAlignmentCenter, FTextAnchorMiddle);
  } else {
    // draw hours (set the em height and draw it before touching minutes_font's
    // em height below, since hours_font and minutes_font may be the same font)
    fctx_set_text_em_height(&fctx, hours_font, font_size);
    time_pos.x = INT_TO_FIXED(center_x);
    time_pos.y = INT_TO_FIXED(v_padding + v_adjust);
    fctx_set_offset(&fctx, time_pos);
    fctx_draw_string(&fctx, time_hours, hours_font, GTextAlignmentCenter, FTextAnchorTop);

    // draw minutes
    fctx_set_text_em_height(&fctx, minutes_font, font_size);
    time_pos.y = INT_TO_FIXED(bounds.size.h - v_padding + v_adjust);
    fctx_set_offset(&fctx, time_pos);
    fctx_draw_string(&fctx, time_minutes, minutes_font, GTextAlignmentCenter, FTextAnchorBaseline);
  }

  fctx_end_fill(&fctx);

  fctx_deinit_context(&fctx);
}


void ClockArea_init(Window* window) {
  // record the screen size, since we NEVER GET IT AGAIN
  screen_rect = layer_get_bounds(window_get_root_layer(window));

  GRect bounds;
  bounds = GRect(0, 0, screen_rect.size.w, screen_rect.size.h);

  // init the clock area layer
  clock_area_layer = layer_create(bounds);
  layer_add_child(window_get_root_layer(window), clock_area_layer);
  layer_set_update_proc(clock_area_layer, update_clock_area_layer);

  // allocate fonts
  avenir =      ffont_create_from_resource(RESOURCE_ID_AVENIR_REGULAR_FFONT);
  avenir_bold = ffont_create_from_resource(RESOURCE_ID_AVENIR_BOLD_FFONT);
  leco =        ffont_create_from_resource(RESOURCE_ID_LECO_REGULAR_FFONT);

  // select fonts based on settings
  update_fonts();
}

void ClockArea_deinit() {
  layer_destroy(clock_area_layer);

  ffont_destroy(avenir);
  ffont_destroy(avenir_bold);
  ffont_destroy(leco);
}

void ClockArea_redraw() {
  // check if the fonts need to be switched
  update_fonts();

  layer_mark_dirty(clock_area_layer);
}

void ClockArea_update_time(struct tm* time_info) {

  // hours
  if (clock_is_24h_style()) {
    strftime(time_hours, sizeof(time_hours), (settings.showLeadingZero) ? "%H" : "%k", time_info);
  } else {
    strftime(time_hours, sizeof(time_hours), (settings.showLeadingZero) ? "%I" : "%l", time_info);
  }

  // minutes
  strftime(time_minutes, sizeof(time_minutes), "%M", time_info);

  // seconds (shown as a third row during the last minute of every 30-minute block)
  showingSeconds = time_is_last_minute_of_half_hour(time_info);
  if(showingSeconds) {
    strftime(time_seconds, sizeof(time_seconds), "%S", time_info);
  }
}

#endif