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
  actor_play_state(fox, WAVING);
  sprites[0] = (SceneSprite){.image = background, .at = {0, 0}};
}

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEBUTTONDOWN:
    set_active_scene(INTRO);
    break;
  }
}

// No update/render: the end card is a static sprite and the fox is the only
// dynamic thing, so the framework ticks and draws her (#147).

static void on_scene_active(void) {}

static void on_scene_inactive(void) {}

Scene outro_scene = {
    .init = init,
    .process_input = process_input,
    // No update/render: the framework ticks and draws the actor (#147); it also
    // owns the fox's lifecycle (#141).
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
