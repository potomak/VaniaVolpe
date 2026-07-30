//
//  outro.c
//  Gina's end card, reached after her last dive. A tap anywhere goes back to
//  the adventure selection.
//

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "constants.h"
#include "game.h"
#include "image.h"

#include "gina_hen_at_the_pool.h"
#include "hen.h"
#include "outro.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "gina_assets.h"

static ImageData images[GINA_OUTRO_IMAGES_COUNT] = GINA_OUTRO_IMAGES_INIT;
static const ImageData *background = &images[GINA_OUTRO_IMAGE_OUTRO_BACKGROUND];

static Hen *hen;

// Static sprite layer: just the end card.
static SceneSprite sprites[1];

static const SDL_FPoint HEN_POSE = {400, 430};

static void init(void) {
  sprites[0] = (SceneSprite){.image = background, .at = {0, 0}};
}

static void process_input(SDL_Event *event) {
  // Drag & drop (LIVELINESS.md): an upward pull on Gina lifts her, and letting
  // go sets her back down. The drag is consumed here.
  if (actor_drag_event(hen, event)) {
    return;
  }
  // A plain click leaves the adventure for the selection menu. Navigate on
  // release, not press, so a press that turns into a drag of Gina doesn't also
  // jump to the hub before she can be picked up.
  if (event->type == SDL_MOUSEBUTTONUP) {
    return_to_hub();
  }
}

static void on_scene_active(void) {
  // Re-pose her whenever the card is shown, so reaching it again (or dragging
  // her away) always returns her to her spot. Matches .actor_start.
  hen->current_position = HEN_POSE;
  hen->target_position = HEN_POSE;
  actor_play_state(hen, IDLE);
}

static void on_scene_inactive(void) {}

Scene gina_outro_scene = {
    .init = init,
    .process_input = process_input,
    .actor = &hen,
    .actor_spec = &HEN_SPEC,
    .actor_start = {400, 430},
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .sprites = sprites,
    .sprites_length = LEN(sprites),
    .images = images,
    .images_length = LEN(images),
};
