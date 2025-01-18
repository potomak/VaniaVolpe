//
//  playground_entrance.c
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

#include "playground_entrance.h"

// Background image
static ImageData background = {NULL, 0, 0};

// Animations
static AnimationData *excavator;
static AnimationData *gate;
static AnimationData *shovel;
static Fox *fox;

// Music
static Mix_Music *music = NULL;

// Sound effects
// TODO

// Mouse position
static SDL_Point m_pos;

// Hotspots
static const SDL_Rect GATE_HOTSPOT = {471, 144, 217, 184};
static const SDL_Rect EXCAVATOR_HOTSPOT = {197, 338, 108, 72};
static const SDL_Rect SHOVEL_HOTSPOT = {128, 385, 77, 65};
static const SDL_Rect WALKABLE_HOTSPOT = {1, 312, 795, 286};
static const SDL_Rect NON_WALKABLE_HOTSPOT = {80, 341, 250, 179};
static SDL_Rect hotspots[5];

// Points of interest
static const SDL_Point GATE_POI = {554, 344};
static const SDL_Point SHOVEL_POI = {258, 507};
static SDL_Point pois[2];

static void init(void) {
  excavator = make_animation_data(4, ONE_SHOT);
  // Loop the animation 3 times before stopping
  excavator->max_loop_count = 3;
  excavator->speed_multiplier = 1. / 5;
  gate = make_animation_data(7, ONE_SHOT);
  shovel = make_animation_data(5, LOOP);
  fox = make_fox((SDL_FPoint){580, 457});

  hotspots[0] = GATE_HOTSPOT;
  hotspots[1] = EXCAVATOR_HOTSPOT;
  hotspots[2] = SHOVEL_HOTSPOT;
  hotspots[3] = WALKABLE_HOTSPOT;
  hotspots[4] = NON_WALKABLE_HOTSPOT;

  pois[0] = GATE_POI;
  pois[1] = SHOVEL_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!load_image("playground_entrance/background.png", renderer,
                  &background)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  if (!load_image("playground_entrance/excavator.png", renderer,
                  &excavator->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  if (!load_animation_data(excavator, "playground_entrance/excavator.anim")) {
    return false;
  }

  if (!load_image("playground_entrance/gate.png", renderer, &gate->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  if (!load_animation_data(gate, "playground_entrance/gate.anim")) {
    return false;
  }

  if (!load_image("playground_entrance/shovel.png", renderer, &shovel->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  if (!load_animation_data(shovel, "playground_entrance/shovel.anim")) {
    return false;
  }

  if (!fox_load_media(fox, renderer)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  // Load music
  music = Mix_LoadMUS("playground_entrance/music.wav");
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
    if (SDL_PointInRect(&m_pos, &GATE_HOTSPOT)) {
      // Walk to gate
      fox_walk_to(fox, (SDL_FPoint){GATE_POI.x, GATE_POI.y});
      // If key is in inventory open gate then go to PLAYGROUND scene
      // Else give hint about where to find the key
      break;
    }
    if (SDL_PointInRect(&m_pos, &EXCAVATOR_HOTSPOT)) {
      // Play excavator animation
      play_animation(excavator);
      break;
    }
    if (SDL_PointInRect(&m_pos, &SHOVEL_HOTSPOT)) {
      // Walk to shovel
      fox_walk_to(fox, (SDL_FPoint){SHOVEL_POI.x, SHOVEL_POI.y});
      // Play shovel animation
      // Add key to inventory
      break;
    }
    if (SDL_PointInRect(&m_pos, &WALKABLE_HOTSPOT) &&
        !SDL_PointInRect(&m_pos, &NON_WALKABLE_HOTSPOT)) {
      // Walk to current position
      fox_walk_to(fox, (SDL_FPoint){m_pos.x, m_pos.y});
    }
    break;
  }
}

static void update(float delta_time) { fox_update(fox, delta_time); }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, &background, (SDL_Point){0, 0});

  render_animation(renderer, excavator, (SDL_Point){180, 350});
  render_animation(renderer, gate, (SDL_Point){491, 162});
  render_animation(renderer, shovel, (SDL_Point){112, 380});
  fox_render(fox, renderer);
}

static void deinit(void) {
  free_image_texture(&background);
  free_animation(excavator);
  free_animation(gate);
  free_animation(shovel);
  fox_free(fox);

  Mix_FreeMusic(music);
  music = NULL;
}

static void on_scene_active(void) {}

static void on_scene_inactive(void) {}

Scene playground_entrance_scene = {
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
