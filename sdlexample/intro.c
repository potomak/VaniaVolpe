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

// Play button animation
const int PLAY_BUTTON_FRAMES = 3;
SDL_Rect play_button_sprite_clips[PLAY_BUTTON_FRAMES];
int play_button_frame = 0;
ImageData play_button_image = {NULL, 0, 0};

// Exit button animation
const int EXIT_BUTTON_FRAMES = 3;
SDL_Rect exit_button_sprite_clips[EXIT_BUTTON_FRAMES];
int exit_button_frame = 0;
ImageData exit_button_image = {NULL, 0, 0};

// Music
Mix_Music *music = NULL;

// Sound effects
Mix_Chunk *play_button_click_sound = NULL;
Mix_Chunk *exit_button_click_sound = NULL;

bool intro_load_media(SDL_Renderer *renderer) {
  if (!load_from_file("intro/background.png", renderer, &background_image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  if (!load_from_file("intro/play_button.png", renderer, &play_button_image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  play_button_sprite_clips[0].x = 0;
  play_button_sprite_clips[0].y = 0;
  play_button_sprite_clips[0].w = 800;
  play_button_sprite_clips[0].h = 600;

  play_button_sprite_clips[1].x = 0;
  play_button_sprite_clips[1].y = 600;
  play_button_sprite_clips[1].w = 800;
  play_button_sprite_clips[1].h = 600;

  play_button_sprite_clips[2].x = 0;
  play_button_sprite_clips[2].y = 1200;
  play_button_sprite_clips[2].w = 800;
  play_button_sprite_clips[2].h = 600;

  if (!load_from_file("intro/exit_button.png", renderer, &exit_button_image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  exit_button_sprite_clips[0].x = 0;
  exit_button_sprite_clips[0].y = 0;
  exit_button_sprite_clips[0].w = 800;
  exit_button_sprite_clips[0].h = 600;

  exit_button_sprite_clips[1].x = 0;
  exit_button_sprite_clips[1].y = 600;
  exit_button_sprite_clips[1].w = 800;
  exit_button_sprite_clips[1].h = 600;

  exit_button_sprite_clips[2].x = 0;
  exit_button_sprite_clips[2].y = 1200;
  exit_button_sprite_clips[2].w = 800;
  exit_button_sprite_clips[2].h = 600;

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

void intro_process_input(void) {}

void intro_update(void) {}

void render_background(SDL_Renderer *renderer, int x, int y) {
  // Set rendering space and render to screen
  SDL_Rect render_quad = {x, y, background_image.width,
                          background_image.height};

  // Render to screen
  SDL_RenderCopy(renderer, background_image.texture, NULL, &render_quad);
}

void render_play_button(SDL_Renderer *renderer, int x, int y) {
  // Set rendering space and render to screen
  SDL_Rect render_quad = {x, y, play_button_image.width,
                          play_button_image.height};

  // Render current frame
  SDL_Rect *clip = &play_button_sprite_clips[play_button_frame / 4];

  // Set clip rendering dimensions
  render_quad.w = clip->w;
  render_quad.h = clip->h;

  // Render to screen
  SDL_RenderCopy(renderer, play_button_image.texture, clip, &render_quad);

  // Go to next frame
  ++play_button_frame;

  // Cycle animation
  if (play_button_frame / 4 >= PLAY_BUTTON_FRAMES) {
    play_button_frame = 0;
  }
}

void render_exit_button(SDL_Renderer *renderer, int x, int y) {
  // Set rendering space and render to screen
  SDL_Rect render_quad = {x, y, exit_button_image.width,
                          exit_button_image.height};

  // Render current frame
  SDL_Rect *clip = &exit_button_sprite_clips[exit_button_frame / 4];

  // Set clip rendering dimensions
  render_quad.w = clip->w;
  render_quad.h = clip->h;

  // Render to screen
  SDL_RenderCopy(renderer, exit_button_image.texture, clip, &render_quad);

  // Go to next frame
  ++exit_button_frame;

  // Cycle animation
  if (exit_button_frame / 4 >= EXIT_BUTTON_FRAMES) {
    exit_button_frame = 0;
  }
}

void intro_render(SDL_Renderer *renderer) {
  render_background(renderer, 0, 0);
  render_play_button(renderer, 260, 140);
  render_exit_button(renderer, 220, 300);
}
