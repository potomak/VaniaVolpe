//
//  vine.c
//  The grape vine. With Carla's basket in hand, tapping the grapes opens the
//  picking minigame; without it, Gina has nothing to collect them with.
//

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "game.h"
#include "image.h"
#include "sound.h"

#include "gina_hen_at_the_pool.h"
#include "gina_state.h"
#include "gina_worn.h"
#include "hen.h"
#include "pool.h"
#include "tree.h"
#include "vine.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "gina_assets.h"

static ImageData images[GINA_VINE_IMAGES_COUNT] = GINA_VINE_IMAGES_INIT;
static const ImageData *background = &images[GINA_VINE_IMAGE_BACKGROUND];

// The grapes boil (LIVELINESS.md) shows the grapes are tappable. Declared as
// data: the framework makes, loads, ticks and frees it; init only aliases it.
static AnimationData *grapes_boil;
// The two places this scene connects to, pictured as tiles up near the
// horizon: the tree on the left, the poolside on the right. The framework
// plays and draws each from its hotspot, like any other tappable thing.
#define VINE_ANIM_TO_TREE (GINA_VINE_ANIMS_COUNT)
#define VINE_ANIM_TO_POOL (GINA_VINE_ANIMS_COUNT + 1)
static AnimationData *animations[GINA_VINE_ANIMS_COUNT + 2];
static const SceneAnimSpec anim_specs[] = {
    GINA_VINE_ANIM_GRAPES_BOIL_SPEC,
    GINA_NAV_ANIM_TO_TREE_BOIL_SPEC,
    GINA_NAV_ANIM_TO_POOL_BOIL_SPEC,
};

// Static sprite layer: just the backdrop. The grapes boil is declared on its
// hotspot, which the framework plays and draws.
static SceneSprite sprites[1];

// The scene's spoken lines: per-line dialogue chunks the framework speaks via
// generated say_<name>() helpers.
static ChunkData chunks[GINA_VINE_DIALOG_CHUNKS_COUNT] =
    GINA_VINE_DIALOG_CHUNKS_INIT;

static Hen *gina;
static const SDL_FPoint HEN_START = {400, 480};

static const SDL_Point GRAPES_AT = {350, 180};

static const SDL_Rect GRAPES_HOTSPOT = {350, 180, 100, 120};
static Hotspot hotspots[3];

// Walk geometry: the ground under the vines, plus a path up either side to the
// tile above it. The paths overlap the near strip so the grid joins them into
// one region — a gap would leave a tile visible but unreachable.
static const SDL_Rect WALKABLE_RECTS[] = {
    {20, 430, 760, 150}, // the near ground, right across the scene
    {20, 250, 150, 190}, // up the left, toward the tree
    {630, 250, 150, 190} // up the right, toward the poolside
};
static const WalkArea WALK_AREA = {WALKABLE_RECTS, LEN(WALKABLE_RECTS), NULL,
                                   0};
static WalkGrid walk_grid;

// Depth (SCALING.md), in feet coordinates (the rects above are centre
// positions; feet sit half a walking frame lower). y_far is the feet line of
// the topmost walkable row, so she is smallest exactly where the paths run out.
// scale_far sits on the 0.6 floor scaling.h documents rather than below it.
static const ScaleRamp SCALE_RAMP = {
    .y_far = 310, .y_near = 639, .scale_far = 0.6F, .scale_near = 1.0F};

// The tiles, and the tappable areas around them — deliberately larger than the
// art, since a small finger aiming at a distant thing should not have to be
// precise.
static const SDL_Point TREE_TILE_AT = {30, 100};
static const SDL_Point POOL_TILE_AT = {680, 100};
static const SDL_Rect TREE_TILE_HOTSPOT = {10, 80, 130, 130};
static const SDL_Rect POOL_TILE_HOTSPOT = {660, 80, 130, 130};

static const SDL_Point GRAPES_POI = {400, 470};
// The far end of each path — one point per door, because it is both where she
// walks to use it and where she is standing when she comes through it. The
// hotspot POIs below are taken from these rather than repeating the numbers.
const SDL_FPoint GINA_VINE_ENTRY_FROM_TREE = {75, 280};
const SDL_FPoint GINA_VINE_ENTRY_FROM_POOL = {725, 280};

static SDL_Point door(SDL_FPoint at) {
  return (SDL_Point){(int)at.x, (int)at.y};
}
static SDL_Point pois[3];

// Interactions (bodies below the loaders).
static void pick_grapes(void);
static void go_to_tree(void);
static void go_to_pool(void);

static void init(void) {
  // Gina is made by the framework (actor_spec/actor_start below) before init.
  walk_grid_init(&walk_grid, &WALK_AREA,
                 (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT}, "vine");

  grapes_boil = animations[GINA_VINE_ANIM_GRAPES_BOIL];

  sprites[0] = (SceneSprite){.image = background, .at = {0, 0}};

  int i = 0;
  hotspots[i++] = (Hotspot){.rect = GRAPES_HOTSPOT,
                            .poi = GRAPES_POI,
                            .on_arrive = pick_grapes,
                            .active_anim = grapes_boil,
                            .anim_at = GRAPES_AT};
  hotspots[i++] = (Hotspot){.rect = TREE_TILE_HOTSPOT,
                            .poi = door(GINA_VINE_ENTRY_FROM_TREE),
                            .on_arrive = go_to_tree,
                            .active_anim = animations[VINE_ANIM_TO_TREE],
                            .anim_at = TREE_TILE_AT};
  hotspots[i] = (Hotspot){.rect = POOL_TILE_HOTSPOT,
                          .poi = door(GINA_VINE_ENTRY_FROM_POOL),
                          .on_arrive = go_to_pool,
                          .active_anim = animations[VINE_ANIM_TO_POOL],
                          .anim_at = POOL_TILE_AT};

  pois[0] = GRAPES_POI;
  pois[1] = door(GINA_VINE_ENTRY_FROM_TREE);
  pois[2] = door(GINA_VINE_ENTRY_FROM_POOL);
}

// Leaving: the destination says where its own door is, so this scene never
// assumes anything about the shape of the next one.
static void go_to_tree(void) {
  set_active_scene_at(GINA_TREE, GINA_TREE_ENTRY_FROM_VINE);
}

static void go_to_pool(void) {
  set_active_scene_at(GINA_POOL, GINA_POOL_ENTRY_FROM_VINE);
}

static void pick_grapes(void) {
  if (gina_state.has_grapes) {
    say_already_grapes();
    return;
  }
  if (gina_state.has_basket) {
    set_active_scene(GINA_GRAPES_MINIGAME);
    return;
  }
  say_nothing_to_pick();
}

static void on_scene_active(void) {
  // Her default spot. Arriving from another scene overrides this straight
  // after, with that scene passing one of the GINA_VINE_ENTRY_FROM_* points
  // above (set_active_scene_at).
  actor_place(gina, HEN_START);
  // Fresh from the grapes minigame: explain what the reward means.
  if (gina_state.announce_grapes) {
    gina_state.announce_grapes = false;
    say_basket_full();
  }
}

static void on_scene_inactive(void) {}

// Custom render only so her kit draws over her; everything else here is
// sprites and hotspots the framework handles.
static void render(SDL_Renderer *renderer) {
  render_action_layer(renderer, &SCALE_RAMP, NULL, 0, &gina, 1);
  gina_render_worn(renderer, gina);
}

Scene vine_scene = {
    .init = init,
    .render = render,
    .actor = &gina,
    .actor_spec = &HEN_SPEC,
    .actor_start = {400, 480},
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .hotspots = hotspots,
    .hotspots_length = LEN(hotspots),
    .pois = pois,
    .pois_length = LEN(pois),
    .scale_ramp = &SCALE_RAMP,
    .walk_grid = &walk_grid,
    .walk_mask_dir = "vine",
    .sprites = sprites,
    .sprites_length = LEN(sprites),
    .images = images,
    .images_length = LEN(images),
    .animations = animations,
    .animations_length = LEN(animations),
    .anim_specs = anim_specs,
    .anim_specs_length = LEN(anim_specs),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
