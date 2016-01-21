#include <pebble.h>

static Window *s_main_window;
static TextLayer *s_timehours_layer;
static TextLayer *s_timemins_layer;
static TextLayer *s_time_ampm_layer;
static TextLayer *s_steps_layer;
static TextLayer *s_day_layer;
static Layer *s_battery_layer;
static GFont s_timehour_font;
static GFont s_timemin_font;
static GFont s_smaller_font;
static GFont s_timeampm_font;
static int s_charge_percent = 90;
static int xHalf = 0;

static const int STEPS_LAP = 10000;
static const int STEP_DIVS = 1000;
//static int steps = 0;

static void battery_state_handler(BatteryChargeState charge) {
  s_charge_percent = charge.charge_percent;
  layer_mark_dirty(s_battery_layer);
}

static void layer_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  
  // Draw battery line
  graphics_context_set_stroke_color(ctx, s_charge_percent <= 30 ? GColorYellow : GColorRed);
  graphics_context_set_stroke_width(ctx, 3);
  int lineSegment = (bounds.size.w - 56) * 10/100;
  for (int i = 0; i < s_charge_percent / 10; i+=1)
  {
    graphics_draw_line(ctx, GPoint(28 + i * lineSegment, 120), GPoint(26 + (lineSegment * (i + 1)), 120));
  }
  
  // Draw arcs for each STEP_DIVS
  GRect inset_bounds = grect_inset(bounds,GEdgeInsets(1,3,2,2));
  graphics_context_set_fill_color(ctx, GColorRed);
  
  
  // Get the sum steps so far today
  HealthValue steps = health_service_sum_today(HealthMetricStepCount);
  int lapSteps = steps%STEPS_LAP;
  int lapSectionAngle = 360 / (STEPS_LAP / STEP_DIVS);
  int subLapSectionAngle = lapSectionAngle / 10;
  
  for (int i = 0; lapSteps > 0; i++) {
    if ( lapSteps > STEP_DIVS ) {
        int32_t add_Degrees = lapSectionAngle - 2;
        graphics_fill_radial(ctx, inset_bounds, GOvalScaleModeFitCircle, 9, DEG_TO_TRIGANGLE(lapSectionAngle * i), DEG_TO_TRIGANGLE(lapSectionAngle * i + add_Degrees ));  
        lapSteps -= STEP_DIVS;
    } else {
      for (int j = 0; lapSteps > 0; j++) {
        int32_t add_Degrees = subLapSectionAngle - 1;
        graphics_fill_radial(ctx, inset_bounds, GOvalScaleModeFitCircle, ((j == 4) ? 13 : 9 ), DEG_TO_TRIGANGLE(lapSectionAngle * i + subLapSectionAngle * j), DEG_TO_TRIGANGLE(lapSectionAngle * i + subLapSectionAngle * j + add_Degrees ));  
        lapSteps -= STEP_DIVS / 10;
      }
    }    
  }
  
  
//   GRect double_inset = grect_inset(inset_bounds, GEdgeInsets(5));
//   graphics_context_set_fill_color(ctx, GColorBlack);
//   for (int i = 1; i < 10; i++) {
//     GRect xywh = grect_centered_from_polar(double_inset, GOvalScaleModeFillCircle, DEG_TO_TRIGANGLE(36 * i), GSize(5,5));
//     graphics_fill_radial(ctx, xywh, GOvalScaleModeFillCircle, 3, 0, TRIG_MAX_ANGLE);
//   } 
  
  GRect double_inset = grect_inset(inset_bounds, GEdgeInsets(15));
  graphics_context_set_fill_color(ctx, GColorWhite);
  if (steps < STEPS_LAP) {
    steps += 150;
    return;
  }
  // draw step lap dots
  for (int i = 0; i < steps/STEPS_LAP; i++) {
    GRect xywh = grect_centered_from_polar(double_inset, GOvalScaleModeFitCircle, DEG_TO_TRIGANGLE(6 * i), GSize(5,5));
    graphics_fill_radial(ctx, xywh, GOvalScaleModeFitCircle, 5, 0, TRIG_MAX_ANGLE);
  }
  steps += 33;
}

static void update_time() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  
  static char s_buffer_h[3];
  strftime(s_buffer_h, sizeof(s_buffer_h), clock_is_24h_style() ? "%H" : "%I", tick_time);
  text_layer_set_text(s_timehours_layer, s_buffer_h);
  static char s_buffer_m[3];
  strftime(s_buffer_m, sizeof(s_buffer_m), "%M", tick_time);
  text_layer_set_text(s_timemins_layer, s_buffer_m);
  static char s_buffer_ampm[3];
  strftime(s_buffer_ampm, sizeof(s_buffer_ampm), clock_is_24h_style() ? "": "%p", tick_time);
  text_layer_set_text(s_time_ampm_layer, s_buffer_ampm);
  
  // Resize to ceneter 
  GSize hoursSize = text_layer_get_content_size(s_timehours_layer);
  GSize minSize = text_layer_get_content_size(s_timemins_layer);
  GSize ampmSize = text_layer_get_content_size(s_time_ampm_layer);
  int combinedWidth = hoursSize.w + minSize.w;
  int halfWidth = combinedWidth / 2;
  
  text_layer_set_size(s_timehours_layer, GSize(xHalf + halfWidth - minSize.w, 90));
  text_layer_set_size(s_timemins_layer, GSize(xHalf + halfWidth, 68));
  text_layer_set_size(s_time_ampm_layer, GSize(xHalf + halfWidth - (minSize.w - ampmSize.w) + 2, 68));  
}

static void update_steps() {
  //static char str[15];
  //snprintf(str, sizeof(str), "%d", steps);
  //text_layer_set_text(s_steps_layer, str);
}

static void update_date() {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  static char s_date_buffer[20];
  strftime(s_date_buffer, sizeof(s_date_buffer), "%m/%d/%y", tick_time);
  text_layer_set_text(s_steps_layer, s_date_buffer);
  
  static char s_day_buffer[20];
  strftime(s_day_buffer, sizeof(s_day_buffer), "%A", tick_time);
  text_layer_set_text(s_day_layer, s_day_buffer);
}

static void workout_reminder() {
  static const uint32_t segments[] = { 100, 300, 100, 300, 200 };
  VibePattern pat = {
    .durations = segments,
    .num_segments = ARRAY_LENGTH(segments),
  };
  vibes_enqueue_custom_pattern(pat);
}


static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (MINUTE_UNIT) {
    update_time();
    update_steps();
  }
  if (HOUR_UNIT)
  {
    if (tick_time->tm_hour >= 9 && tick_time->tm_hour <= 17 && tick_time->tm_min == 0)
    {
      workout_reminder();  
    }
  }
  if (DAY_UNIT) {
    update_date();
  }
}

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  s_timehour_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_Font_AgentB_72));
  s_timemin_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_Font_AgentB_44));
  s_timeampm_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_Font_AgentB_22));
  s_smaller_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_Font_AgentB_22));
  xHalf = bounds.size.w / 2;
  
  // Create Hours Time display
  s_timehours_layer = text_layer_create(
    GRect(0, 39, xHalf + 20, 94));
  text_layer_set_background_color(s_timehours_layer, GColorClear);  
  text_layer_set_text_color(s_timehours_layer, GColorRed);
  text_layer_set_font(s_timehours_layer, s_timehour_font);
  text_layer_set_text_alignment(s_timehours_layer, GTextAlignmentRight);
  
  // Create Minutes Time display
  s_timemins_layer = text_layer_create(
    GRect(0, 46, xHalf + 20, 68));
  text_layer_set_background_color(s_timemins_layer, GColorClear);  
  text_layer_set_text_color(s_timemins_layer, GColorWhite);
  text_layer_set_font(s_timemins_layer, s_timemin_font);
  text_layer_set_text_alignment(s_timemins_layer, GTextAlignmentRight);
  
  // Create AM or PM Time display
  s_time_ampm_layer = text_layer_create(
    GRect(0, 89, xHalf + 20, 68));
  text_layer_set_background_color(s_time_ampm_layer, GColorClear);  
  text_layer_set_text_color(s_time_ampm_layer, GColorLightGray);
  text_layer_set_font(s_time_ampm_layer, s_timeampm_font);
  text_layer_set_text_alignment(s_time_ampm_layer, GTextAlignmentRight);
  
  // Create Steps display
  s_steps_layer = text_layer_create(
    GRect(0, 125, bounds.size.w, 24));
  text_layer_set_background_color(s_steps_layer, GColorClear);
  text_layer_set_text_color(s_steps_layer, GColorWhite);
  text_layer_set_font(s_steps_layer, s_smaller_font);
  text_layer_set_text_alignment(s_steps_layer, GTextAlignmentCenter);
  
  //Create Day display
  s_day_layer = text_layer_create(
    GRect(0, 24, bounds.size.w, 26));  
  text_layer_set_background_color(s_day_layer, GColorClear);
  text_layer_set_text_color(s_day_layer, GColorLightGray);
  text_layer_set_font(s_day_layer, s_smaller_font);
  text_layer_set_text_alignment(s_day_layer, GTextAlignmentCenter);
  
  //Create Battery display
  s_battery_layer = layer_create(bounds);
  layer_set_update_proc(s_battery_layer, layer_update_proc);
  
  update_time();
  update_date();
  battery_state_handler(battery_state_service_peek());
  
  layer_add_child(window_layer, text_layer_get_layer(s_timehours_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_timemins_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_time_ampm_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_steps_layer));
  layer_add_child(window_layer, text_layer_get_layer(s_day_layer));
  layer_add_child(window_layer, s_battery_layer);
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_timehours_layer);
  text_layer_destroy(s_timemins_layer);
  text_layer_destroy(s_time_ampm_layer);
  text_layer_destroy(s_steps_layer);
  text_layer_destroy(s_day_layer);
  layer_destroy(s_battery_layer);
  fonts_unload_custom_font(s_timehour_font);
  fonts_unload_custom_font(s_timemin_font);
  fonts_unload_custom_font(s_smaller_font);
  fonts_unload_custom_font(s_timeampm_font);
}

static void init() {
  s_main_window = window_create();
  
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  window_set_background_color(s_main_window, GColorBlack);
  
  window_stack_push(s_main_window, true);
  
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  battery_state_service_subscribe(battery_state_handler);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}