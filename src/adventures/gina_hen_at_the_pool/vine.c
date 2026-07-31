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
// The two exit arrows: one sheet, one instance per edge so each squiggles on
// its own cursor (gina_nav_render mirrors the left one).
static AnimationData *arrow_left;
static AnimationData *arrow_right;
#define VINE_ANIM_ARROW_LEFT (GINA_VINE_ANIMS_COUNT)
#define VINE_ANIM_ARROW_RIGHT (GINA_VINE_ANIMS_COUNT + 1)
static AnimationData *animations[GINA_VINE_ANIMS_COUNT + 2];
static const SceneAnimSpec anim_specs[] = {
    GINA_VINE_ANIM_GRAPES_BOIL_SPEC,
    GINA_NAV_ANIM_ARROW_BOIL_SPEC,
    GINA_NAV_ANIM_ARROW_BOIL_SPEC,
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

// Walk geometry: the ground strip along the vineyard; no blocked areas.
static const SDL_Rect WALKABLE_RECTS[] = {{20, 430, 760, 150}};
static const WalkArea WALK_AREA = {WALKABLE_RECTS, LEN(WALKABLE_RECTS), NULL,
                                   0};
static WalkGrid walk_grid;

// Depth (SCALING.md): a gentle ramp across the walkable strip, in feet
// coordinates (the walk rects above are centre positions; feet sit half a
// walking frame lower). scale_far is the knob — raise it toward 1 for a
// flatter backdrop, lower it for more perspective.
static const ScaleRamp SCALE_RAMP = {
    .y_far = 490, .y_near = 639, .scale_far = 0.85F, .scale_near = 1.0F};

static const SDL_Point GRAPES_POI = {400, 470};
static SDL_Point pois[3];

// Interactions (bodies below the loaders).
static void pick_grapes(void);

static void init(void) {
  // Gina is made by the framework (actor_spec/actor_start below) before init.
  walk_grid_init(&walk_grid, &WALK_AREA,
                 (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT}, "vine");

  grapes_boil = animations[GINA_VINE_ANIM_GRAPES_BOIL];
  arrow_left = animations[VINE_ANIM_ARROW_LEFT];
  arrow_right = animations[VINE_ANIM_ARROW_RIGHT];

  sprites[0] = (SceneSprite){.image = background, .at = {0, 0}};

  int i = 0;
  hotspots[i++] = (Hotspot){.rect = GRAPES_HOTSPOT,
                            .poi = GRAPES_POI,
                            .on_arrive = pick_grapes,
                            .active_anim = grapes_boil,
                            .anim_at = GRAPES_AT};
  gina_nav_hotspots(&hotspots[i], NULL);

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
  // Stand where she walked in from, so leaving the tree by its right edge puts
  // her at this scene's left one (gina_nav.h).
  SDL_FPoint at = gina_nav_entry(HEN_START);
  gina->current_position = at;
  gina->target_position = at;
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
  gina_nav_render(renderer, arrow_left, arrow_right, NULL);
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
