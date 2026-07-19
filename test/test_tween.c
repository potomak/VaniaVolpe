//
//  test_tween.c
//  Unit tests for the scene-object tween helper (tween.{c,h}, #107): easing,
//  the parabolic hop, scale interpolation, exact landing, the one-shot end
//  callback and chaining.
//

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "test_tween.h"
#include "tween.h"

static int failures;

static void check(bool ok, const char *what) {
  if (ok) {
    fprintf(stderr, "OK    %s\n", what);
  } else {
    fprintf(stderr, "MISS  %s\n", what);
    failures++;
  }
}

static bool near_f(float a, float b) { return fabsf(a - b) < 0.001F; }

static int end_calls;
static void count_end(void) { end_calls++; }

// Chaining: the first tween's on_end starts a second one on the same struct.
static Tween chain;
static void chain_next(void) {
  tween_start(&chain, (SDL_FPoint){100, 0}, (SDL_FPoint){200, 0}, 1000,
              TWEEN_LINEAR, NULL);
}

int test_tween(void) {
  failures = 0;
  fprintf(stderr, "\n-- tween unit tests --\n");

  // A linear tween crosses the midpoint at half time.
  Tween t;
  tween_start(&t, (SDL_FPoint){0, 0}, (SDL_FPoint){100, 50}, 1000, TWEEN_LINEAR,
              NULL);
  check(t.active && near_f(tween_pos(&t).x, 0) && near_f(tween_pos(&t).y, 0),
        "a started tween begins at its origin");
  tween_update(&t, 0.5F);
  check(near_f(tween_pos(&t).x, 50) && near_f(tween_pos(&t).y, 25),
        "linear easing crosses the midpoint at half time");
  check(near_f(tween_scale(&t), 1), "scale defaults to 1 throughout");

  // Ease-in starts slower than linear; ease-out starts faster.
  Tween in, out;
  tween_start(&in, (SDL_FPoint){0, 0}, (SDL_FPoint){100, 0}, 1000,
              TWEEN_EASE_IN, NULL);
  tween_start(&out, (SDL_FPoint){0, 0}, (SDL_FPoint){100, 0}, 1000,
              TWEEN_EASE_OUT, NULL);
  tween_update(&in, 0.25F);
  tween_update(&out, 0.25F);
  check(tween_pos(&in).x < 25 - 0.001F, "ease-in lags linear early on");
  check(tween_pos(&out).x > 25 + 0.001F, "ease-out leads linear early on");

  // The arc lifts y mid-flight and vanishes at both ends.
  Tween hop;
  tween_start(&hop, (SDL_FPoint){0, 100}, (SDL_FPoint){100, 100}, 1000,
              TWEEN_LINEAR, NULL);
  hop.arc_height = 40;
  check(near_f(tween_pos(&hop).y, 100), "the hop starts on the ground");
  tween_update(&hop, 0.5F);
  check(near_f(tween_pos(&hop).y, 60), "the hop peaks at arc_height mid-way");
  tween_update(&hop, 0.5F);
  check(near_f(tween_pos(&hop).y, 100), "the hop lands back on the path");

  // Scale interpolates from from_scale to to_scale.
  Tween shrink;
  tween_start(&shrink, (SDL_FPoint){0, 0}, (SDL_FPoint){100, 0}, 1000,
              TWEEN_LINEAR, NULL);
  shrink.from_scale = 1.0F;
  shrink.to_scale = 0.5F;
  tween_update(&shrink, 0.5F);
  check(near_f(tween_scale(&shrink), 0.75F), "scale is halfway at half time");

  // Completion: lands exactly on the target, deactivates, fires on_end once.
  Tween done;
  end_calls = 0;
  tween_start(&done, (SDL_FPoint){0, 0}, (SDL_FPoint){100, 50}, 1000,
              TWEEN_BOUNCE, count_end);
  done.arc_height = 40;
  bool still_running = tween_update(&done, 5.0F); // huge delta overshoots
  check(!still_running && !done.active, "a finished tween deactivates");
  check(near_f(tween_pos(&done).x, 100) && near_f(tween_pos(&done).y, 50),
        "a finished tween sits exactly on its target (no arc, full bounce)");
  check(end_calls == 1, "on_end fires exactly once");
  tween_update(&done, 1.0F);
  check(end_calls == 1, "updating a finished tween does nothing");

  // Bounce ends at the target like every ease (the curve's last point is 1).
  Tween bounce;
  tween_start(&bounce, (SDL_FPoint){0, 0}, (SDL_FPoint){0, 200}, 1000,
              TWEEN_BOUNCE, NULL);
  tween_update(&bounce, 0.999F);
  float near_end = tween_pos(&bounce).y;
  check(near_end > 190, "the bounce settles near the target at the end");

  // Chaining: on_end may start the next leg on the same struct.
  end_calls = 0;
  tween_start(&chain, (SDL_FPoint){0, 0}, (SDL_FPoint){100, 0}, 1000,
              TWEEN_LINEAR, chain_next);
  tween_update(&chain, 2.0F);
  check(chain.active && near_f(chain.from.x, 100),
        "on_end may chain a new tween on the same struct");

  return failures;
}
