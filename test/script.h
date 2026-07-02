// A data-driven playthrough script, shared with the browser test.
//
// The steps come from test/scripts/<name>.json (the single source of truth):
// tools/gen_playtest.py turns that JSON into a generated header of ScriptStep[]
// for the native test, while test/web/run_playtest.js reads the same JSON. Keep
// the two interpreters (script_run below and the JS runner) in lockstep.

#ifndef test_script_h
#define test_script_h

#include <stddef.h>

typedef enum {
  SCRIPT_CLICK, // click at (x, y), then wait wait_ms
  SCRIPT_BRUSH, // hold + serpentine over the [x0,x1]x[y0,y1] box, rows sweeps
  SCRIPT_WAIT,  // just run the loop for wait_ms
  SCRIPT_SCREENSHOT, // browser: save a PNG; native: assert the frame isn't
                     // blank
} ScriptAction;

typedef struct {
  ScriptAction action;
  double x, y;           // SCRIPT_CLICK (screen fractions, 0..1)
  double x0, x1, y0, y1; // SCRIPT_BRUSH extent (screen fractions)
  int rows;              // SCRIPT_BRUSH sweep count
  int wait_ms;           // pause after the action
  const char *name;      // SCRIPT_SCREENSHOT label
} ScriptStep;

// Number of horizontal samples per brush row. A shared constant so the native
// and browser brushes paint identically.
#define SCRIPT_BRUSH_SAMPLES 16

// Run a scripted playthrough through the harness. SCRIPT_SCREENSHOT steps
// assert the current frame has some colour variation (the native stand-in for
// the browser screenshot). Returns the number of blank-frame failures.
int script_run(const ScriptStep *steps, size_t count);

#endif /* test_script_h */
