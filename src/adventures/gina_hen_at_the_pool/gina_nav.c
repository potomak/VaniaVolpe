//
//  gina_nav.c
//  See gina_nav.h. The ring, the arrows that advertise it, and the rule for
//  where she lands.
//

#include "gina_nav.h"

#include "constants.h" // LEN
#include "game.h"
#include "gina_hen_at_the_pool.h"
#include "gina_state.h"

// The exits, one row per walkable scene: walk off the left edge and you are in
// `left`, off the right edge and you are in `right`. This is the whole map —
// the ring runs pool -> tree -> vine -> pool going right.
typedef struct gina_exits {
  bool in_ring; // false for the rows this sparse table skips
  int left;
  int right;
} GinaExits;

static const GinaExits EXITS[] = {
    [GINA_POOL] = {.in_ring = true, .left = GINA_VINE, .right = GINA_TREE},
    [GINA_TREE] = {.in_ring = true, .left = GINA_POOL, .right = GINA_VINE},
    [GINA_VINE] = {.in_ring = true, .left = GINA_TREE, .right = GINA_POOL},
};

// The intro, the minigames and the end card are not on the ring: no edges to
// leave by, and nothing to place on arrival.
static bool has_exits(int scene) {
  return scene >= 0 && scene < (int)LEN(EXITS) && EXITS[scene].in_ring;
}

// The tappable strips, and the arrows inside them. Both sit just *above* the
// walkable band rather than on it: she arrives standing on the edge she came
// in by, and at ground level her sprite would cover the very arrow that says
// how to go back. Each strip is comfortably larger than its art — a toddler
// aiming at the edge of a phone should not have to be precise.
static const SDL_Rect LEFT_EXIT_HOTSPOT = {0, 336, 72, 120};
static const SDL_Rect RIGHT_EXIT_HOTSPOT = {728, 336, 72, 120};

// Where she walks before the scene changes, on the walkable strip all three
// scenes share.
static const SDL_Point LEFT_EDGE_POI = {40, 500};
static const SDL_Point RIGHT_EDGE_POI = {760, 500};

static const SDL_Point LEFT_ARROW_AT = {10, 356};
static const SDL_Point RIGHT_ARROW_AT = {742, 356};

// Leaving: remember the scene being left, so the arriving one knows which edge
// to put her on, then switch.
static void leave_towards(int target) {
  gina_state.came_from = game.current_scene;
  set_active_scene(target);
}

static void exit_left(void) {
  if (has_exits(game.current_scene)) {
    leave_towards(EXITS[game.current_scene].left);
  }
}

static void exit_right(void) {
  if (has_exits(game.current_scene)) {
    leave_towards(EXITS[game.current_scene].right);
  }
}

int gina_nav_hotspots(Hotspot *out, bool (*enabled)(void)) {
  out[0] = (Hotspot){.rect = LEFT_EXIT_HOTSPOT,
                     .enabled = enabled,
                     .poi = LEFT_EDGE_POI,
                     .on_arrive = exit_left};
  out[1] = (Hotspot){.rect = RIGHT_EXIT_HOTSPOT,
                     .enabled = enabled,
                     .poi = RIGHT_EDGE_POI,
                     .on_arrive = exit_right};
  return 2;
}

int gina_nav_pois(SDL_Point *out) {
  out[0] = LEFT_EDGE_POI;
  out[1] = RIGHT_EDGE_POI;
  return 2;
}

void gina_nav_render(SDL_Renderer *renderer, AnimationData *left,
                     AnimationData *right, bool (*visible)(void)) {
  if (visible != NULL && !visible()) {
    return;
  }
  // The sheet points right, so the left-hand exit is the same art mirrored.
  left->flip = SDL_FLIP_HORIZONTAL;
  render_animation(renderer, left, LEFT_ARROW_AT);
  render_animation(renderer, right, RIGHT_ARROW_AT);
}

SDL_FPoint gina_nav_entry(SDL_FPoint fallback) {
  int from = gina_state.came_from;
  // One-shot: only the arrival immediately after a walk gets placed.
  gina_state.came_from = GINA_NO_SCENE;

  int self = game.current_scene;
  if (!has_exits(self) || from == GINA_NO_SCENE) {
    return fallback;
  }
  // She came through the exit that leads back to `from`, so that is where she
  // is standing now.
  if (EXITS[self].left == from) {
    return (SDL_FPoint){LEFT_EDGE_POI.x, LEFT_EDGE_POI.y};
  }
  if (EXITS[self].right == from) {
    return (SDL_FPoint){RIGHT_EDGE_POI.x, RIGHT_EDGE_POI.y};
  }
  return fallback;
}
