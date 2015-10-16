#include <pebble.h>
#include <types.h>
#include <array.h>
  
#define TIME_ANGLE(time) time * (TRIG_MAX_ANGLE / 60)
#define HOUR_ANGLE(hour) hour * (TRIG_MAX_ANGLE / 12)

// Define struct to store colors for each time unit

static Window *s_window;
static Layer *s_layer, *s_date_layer;
static TextLayer *s_time_layer, *s_day_label, *s_num_label;
static Palette *s_palette;

static uint8_t s_hour;
static uint8_t s_minute;
static uint8_t s_second;
static char s_num_buffer[4], s_day_buffer[6];
static TimeSegment *s_time_segments[3];

// Draw an arc with given inner/outer radii 
static void draw_arc(GContext *ctx, GColor color, GRect rect, uint16_t thickness, uint32_t start_angle, uint32_t end_angle) {
  graphics_context_set_fill_color(ctx, color);
  
  if(end_angle == 0) {
    graphics_fill_radial(ctx, rect, GOvalScaleModeFitCircle, thickness, start_angle, TRIG_MAX_ANGLE);
  } else {
    graphics_fill_radial(ctx, rect, GOvalScaleModeFitCircle, thickness, start_angle, end_angle); 
  }
}

static GRect calculate_rect(Layer *layer) {
  return grect_inset(layer_get_bounds(layer), GEdgeInsets(10));
}

static int compare_colors(GColor colorA, GColor colorB) {
  if (colorA.r == colorB.r && colorA.b == colorB.b && colorA.g == colorA.g && colorA.a == colorB.a) {
    return 0;
  }
  return -1;
}

static GColor mix_colors(GColor colorA, GColor colorB) {
  
  if ((compare_colors(colorA, s_palette->red) == 0 && compare_colors(colorB, s_palette->blue) == 0) ||
      (compare_colors(colorA, s_palette->blue) == 0 && compare_colors(colorB, s_palette->red) == 0)) {
    return s_palette->purple;
  }
  
  if ((compare_colors(colorA, s_palette->red) == 0 && compare_colors(colorB, s_palette->yellow) == 0) ||
      (compare_colors(colorA, s_palette->yellow) == 0 && compare_colors(colorB, s_palette->red) == 0)) {
    return s_palette->orange;
  }
  
  if ((compare_colors(colorA, s_palette->green) == 0 && compare_colors(colorB, s_palette->blue) == 0) ||
      (compare_colors(colorA, s_palette->blue) == 0 && compare_colors(colorB, s_palette->green) == 0)) {
    return s_palette->green;
  }
  
  return s_palette->all;
}

static int32_t get_angle_for_hour(int hour) {
  // Progress through 12 hours, out of 360 degrees
  return (hour * 360) / 12;
}

static void draw_hands(GRect frame, GContext *ctx) {
  //draw hour, minute, second, hands
  GPoint center = grect_center_point(&frame);
  int16_t hand_length = 40;
  for (int i=0; i<3; i++) {
    GPoint hand = {
      .x = (int16_t)(sin_lookup(s_time_segments[i]->value) * (int32_t)hand_length / TRIG_MAX_RATIO) + center.x,
      .y = (int16_t)(-cos_lookup(s_time_segments[i]->value) * (int32_t)hand_length / TRIG_MAX_RATIO) + center.y,
    };
    
    graphics_context_set_stroke_color(ctx, s_time_segments[i]->color);
    graphics_context_set_stroke_width(ctx, 2);
    
    graphics_draw_line(ctx, hand, center);
  }
}

static void draw_color_ring(GRect bounds, GContext *ctx) {
  //draw time segments and overlaps
  draw_arc(ctx, s_palette->all, bounds, 33, 0, s_time_segments[0]->value);
  draw_arc(ctx, mix_colors(s_time_segments[1]->color, s_time_segments[2]->color), bounds, 33, s_time_segments[0]->value, s_time_segments[1]->value);
  draw_arc(ctx, s_time_segments[2]->color, bounds, 33, s_time_segments[1]->value, s_time_segments[2]->value);
  draw_arc(ctx, s_palette->none, bounds, 33, s_time_segments[2]->value, 0);
}

static void draw_dots(GRect frame, GContext *ctx) {
  // Hours are dots
  for(int i = 0; i < 12; i=i+3) {
    int hour_angle = get_angle_for_hour(i);
    GPoint pos = gpoint_from_polar(frame, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(hour_angle));

    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_circle(ctx, pos, 2);
  }
}

// Update the watchface display
static void update_display(Layer *layer, GContext *ctx) {
  GRect bounds = calculate_rect(layer);
  GRect frame = grect_inset(layer_get_bounds(layer), GEdgeInsets(18));
  
  sort(s_time_segments, 3);
  
  //draw_hands(frame, ctx);
  draw_color_ring(bounds, ctx);
  draw_dots(frame, ctx);
}

static void update_date(Layer *layer, GContext *ctx) {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  strftime(s_day_buffer, sizeof(s_day_buffer), "%b", t);
  text_layer_set_text(s_day_label, s_day_buffer);

  strftime(s_num_buffer, sizeof(s_num_buffer), "%d", t);
  text_layer_set_text(s_num_label, s_num_buffer);
}

// Update the current time values for the watchface
static void update_time(struct tm *tick_time) {
  s_hour = tick_time->tm_hour;
  s_minute = tick_time->tm_min;
  s_second = tick_time->tm_sec;
  
  TimeSegment *second_time_segment = malloc(sizeof(TimeSegment));
  TimeSegment *minute_time_segment = malloc(sizeof(TimeSegment));
  TimeSegment *hour_time_segment = malloc(sizeof(TimeSegment));
  
  *second_time_segment = (TimeSegment) {
      s_palette->blue,
      TIME_ANGLE(s_second),
      's'
  };
  
  *minute_time_segment = (TimeSegment) {
      s_palette->red,
      TIME_ANGLE(s_minute),
      'm'
  };
  
  *hour_time_segment = (TimeSegment) {
      s_palette->yellow,
      HOUR_ANGLE(s_hour),
      'h'
  };
  
  s_time_segments[0] = second_time_segment;
  s_time_segments[1] = minute_time_segment;
  s_time_segments[2] = hour_time_segment;
  
  // Create a long-lived buffer
  static char buffer[] = "00:00";

  // Write the current hours and minutes into the buffer
  if(clock_is_24h_style() == true) {
    // Use 24 hour format
    strftime(buffer, sizeof("00:00"), "%H:%M", tick_time);
  } else {
    // Use 12 hour format
    strftime(buffer, sizeof("00:00"), "%I:%M", tick_time);
  }

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);
  layer_mark_dirty(s_layer);
  layer_mark_dirty(s_date_layer);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  update_time(tick_time);
}

static void window_load(Window *window) {
  s_palette = malloc(sizeof(Palette));
  *s_palette = (Palette) {
      COLOR_FALLBACK(GColorDarkCandyAppleRed,GColorGray),
      COLOR_FALLBACK(GColorChromeYellow,GColorGray),
      COLOR_FALLBACK(GColorDukeBlue,GColorGray),
      COLOR_FALLBACK(GColorOrange,GColorWhite),
      COLOR_FALLBACK(GColorDarkGreen,GColorWhite),
      COLOR_FALLBACK(GColorImperialPurple,GColorWhite),
      COLOR_FALLBACK(GColorBlack,GColorBlack),
      COLOR_FALLBACK(GColorBlack,GColorBlack)
  };
  
  GRect bounds = layer_get_bounds(window_get_root_layer(s_window));
  GSize bounds_size = bounds.size;
  
  //Create main background arc layer
  s_layer = layer_create(bounds);
  layer_add_child(window_get_root_layer(s_window), s_layer);
  layer_set_update_proc(s_layer, update_display);
  
  // Create time TextLayer
  s_time_layer = text_layer_create(grect_inset(layer_get_bounds(window_get_root_layer(s_window)), GEdgeInsets(65, 10, 20, 10)));
  text_layer_set_background_color(s_time_layer, GColorClear);
  text_layer_set_text_color(s_time_layer, GColorWhite);
  
  text_layer_set_text(s_time_layer, "00:00");
  text_layer_set_font(s_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_time_layer, GTextAlignmentCenter);
  
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));
  
  //Create date layer
  s_date_layer = layer_create(layer_get_bounds(window_get_root_layer(s_window)));
  layer_set_update_proc(s_date_layer, update_date);
  layer_add_child(s_layer, s_date_layer);

  s_day_label = text_layer_create(GRect(0, 111, bounds_size.w / 2 - 2, 20));
  text_layer_set_text(s_day_label, s_day_buffer);
  text_layer_set_background_color(s_day_label, GColorClear);
  text_layer_set_text_color(s_day_label, GColorWhite);
  text_layer_set_text_alignment(s_day_label, GTextAlignmentRight);
  text_layer_set_font(s_day_label, fonts_get_system_font(FONT_KEY_GOTHIC_18));

  layer_add_child(s_date_layer, text_layer_get_layer(s_day_label));

  s_num_label = text_layer_create(GRect(bounds_size.w / 2 + 4, 111, bounds_size.w / 2 - 2, 20));
  text_layer_set_text(s_num_label, s_num_buffer);
  text_layer_set_background_color(s_num_label, GColorClear);
  text_layer_set_text_color(s_num_label, GColorWhite);
  text_layer_set_text_alignment(s_num_label, GTextAlignmentLeft);
  text_layer_set_font(s_num_label, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));

  layer_add_child(s_date_layer, text_layer_get_layer(s_num_label));
}

static void window_unload(Window *window) {
  free(s_palette);
  layer_destroy(s_layer);
  text_layer_destroy(s_time_layer);
}

static void init(void) {
  s_window = window_create();
  
#ifdef PBL_SDK_2
  window_set_fullscreen(s_window, true);
#endif
  
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload
  });
  window_set_background_color(s_window, GColorBlack);
  window_stack_push(s_window, true);

  time_t start = time(NULL);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  update_time(localtime(&start));
}

static void deinit(void) {
  tick_timer_service_unsubscribe();
  window_destroy(s_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}