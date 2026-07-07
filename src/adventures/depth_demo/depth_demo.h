//
//  depth_demo.h
//  A one-scene developer demo adventure: the reference implementation for
//  the depth features (DEPTH_AND_CAMERA.md) — the y-sorted action layer
//  (Phase 1) and depth bands + sprite variants (Phase 2).
//

#ifndef depth_demo_h
#define depth_demo_h

#include "adventure.h"

enum depth_demo_scenes { FIELD };

extern Adventure depth_demo;

// Build the adventure's scene table (call before game_init).
void depth_demo_register(void);

#endif /* depth_demo_h */
