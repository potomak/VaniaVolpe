//
//  test_scene.c
//  Unit tests for the y-sorted action layer (DEPTH_AND_CAMERA.md Phase 1):
//  actor_feet_y and action_layer_order. Everything is synthetic — no window,
//  renderer, or assets; actors get hand-built animation frames so the feet
//  math has a frame height to work with.
//

#include <stdio.h>

#include "actor.h"
#include "scene.h"

#include "test_scene.h"

static int failures;

static void check(bool ok, const char *what) {
  if (ok) {
    fprintf(stderr, "OK    %s\n", what);
  } else {
    fprintf(stderr, "MISS  %s\n", what);
    failures++;
  }
}

// A spec with no animations: enough for make_actor, and the no-frames
// fallback case for actor_feet_y.
static const ActorSpec BARE_SPEC = {
    .id = "test_actor",
    .display_name = "Test",
    .assets_dir = "test",
    .velocity = 100,
    .idle_state = IDLE,
    .move_state = WALKING,
    .anims = NULL,
    .anims_length = 0,
};

// Give an actor a WALKING animation with one frame_h-tall frame, so
// reference_animation resolves and feet y = centre y + frame_h / 2.
static void give_reference_frame(Actor *actor, int frame_h) {
  AnimationData *animation = make_animation_data(1, LOOP);
  animation->sprite_clips[0] = (SDL_Rect){0, 0, 220, frame_h};
  actor->animations[WALKING] = animation;
}

static bool order_is(const int *order, int count, const int *expected,
                     int expected_count) {
  if (count != expected_count) {
    return false;
  }
  for (int i = 0; i < count; i++) {
    if (order[i] != expected[i]) {
      return false;
    }
  }
  return true;
}

int test_scene(void) {
  failures = 0;
  fprintf(stderr, "\naction layer:\n");

  // ── actor_feet_y ──────────────────────────────────────────────────────────

  Actor *actor = make_actor(&BARE_SPEC, (SDL_FPoint){400, 450});
  check(actor_feet_y(actor) == 450.0F,
        "feet y falls back to the centre with no animation frames");

  give_reference_frame(actor, 240);
  check(actor_feet_y(actor) == 450.0F + 120.0F,
        "feet y is centre y + half the reference frame height");

  // ── action_layer_order ────────────────────────────────────────────────────

  ImageData image = {NULL, "prop.png", "test", 68, 79};
  Prop props[3] = {
      {.image = &image, .pos = {687, 503}, .baseline = 582, .visible = true},
      {.image = &image, .pos = {109, 474}, .baseline = 541, .visible = true},
      {.image = &image, .pos = {0, 0}, .baseline = 10, .visible = false},
  };
  int order[ACTION_LAYER_MAX];

  // Fox feet at 570 + 120 = 690: below (in front of) both props.
  actor->current_position = (SDL_FPoint){400, 570};
  int count = action_layer_order(props, 3, &actor, 1, order);
  check(order_is(order, count, (int[]){1, 0, 3}, 3),
        "actor below every baseline draws last (in front)");

  // Fox feet at 430 + 120 = 550: between the two baselines.
  actor->current_position = (SDL_FPoint){400, 430};
  count = action_layer_order(props, 3, &actor, 1, order);
  check(order_is(order, count, (int[]){1, 3, 0}, 3),
        "actor between baselines draws between the props");

  // Fox feet at 300 + 120 = 420: above (behind) both props.
  actor->current_position = (SDL_FPoint){400, 300};
  count = action_layer_order(props, 3, &actor, 1, order);
  check(order_is(order, count, (int[]){3, 1, 0}, 3),
        "actor above every baseline draws first (behind)");

  // Tie rule: feet exactly on a baseline → the prop draws first, the actor
  // overdraws it.
  actor->current_position = (SDL_FPoint){400, 541 - 120};
  count = action_layer_order(props, 3, &actor, 1, order);
  check(order_is(order, count, (int[]){1, 3, 0}, 3),
        "on a baseline tie the prop draws before the actor");

  // Invisible props are skipped entirely (prop 2 never appeared above).
  props[2].visible = true;
  count = action_layer_order(props, 3, &actor, 1, order);
  check(order_is(order, count, (int[]){2, 1, 3, 0}, 4),
        "a prop toggled visible joins the order");

  // Equal-baseline props keep their table order (stable sort).
  props[2].visible = false;
  Prop twins[2] = {
      {.image = &image, .baseline = 500, .visible = true},
      {.image = &image, .baseline = 500, .visible = true},
  };
  count = action_layer_order(twins, 2, NULL, 0, order);
  check(order_is(order, count, (int[]){0, 1}, 2),
        "equal-baseline props keep declaration order");

  actor_free(actor);

  return failures;
}
