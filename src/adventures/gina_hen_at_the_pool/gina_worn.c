//
//  gina_worn.c
//  What Gina is carrying or wearing. Once she collects an item she keeps it in
//  view, so the art has to outlive the scene she picked it up in — hence the
//  adventure-level image bank rather than a scene's images table.
//

#include "gina_worn.h"

#include "gina_state.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "gina_assets.h"

ImageData gina_worn_images[GINA_HEN_IMAGES_COUNT] = GINA_HEN_IMAGES_INIT;
const int gina_worn_images_length = GINA_HEN_IMAGES_COUNT;

// A worn overlay is authored on Gina's own frame, so it lines up with her
// sprite when its top-left sits half a frame up and left of her centre. Taking
// the offset from the image means no per-item tuning: the art carries the
// registration.
static void render_overlay(SDL_Renderer *renderer, const Hen *gina,
                           const ImageData *image) {
  actor_render_carried(gina, renderer, image,
                       (SDL_Point){-image->width / 2, -image->height / 2});
}

void gina_render_worn(SDL_Renderer *renderer, const Hen *gina) {
  // Carla's basket, from her offer until Gina hands it back full of grapes.
  if (gina_state.has_basket) {
    render_overlay(renderer, gina,
                   &gina_worn_images[GINA_HEN_IMAGE_BASKET_CARRIED]);
  }
  if (gina_state.has_goggles) {
    render_overlay(renderer, gina,
                   &gina_worn_images[GINA_HEN_IMAGE_GOGGLES_WORN]);
  }
  if (gina_state.float_state == FLOAT_RETRIEVED) {
    render_overlay(renderer, gina,
                   &gina_worn_images[GINA_HEN_IMAGE_FLOAT_WORN]);
  }
}
