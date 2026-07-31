//
//  gina_nav.c
//  See gina_nav.h. The ring, the ground that reaches it, and the tiles that
//  advertise it.
//

#include "gina_nav.h"

#include "constants.h" // LEN
#include "game.h"
#include "gina_hen_at_the_pool.h"

// The exits, one row per walkable scene: walk to the tile on the left and you
// are in `left`, the one on the right and you are in `right`. This is the whole
// map — the ring runs pool -> tree -> vine -> pool going right.
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

// The intro, the minigames and the end card are not on the ring.
static bool has_exits(int scene) {
  return scene >= 0 && scene < (int)LEN(EXITS) && EXITS[scene].in_ring;
}

// The ground: the near strip she starts on, and a path up each side leading to
// the tiles. The paths overlap the strip vertically so the grid joins them into
// one connected region — a gap would leave the tiles unreachable.
static const SDL_Rect WALKABLE_RECTS[] = {
    {20, 430, 760, 150}, // the near ground, right across the scene
    {20, 250, 150, 190}, // the path up the left, toward that tile
    {630, 250, 150, 190} // and up the right
};
const WalkArea GINA_OUTDOOR_WALK_AREA = {WALKABLE_RECTS, LEN(WALKABLE_RECTS),
                                         NULL, 0};

// Depth over that ground, in feet coordinates (the rects above are centre
// positions; feet sit half a walking frame lower). y_far is the feet line of
// the topmost walkable row, so she is at her smallest exactly where the paths
// run out. scale_far sits on the 0.6 floor scaling.h documents rather than
// below it — the walk up still reads as walking away, and her mouth shapes
// stay legible if she ever speaks up there.
const ScaleRamp GINA_OUTDOOR_RAMP = {
    .y_far = 310, .y_near = 639, .scale_far = 0.6F, .scale_near = 1.0F};

// The tiles, near the horizon in the two upper corners, and the tappable areas
// around them — deliberately larger than the art, since a small finger aiming
// at a distant thing should not have to be precise.
static const SDL_Point LEFT_TILE_AT = {30, 60};
static const SDL_Point RIGHT_TILE_AT = {680, 60};
static const SDL_Rect LEFT_TILE_HOTSPOT = {10, 40, 130, 130};
static const SDL_Rect RIGHT_TILE_HOTSPOT = {660, 40, 130, 130};

// Where she stands to use a tile: the top of the path below it. Also where she
// arrives from the other side, which is why leaving by one tile drops her on
// the opposite one — the two scenes meet at the same place in the world.
static const SDL_Point LEFT_PATH_END = {75, 280};
static const SDL_Point RIGHT_PATH_END = {725, 280};

static SDL_FPoint as_fpoint(SDL_Point p) {
  return (SDL_FPoint){(float)p.x, (float)p.y};
}

// Leaving by the left tile means arriving at the far scene's right one, and the
// other way about: the tile you walk into is the tile you walk back out of.
static void exit_left(void) {
  if (has_exits(game.current_scene)) {
    set_active_scene_at(EXITS[game.current_scene].left,
                        as_fpoint(RIGHT_PATH_END));
  }
}

static void exit_right(void) {
  if (has_exits(game.current_scene)) {
    set_active_scene_at(EXITS[game.current_scene].right,
                        as_fpoint(LEFT_PATH_END));
  }
}

int gina_nav_hotspots(Hotspot *out, AnimationData *left_boil,
                      AnimationData *right_boil, bool (*enabled)(void)) {
  out[0] = (Hotspot){.rect = LEFT_TILE_HOTSPOT,
                     .enabled = enabled,
                     .poi = LEFT_PATH_END,
                     .on_arrive = exit_left,
                     .active_anim = left_boil,
                     .anim_at = LEFT_TILE_AT};
  out[1] = (Hotspot){.rect = RIGHT_TILE_HOTSPOT,
                     .enabled = enabled,
                     .poi = RIGHT_PATH_END,
                     .on_arrive = exit_right,
                     .active_anim = right_boil,
                     .anim_at = RIGHT_TILE_AT};
  return 2;
}

int gina_nav_pois(SDL_Point *out) {
  out[0] = LEFT_PATH_END;
  out[1] = RIGHT_PATH_END;
  return 2;
}
