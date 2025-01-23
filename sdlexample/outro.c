//
//  outro.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/23/25.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "constants.h"
#include "fox.h"
#include "game.h"
#include "image.h"

#include "outro.h"

static ImageData images[1] = {
    {NULL, "outro/background.png", 0, 0},
};
static const ImageData *background = &images[0];

static Fox *fox;

// Music
static Mix_Music *music = NULL;

static void init(void) {
  fox = make_fox((SDL_FPoint){398, 329});
  fox_wave(fox);
}

static bool load_media(SDL_Renderer *renderer) {
  if (!fox_load_media(fox, renderer)) {
    fprintf(stderr, "Failed to load fox!\n");
    return false;
  }

  // Load music
  music = Mix_LoadMUS("intro/music.wav");
  if (music == NULL) {
    fprintf(stderr, "Failed to load music! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  return true;
}

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEBUTTONDOWN:
    set_active_scene(INTRO);
    break;
  }
}

static void update(float delta_time) { fox_update(fox, delta_time); }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});

  fox_render(fox, renderer);
}

static void deinit(void) {
  fox_free(fox);

  Mix_FreeMusic(music);
  music = NULL;
}

static void on_scene_active(void) { Mix_PlayMusic(music, -1); }

static void on_scene_inactive(void) { Mix_HaltMusic(); }

Scene outro_scene = {
    .init = init,
    .load_media = load_media,
    .process_input = process_input,
    .update = update,
    .render = render,
    .deinit = deinit,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .images = images,
    .images_length = LEN(images),
};
