//
//  gina_worn.c
//  What Gina is carrying or wearing. Once she collects an item she keeps it in
//  view, so the art has to outlive the scene she picked it up in — hence the
//  adventure-level image bank rather than a scene's images table.
//
//  The art is the same common/items drawing the scenes use for the object lying
//  on the ground, so each item is drawn once and stays the same size in her
//  wings as it was at her feet.
//

#include "gina_worn.h"

#include "gina_state.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "gina_assets.h"

ImageData gina_worn_images[GINA_ITEMS_IMAGES_COUNT] = GINA_ITEMS_IMAGES_INIT;

// Where each item rides on Gina, as an offset from her sprite centre, measured
// off her idle frame: the goggles over her eye, the ring around her middle, a
// carried thing at beak height out in front. The art is *centred* on the point,
// so real art can come in at a different size without re-tuning these.
static const SDL_Point ON_HER_EYE = {-12, -11};
static const SDL_Point AROUND_HER_MIDDLE = {9, 22};
static const SDL_Point AT_BEAK_HEIGHT = {-30, 2};

static void render_item(SDL_Renderer *renderer, const Hen *gina, int image,
                        SDL_Point at) {
  const ImageData *item = &gina_worn_images[image];
  actor_render_carried(
      gina, renderer, item,
      (SDL_Point){at.x - item->width / 2, at.y - item->height / 2});
}

void gina_render_worn(SDL_Renderer *renderer, const Hen *gina) {
  // Carla's basket, from her offer until Gina hands it back full of grapes.
  if (gina_state.has_basket) {
    render_item(renderer, gina, GINA_ITEMS_IMAGE_BASKET, AT_BEAK_HEIGHT);
  }
  if (gina_state.has_goggles) {
    render_item(renderer, gina, GINA_ITEMS_IMAGE_GOGGLES, ON_HER_EYE);
  }
  if (gina_state.float_state == FLOAT_RETRIEVED) {
    render_item(renderer, gina, GINA_ITEMS_IMAGE_FLOAT, AROUND_HER_MIDDLE);
  }
}
