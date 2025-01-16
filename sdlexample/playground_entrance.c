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

// Music
static Mix_Music *music = NULL;

// Sound effects

static void init(void) {}

static bool load_media(SDL_Renderer *renderer) {
  if (!load_from_file("playground_entrance/background.png", renderer,
                      &background)) {
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

  return true;
}

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEBUTTONDOWN:
    game.current_scene = EXAMPLE;
    break;
  }
}

static void update(float delta_time) {}

static void render(SDL_Renderer *renderer) {
  render_image(renderer, &background, (SDL_Point){0, 0});
}

static void deinit(void) {
  free_image_texture(&background);

  Mix_FreeMusic(music);
  music = NULL;
}

Scene playground_entrance_scene = {
    init, load_media, process_input, update, render, deinit,
};
