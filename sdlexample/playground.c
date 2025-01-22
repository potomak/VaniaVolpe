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
static const SDL_Rect SQUIRREL_HOTSPOT = {70, 147, 113, 97};
static const SDL_Rect ACORNS_TREE_HOTSPOT = {668, 201, 126, 118};
static const SDL_Rect ACORNS_FLOOR_HOTSPOT = {667, 500, 105, 98};
static const SDL_Rect PEG_HOTSPOT = {83, 460, 91, 94};
static const SDL_Rect WALKABLE_HOTSPOT_1 = {63, 333, 645, 262};
static const SDL_Rect WALKABLE_HOTSPOT_2 = {707, 491, 88, 105};
static const SDL_Rect WALKABLE_HOTSPOT_3 = {4, 531, 68, 63};
static const SDL_Rect NON_WALKABLE_HOTSPOT = {236, 131, 403, 312};
static SDL_Rect hotspots[9];

// Points of interest
static const SDL_Point SLIDE_POI = {276, 454};
static const SDL_Point SQUIRREL_POI = {219, 455};
static const SDL_Point ACORNS_POI = {635, 498};
static SDL_Point pois[3];

// Scene state
static bool has_slide_been_fixed = false;
static bool have_acorns_fallen = false;
static bool has_peg_been_dropped = false;

static void init(void) {
  fox = make_fox((SDL_FPoint){580, 457});

  int i = 0;
  hotspots[i++] = SLIDE_HOTSPOT;
  hotspots[i++] = SQUIRREL_HOTSPOT;
  hotspots[i++] = ACORNS_TREE_HOTSPOT;
  hotspots[i++] = ACORNS_FLOOR_HOTSPOT;
  hotspots[i++] = PEG_HOTSPOT;
  hotspots[i++] = WALKABLE_HOTSPOT_1;
  hotspots[i++] = WALKABLE_HOTSPOT_2;
  hotspots[i++] = WALKABLE_HOTSPOT_3;
  hotspots[i++] = NON_WALKABLE_HOTSPOT;

  i = 0;
  pois[i++] = SLIDE_POI;
  pois[i++] = SQUIRREL_POI;
  pois[i++] = ACORNS_POI;
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

static void maybe_use_slide(void) {
  // If peg is in the inventory fix the slide
  if (fox->has_peg) {
    fox->has_peg = false;
    has_slide_been_fixed = true;
    fox_talk_for(fox, 2000);
    return;
  }

  // Else if slide is working use: win the game
  if (has_slide_been_fixed) {
    fox->current_position = (SDL_FPoint){336, 142};
    fox_talk_for(fox, 2000);
    // TODO: End screen
    return;
  }

  // Else give hint to fix the slide
  fox_talk_for(fox, 2000);
}

static void maybe_get_peg(void) {
  // If acorns are in the inventory exchange them for the peg
  if (fox->has_acorns) {
    fox->has_acorns = false;
    has_peg_been_dropped = true;
    return;
  }

  // Else give hint to find acorns
  fox_talk_for(fox, 2000);
}

static void make_acorns_fall(void) { have_acorns_fallen = true; }

static void pickup_acorns(void) { fox->has_acorns = true; }

static void pickup_peg(void) { fox->has_peg = true; }

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEMOTION:
    // Get mouse position
    SDL_GetMouseState(&m_pos.x, &m_pos.y);
    break;
  case SDL_MOUSEBUTTONDOWN:
    if (SDL_PointInRect(&m_pos, &SLIDE_HOTSPOT)) {
      // Walk to slide
      fox_walk_to(fox, (SDL_FPoint){SLIDE_POI.x, SLIDE_POI.y}, maybe_use_slide);
      break;
    }
    // If the squirrel already dropped the peg skip this case
    if (!has_peg_been_dropped && SDL_PointInRect(&m_pos, &SQUIRREL_HOTSPOT)) {
      // Walk to squirrel
      fox_walk_to(fox, (SDL_FPoint){SQUIRREL_POI.x, SQUIRREL_POI.y},
                  maybe_get_peg);
      break;
    }
    // If the acorns have already fallen skip this case
    if (!have_acorns_fallen && SDL_PointInRect(&m_pos, &ACORNS_TREE_HOTSPOT)) {
      // Walk to acorns
      fox_walk_to(fox, (SDL_FPoint){ACORNS_POI.x, ACORNS_POI.y},
                  make_acorns_fall);
      break;
    }
    // If the acorns haven't fallen yet, or if fox has acorns, or if acorns have
    // been exchanged for the peg, skip this case
    if (have_acorns_fallen && !fox->has_acorns && !has_peg_been_dropped &&
        SDL_PointInRect(&m_pos, &ACORNS_FLOOR_HOTSPOT)) {
      // Walk to acorns
      fox_walk_to(fox, (SDL_FPoint){ACORNS_POI.x, ACORNS_POI.y}, pickup_acorns);
      break;
    }
    // If the peg hasn't been dropped yet, or if fox has the peg, or if the
    // slide has been fixed already, skip this case
    if (has_peg_been_dropped && !fox->has_peg && !has_slide_been_fixed &&
        SDL_PointInRect(&m_pos, &PEG_HOTSPOT)) {
      // Walk to peg
      fox_walk_to(fox, (SDL_FPoint){SQUIRREL_POI.x, SQUIRREL_POI.y},
                  pickup_peg);
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

  if (has_peg_been_dropped) {
    if (fox->has_peg) {
      render_image(renderer, &peg,
                   (SDL_Point){fox->current_position.x - 20,
                               fox->current_position.y - 100});
    } else {
      if (has_slide_been_fixed) {
        render_image(renderer, &peg, (SDL_Point){272, 263});
      } else {
        render_image(renderer, &peg, (SDL_Point){109, 474});
      }
    }
  } else {
    render_image(renderer, &peg, (SDL_Point){127, 175});
  }

  if (have_acorns_fallen) {
    if (fox->has_acorns) {
      render_image(renderer, &acorns,
                   (SDL_Point){fox->current_position.x - 50,
                               fox->current_position.y - 100});
    } else {
      if (has_peg_been_dropped) {
        render_image(renderer, &acorns, (SDL_Point){137, 135});
      } else {
        render_image(renderer, &acorns, (SDL_Point){687, 503});
      }
    }
  } else {
    render_image(renderer, &acorns, (SDL_Point){698, 225});
  }

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
