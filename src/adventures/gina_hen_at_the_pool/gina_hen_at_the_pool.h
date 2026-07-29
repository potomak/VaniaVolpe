//
//  gina_hen_at_the_pool.h
//  The "Gina la Gallina in Piscina" adventure module: scene indices and
//  registration. (English identifier "at the pool": Gina is at the poolside
//  throughout and only dives in at the very end.)
//

#ifndef gina_hen_at_the_pool_h
#define gina_hen_at_the_pool_h

#include "adventure.h"

// Scene indices for this adventure (used by its scenes for transitions).
// Enumerators are global in C and every adventure's header is in scope at once,
// so these carry the adventure's prefix — plain INTRO/OUTRO belong to Vania.
enum gina_hen_at_the_pool_scene {
  GINA_INTRO,              // entry: title screen (Play / Exit)
  GINA_POOL,               // poolside, where the puzzle starts
  GINA_TREE,               // the tree with Carla the crow
  GINA_VINE,               // the grape vine
  GINA_SUNSCREEN_MINIGAME, // brush sunscreen onto Gina
  GINA_GRAPES_MINIGAME,    // pick every grape
};

extern Adventure gina_hen_at_the_pool;

// Populate the adventure's scene table. Call once before register_adventures.
void gina_hen_at_the_pool_register(void);

#endif /* gina_hen_at_the_pool_h */
