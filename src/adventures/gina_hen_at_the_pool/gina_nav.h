//
//  gina_nav.h
//  Walking between Gina's three outdoor scenes.
//
//  Each scene shows, up near the horizon, a tile standing in for each place it
//  connects to — the poolside sees a brown tree on its right and a green vine
//  on its left. Walking to a tile takes her there, and she arrives at the tile
//  that leads back, so the two scenes agree about which side of the world they
//  share.
//
//  The ground reaches up to those tiles, and the depth ramp is steep enough
//  that she is visibly smaller when she gets there. All three scenes share that
//  geometry, so it lives here rather than in triplicate.
//

#ifndef gina_nav_h
#define gina_nav_h

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "image.h"
#include "scaling.h"
#include "scene.h"
#include "walk.h"

// The ground all three outdoor scenes stand on: the near strip, plus a path up
// each side to the tiles on the horizon.
extern const WalkArea GINA_OUTDOOR_WALK_AREA;

// Depth over that ground (SCALING.md). Shared with the walk area because the
// two are one decision: the ramp has to reach whatever the paths reach, or she
// would arrive at the horizon still full size.
extern const ScaleRamp GINA_OUTDOOR_RAMP;

// The two destination tiles, appended to the scene's own hotspot table. Each
// walks her to the path end below it and changes scene on arrival, standing
// her at the far scene's opposite tile. `left_boil` / `right_boil` are the
// destination sheets — a scene lists the two GINA_NAV_ANIM_TO_*_BOIL_SPECs its
// row of the ring calls for, since a spec table is built at compile time.
// `enabled` gates both (NULL for always) — the poolside keeps her in the shade
// until the sunscreen is on. Writes 2 entries; returns 2.
int gina_nav_hotspots(Hotspot *out, AnimationData *left_boil,
                      AnimationData *right_boil, bool (*enabled)(void));

// The matching walk targets, for the scene's POI table (debug overlay). Writes
// 2 entries; returns 2.
int gina_nav_pois(SDL_Point *out);

#endif /* gina_nav_h */
