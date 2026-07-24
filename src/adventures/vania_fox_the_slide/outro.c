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
  // Drag & drop (LIVELINESS.md Part 2, #41): a press-drag on the fox picks her
  // up here too. The outro has no walkable area, so the drag takes a NULL grid
  // — she is set back down where released. A press that becomes a drag is
  // consumed here.
  if (walk_actor_drag_event(fox, NULL, event)) {
    return;
  }
  // A plain click (not a drag) returns to the adventure-selection menu: the
  // outro is the end of the adventure, so a click leaves it rather than
  // restarting in place. Navigate on release, not press, so a press that turns
  // into a drag of the fox doesn't also jump to the hub before she can be
  // picked up.
  if (event->type == SDL_MOUSEBUTTONUP) {
    return_to_hub();
  }
}

// No update/render: the end card is a static sprite and the fox is the only
// dynamic thing, so the framework ticks and draws her (#147).

static void on_scene_active(void) {
  // Re-pose the fox every time the outro is shown, so reaching it again (or
  // dragging her away, #41) always returns her to her waving spot. Matches
  // .actor_start below.
  fox->current_position = (SDL_FPoint){398, 329};
  actor_play_state(fox, WAVING);
}

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
