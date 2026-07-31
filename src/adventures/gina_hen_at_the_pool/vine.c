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
#include "gina_nav.h"
#include "gina_state.h"
#include "gina_worn.h"
#include "hen.h"
#include "vine.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "gina_assets.h"

static ImageData images[GINA_VINE_IMAGES_COUNT] = GINA_VINE_IMAGES_INIT;
static const ImageData *background = &images[GINA_VINE_IMAGE_BACKGROUND];

// The grapes boil (LIVELINESS.md) shows the grapes are tappable. Declared as
// data: the framework makes, loads, ticks and frees it; init only aliases it.
static AnimationData *grapes_boil;
// The two places this scene connects to, shown as tiles on the horizon: the
// tree on the left, the poolside on the right (gina_nav.h). The framework plays
// and draws each from its hotspot, like any other tappable thing.
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

static WalkGrid walk_grid;

static const SDL_Point GRAPES_POI = {400, 470};
static SDL_Point pois[3];

// Interactions (bodies below the loaders).
static void pick_grapes(void);

static void init(void) {
  // Gina is made by the framework (actor_spec/actor_start below) before init.
  walk_grid_init(&walk_grid, &GINA_OUTDOOR_WALK_AREA,
                 (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT}, "vine");

  grapes_boil = animations[GINA_VINE_ANIM_GRAPES_BOIL];

  sprites[0] = (SceneSprite){.image = background, .at = {0, 0}};

  int i = 0;
  hotspots[i++] = (Hotspot){.rect = GRAPES_HOTSPOT,
                            .poi = GRAPES_POI,
                            .on_arrive = pick_grapes,
                            .active_anim = grapes_boil,
                            .anim_at = GRAPES_AT};
  gina_nav_hotspots(&hotspots[i], animations[VINE_ANIM_TO_TREE],
                    animations[VINE_ANIM_TO_POOL], NULL);

  pois[0] = GRAPES_POI;
  gina_nav_pois(&pois[1]);
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
  // after, standing her at the tile she came through (set_active_scene_at).
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
  render_action_layer(renderer, &GINA_OUTDOOR_RAMP, NULL, 0, &gina, 1);
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
    .scale_ramp = &GINA_OUTDOOR_RAMP,
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
