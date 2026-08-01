//
//  pool.h
//

#ifndef pool_h
#define pool_h

#include "scene.h"

// Where Gina stands when she walks in from each neighbouring scene, for that
// scene to pass to set_active_scene_at. Exported per scene because the door
// is a feature of *this* location's art: when the real background is drawn,
// nothing says the two will sit at mirrored positions, or at the same height.
extern const SDL_FPoint GINA_POOL_ENTRY_FROM_TREE; // arriving from the tree
extern const SDL_FPoint GINA_POOL_ENTRY_FROM_VINE; // arriving from the vine

extern Scene pool_scene;

#endif /* pool_h */
