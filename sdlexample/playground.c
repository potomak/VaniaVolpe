//
//  playground.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "constants.h"
#include "fox.h"
#include "game.h"
#include "image.h"

#include "playground.h"

static ImageData background = {NULL, 0, 0};
static ImageData squirrel = {NULL, 0, 0};
static ImageData peg = {NULL, 0, 0};
static ImageData acorns = {NULL, 0, 0};
static Fox *fox;

// Music
static Mix_Music *music = NULL;

// Sound effects
// TODO

// Mouse position
static SDL_Point m_pos;

// Hotspots
static const SDL_Rect SLIDE_HOTSPOT = {253, 157, 302, 285};
static const SDL_Rect SQUIRREL_HOTSPOT = {39, 147, 113, 97};
static const SDL_Rect ACORNS_HOTSPOT = {659, 172, 136, 137};
static const SDL_Rect WALKABLE_HOTSPOT_1 = {63, 333, 645, 262};
static const SDL_Rect WALKABLE_HOTSPOT_2 = {707, 491, 88, 105};
static const SDL_Rect WALKABLE_HOTSPOT_3 = {4, 531, 68, 63};
static const SDL_Rect NON_WALKABLE_HOTSPOT = {236, 131, 403, 312};
static SDL_Rect hotspots[7];

// Points of interest
static const SDL_Point SLIDE_POI = {276, 454};
static const SDL_Point SQUIRREL_POI = {115, 468};
static const SDL_Point ACORNS_POI = {697, 455};
static SDL_Point pois[3];

static void init(void) {
  fox = make_fox((SDL_FPoint){580, 457});

  hotspots[0] = SLIDE_HOTSPOT;
  hotspots[1] = SQUIRREL_HOTSPOT;
  hotspots[2] = ACORNS_HOTSPOT;
  hotspots[3] = WALKABLE_HOTSPOT_1;
  hotspots[4] = WALKABLE_HOTSPOT_2;
  hotspots[5] = WALKABLE_HOTSPOT_3;
  hotspots[6] = NON_WALKABLE_HOTSPOT;

  pois[0] = SLIDE_POI;
  pois[1] = SQUIRREL_POI;
  pois[2] = ACORNS_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!load_image(renderer, &background, "playground/background.png")) {
    fprintf(stderr, "Failed to load background!\n");
    return false;
  }

  if (!load_image(renderer, &squirrel, "playground/squirrel.png")) {
    fprintf(stderr, "Failed to load squirrel!\n");
    return false;
  }

  if (!load_image(renderer, &peg, "playground/peg.png")) {
    fprintf(stderr, "Failed to load peg!\n");
    return false;
  }

  if (!load_image(renderer, &acorns, "playground/acorns.png")) {
    fprintf(stderr, "Failed to load acorns!\n");
    return false;
  }

  if (!fox_load_media(fox, renderer)) {
    fprintf(stderr, "Failed to load fox!\n");
    return false;
  }

  // Load music
  music = Mix_LoadMUS("playground/music.wav");
  if (music == NULL) {
    fprintf(stderr, "Failed to load music! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  // Load sound effects
  // TODO

  return true;
}

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEMOTION:
    // Get mouse position
    SDL_GetMouseState(&m_pos.x, &m_pos.y);
    break;
  case SDL_MOUSEBUTTONDOWN:
    if (SDL_PointInRect(&m_pos, &SLIDE_HOTSPOT)) {
      // Walk to slide
      fox_walk_to(fox, (SDL_FPoint){SLIDE_POI.x, SLIDE_POI.y}, NULL);
      // If peg is in inventory fix slide
      // Else if slide is working use: win the game
      // Else give hint to fix the slide
      break;
    }
    if (SDL_PointInRect(&m_pos, &SQUIRREL_HOTSPOT)) {
      // Walk to squirrel
      fox_walk_to(fox, (SDL_FPoint){SQUIRREL_POI.x, SQUIRREL_POI.y}, NULL);
      // If peg is in inventory give hint to fix the slide
      // Else if acorns are in inventory exchange peg for acorns
      // Else give hint to find acorns
      break;
    }
    if (SDL_PointInRect(&m_pos, &ACORNS_HOTSPOT)) {
      // Walk to acorns
      fox_walk_to(fox, (SDL_FPoint){ACORNS_POI.x, ACORNS_POI.y}, NULL);
      // Add acorns to inventory
      break;
    }
    if ((SDL_PointInRect(&m_pos, &WALKABLE_HOTSPOT_1) ||
         SDL_PointInRect(&m_pos, &WALKABLE_HOTSPOT_2) ||
         SDL_PointInRect(&m_pos, &WALKABLE_HOTSPOT_3)) &&
        !SDL_PointInRect(&m_pos, &NON_WALKABLE_HOTSPOT)) {
      // Walk to current position
      fox_walk_to(fox, (SDL_FPoint){m_pos.x, m_pos.y}, NULL);
    }
    break;
  }
}

static void update(float delta_time) { fox_update(fox, delta_time); }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, &background, (SDL_Point){0, 0});
  render_image(renderer, &squirrel, (SDL_Point){85, 160});
  render_image(renderer, &peg, (SDL_Point){127, 175});
  render_image(renderer, &acorns, (SDL_Point){698, 225});

  fox_render(fox, renderer);
}

static void deinit(void) {
  free_image_texture(&background);
  free_image_texture(&squirrel);
  free_image_texture(&peg);
  free_image_texture(&acorns);

  fox_free(fox);

  Mix_FreeMusic(music);
  music = NULL;
}

static void on_scene_active(void) {}

static void on_scene_inactive(void) {}

Scene playground_scene = {
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
};
