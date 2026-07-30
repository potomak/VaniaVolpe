//
//  gina_worn.h
//  What Gina is carrying or wearing, drawn on top of her wherever she is.
//

#ifndef gina_worn_h
#define gina_worn_h

#include "hen.h"
#include "scene.h"

// The adventure's shared image bank (Adventure.images): these follow Gina from
// scene to scene, so they cannot belong to any one scene's image table.
extern ImageData gina_worn_images[];

// Draw whatever she has picked up, on top of her sprite. Reads gina_state
// directly, so there is nothing to keep in sync — a scene just calls this after
// drawing her and her kit appears, in that scene and every other one.
void gina_render_worn(SDL_Renderer *renderer, const Hen *gina);

#endif /* gina_worn_h */
