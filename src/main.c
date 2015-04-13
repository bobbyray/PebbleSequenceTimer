// Grant of license under MIT License (MIT) stated in license.h.
#include "license.h"
#include <pebble.h>
  
static Window *s_main_window;
static TextLayer *s_status_layer; // Text msgs on watch face.

enum EVibe { NONE, SHORT, LONG}; // Type of vibration.
// A step in the sequence of steps.
struct Step {
  int secs; // number of sections for step.
  char* descr; // description of step
  enum EVibe vibe;  // Type of vibration.
};

// Action at completion of sequence.
struct Completion {
  int nVibes;     // Number of vibration to issue. 0 for no vibrations at end.
  enum EVibe vibe; // Type of vibe.
  int msSleep; // Number of milliseconds to wait between vibes.
};

// Steps in the sequence.
static struct Step steps[] = {{.secs = 5, .descr = "Get Ready", .vibe = NONE},
                              {.secs = 30, .descr = "Quad1", .vibe = LONG}, 
                              {.secs = 30, .descr = "Quad2", .vibe = LONG},
                              {.secs = 30, .descr = "Quad3", .vibe = LONG},
                              {.secs = 30, .descr = "Quad4", .vibe = LONG}
                             };
// Action at completion of sequence.
static struct Completion s_completion = {.nVibes = 4, .vibe = SHORT, .msSleep = 333};

const int N_STEPS = sizeof(steps) / sizeof (struct Step);
const int START_STEP_IX = 1; // Index of first step for which timing starts.
                             // Note: Step 0 is the Get Ready Step and is before the starting time.

static int s_curStep = 0;
static int s_stepSecsLeft = 0;
static int s_stepsSecs = 0;   // Total seconds for the steps (starting with starting step).
static struct tm s_startTime; // Starting time of the step for which timing starts.
static char s_status_hdr_buffer[32]; // Completion msg heading status after timer sequence has completed.

static const char* AmOrPm(struct tm* tick_time) {
  static const char* xm = "p";
  if (tick_time->tm_hour < 12)
    xm = "a";
  return xm;
};


// Tick event handler for change every mninute to show time of day.
// Handler enabled after timing sequence has completed.
static void tick_handler_minute(struct tm* tick_time, TimeUnits units_changed) {
  // Use 12 hour format
  // Create a long-lived buffer
  static char s_time_buffer[] = "00:00";
  strftime(s_time_buffer, sizeof("00:00"), "%I:%M", tick_time);
  const char* xm_time = AmOrPm(tick_time);
  
  static char s_status_buffer[64];
  snprintf(s_status_buffer, sizeof(s_status_buffer), "%sNow: %s%s", 
           s_status_hdr_buffer, s_time_buffer, xm_time);
  
  text_layer_set_text(s_status_layer, s_status_buffer);
};

// Tick event handler for change every second during the timing sequence.
static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  if (s_curStep >= N_STEPS)
    return; // Should not happen.
  
  // Ensure start time is initialized, but this will be over-written
  // later by start time of the actual starting step. 
  if (s_stepsSecs == 0)
    s_startTime = *tick_time;
  
  // Use a long-lived buffer
  static char s_status_buffer[32];
  
  struct Step* pStep = &steps[s_curStep];
  if (s_stepSecsLeft == 0)
    s_stepSecsLeft = pStep->secs;
  
  snprintf(s_status_buffer, sizeof(s_status_buffer), "%s\nSecs Left %i", pStep->descr, s_stepSecsLeft);

  // Vibrate at start of a step.
  if (s_stepSecsLeft == pStep->secs) {
    if (pStep->vibe == LONG) 
      vibes_long_pulse();
    else if (pStep->vibe == SHORT)
      vibes_short_pulse();
    // Save start time for starting step.
    if (s_curStep == START_STEP_IX) {
      s_startTime = *tick_time;
      s_stepsSecs = 0; 
    }
  }
  
  s_stepSecsLeft--;
  s_stepsSecs++;
  
  // Advance to next step when current step is finished.
  if (s_stepSecsLeft == 0) {
    s_curStep++;
  }
  
  if (s_curStep >= N_STEPS) {
    // Do vibrations to indicate completion of the sequence.
    for (int i=0; i < s_completion.nVibes; i++) {
      if (s_completion.vibe == LONG)
        vibes_long_pulse();
      else if (s_completion.vibe == SHORT)
        vibes_short_pulse();
      if (s_completion.msSleep > 0)
        psleep(s_completion.msSleep); // Wait specified milliseconds between vibes.
    }
    // Set completion heading for status.
    const char* xm_start = AmOrPm(&s_startTime);
    static char s_start_time_buffer[] = "00:00:00";
    strftime(s_start_time_buffer, sizeof("00:00:00"), "%I:%M:%S", &s_startTime);
    snprintf(s_status_hdr_buffer, sizeof(s_status_hdr_buffer), "Start: %s%s for %ds\n",
             s_start_time_buffer, xm_start, s_stepsSecs);
    
    tick_timer_service_unsubscribe();
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler_minute);
  }
  
    
  // Update the TextLayer
  text_layer_set_text(s_status_layer, s_status_buffer);
}


static void main_window_load(Window *window) {
  // Todo: Create and add child windows (text layers).
  // Create status msg TextLayer
  s_status_layer = text_layer_create(GRect(0, 20, 144, 150));
  text_layer_set_background_color(s_status_layer, GColorClear);
  text_layer_set_text_color(s_status_layer, GColorBlack);
  text_layer_set_text(s_status_layer, "00:00");
  
  // Improve the layout to use larger, bolder font.
  text_layer_set_font(s_status_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(s_status_layer, GTextAlignmentCenter);
  
  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_status_layer));
}

static void main_window_unload(Window *window) {
  // Todo: Destroy child windows created by main_window_load(..).
  // Destroy status msg TextLayer
  text_layer_destroy(s_status_layer);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();
  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  
  // Todo: more initialization. Register ui handlers, etc.
  
  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);
  
  // Subscribe to TickTimerService
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

}

static void deinit() {
  tick_timer_service_unsubscribe();
  // Destroy Window created by init(..).
   window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}




