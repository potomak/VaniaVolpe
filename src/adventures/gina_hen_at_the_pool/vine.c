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
static Mix_Chunk *voice(void) { return chunks[0].chunk; }

static SDL_Point m_pos;

static Hen *gina;
static const SDL_FPoint HEN_START = {400, 480};

static const SDL_Point GRAPES_AT = {350, 180};

static const SDL_Rect GRAPES_HOTSPOT = {350, 180, 100, 120};
static const SDL_Rect TREE_NAV_HOTSPOT = {0, 200, 30, 250};
static const SDL_Rect POOL_NAV_HOTSPOT = {770, 200, 30, 250};
static const SDL_Rect WALKABLE_HOTSPOT = {20, 430, 760, 150};
static SDL_Rect hotspots[4];

static const SDL_Point GRAPES_POI = {400, 470};
static SDL_Point pois[1];

static void init(void) {
  gina = make_hen(HEN_START);

  int i = 0;
  hotspots[i++] = GRAPES_HOTSPOT;
  hotspots[i++] = TREE_NAV_HOTSPOT;
  hotspots[i++] = POOL_NAV_HOTSPOT;
  hotspots[i++] = WALKABLE_HOTSPOT;

  pois[0] = GRAPES_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  return hen_load_media(gina, renderer);
}

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
    if (SDL_PointInRect(&m_pos, &GRAPES_HOTSPOT)) {
      hen_walk_to(gina, (SDL_FPoint){GRAPES_POI.x, GRAPES_POI.y}, pick_grapes);
      break;
    }
    if (SDL_PointInRect(&m_pos, &TREE_NAV_HOTSPOT)) {
      set_active_scene(TREE);
      break;
    }
    if (SDL_PointInRect(&m_pos, &POOL_NAV_HOTSPOT)) {
      set_active_scene(POOL);
      break;
    }
    hen_walk_to(gina,
                nearest_walkable_point(m_pos, &WALKABLE_HOTSPOT, 1,
                                       (SDL_Rect){0, 0, 0, 0}),
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
    .images = images,
    .images_length = LEN(images),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
