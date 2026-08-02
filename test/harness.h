// Reusable headless-test harness for Tiny Adventures.
//
// It runs the game with no display server or audio hardware (offscreen SDL
// video driver + software renderer + dummy audio — the same trick the terminal
// build uses), drives it with scripted mouse events pushed onto SDL's own
// queue, and captures the game's SDL_Log output so tests can assert on it.
//
// A play-test (e.g. play_gina) uses these primitives to script an adventure;
// main_test wires the harness up and runs the play-tests.

#ifndef test_harness_h
#define test_harness_h

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

// ── lifecycle ────────────────────────────────────────────────────────────────

// Bring up SDL headless (offscreen video, software renderer, dummy audio) plus
// the image/mixer subsystems. Returns false on failure.
bool harness_init(void);

// Tear down media and SDL.
void harness_shutdown(void);

// Register the adventures, initialise the game, load all media, and activate
// the hub's entry scene — leaving the game ready at the hub. Returns false on
// failure.
bool harness_start_game(void);

// ── loop ─────────────────────────────────────────────────────────────────────

// One game-loop iteration: drain input, update, render.
void harness_step_frame(void);

// Run the loop for ms milliseconds of wall-clock time, so walks and spoken
// lines have time to finish.
void harness_pump_for(Uint32 ms);

// ── scripted input (fractional screen coordinates, 0..1) ─────────────────────

void harness_mouse_move(double fx, double fy);
void harness_mouse_down(double fx, double fy);
void harness_mouse_up(double fx, double fy);
// A click: move (so scenes that track the cursor see it), then press + release.
void harness_click(double fx, double fy);

// Touch input, in window-normalized coordinates like SDL's own finger events.
// harness_tap is one finger down-and-up; the id lets a test hold two at once.
void harness_finger_down(SDL_FingerID id, double fx, double fy);
void harness_finger_up(SDL_FingerID id, double fx, double fy);
void harness_tap(double fx, double fy);

// A key press; `repeat` marks it as an auto-repeat, as SDL does while held.
void harness_key_down(SDL_Keycode key, bool repeat);

// A mouse event carrying SDL_TOUCH_MOUSEID, as SDL synthesizes from a touch.
void harness_synthesized_click(double fx, double fy);

// ── assertions ───────────────────────────────────────────────────────────────

// True if the current frame has more than one distinct colour — a cheap guard
// against a blank-screen / missing-texture regression, without pinning exact
// pixels.
bool harness_frame_has_variation(void);

// Install an SDL_Log sink that captures the game's log output (all dialogue) so
// tests can assert on it. Call once, before starting the game. Returns true.
bool harness_capture_begin(void);

// Check that each expected substring appears, in order, in the captured log.
// Prints an OK/MISS line per expectation to stderr. Returns the number of
// missing expectations (0 = all present).
// Report one assertion in the harness's OK/MISS format, for a play-test
// checking something the dialogue log cannot express — which scene the game
// ended on, say. Returns 1 when it failed, so callers can add it to a total.
// Empty the captured log. The capture runs for the whole process, so a test
// that asserts something did *not* happen has to start from a clean buffer —
// otherwise it sees an earlier test's matching line.
void harness_log_reset(void);

// Whether the captured log holds the given substring. Silent, unlike
// harness_check_lines_in_order, which reports every line it looks for — the
// wrong shape when the expected answer is "absent".
bool harness_log_contains(const char *needle);

int harness_check(bool ok, const char *what);

int harness_check_lines_in_order(const char *const *expected, size_t count);

#endif /* test_harness_h */
