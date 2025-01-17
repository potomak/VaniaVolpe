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
static SDL_Rect hotspots[4];

static void init(void) {
  excavator = make_animation_data(4, ONE_SHOT);
  gate = make_animation_data(7, ONE_SHOT);
  shovel = make_animation_data(5, LOOP);
  fox = make_fox((SDL_FPoint){580, 457});

  hotspots[0] = GATE_HOTSPOT;
  hotspots[1] = EXCAVATOR_HOTSPOT;
  hotspots[2] = SHOVEL_HOTSPOT;
  hotspots[3] = WALKABLE_HOTSPOT;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!load_from_file("playground_entrance/background.png", renderer,
                      &background)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  if (!load_from_file("playground_entrance/excavator.png", renderer,
                      &excavator->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  excavator->sprite_clips[0].x = 0;
  excavator->sprite_clips[0].y = 0;
  excavator->sprite_clips[0].w = 119;
  excavator->sprite_clips[0].h = 56;

  excavator->sprite_clips[1].x = 0;
  excavator->sprite_clips[1].y = 56;
  excavator->sprite_clips[1].w = 119;
  excavator->sprite_clips[1].h = 56;

  excavator->sprite_clips[2].x = 0;
  excavator->sprite_clips[2].y = 112;
  excavator->sprite_clips[2].w = 119;
  excavator->sprite_clips[2].h = 56;

  excavator->sprite_clips[3].x = 0;
  excavator->sprite_clips[3].y = 168;
  excavator->sprite_clips[3].w = 119;
  excavator->sprite_clips[3].h = 56;

  if (!load_from_file("playground_entrance/gate.png", renderer, &gate->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  gate->sprite_clips[0].x = 0;
  gate->sprite_clips[0].y = 0;
  gate->sprite_clips[0].w = 182;
  gate->sprite_clips[0].h = 208;

  gate->sprite_clips[1].x = 182;
  gate->sprite_clips[1].y = 0;
  gate->sprite_clips[1].w = 182;
  gate->sprite_clips[1].h = 208;

  gate->sprite_clips[2].x = 0;
  gate->sprite_clips[2].y = 208;
  gate->sprite_clips[2].w = 182;
  gate->sprite_clips[2].h = 208;

  gate->sprite_clips[3].x = 182;
  gate->sprite_clips[3].y = 208;
  gate->sprite_clips[3].w = 182;
  gate->sprite_clips[3].h = 208;

  gate->sprite_clips[4].x = 0;
  gate->sprite_clips[4].y = 416;
  gate->sprite_clips[4].w = 182;
  gate->sprite_clips[4].h = 208;

  gate->sprite_clips[5].x = 182;
  gate->sprite_clips[5].y = 416;
  gate->sprite_clips[5].w = 182;
  gate->sprite_clips[5].h = 208;

  gate->sprite_clips[6].x = 0;
  gate->sprite_clips[6].y = 624;
  gate->sprite_clips[6].w = 182;
  gate->sprite_clips[6].h = 208;

  if (!load_from_file("playground_entrance/shovel.png", renderer,
                      &shovel->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  shovel->sprite_clips[0].x = 0;
  shovel->sprite_clips[0].y = 0;
  shovel->sprite_clips[0].w = 88;
  shovel->sprite_clips[0].h = 67;

  shovel->sprite_clips[1].x = 88;
  shovel->sprite_clips[1].y = 0;
  shovel->sprite_clips[1].w = 88;
  shovel->sprite_clips[1].h = 67;

  shovel->sprite_clips[2].x = 0;
  shovel->sprite_clips[2].y = 67;
  shovel->sprite_clips[2].w = 88;
  shovel->sprite_clips[2].h = 67;

  shovel->sprite_clips[3].x = 88;
  shovel->sprite_clips[3].y = 67;
  shovel->sprite_clips[3].w = 88;
  shovel->sprite_clips[3].h = 67;

  shovel->sprite_clips[4].x = 0;
  shovel->sprite_clips[4].y = 134;
  shovel->sprite_clips[4].w = 88;
  shovel->sprite_clips[4].h = 67;

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
      // If key is in inventory open gate then go to PLAYGROUND scene
      // Else give hint about where to find the key
      set_active_scene(EXAMPLE);
    }
    if (SDL_PointInRect(&m_pos, &EXCAVATOR_HOTSPOT)) {
      // Play excavator animation
      excavator->is_playing = true;
    }
    if (SDL_PointInRect(&m_pos, &SHOVEL_HOTSPOT)) {
      // Walk to shovel
      // Play shovel animation
      // Add key to inventory
    }
    if (SDL_PointInRect(&m_pos, &WALKABLE_HOTSPOT)) {
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
    .hotspots_length = 4,
};
