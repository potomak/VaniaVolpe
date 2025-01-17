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
#include "game.h"
#include "image.h"

#include "playground_entrance.h"

// Background image
static ImageData background = {NULL, 0, 0};

// Animations
static AnimationData *excavator;
static AnimationData *gate;
static AnimationData *shovel;
static AnimationData *fox;

// Music
static Mix_Music *music = NULL;

// Sound effects

static void init(void) {
  excavator = make_animation_data(4);
  gate = make_animation_data(7);
  shovel = make_animation_data(5);
  fox = make_animation_data(4);
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

  if (!load_from_file("playground_entrance/fox.png", renderer, &fox->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  fox->sprite_clips[0].x = 0;
  fox->sprite_clips[0].y = 0;
  fox->sprite_clips[0].w = 211;
  fox->sprite_clips[0].h = 163;

  fox->sprite_clips[1].x = 0;
  fox->sprite_clips[1].y = 163;
  fox->sprite_clips[1].w = 211;
  fox->sprite_clips[1].h = 163;

  fox->sprite_clips[2].x = 0;
  fox->sprite_clips[2].y = 326;
  fox->sprite_clips[2].w = 211;
  fox->sprite_clips[2].h = 163;

  fox->sprite_clips[3].x = 0;
  fox->sprite_clips[3].y = 489;
  fox->sprite_clips[3].w = 211;
  fox->sprite_clips[3].h = 163;

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
  case SDL_MOUSEBUTTONDOWN:
    //    set_active_scene(EXAMPLE);
    break;
  }
}

static void update(float delta_time) {}

static void render(SDL_Renderer *renderer) {
  render_image(renderer, &background, (SDL_Point){0, 0});

  render_animation(renderer, excavator, (SDL_Point){180, 350});
  render_animation(renderer, gate, (SDL_Point){491, 162});
  render_animation(renderer, shovel, (SDL_Point){122, 380});
  render_animation(renderer, fox, (SDL_Point){474, 376});
}

static void deinit(void) {
  free_image_texture(&background);
  free_animation(excavator);
  free_animation(gate);
  free_animation(shovel);
  free_animation(fox);

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
};
