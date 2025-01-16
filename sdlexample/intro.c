//
//  intro.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "constants.h"
#include "image.h"

#include "intro.h"

// Background image
ImageData background_image = {NULL, 0, 0};

// Animations
AnimationData *play_button;
AnimationData *exit_button;

// Music
Mix_Music *music = NULL;

// Sound effects
Mix_Chunk *play_button_click_sound = NULL;
Mix_Chunk *exit_button_click_sound = NULL;

static void init(void) {
  play_button = make_animation_data(3);
  exit_button = make_animation_data(3);
}

static bool load_media(SDL_Renderer *renderer) {
  if (!load_from_file("intro/background.png", renderer, &background_image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  if (!load_from_file("intro/play_button.png", renderer, &play_button->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  play_button->sprite_clips[0].x = 0;
  play_button->sprite_clips[0].y = 0;
  play_button->sprite_clips[0].w = 800;
  play_button->sprite_clips[0].h = 600;

  play_button->sprite_clips[1].x = 0;
  play_button->sprite_clips[1].y = 600;
  play_button->sprite_clips[1].w = 800;
  play_button->sprite_clips[1].h = 600;

  play_button->sprite_clips[2].x = 0;
  play_button->sprite_clips[2].y = 1200;
  play_button->sprite_clips[2].w = 800;
  play_button->sprite_clips[2].h = 600;

  if (!load_from_file("intro/exit_button.png", renderer, &exit_button->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  exit_button->sprite_clips[0].x = 0;
  exit_button->sprite_clips[0].y = 0;
  exit_button->sprite_clips[0].w = 800;
  exit_button->sprite_clips[0].h = 600;

  exit_button->sprite_clips[1].x = 0;
  exit_button->sprite_clips[1].y = 600;
  exit_button->sprite_clips[1].w = 800;
  exit_button->sprite_clips[1].h = 600;

  exit_button->sprite_clips[2].x = 0;
  exit_button->sprite_clips[2].y = 1200;
  exit_button->sprite_clips[2].w = 800;
  exit_button->sprite_clips[2].h = 600;

  // Load music
  music = Mix_LoadMUS("intro/music.wav");
  if (music == NULL) {
    fprintf(stderr, "Failed to load music! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  // Load sound effects
  play_button_click_sound = Mix_LoadWAV("intro/play_button_click.wav");
  if (play_button_click_sound == NULL) {
    fprintf(stderr, "Failed to load sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  exit_button_click_sound = Mix_LoadWAV("intro/exit_button_click.wav");
  if (exit_button_click_sound == NULL) {
    fprintf(stderr, "Failed to load sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  return true;
}

static void process_input(SDL_Event *event) {}

static void update(float delta_time) {}

static void render(SDL_Renderer *renderer) {
  render_image(renderer, &background_image, (SDL_Point){0, 0});
  render_animation(renderer, play_button, (SDL_Point){260, 140});
  render_animation(renderer, exit_button, (SDL_Point){220, 300});
}

static void deinit(void) {
  free_image_texture(&background_image);
  free_animation(play_button);
  free_animation(exit_button);

  Mix_FreeChunk(play_button_click_sound);
  play_button_click_sound = NULL;
  Mix_FreeChunk(exit_button_click_sound);
  exit_button_click_sound = NULL;

  Mix_FreeMusic(music);
  music = NULL;
}

Scene intro_scene = {
    init, load_media, process_input, update, render, deinit,
};
