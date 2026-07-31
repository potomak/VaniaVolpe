//
//  gina_nav.h
//  Walking between Gina's three outdoor scenes.
//
//  The pool, the tree and the vine form a ring joined at the screen edges, and
//  all three had the same two problems: the exits were invisible (a bare 30px
//  strip of background), and arriving always dropped her on the same spot
//  regardless of which way she had come. Both are fixed once here rather than
//  three times.
//

#ifndef gina_nav_h
#define gina_nav_h

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "image.h"
#include "scene.h"

// No scene: she did not arrive over a scene edge (a fresh start, or back from
// a minigame), so there is no entry side to place her on.
#define GINA_NO_SCENE (-1)

// The two exit hotspots, appended to the scene's own table. Both walk her to
// the edge first and switch scenes on arrival, recording where she left from
// so the next scene can place her. `enabled` gates them (NULL for always) —
// the poolside keeps her in the shade until the sunscreen is on. Writes 2
// entries; returns 2.
int gina_nav_hotspots(Hotspot *out, bool (*enabled)(void));

// The matching walk targets, for the scene's POI table (debug overlay). Writes
// 2 entries; returns 2.
int gina_nav_pois(SDL_Point *out);

// Draw both exit arrows. They are two instances of one sheet — one
// AnimationData carries one frame cursor and these squiggle independently —
// and the left one is drawn mirrored, so a single drawing marks both edges.
// Pass the same predicate given to gina_nav_hotspots, so an arrow is never
// advertising an exit that a tap would refuse.
void gina_nav_render(SDL_Renderer *renderer, AnimationData *left,
                     AnimationData *right, bool (*visible)(void));

// Where to stand on entering the current scene: the exit that leads back the
// way she came, so walking off the pool's right edge puts her at the tree's
// left one. Returns `fallback` when she did not arrive over an edge. Consumes
// the recorded origin, so a later re-entry that is not a walk (returning from
// a minigame) falls back rather than reusing a stale side.
SDL_FPoint gina_nav_entry(SDL_FPoint fallback);

#endif /* gina_nav_h */
