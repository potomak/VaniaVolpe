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

#include "actor.h"
#include "constants.h"
#include "walk.h"

#include "test_walk.h"

static int failures;

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
  int steps = (int)(sqrtf(dx * dx + dy * dy) / (WALK_CELL_SIZE / 2)) + 1;
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
  walk_grid_build(&grid, &ISLAND_AREA);
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

static void test_pool_state_switch(void) {
  // The same grid rebuilt from the state-appropriate area, as pool.c does.
  WalkGrid grid;
  SDL_Point hen_start = {150, 480};
  SDL_Point sunscreen_poi = {150, 545};
  SDL_Point float_poi = {600, 545};

  walk_grid_build(&grid, &POOL_SHADE_AREA);
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

  walk_grid_build(&grid, &POOL_POOLSIDE_AREA);
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
    actor_update(actor, 1.0f / 30.0f);
  }
}

static void test_exact_goal_walk(const WalkGrid *entrance_grid) {
  Actor *actor = make_actor(&TEST_SPEC, (SDL_FPoint){580, 457});

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
  Actor *actor = make_actor(&TEST_SPEC, (SDL_FPoint){150, 400});

  stale_fired = false;
  walk_actor_to(actor, grid, (SDL_FPoint){700, 400}, true, on_stale);
  check(actor->state == WALKING, "walk with callback starts");

  actor_walk_to(actor, actor->current_position, NULL);
  check(actor->state == IDLE && actor->on_end_walking == NULL,
        "tap on the actor cancels the walk");

  for (int i = 0; i < 30; i++) {
    actor_update(actor, 1.0f / 30.0f);
  }
  check(!stale_fired, "cancelled walk never fires its stale callback");

  actor_free(actor);
}

int test_walk(void) {
  failures = 0;
  fprintf(stderr, "\n-- walk geometry unit tests --\n");

  static WalkGrid playground_grid;
  static WalkGrid entrance_grid;
  walk_grid_build(&playground_grid, &PLAYGROUND_AREA);
  walk_grid_build(&entrance_grid, &ENTRANCE_AREA);

  test_nearest_postcondition(&playground_grid);
  test_nearest_postcondition(&entrance_grid);
  test_playground_routing(&playground_grid);
  test_determinism(&playground_grid);
  test_best_effort();
  test_pool_state_switch();
  test_exact_goal_walk(&entrance_grid);
  test_stale_callback_cancelled(&playground_grid);

  return failures;
}
