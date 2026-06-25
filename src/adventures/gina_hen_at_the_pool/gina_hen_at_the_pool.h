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
enum gina_hen_at_the_pool_scene {
  POOL,                // entry: poolside, where the puzzle starts
  TREE,                // the tree with Carla the crow
  VINE,                // the grape vine
  SUNSCREEN_MINIGAME,  // brush sunscreen onto Gina
  GRAPES_MINIGAME,     // pick every grape
};

extern Adventure gina_hen_at_the_pool;

// Populate the adventure's scene table. Call once before register_adventures.
void gina_hen_at_the_pool_register(void);

#endif /* gina_hen_at_the_pool_h */
