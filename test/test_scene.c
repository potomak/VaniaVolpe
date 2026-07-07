//
//  test_scene.c
//  Unit tests for the y-sorted action layer (DEPTH_AND_CAMERA.md Phase 1) and
//  the depth bands / actor variants (Phase 2): actor_feet_y,
//  action_layer_order, depth_variant_for, actor_set_variant and the per-band
//  speed scale. Everything is synthetic — no window, renderer, or assets;
//  actors get hand-built animation frames where the checks need them.
//

#include <math.h>
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
static const ActorVariantSpec BARE_VARIANTS[] = {
    {.anims = NULL, .anims_length = 0, .speed_scale = 1.0F},
};
static const ActorSpec BARE_SPEC = {
    .id = "test_actor",
    .display_name = "Test",
    .assets_dir = "test",
    .velocity = 100,
    .idle_state = IDLE,
    .move_state = WALKING,
    .variants = BARE_VARIANTS,
    .variants_length = 1,
};

// A two-variant spec whose far variant moves at half speed. The animations
// are never loaded from disk — make_actor builds their tables from the spec
// alone, which is all walking and variant switching touch.
static const ActorAnimSpec VARIANT_ANIMS[] = {
    {IDLE, "idle.png", "idle.anim", 1, LOOP},
    {WALKING, "walking.png", "walking.anim", 1, LOOP},
};
static const ActorVariantSpec TWO_VARIANTS[] = {
    {.anims = VARIANT_ANIMS, .anims_length = 2, .speed_scale = 1.0F},
    {.anims = VARIANT_ANIMS, .anims_length = 2, .speed_scale = 0.5F},
};
static const ActorSpec TWO_VARIANT_SPEC = {
    .id = "test_two_variants",
    .display_name = "Test",
    .assets_dir = "test",
    .velocity = 100,
    .idle_state = IDLE,
    .move_state = WALKING,
    .variants = TWO_VARIANTS,
    .variants_length = 2,
};

// Give an actor a WALKING animation with one frame_h-tall frame, so
// reference_animation resolves and feet y = centre y + frame_h / 2.
static void give_reference_frame(Actor *actor, int frame_h) {
  AnimationData *animation = make_animation_data(1, LOOP);
  animation->sprite_clips[0] = (SDL_Rect){0, 0, 220, frame_h};
  actor->animations[0][WALKING] = animation;
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
  int order[4]; // 3 props + 1 actor: the largest set below

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

  // ── depth_variant_for ─────────────────────────────────────────────────────

  // Mirrors the shape a scene would declare: near ground at the bottom,
  // farther bands higher up (variant indices need not be ordered).
  const DepthBand bands[] = {{0, 2}, {400, 1}, {520, 0}};
  check(depth_variant_for(bands, 3, 120.0F) == 2,
        "feet in the first band pick its variant");
  check(depth_variant_for(bands, 3, 399.0F) == 2,
        "feet just above a boundary stay in the earlier band");
  check(depth_variant_for(bands, 3, 400.0F) == 1,
        "feet exactly on a boundary enter the later band");
  check(depth_variant_for(bands, 3, 700.0F) == 0,
        "feet beyond the last band pick the last variant");
  const DepthBand offset_bands[] = {{300, 1}};
  check(depth_variant_for(offset_bands, 1, 100.0F) == 1,
        "feet above every band fall back to the first band's variant");

  // ── actor_set_variant + speed scale ───────────────────────────────────────

  Actor *walker = make_actor(&TWO_VARIANT_SPEC, (SDL_FPoint){0, 0});
  actor_walk_to(walker, (SDL_FPoint){100, 0}, NULL);
  actor_update(walker, 0.1F);
  check(fabsf(walker->current_position.x - 10.0F) < 0.001F,
        "variant 0 walks at the spec velocity");

  // Pin the live walking animation's timing and facing, then switch.
  AnimationData *near_walk = walker->animations[0][WALKING];
  AnimationData *far_walk = walker->animations[1][WALKING];
  near_walk->start_time = 123;
  near_walk->flip = SDL_FLIP_HORIZONTAL;
  actor_set_variant(walker, 1);
  check(walker->variant == 1 && far_walk->is_playing &&
            far_walk->start_time == 123 &&
            far_walk->flip == SDL_FLIP_HORIZONTAL,
        "a variant switch hands the walk cycle over mid-stride");
  check(!near_walk->is_playing,
        "the old variant's animation stops without restarting anything");

  actor_update(walker, 0.1F);
  check(fabsf(walker->current_position.x - 15.0F) < 0.001F,
        "the far variant's speed scale slows the walk");

  actor_set_variant(walker, 5);
  actor_set_variant(walker, -1);
  check(walker->variant == 1, "out-of-range variants are ignored");

  actor_free(walker);

  // Variants whose frame counts disagree with variant 0 would make the
  // mid-cycle handover glitch; actor_load_media must reject them before it
  // ever touches a file (so no renderer or assets are needed here).
  static const ActorAnimSpec MISMATCHED_ANIMS[] = {
      {IDLE, "idle.png", "idle.anim", 1, LOOP},
      {WALKING, "walking.png", "walking.anim", 3, LOOP}, // variant 0 has 1
  };
  static const ActorVariantSpec MISMATCHED_VARIANTS[] = {
      {.anims = VARIANT_ANIMS, .anims_length = 2, .speed_scale = 1.0F},
      {.anims = MISMATCHED_ANIMS, .anims_length = 2, .speed_scale = 0.6F},
  };
  static const ActorSpec MISMATCHED_SPEC = {
      .id = "test_mismatch",
      .display_name = "Test",
      .assets_dir = "test",
      .velocity = 100,
      .idle_state = IDLE,
      .move_state = WALKING,
      .variants = MISMATCHED_VARIANTS,
      .variants_length = 2,
  };
  Actor *mismatched = make_actor(&MISMATCHED_SPEC, (SDL_FPoint){0, 0});
  check(!actor_load_media(mismatched, NULL),
        "variants with mismatched frame counts fail loudly at load");
  actor_free(mismatched);

  return failures;
}
