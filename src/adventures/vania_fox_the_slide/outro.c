//
//  outro.c
//  Tiny Adventures
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

// Static sprite layer: just the end card.
static SceneSprite sprites[1];

static void init(void) {
  // Pose the fox waving.
  actor_play_state(fox, WAVING);
  sprites[0] = (SceneSprite){.image = background, .at = {0, 0}};
}

static void process_input(SDL_Event *event) {
  // Drag & drop (LIVELINESS.md): an upward pull on the fox lifts her, and
  // letting go sets her back down. The drag is consumed here.
  if (actor_drag_event(fox, event)) {
    return;
  }
  // A plain click leaves the adventure for the selection menu. Navigate on
  // release, not press, so a press that turns into a drag of the fox doesn't
  // also jump to the hub before she can be picked up.
  if (event->type == SDL_MOUSEBUTTONUP) {
    return_to_hub();
  }
}

static void on_scene_active(void) {
  // Re-pose the fox whenever the outro is shown, so reaching it again (or
  // dragging her away) always returns her to her waving spot. Matches
  // .actor_start.
  fox->current_position = (SDL_FPoint){398, 329};
  actor_play_state(fox, WAVING);
}

static void on_scene_inactive(void) {}

Scene outro_scene = {
    .init = init,
    .process_input = process_input,
    .actor = &fox,
    .actor_spec = &FOX_SPEC,
    .actor_start = {398, 329},
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .sprites = sprites,
    .sprites_length = LEN(sprites),
    .images = images,
    .images_length = LEN(images),
    // The outro reuses the intro theme.
    .music = VANIA_MUSIC_CHUNK_INTRO_ASSET_INIT,
};
