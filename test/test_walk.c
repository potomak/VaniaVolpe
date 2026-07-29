//
//  test_walk.c
//  Unit tests for the walkability grid + pathfinding (MOVEMENT.md Phase 4).
//  The fixtures mirror the real playground / playground_entrance geometry so
//  the checks exercise the shipped scene data, plus a synthetic disconnected
//  layout for the best-effort case. Everything is pure geometry — no window,
//  renderer, or assets.
//

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "actor.h"
#include "clock.h"
#include "constants.h"
#include "walk.h"

#include "test_walk.h"

static int failures;

// Every fixture is a window-sized (800x600) scene.
static const SDL_Point WINDOW_SIZE = {WINDOW_WIDTH, WINDOW_HEIGHT};

// Compare only the used w x h cells: the storage beyond them is uninitialised.
static bool grids_equal(const WalkGrid *a, const WalkGrid *b) {
  if (a->w != b->w || a->h != b->h) {
    return false;
  }
  for (int cy = 0; cy < a->h; cy++) {
    if (memcmp(a->cells[cy], b->cells[cy], (size_t)a->w) != 0) {
      return false;
    }
  }
  return true;
}

static void check(bool ok, const char *what) {
  if (ok) {
    fprintf(stderr, "OK    %s\n", what);
  } else {
    fprintf(stderr, "MISS  %s\n", what);
    failures++;
  }
}

// Mirrors src/adventures/vania_fox_the_slide/playground.c.
static const SDL_Rect PLAYGROUND_WALKABLES[] = {
    {63, 333, 645, 262},
    {707, 491, 88, 105},
    {4, 531, 68, 63},
};
static const SDL_Rect PLAYGROUND_BLOCKED[] = {{236, 131, 403, 312}};
static const WalkArea PLAYGROUND_AREA = {
    PLAYGROUND_WALKABLES, LEN(PLAYGROUND_WALKABLES), PLAYGROUND_BLOCKED,
    LEN(PLAYGROUND_BLOCKED)};

// Mirrors src/adventures/vania_fox_the_slide/playground_entrance.c.
static const SDL_Rect ENTRANCE_WALKABLES[] = {{1, 312, 795, 286}};
static const SDL_Rect ENTRANCE_BLOCKED[] = {{80, 341, 250, 179}};
static const WalkArea ENTRANCE_AREA = {ENTRANCE_WALKABLES,
                                       LEN(ENTRANCE_WALKABLES),
                                       ENTRANCE_BLOCKED, LEN(ENTRANCE_BLOCKED)};
static const SDL_FPoint SHOVEL_POI = {258, 507}; // inside the sandbox

// Two walkable islands with no connection: the best-effort case.
static const SDL_Rect ISLAND_WALKABLES[] = {{0, 0, 100, 100},
                                            {300, 300, 100, 100}};
static const WalkArea ISLAND_AREA = {ISLAND_WALKABLES, LEN(ISLAND_WALKABLES),
                                     NULL, 0};

// Mirrors src/adventures/gina_hen_at_the_pool/pool.c, where the walkable area
// is a function of game state (umbrella shade before the sunscreen, the whole
// poolside strip after).
static const SDL_Rect POOL_POOLSIDE_RECTS[] = {{20, 430, 760, 150}};
static const SDL_Rect POOL_SHADE_RECTS[] = {{60, 430, 200, 150}};
static const WalkArea POOL_POOLSIDE_AREA = {POOL_POOLSIDE_RECTS,
                                            LEN(POOL_POOLSIDE_RECTS), NULL, 0};
static const WalkArea POOL_SHADE_AREA = {POOL_SHADE_RECTS,
                                         LEN(POOL_SHADE_RECTS), NULL, 0};

// Same sampling rule as the smoother in walk.c: every point along the
// segment (5 px apart) must sit in a walkable cell.
static bool segment_clear(const WalkGrid *grid, SDL_FPoint a, SDL_FPoint b) {
  float dx = b.x - a.x;
  float dy = b.y - a.y;
  int steps = (int)(sqrtf(dx * dx + dy * dy) / (WALK_CELL_SIZE / 2.0F)) + 1;
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;
    SDL_Point p = {(int)(a.x + dx * t), (int)(a.y + dy * t)};
    if (!walk_grid_contains(grid, p)) {
      return false;
    }
  }
  return true;
}

// Every returned waypoint sits on walkable ground and every leg (including
// from the start) passes the line-of-sight sampling.
static bool path_is_legal(const WalkGrid *grid, SDL_FPoint from,
                          const SDL_FPoint *path, int count) {
  SDL_FPoint prev = from;
  for (int i = 0; i < count; i++) {
    SDL_Point p = {(int)path[i].x, (int)path[i].y};
    if (!walk_grid_contains(grid, p)) {
      return false;
    }
    if (!segment_clear(grid, prev, path[i])) {
      return false;
    }
    prev = path[i];
  }
  return true;
}

static void test_nearest_postcondition(const WalkGrid *grid) {
  int violations = 0;
  for (int y = 0; y < WINDOW_HEIGHT; y += 5) {
    for (int x = 0; x < WINDOW_WIDTH; x += 5) {
      SDL_FPoint n = walk_grid_nearest(grid, (SDL_Point){x, y});
      if (!walk_grid_contains(grid, (SDL_Point){(int)n.x, (int)n.y})) {
        violations++;
      }
    }
  }
  check(violations == 0, "nearest point always lands on walkable ground");
}

static void test_playground_routing(const WalkGrid *grid) {
  // Left and right of the sandbox, in the band where a straight line would
  // cut through it.
  SDL_FPoint left = {150, 400};
  SDL_FPoint right = {700, 400};
  SDL_FPoint path[ACTOR_MAX_WAYPOINTS];

  check(walk_grid_contains(grid, (SDL_Point){150, 400}) &&
            walk_grid_contains(grid, (SDL_Point){700, 400}) &&
            !walk_grid_contains(grid, (SDL_Point){400, 400}),
        "playground grid: sides walkable, sandbox blocked");
  check(!segment_clear(grid, left, right),
        "straight left-right segment crosses the sandbox");

  int count = walk_grid_find_path(grid, left, right, path, LEN(path));
  check(count >= 2, "left-to-right route bends around the sandbox");
  check(path_is_legal(grid, left, path, count),
        "left-to-right route stays on walkable ground");
  check(count >= 1 && fabsf(path[count - 1].x - right.x) < 1 &&
            fabsf(path[count - 1].y - right.y) < 1,
        "left-to-right route ends at the destination");

  int back = walk_grid_find_path(grid, right, left, path, LEN(path));
  check(back >= 2 && path_is_legal(grid, right, path, back),
        "right-to-left route is also legal");
}

static void test_determinism(const WalkGrid *grid) {
  SDL_FPoint a[ACTOR_MAX_WAYPOINTS];
  SDL_FPoint b[ACTOR_MAX_WAYPOINTS];
  int na = walk_grid_find_path(grid, (SDL_FPoint){150, 400},
                               (SDL_FPoint){700, 400}, a, LEN(a));
  int nb = walk_grid_find_path(grid, (SDL_FPoint){150, 400},
                               (SDL_FPoint){700, 400}, b, LEN(b));
  bool same = na == nb;
  for (int i = 0; same && i < na; i++) {
    same = a[i].x == b[i].x && a[i].y == b[i].y;
  }
  check(same, "same inputs produce the same path");
}

static void test_best_effort(void) {
  WalkGrid grid;
  walk_grid_build(&grid, &ISLAND_AREA, WINDOW_SIZE);
  SDL_FPoint path[ACTOR_MAX_WAYPOINTS];
  int count = walk_grid_find_path(&grid, (SDL_FPoint){50, 50},
                                  (SDL_FPoint){350, 350}, path, LEN(path));
  bool last_legal =
      count >= 1 &&
      walk_grid_contains(
          &grid, (SDL_Point){(int)path[count - 1].x, (int)path[count - 1].y});
  check(last_legal && path[count - 1].x < 300,
        "unreachable goal routes to the nearest reachable point");
}

static void test_walk_mask_format(void) {
  // Serialize -> parse round-trip on the real playground grid.
  WalkGrid built;
  walk_grid_build(&built, &PLAYGROUND_AREA, WINDOW_SIZE);
  static char buffer[WALK_FILE_MAX];
  int length = walk_grid_serialize(&built, buffer, sizeof(buffer));
  WalkGrid parsed;
  check(length > 0 && walk_grid_parse(buffer, (size_t)length, &parsed) &&
            grids_equal(&parsed, &built),
        "walk mask serialize -> parse round-trips");

  // Strict parser: each corruption rejects the whole file.
  check(!walk_grid_parse("walk 80 59\n", 11, &parsed),
        "a header promising missing rows rejects");
  check(!walk_grid_parse("walk 999 60\n#\n", 14, &parsed),
        "grid dimensions beyond the scene maximum reject");
  buffer[length - 2] = 'x'; // a non-cell byte in the last row
  check(!walk_grid_parse(buffer, (size_t)length, &parsed),
        "a bad cell character rejects");
  buffer[length - 2] = '#';
  check(!walk_grid_parse(buffer, (size_t)length - 100, &parsed),
        "a truncated file rejects");
  buffer[length] = '#'; // junk beyond the final newline
  check(!walk_grid_parse(buffer, (size_t)length + 1, &parsed),
        "trailing junk rejects");

  // The committed playground mask must equal the rect rasterisation while
  // both exist, so the two sources can't drift apart silently.
  FILE *mask = fopen("src/adventures/vania_fox_the_slide/assets/common/"
                     "playground/walkable.walk",
                     "rb");
  bool matches = false;
  if (mask != NULL) {
    static char data[WALK_FILE_MAX];
    size_t size = fread(data, 1, sizeof(data), mask);
    fclose(mask);
    matches =
        walk_grid_parse(data, size, &parsed) && grids_equal(&parsed, &built);
  }
  check(matches, "the committed playground mask matches its rects");
}

static void test_pool_state_switch(void) {
  // The same grid rebuilt from the state-appropriate area, as pool.c does.
  WalkGrid grid;
  SDL_Point hen_start = {150, 480};
  SDL_Point sunscreen_poi = {150, 545};
  SDL_Point float_poi = {600, 545};

  walk_grid_build(&grid, &POOL_SHADE_AREA, WINDOW_SIZE);
  check(walk_grid_contains(&grid, hen_start) &&
            walk_grid_contains(&grid, sunscreen_poi) &&
            !walk_grid_contains(&grid, float_poi),
        "shade grid: umbrella walkable, poolside not");

  // A pre-sunscreen walk toward the pool stays under the umbrella
  // (best-effort routing to the nearest reachable point).
  SDL_FPoint path[ACTOR_MAX_WAYPOINTS];
  int count = walk_grid_find_path(&grid, (SDL_FPoint){150, 480},
                                  (SDL_FPoint){600, 545}, path, LEN(path));
  bool inside_shade = count >= 1;
  for (int i = 0; i < count; i++) {
    SDL_Point p = {(int)path[i].x, (int)path[i].y};
    if (!walk_grid_contains(&grid, p)) {
      inside_shade = false;
    }
  }
  check(inside_shade, "shade grid keeps walks under the umbrella");

  walk_grid_build(&grid, &POOL_POOLSIDE_AREA, WINDOW_SIZE);
  check(walk_grid_contains(&grid, float_poi),
        "poolside grid unlocks the pool area");
}

// Minimal texture-less actor for simulating walks headlessly.
static const ActorAnimSpec TEST_ANIMS[] = {
    {WALKING, "walking.png", "walking.anim", 1, LOOP, 0},
};
static const ActorSpec TEST_SPEC = {
    .id = "test",
    .assets_dir = "test",
    .velocity = 200,
    .move_sound_filename = NULL,
    .move_sound_volume = 0,
    .idle_state = WALKING,
    .move_state = WALKING,
    .anims = TEST_ANIMS,
    .anims_length = LEN(TEST_ANIMS),
};

static bool arrived;
static void on_arrive(void) { arrived = true; }
static bool stale_fired;
static void on_stale(void) { stale_fired = true; }

// Step the actor until it goes IDLE (or the frame budget runs out).
static void run_to_idle(Actor *actor) {
  for (int i = 0; i < 3000 && actor->state == WALKING; i++) {
    actor_update(actor, 1.0F / 30.0F);
  }
}

static void test_exact_goal_walk(const WalkGrid *entrance_grid) {
  Actor *actor = make_actor(&TEST_SPEC, (SDL_FPoint){580, 457}, NULL);

  arrived = false;
  walk_actor_to(actor, entrance_grid, SHOVEL_POI, true, on_arrive);

  // The final approach is exempt; every earlier waypoint must be legal.
  bool earlier_legal = actor->waypoints_length >= 1;
  for (int i = 0; i + 1 < actor->waypoints_length; i++) {
    SDL_Point p = {(int)actor->waypoints[i].x, (int)actor->waypoints[i].y};
    if (!walk_grid_contains(entrance_grid, p)) {
      earlier_legal = false;
    }
  }
  SDL_FPoint last = actor->waypoints[actor->waypoints_length - 1];
  check(earlier_legal, "exact-goal walk approaches along walkable ground");
  check(last.x == SHOVEL_POI.x && last.y == SHOVEL_POI.y,
        "exact-goal walk ends exactly on the POI");

  run_to_idle(actor);
  check(arrived && actor->state == IDLE &&
            fabsf(actor->current_position.x - SHOVEL_POI.x) <=
                ACTOR_ARRIVE_EPSILON &&
            fabsf(actor->current_position.y - SHOVEL_POI.y) <=
                ACTOR_ARRIVE_EPSILON,
        "exact-goal walk arrives and fires its callback");

  actor_free(actor);
}

static void test_stale_callback_cancelled(const WalkGrid *grid) {
  // Regression test for #63: start a walk with a callback, then tap the
  // actor itself. The pending callback must be dropped, not fired from
  // wherever the actor happens to stand.
  Actor *actor = make_actor(&TEST_SPEC, (SDL_FPoint){150, 400}, NULL);

  stale_fired = false;
  walk_actor_to(actor, grid, (SDL_FPoint){700, 400}, true, on_stale);
  check(actor->state == WALKING, "walk with callback starts");

  actor_walk_to(actor, actor->current_position, NULL);
  check(actor->state == IDLE && actor->on_end_walking == NULL,
        "tap on the actor cancels the walk");

  for (int i = 0; i < 30; i++) {
    actor_update(actor, 1.0F / 30.0F);
  }
  check(!stale_fired, "cancelled walk never fires its stale callback");

  actor_free(actor);
}

// A scrolling scene's grid (mirrors the depth demo's 1600x600 field): the
// walk layer must work identically beyond the window's edge.
static void test_wide_scene(void) {
  static const SDL_Rect WIDE_RECTS[] = {{40, 260, 1520, 300}};
  static const WalkArea WIDE_AREA = {WIDE_RECTS, 1, NULL, 0};
  static WalkGrid grid;
  walk_grid_build(&grid, &WIDE_AREA, (SDL_Point){1600, 600});
  check(grid.w == 160 && grid.h == 60, "a wide scene gets a wide grid");
  check(walk_grid_contains(&grid, (SDL_Point){1500, 400}) &&
            !walk_grid_contains(&grid, (SDL_Point){1500, 100}) &&
            !walk_grid_contains(&grid, (SDL_Point){1620, 400}),
        "walkability works beyond the window's edge");

  SDL_FPoint path[ACTOR_MAX_WAYPOINTS];
  int count = walk_grid_find_path(&grid, (SDL_FPoint){100, 400},
                                  (SDL_FPoint){1500, 400}, path, LEN(path));
  check(count >= 1 && (int)path[count - 1].x == 1500 &&
            (int)path[count - 1].y == 400,
        "a walk crosses the whole wide scene");

  static char buffer[WALK_FILE_MAX];
  int length = walk_grid_serialize(&grid, buffer, sizeof(buffer));
  static WalkGrid parsed;
  check(length > 0 && strncmp(buffer, "walk 160 60\n", 12) == 0 &&
            walk_grid_parse(buffer, (size_t)length, &parsed) &&
            grids_equal(&parsed, &grid),
        "a wide mask is self-describing and round-trips");
}

// ── drag & drop (LIVELINESS.md Part 2) ───────────────────────────────────────

// Synthesized mouse events, as walk_actor_drag_event consumes them.
static SDL_Event mouse_down(int x, int y) {
  SDL_Event e = {0};
  e.type = SDL_MOUSEBUTTONDOWN;
  e.button.x = x;
  e.button.y = y;
  return e;
}

static SDL_Event mouse_motion(int x, int y) {
  SDL_Event e = {0};
  e.type = SDL_MOUSEMOTION;
  e.motion.x = x;
  e.motion.y = y;
  e.motion.state = SDL_BUTTON_LMASK; // dragging = the button is held
  return e;
}

static SDL_Event mouse_up(int x, int y) {
  SDL_Event e = {0};
  e.type = SDL_MOUSEBUTTONUP;
  e.button.x = x;
  e.button.y = y;
  return e;
}

// Step the actor until the fall (and any landing beat) resolves to IDLE.
static void run_out_fall(Actor *actor) {
  for (int i = 0; i < 3000 && actor->state != IDLE; i++) {
    actor_update(actor, 1.0F / 30.0F);
  }
}

static void test_drag_and_drop(void) {
  fprintf(stderr, "\n-- drag & drop unit tests --\n");

  // A drag needs no walkable area at all: it only lifts the actor and puts her
  // back. The grid here is just to prove nothing consults it.
  Actor *actor = make_actor(&TEST_SPEC, (SDL_FPoint){150, 480}, NULL);
  // make_actor leaves the (never loaded) sprite clips unset; the grab
  // hit-test reads frame 0, so give it the hen's 120x120.
  actor->animations[WALKING]->sprite_clips[0] = (SDL_Rect){0, 0, 120, 120};
  float home_x = actor->current_position.x;
  float home_feet = actor_feet_y(actor);

  // A press away from the sprite neither arms nor consumes.
  SDL_Event e = mouse_down(500, 100);
  check(!actor_drag_event(actor, &e) && !actor->drag_armed,
        "a press off the sprite is ignored");

  // A press on the sprite arms the drag but still falls through, so a hotspot
  // the actor stands on keeps working for plain taps.
  e = mouse_down(150, 480);
  bool pressed_through = !actor_drag_event(actor, &e) && actor->drag_armed;
  e = mouse_motion(153, 478);
  bool wiggle_through = !actor_drag_event(actor, &e);
  e = mouse_up(153, 478);
  bool released_through = !actor_drag_event(actor, &e) && !actor->drag_armed;
  check(pressed_through && wiggle_through && released_through &&
            actor->state == IDLE && actor->current_position.x == home_x &&
            actor_feet_y(actor) == home_feet,
        "a tap on the actor falls through to the scene untouched");

  // Only an upward pull starts a drag: a sideways or downward one leaves the
  // event to the scene, so swiping across the actor still reaches a hotspot.
  e = mouse_down(150, 480);
  actor_drag_event(actor, &e);
  e = mouse_motion(400, 480);
  check(!actor_drag_event(actor, &e) && actor->state == IDLE,
        "a sideways pull does not start a drag");
  e = mouse_motion(150, 560);
  check(!actor_drag_event(actor, &e) && actor->state == IDLE,
        "a downward pull does not start a drag");

  // A stale press (button no longer held) disarms instead of grabbing.
  e = mouse_motion(150, 300);
  e.motion.state = 0; // the release went elsewhere (e.g. a scene switch)
  check(!actor_drag_event(actor, &e) && !actor->drag_armed &&
            actor->state == IDLE,
        "a button-less motion disarms a stale press");

  // Press + upward travel: the drag begins and she rises with the pointer.
  e = mouse_down(150, 480);
  actor_drag_event(actor, &e);
  e = mouse_motion(150, 400);
  check(actor_drag_event(actor, &e) && actor->state == DRAGGED,
        "an upward pull past the threshold grabs");
  check(actor->current_position.x == home_x, "a drag never moves her sideways");

  // Sideways pointer travel is ignored outright; only y is followed.
  e = mouse_motion(700, 380);
  actor_drag_event(actor, &e);
  check(actor->current_position.x == home_x,
        "even a wild sideways drag leaves x alone");
  check(fabsf(actor_feet_y(actor) - (home_feet - 100.0F)) <=
            DRAG_START_THRESHOLD,
        "her height follows the pointer");

  // She cannot be pushed below the ground she was picked up from.
  e = mouse_motion(150, 700);
  actor_drag_event(actor, &e);
  check(fabsf(actor_feet_y(actor) - home_feet) < 0.001F,
        "dragging below the ground stops at it");
  check(!actor_shadow_visible(actor),
        "with no lift there is no shadow to draw");

  // Lift again: the shadow marks the ground she came from.
  e = mouse_motion(150, 350);
  actor_drag_event(actor, &e);
  check(actor_shadow_visible(actor) && actor->ground_y == home_feet,
        "held above her ground, the shadow sits on it");

  // Release: she falls back and comes to rest exactly where she was picked up.
  e = mouse_up(150, 350);
  check(actor_drag_event(actor, &e) && actor->state == FALLING,
        "a release above the ground starts a fall");
  float before_y = actor->current_position.y;
  actor_update(actor, 0.1F);
  check(fabsf(actor->current_position.y - (before_y + FALL_SPEED * 0.1F)) <
            0.001F,
        "the fall descends at FALL_SPEED");
  run_out_fall(actor);
  check(actor->state == IDLE && actor->current_position.x == home_x &&
            fabsf(actor_feet_y(actor) - home_feet) < 0.001F,
        "she lands back exactly where she was picked up");

  // A release with no lift lands her at once rather than "falling" upward.
  e = mouse_down(150, 480);
  actor_drag_event(actor, &e);
  e = mouse_motion(150, 460);
  actor_drag_event(actor, &e);
  e = mouse_motion(150, 600); // pushed back down to the ground clamp
  actor_drag_event(actor, &e);
  e = mouse_up(150, 600);
  actor_drag_event(actor, &e);
  check(actor->state == IDLE && fabsf(actor_feet_y(actor) - home_feet) < 0.001F,
        "a release with no lift lands her straight away");

  // TALKING refuses the grab, like it refuses walks.
  actor->state = TALKING;
  e = mouse_down(150, 480);
  actor_drag_event(actor, &e);
  check(!actor->drag_armed, "a talking actor cannot be picked up");
  actor->state = IDLE;

  actor_free(actor);
}

static void test_drag_depth(void) {
  fprintf(stderr, "\n-- drag depth unit tests --\n");

  // A ramp over the strip, so any change in depth would be visible as a change
  // in scale: feet 430 -> 0.5, 630 -> 1.0.
  static const ScaleRamp RAMP = {
      .y_far = 430, .y_near = 630, .scale_far = 0.5F, .scale_near = 1.0F};
  Actor *a = make_actor(&TEST_SPEC, (SDL_FPoint){400, 500}, &RAMP);
  a->animations[WALKING]->sprite_clips[0] = (SDL_Rect){0, 0, 120, 120};
  float standing_scale = actor_scale(a);
  float home_feet = actor_feet_y(a);

  SDL_Event e = mouse_down(400, 500);
  actor_drag_event(a, &e);
  e = mouse_motion(400, 480);
  actor_drag_event(a, &e);
  check(a->state == DRAGGED, "the drag started");
  check(actor_scale(a) == standing_scale,
        "lifting her does not change her size");

  // The whole point of the fixed ground: however far she is lifted, her depth
  // is where she will land, so she is drawn at one size throughout.
  e = mouse_motion(400, 120);
  actor_drag_event(a, &e);
  check(actor_scale(a) == standing_scale,
        "lifting her far does not change her size either");
  check(a->ground_y == home_feet, "and her ground has not moved");

  float held_scale = actor_scale(a);
  e = mouse_up(400, 120);
  actor_drag_event(a, &e);
  check(actor_scale(a) == held_scale,
        "releasing changes nothing about her size");
  run_out_fall(a);
  check(actor_scale(a) == standing_scale, "and she lands at the size she left");
  actor_free(a);

  // Catching her mid-fall must keep the ground she was already bound for,
  // otherwise a re-grab would strand her wherever she happened to be.
  Actor *falling = make_actor(&TEST_SPEC, (SDL_FPoint){400, 500}, &RAMP);
  falling->animations[WALKING]->sprite_clips[0] = (SDL_Rect){0, 0, 120, 120};
  float ground = actor_feet_y(falling);
  e = mouse_down(400, 500);
  actor_drag_event(falling, &e);
  e = mouse_motion(400, 300);
  actor_drag_event(falling, &e);
  e = mouse_up(400, 300);
  actor_drag_event(falling, &e);
  check(falling->state == FALLING, "she is falling before the catch");
  actor_update(falling, 0.05F);
  float mid_fall_scale = actor_scale(falling);
  e = mouse_down(400, (int)falling->current_position.y);
  actor_drag_event(falling, &e);
  e = mouse_motion(400, (int)falling->current_position.y - 40);
  actor_drag_event(falling, &e);
  check(falling->state == DRAGGED, "she is caught mid-fall");
  check(falling->ground_y == ground,
        "catching her keeps the ground she was falling to");
  check(actor_scale(falling) == mid_fall_scale,
        "catching her mid-fall doesn't rescale her");
  e = mouse_up(400, (int)falling->current_position.y);
  actor_drag_event(falling, &e);
  run_out_fall(falling);
  check(fabsf(actor_feet_y(falling) - ground) < 0.001F,
        "and she still lands on it");
  actor_free(falling);
}

int test_walk(void) {
  failures = 0;
  fprintf(stderr, "\n-- walk geometry unit tests --\n");

  static WalkGrid playground_grid;
  static WalkGrid entrance_grid;
  walk_grid_build(&playground_grid, &PLAYGROUND_AREA, WINDOW_SIZE);
  walk_grid_build(&entrance_grid, &ENTRANCE_AREA, WINDOW_SIZE);

  test_nearest_postcondition(&playground_grid);
  test_nearest_postcondition(&entrance_grid);
  test_playground_routing(&playground_grid);
  test_determinism(&playground_grid);
  test_best_effort();
  test_walk_mask_format();
  test_wide_scene();
  test_pool_state_switch();
  test_exact_goal_walk(&entrance_grid);
  test_stale_callback_cancelled(&playground_grid);
  test_drag_and_drop();
  test_drag_depth();

  return failures;
}
