//
//  test_confirm.c
//  The leave-the-adventure question (src/confirm.c). Pure state: it owns a
//  flag and a callback, so the tests drive it with synthetic events.
//

#include <stdio.h>

#include "confirm.h"
#include "constants.h"

#include "test_confirm.h"

static int failures;

static void check(bool ok, const char *what) {
  if (ok) {
    fprintf(stderr, "OK    %s\n", what);
  } else {
    fprintf(stderr, "MISS  %s\n", what);
    failures++;
  }
}

static int confirmed_count;
static void on_confirm(void) { confirmed_count++; }

static SDL_Event tap_release(int x, int y) {
  SDL_Event e = {.type = SDL_MOUSEBUTTONUP};
  e.button.x = x;
  e.button.y = y;
  return e;
}

// Mirrors confirm.c: the answers sit either side of a centred panel.
static const int MID_Y = WINDOW_HEIGHT / 2;
static const int YES_X = (WINDOW_WIDTH - 420) / 2 + 36 + 60;
static const int NO_X = (WINDOW_WIDTH - 420) / 2 + 420 - 36 - 60;

int test_confirm(void) {
  fprintf(stderr, "\n-- leave-confirmation unit tests --\n");
  failures = 0;
  confirm_close();

  // Closed, it is invisible to input: every event belongs to the scene.
  SDL_Event e = tap_release(YES_X, MID_Y);
  check(!confirm_is_open(), "it starts closed");
  check(!confirm_process_input(&e), "closed, it consumes nothing");

  // Open, it takes the whole pointer — a press that reached the scene would
  // walk the actor or start a drag behind the question.
  confirmed_count = 0;
  confirm_open(on_confirm);
  check(confirm_is_open(), "opening puts the question up");
  SDL_Event press = {.type = SDL_MOUSEBUTTONDOWN};
  press.button.x = YES_X;
  press.button.y = MID_Y;
  check(confirm_process_input(&press), "it swallows the press");
  check(confirmed_count == 0, "and the press alone answers nothing");
  SDL_Event motion = {.type = SDL_MOUSEMOTION};
  check(confirm_process_input(&motion), "it swallows motion too");

  // Yes answers, once, and closes before the callback runs — that callback
  // leaves for the hub, and the question must not still be up over it.
  e = tap_release(YES_X, MID_Y);
  check(confirm_process_input(&e), "the tick consumes the release");
  check(confirmed_count == 1, "and answers yes");
  check(!confirm_is_open(), "which closes the question");
  check(!confirm_process_input(&e),
        "a second release goes to the scene, not a closed question");
  check(confirmed_count == 1, "and does not answer twice");

  // No dismisses without answering.
  confirmed_count = 0;
  confirm_open(on_confirm);
  e = tap_release(NO_X, MID_Y);
  confirm_process_input(&e);
  check(confirmed_count == 0 && !confirm_is_open(),
        "the cross dismisses without leaving");

  // A miss is a no. Tapping past the panel is the likeliest slip of all, and
  // the forgiving answer is the one it should give.
  confirm_open(on_confirm);
  e = tap_release(4, 4);
  confirm_process_input(&e);
  check(confirmed_count == 0 && !confirm_is_open(),
        "a tap outside the panel dismisses rather than confirming");

  // Closing from outside (the adventure changed under it) drops the callback.
  confirm_open(on_confirm);
  confirm_close();
  e = tap_release(YES_X, MID_Y);
  confirm_process_input(&e);
  check(confirmed_count == 0, "a closed question cannot still be answered");

  return failures;
}
