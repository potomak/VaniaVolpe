//
//  outro.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/23/25.
//

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "constants.h"
#include "fox.h"
#include "game.h"
#include "image.h"
#include "vania_fox_the_slide.h"

#include "outro.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "vania_assets.h"

static ImageData images[VANIA_OUTRO_IMAGES_COUNT] = VANIA_OUTRO_IMAGES_INIT;
static const ImageData *background =
    &images[VANIA_OUTRO_IMAGE_OUTRO_BACKGROUND];

static Fox *fox;

// The static sprite layer (SCENES.md milestone 2): just the end card. render()
// draws only the waving fox. Music is declared on the Scene (.music, SCENES.md
// milestone 4); the framework plays and frees it.
static SceneSprite sprites[1];

static void init(void) {
  // The framework made the fox (actor_spec/actor_start below) before init; the
  // outro poses her waving.
  fox_wave(fox);
  sprites[0] = (SceneSprite){.image = background, .at = {0, 0}};
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
  // The end card is a static sprite (drawn by the framework); only the fox is
  // dynamic.
  fox_render(fox, renderer);
}

static void on_scene_active(void) {}

static void on_scene_inactive(void) {}

Scene outro_scene = {
    .init = init,
    .process_input = process_input,
    .update = update,
    .render = render,
    // The framework owns the fox's lifecycle (#141).
    .actor = &fox,
    .actor_spec = &FOX_SPEC,
    .actor_start = {398, 329},
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .sprites = sprites,
    .sprites_length = LEN(sprites),
    .images = images,
    .images_length = LEN(images),
    // The outro reuses the intro theme (SCENES.md milestone 4).
    .music = VANIA_MUSIC_CHUNK_INTRO_ASSET_INIT,
};
