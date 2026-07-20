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
#include "hen.h"
#include "vine.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "gina_assets.h"

static ImageData images[GINA_VINE_IMAGES_COUNT] = GINA_VINE_IMAGES_INIT;
static const ImageData *background = &images[GINA_VINE_IMAGE_BACKGROUND];

// The grapes boil (LIVELINESS.md Part 3) to show they are tappable; same size
// as the old still PNG, so the render position is unchanged. Declared as data
// (SCENES.md milestone 1): the framework makes and loads it: init only aliases
// the made object, load_media no longer touches it.
static AnimationData *grapes_boil;
static AnimationData *animations[GINA_VINE_ANIMS_COUNT];
static const SceneAnimSpec anim_specs[] = {
    GINA_VINE_ANIM_GRAPES_BOIL_SPEC,
};

// The static sprite layer (SCENES.md milestone 2): just the backdrop. The
// grapes boil is declared on its hotspot (milestone 3), which the framework
// both plays and draws.
static SceneSprite sprites[1];

static ChunkData chunks[GINA_VINE_CHUNKS_COUNT] = GINA_VINE_CHUNKS_INIT;
static const ChunkData *voice(void) { return &chunks[GINA_VINE_CHUNK_VOICE]; }

static Hen *gina;
static const SDL_FPoint HEN_START = {400, 480};

static const SDL_Point GRAPES_AT = {350, 180};

static const SDL_Rect GRAPES_HOTSPOT = {350, 180, 100, 120};
static const SDL_Rect TREE_NAV_HOTSPOT = {0, 200, 30, 250};
static const SDL_Rect POOL_NAV_HOTSPOT = {770, 200, 30, 250};
static Hotspot hotspots[3];

// Walk geometry: the ground strip along the vineyard; no blocked areas.
static const SDL_Rect WALKABLE_RECTS[] = {{20, 430, 760, 150}};
static const WalkArea WALK_AREA = {WALKABLE_RECTS, LEN(WALKABLE_RECTS), NULL,
                                   0};
static WalkGrid walk_grid;

static const SDL_Point GRAPES_POI = {400, 470};
static SDL_Point pois[1];

// Interactions (bodies below the loaders).
static void pick_grapes(void);
static void go_to_tree(void);
static void go_to_pool(void);

static void init(void) {
  gina = make_hen(HEN_START);

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
  hotspots[i++] = (Hotspot){
      .rect = TREE_NAV_HOTSPOT, .immediate = true, .on_arrive = go_to_tree};
  hotspots[i++] = (Hotspot){
      .rect = POOL_NAV_HOTSPOT, .immediate = true, .on_arrive = go_to_pool};

  pois[0] = GRAPES_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  // The grapes boil is loaded by the framework from anim_specs; only the actor
  // remains for the scene to load.
  return hen_load_media(gina, renderer);
}

static void go_to_tree(void) { set_active_scene(TREE); }

static void go_to_pool(void) { set_active_scene(POOL); }

static void pick_grapes(void) {
  if (gina_state.has_grapes) {
    gina_say(gina, "Ho gia' l'uva nel cestino.", voice());
    return;
  }
  if (gina_state.has_basket) {
    set_active_scene(GRAPES_MINIGAME);
    return;
  }
  gina_say(gina, "Non ho niente per raccoglierle.", voice());
}

static void update(float delta_time) { hen_update(gina, delta_time); }

static void render(SDL_Renderer *renderer) {
  // The background and the grapes boil are the scene's static sprites (drawn
  // by the framework); only the actor is dynamic.
  hen_render(gina, renderer);
}

static void deinit(void) { hen_free(gina); }

static void on_scene_active(void) {
  gina->current_position = HEN_START;
  gina->target_position = HEN_START;
  // Fresh from the grapes minigame (#116): explain what the reward means.
  if (gina_state.announce_grapes) {
    gina_state.announce_grapes = false;
    gina_say(gina, "Cestino pieno d'uva! Ora torno da Carla.", voice());
  }
}

static void on_scene_inactive(void) {}

Scene vine_scene = {
    .init = init,
    .load_media = load_media,
    // No process_input: the framework's default drag/hit-test/walk handler
    // drives this scene (SCENES.md milestone 5).
    .actor = &gina,
    .update = update,
    .render = render,
    .deinit = deinit,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .hotspots = hotspots,
    .hotspots_length = LEN(hotspots),
    .pois = pois,
    .pois_length = LEN(pois),
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
