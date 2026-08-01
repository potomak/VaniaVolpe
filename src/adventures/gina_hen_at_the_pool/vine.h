//
//  vine.h
//

#ifndef vine_h
#define vine_h

#include "scene.h"

// Where Gina stands when she walks in from each neighbouring scene, for that
// scene to pass to set_active_scene_at. Exported per scene because the door
// is a feature of *this* location's art: when the real background is drawn,
// nothing says the two will sit at mirrored positions, or at the same height.
extern const SDL_FPoint GINA_VINE_ENTRY_FROM_TREE; // arriving from the tree
extern const SDL_FPoint GINA_VINE_ENTRY_FROM_POOL; // arriving from the poolside

extern Scene vine_scene;

#endif /* vine_h */
