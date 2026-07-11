//
//  vine.c
//  The grape vine. With Carla's basket in hand, tapping the grapes opens the
//  picking minigame; without it, Gina has nothing to collect them with.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
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

static ImageData images[2] = {
    {NULL, "background.png", "vine", 0, 0},
    {NULL, "grapes.png", "vine", 0, 0},
};
static const ImageData *background = &images[0];
static const ImageData *grapes = &images[1];

static ChunkData chunks[1] = {
    {NULL, "voice.wav", "vine"},
};
static const ChunkData *voice(void) { return &chunks[0]; }

static SDL_Point m_pos;

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

  int i = 0;
  hotspots[i++] = (Hotspot){
      .rect = GRAPES_HOTSPOT, .poi = GRAPES_POI, .on_arrive = pick_grapes};
  hotspots[i++] = (Hotspot){
      .rect = TREE_NAV_HOTSPOT, .immediate = true, .on_arrive = go_to_tree};
  hotspots[i++] = (Hotspot){
      .rect = POOL_NAV_HOTSPOT, .immediate = true, .on_arrive = go_to_pool};

  pois[0] = GRAPES_POI;
}

static bool load_media(SDL_Renderer *renderer) {
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

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEMOTION:
    m_pos.x = event->motion.x;
    m_pos.y = event->motion.y;
    break;
  case SDL_MOUSEBUTTONDOWN:
    // Hit-test the click's own coordinates (#64): the cached motion position
    // can be stale — e.g. a repeated tap with no motion in between while the
    // camera moved.
    m_pos.x = event->button.x;
    m_pos.y = event->button.y;
    // The hotspot table says what each region does (see init); anything else
    // is a walk toward the click.
    if (hotspots_handle_click(hotspots, LEN(hotspots), gina, &walk_grid,
                              m_pos)) {
      break;
    }
    walk_actor_to(gina, &walk_grid, (SDL_FPoint){m_pos.x, m_pos.y}, false,
                  NULL);
    break;
  }
}

static void update(float delta_time) { hen_update(gina, delta_time); }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});
  render_image(renderer, grapes, GRAPES_AT);
  hen_render(gina, renderer);
}

static void deinit(void) { hen_free(gina); }

static void on_scene_active(void) {
  gina->current_position = HEN_START;
  gina->target_position = HEN_START;
}

static void on_scene_inactive(void) {}

Scene vine_scene = {
    .init = init,
    .load_media = load_media,
    .process_input = process_input,
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
    .images = images,
    .images_length = LEN(images),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
