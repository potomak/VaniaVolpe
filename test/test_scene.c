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
#include "clock.h"
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

// A fidgeting spec (LIVELINESS.md Part 1): synthetic fidgets, never loaded
// from disk — make_actor builds their AnimationData from the spec, which is
// all the trigger/interrupt logic touches. Fidget lists are per variant:
// variant 0 has two, variant 1 has none, variant 2 has its own.
static const ActorFidgetSpec TEST_FIDGETS[] = {
    {"peck.png", "peck.anim", 2, 0},
    {"blink.png", "blink.anim", 1, 0},
};
static const ActorFidgetSpec FAR_TEST_FIDGETS[] = {
    {"peck_far.png", "peck_far.anim", 2, 0},
};
static const ActorVariantSpec FIDGET_VARIANTS[] = {
    {.anims = VARIANT_ANIMS,
     .anims_length = 2,
     .speed_scale = 1.0F,
     .fidgets = TEST_FIDGETS,
     .fidgets_length = 2},
    {.anims = VARIANT_ANIMS, .anims_length = 2, .speed_scale = 1.0F},
    {.anims = VARIANT_ANIMS,
     .anims_length = 2,
     .speed_scale = 1.0F,
     .fidgets = FAR_TEST_FIDGETS,
     .fidgets_length = 1},
};
static const ActorSpec FIDGET_SPEC = {
    .id = "test_fidgeter",
    .display_name = "Test",
    .assets_dir = "test",
    .velocity = 100,
    .idle_state = IDLE,
    .move_state = WALKING,
    .variants = FIDGET_VARIANTS,
    .variants_length = 3,
};

// Give an actor a WALKING animation with one frame_h-tall frame, so
// reference_animation resolves and feet y = centre y + frame_h / 2.
static void give_reference_frame(Actor *actor, int frame_h) {
  AnimationData *animation = make_animation_data(1, LOOP);
  animation->sprite_clips[0] = (SDL_Rect){0, 0, 220, frame_h};
  actor->animations[0][WALKING] = animation;
}

// Toggleable predicates for the hotspot active-animation sync test below.
static bool pred_a_on;
static bool pred_b_on;
static bool pred_a(void) { return pred_a_on; }
static bool pred_b(void) { return pred_b_on; }

// A complementary pair, like the sunscreen bottle's before/after rows: exactly
// one is enabled at a time, and they share one boil the object shows in both
// states.
static bool sunscreen_on;
static bool bottle_before(void) { return !sunscreen_on; }
static bool bottle_after(void) { return sunscreen_on; }

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

  // No props: the actor is the whole action layer — the scene_default_render
  // path for a scene that declares none (#149/#154). The props loop must run
  // zero times without touching the NULL props pointer.
  actor->current_position = (SDL_FPoint){400, 300};
  count = action_layer_order(NULL, 0, &actor, 1, order);
  check(order_is(order, count, (int[]){0}, 1),
        "with no props the actor is the only drawable");

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

  // ── planes: parallax offset + coverage ────────────────────────────────────

  const SDL_Point wide = {1600, 600};

  // A fixed plane (parallax 0) never moves with the camera.
  Plane sky = {.image = {NULL, "sky.png", "field", 800, 600}, .parallax = 0.0F};
  SDL_Point at = plane_screen_pos(&sky, (SDL_FPoint){500, 0});
  check(at.x == 0 && at.y == 0, "a parallax-0 plane stays fixed");

  // A scene-locked plane (parallax 1) tracks the camera one-to-one.
  Plane ground = {.image = {NULL, "ground.png", "field", 1600, 600},
                  .parallax = 1.0F};
  at = plane_screen_pos(&ground, (SDL_FPoint){500, 0});
  check(at.x == -500, "a parallax-1 plane moves with the camera");

  // A foreground plane (parallax > 1) slides past faster, and its origin
  // offsets the screen position.
  Plane bushes = {.image = {NULL, "bushes.png", "field", 1720, 140},
                  .parallax = 1.5F,
                  .origin = {0, 460}};
  at = plane_screen_pos(&bushes, (SDL_FPoint){200, 0});
  check(at.x == -300 && at.y == 460,
        "a foreground plane slides faster than the camera, from its origin");

  // Coverage: the image must span WINDOW + parallax * (scene - WINDOW).
  // ground at 1600 wide exactly covers a 1600-wide scene at parallax 1.
  check(plane_covers_view(&ground, wide), "a 1600px parallax-1 plane covers");
  check(plane_covers_view(&sky, wide), "an 800px parallax-0 plane covers");
  Plane narrow = {.image = {NULL, "narrow.png", "field", 1000, 600},
                  .parallax = 1.0F};
  check(!plane_covers_view(&narrow, wide),
        "a too-narrow parallax-1 plane fails coverage");
  // hills at 1120 exactly meet 800 + 0.4 * 800 = 1120.
  Plane hills = {.image = {NULL, "hills.png", "field", 1120, 600},
                 .parallax = 0.4F};
  check(plane_covers_view(&hills, wide),
        "a 1120px parallax-0.4 plane exactly covers");

  // ── boiling hotspots: sync_hotspot_active_anims ───────────────────────────

  fprintf(stderr, "\nboiling hotspots:\n");

  AnimationData *boil_a = make_animation_data(3, LOOP);
  AnimationData *boil_b = make_animation_data(3, LOOP);
  // Two hotspots share boil_a (an object with a before/after pair); a third
  // always-on hotspot carries boil_b. A fourth hotspot has no active_anim.
  Hotspot hs[4] = {
      {.rect = {0, 0, 10, 10}, .enabled = pred_a, .active_anim = boil_a},
      {.rect = {0, 0, 10, 10}, .enabled = pred_b, .active_anim = boil_a},
      {.rect = {0, 0, 10, 10}, .active_anim = boil_b},
      {.rect = {0, 0, 10, 10}, .enabled = pred_a},
  };
  Scene s = {0};
  s.hotspots = hs;
  s.hotspots_length = 4;

  pred_a_on = false;
  pred_b_on = false;
  sync_hotspot_active_anims(&s);
  check(!boil_a->is_playing,
        "a shared boil freezes when neither carrier is enabled");
  check(boil_b->is_playing, "an always-enabled hotspot's boil plays");

  pred_b_on = true;
  sync_hotspot_active_anims(&s);
  check(boil_a->is_playing,
        "a shared boil plays when either carrier is enabled (OR)");

  pred_a_on = true;
  pred_b_on = false;
  sync_hotspot_active_anims(&s);
  check(boil_a->is_playing, "and plays on the other carrier alone");

  pred_a_on = false;
  sync_hotspot_active_anims(&s);
  check(!boil_a->is_playing, "and refreezes once every carrier is disabled");

  // The exact case the OR-across-carriers logic exists for (the sunscreen
  // bottle): two complementary rows share one boil, so the object boils in
  // *both* states — including the state where the enabled row comes before the
  // disabled one in the table, which a naive per-row play/freeze would let the
  // later, disabled row freeze.
  AnimationData *bottle_boil = make_animation_data(3, LOOP);
  Hotspot bottle[2] = {
      {.rect = {0, 0, 10, 10},
       .enabled = bottle_before,
       .active_anim = bottle_boil},
      {.rect = {0, 0, 10, 10},
       .enabled = bottle_after,
       .active_anim = bottle_boil},
  };
  Scene bs = {0};
  bs.hotspots = bottle;
  bs.hotspots_length = 2;

  sunscreen_on = false; // row 0 (before) enabled, row 1 (after) disabled
  sync_hotspot_active_anims(&bs);
  check(bottle_boil->is_playing,
        "a shared boil stays on when the later row is the disabled one");

  sunscreen_on = true; // row 0 disabled, row 1 enabled — the other ordering
  sync_hotspot_active_anims(&bs);
  check(bottle_boil->is_playing,
        "and across the complementary state, so it boils throughout");

  free_animation(bottle_boil);
  free_animation(boil_a);
  free_animation(boil_b);

  // ── idle fidgets (LIVELINESS.md Part 1) ───────────────────────────────────

  fprintf(stderr, "\nidle fidgets:\n");

  Actor *fidgeter = make_actor(&FIDGET_SPEC, (SDL_FPoint){100, 100});
  check(fidgeter->state == IDLE &&
            fidgeter->next_fidget_at >= clock_now_ms() + FIDGET_MIN_DELAY_MS,
        "a fresh actor is idle with a fidget timer at least MIN_DELAY out");

  actor_update(fidgeter, 1.0F / 30.0F);
  check(fidgeter->state == IDLE, "no fidget before the rolled delay");

  // Force the timer: the next update picks a fidget and plays it.
  fidgeter->next_fidget_at = 0;
  actor_update(fidgeter, 1.0F / 30.0F);
  bool playing = fidgeter->fidget_anims[0][fidgeter->active_fidget] != NULL &&
                 fidgeter->fidget_anims[0][fidgeter->active_fidget]->is_playing;
  check(fidgeter->state == FIDGETING && playing &&
            fidgeter->active_fidget >= 0 && fidgeter->active_fidget < 2,
        "the elapsed timer starts one of the spec's fidgets");

  // A walk interrupts the fidget instantly and stops its animation.
  AnimationData *interrupted =
      fidgeter->fidget_anims[0][fidgeter->active_fidget];
  actor_walk_to(fidgeter, (SDL_FPoint){200, 100}, NULL);
  check(fidgeter->state == WALKING && !interrupted->is_playing,
        "a tap wins over a fidget: the walk starts, the fidget stops");
  for (int i = 0; i < 300 && fidgeter->state == WALKING; i++) {
    actor_update(fidgeter, 1.0F / 30.0F);
  }
  check(fidgeter->state == IDLE &&
            fidgeter->next_fidget_at >= clock_now_ms() + FIDGET_MIN_DELAY_MS,
        "arriving re-enters IDLE and re-rolls the fidget timer");

  // A finished one-shot fidget returns to IDLE on its own.
  fidgeter->next_fidget_at = 0;
  actor_update(fidgeter, 1.0F / 30.0F);
  AnimationData *beat = fidgeter->fidget_anims[0][fidgeter->active_fidget];
  beat->start_time = (int)clock_now_ms() - 10000; // age it past its runtime
  actor_update(fidgeter, 1.0F / 30.0F);
  actor_update(fidgeter, 1.0F / 30.0F);
  check(fidgeter->state == IDLE && !beat->is_playing &&
            fidgeter->next_fidget_at >= clock_now_ms() + FIDGET_MIN_DELAY_MS,
        "a finished fidget returns to IDLE with a fresh timer");

  // A grab interrupts a fidget too.
  fidgeter->next_fidget_at = 0;
  actor_update(fidgeter, 1.0F / 30.0F);
  check(fidgeter->state == FIDGETING, "fidgeting again");
  check(actor_begin_drag(fidgeter) && fidgeter->state == DRAGGED,
        "a grab wins over a fidget");
  actor_drop(fidgeter, fidgeter->current_position);
  for (int i = 0; i < 30 && fidgeter->state != IDLE; i++) {
    actor_update(fidgeter, 1.0F / 30.0F);
  }

  // Fidget lists are per variant: one without any never fidgets, one with
  // its own uses it.
  actor_set_variant(fidgeter, 1);
  fidgeter->next_fidget_at = 0;
  actor_update(fidgeter, 1.0F / 30.0F);
  check(fidgeter->state == IDLE, "a variant without fidgets never fidgets");
  actor_set_variant(fidgeter, 2);
  fidgeter->next_fidget_at = 0;
  actor_update(fidgeter, 1.0F / 30.0F);
  check(fidgeter->state == FIDGETING && fidgeter->active_fidget == 0 &&
            fidgeter->fidget_anims[2][0] != NULL &&
            fidgeter->fidget_anims[2][0]->is_playing,
        "a variant with its own fidgets plays from its own list");
  actor_free(fidgeter);

  // An actor with no fidget list never fidgets.
  Actor *still = make_actor(&BARE_SPEC, (SDL_FPoint){0, 0});
  still->next_fidget_at = 0;
  actor_update(still, 1.0F / 30.0F);
  check(still->state == IDLE, "an actor with no fidgets stays still");
  actor_free(still);

  return failures;
}
