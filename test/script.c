#include "script.h"

#include <stdio.h>

#include "harness.h"

// The sunscreen minigame gesture: hold the button and serpentine over the
// close-up so every cell gets painted.
static void run_brush(const ScriptStep *s) {
  harness_mouse_down(s->x0, s->y0);
  for (int row = 0; row < s->rows; row++) {
    double t = s->rows > 1 ? (double)row / (s->rows - 1) : 0.0;
    double y = s->y0 + t * (s->y1 - s->y0);
    double from = row % 2 == 0 ? s->x0 : s->x1;
    double to = row % 2 == 0 ? s->x1 : s->x0;
    for (int k = 0; k <= SCRIPT_BRUSH_SAMPLES; k++) {
      harness_mouse_move(from + (to - from) * k / SCRIPT_BRUSH_SAMPLES, y);
    }
    harness_step_frame(); // let the paint register progressively
  }
  harness_mouse_up(s->x1, s->y0);
  harness_pump_for(s->wait_ms);
}

int script_run(const ScriptStep *steps, size_t count) {
  int blanks = 0;
  for (size_t i = 0; i < count; i++) {
    const ScriptStep *s = &steps[i];
    switch (s->action) {
    case SCRIPT_CLICK:
      harness_click(s->x, s->y);
      harness_pump_for(s->wait_ms);
      break;
    case SCRIPT_BRUSH:
      run_brush(s);
      break;
    case SCRIPT_WAIT:
      harness_pump_for(s->wait_ms);
      break;
    case SCRIPT_SCREENSHOT:
      if (harness_frame_has_variation()) {
        fprintf(stderr, "OK    frame '%s' rendered (non-blank)\n", s->name);
      } else {
        fprintf(stderr, "MISS  frame '%s' was blank\n", s->name);
        blanks++;
      }
      break;
    }
  }
  return blanks;
}
