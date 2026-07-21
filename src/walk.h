//
//  walk.h
//  Walkable-area geometry and pathfinding (see MOVEMENT.md): scenes declare
//  their walk geometry as rectangles (a WalkArea), the engine rasterises it
//  once into a low-resolution walkability grid (a WalkGrid), and clicks are
//  routed around blocked cells with A* plus line-of-sight smoothing.
//

#ifndef walk_h
#define walk_h

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "actor.h"
#include "constants.h"

// Authoring format: rectangles, as scenes define them.
typedef struct walk_area {
  const SDL_Rect *walkables; // union of legal ground
  int walkables_length;
  const SDL_Rect *blocked; // exclusion zones carved out of the union
  int blocked_length;      // 0 is fine (blocked may be NULL)
} WalkArea;

#define WALK_CELL_SIZE 10
// Grids are scene-sized (scene_size / WALK_CELL_SIZE); these bound the static
// storage for the largest scrollable scene (240x120 cells).
#define WALK_GRID_MAX_W (MAX_SCENE_W / WALK_CELL_SIZE)
#define WALK_GRID_MAX_H (MAX_SCENE_H / WALK_CELL_SIZE)

// Runtime format: 1 byte per cell, 1 = walkable. w x h cells are in use;
// storage is sized for the largest scene.
typedef struct walk_grid {
  int w;
  int h;
  Uint8 cells[WALK_GRID_MAX_H][WALK_GRID_MAX_W];
  // Horizontal extent of the walkable cells (leftmost / rightmost column
  // holding any walkable cell), cached when the grid is built or parsed so the
  // drag clamp is O(1) per event instead of scanning the grid. -1 when the
  // grid has no walkable cell. Recomputed on every rebuild (e.g. the pool's
  // shade -> strip switch), so it always matches `cells`.
  int walkable_min_cx;
  int walkable_max_cx;
} WalkGrid;

// Rasterise the rects into the grid (called once from the scene's init). A
// cell is walkable iff its centre point is inside a walkable rect and inside
// no blocked rect. scene_size is the scene's world size in px — for every
// static scene, {WINDOW_WIDTH, WINDOW_HEIGHT}.
void walk_grid_build(WalkGrid *grid, const WalkArea *area,
                     SDL_Point scene_size);

// Fill the grid from the scene's committed mask (<dir>/walkable.walk, painted
// with the debug overlay's paint mode — see TOOLS.md) when one exists, parses
// cleanly, and matches the scene's grid size; else rasterise the rects. `dir`
// is the scene's asset directory, e.g. "playground"; NULL skips straight to
// the rects.
void walk_grid_init(WalkGrid *grid, const WalkArea *area, SDL_Point scene_size,
                    const char *dir);

// Strict .walk parser (see MOVEMENT.md): a "walk <w> <h>" header (dimensions
// within the maximums above), then <h> rows of <w> cells, '#' walkable / '.'
// blocked. Tolerates \r and a missing final newline; anything else rejects
// the file. The grid takes its dimensions from the header; callers loading a
// scene's mask check them against the scene (see walk_grid_init).
bool walk_grid_parse(const char *data, size_t size, WalkGrid *grid);

// Serialize the grid in the .walk format. Returns the byte length written
// (excluding the NUL), or -1 if out_size is too small. WALK_FILE_MAX bytes
// always suffice.
#define WALK_FILE_MAX (32 + WALK_GRID_MAX_H * (WALK_GRID_MAX_W + 1))
int walk_grid_serialize(const WalkGrid *grid, char *out, size_t out_size);

// Write the grid to <asset root>/common/<dir>/walkable.walk (the source tree,
// when running from the repo root). Native builds only; returns false (with a
// log) on the web build or on I/O failure.
bool walk_grid_save(const WalkGrid *grid, const char *dir);

// Is the cell containing p walkable? False outside the scene.
bool walk_grid_contains(const WalkGrid *grid, SDL_Point p);

// Nearest legal point to p: p itself if its cell is walkable, else the centre
// of the nearest walkable cell (BFS). Degenerate all-blocked grids return p
// with a warning.
SDL_FPoint walk_grid_nearest(const WalkGrid *grid, SDL_Point p);

// Clamp x to the horizontal extent of the grid's walkable cells — the centre
// of the leftmost walkable column to the centre of the rightmost. Used while
// dragging the actor so her horizontal position never leaves the walkable
// area (e.g. Gina, before the sunscreen, stays within the umbrella's shade)
// even as the pointer roams outside it. A grid with no walkable cell leaves x
// unchanged. Clamping to column centres keeps every reachable x over ground,
// so the straight-down drop scan lands with no horizontal jump.
float walk_grid_clamp_x(const WalkGrid *grid, float x);

// Fill out[] with up to max_out waypoints from `from` to `to` (excluding
// `from`, including the final destination). Returns the count (>= 1).
// Illegal endpoints are snapped to legal ground first; an unreachable goal
// routes to the reachable cell closest to it (best effort).
int walk_grid_find_path(const WalkGrid *grid, SDL_FPoint from, SDL_FPoint to,
                        SDL_FPoint *out, int max_out);

// Route `actor` through `grid` to `goal`. exact_goal = the walk must end at
// `goal` even if it is on non-walkable ground (hotspot POIs, e.g. the shovel
// inside the sandbox: the actor walks close along legal ground, then the
// final approach is exempt); otherwise the goal is snapped to legal ground
// first (fall-through clicks).
void walk_actor_to(Actor *actor, const WalkGrid *grid, SDL_FPoint goal,
                   bool exact_goal, void (*on_end)(void));

// Drag & drop (LIVELINESS.md Part 2). Scenes call this first in
// process_input, with their live grid. A press on the actor's (padded)
// sprite arms a drag but still falls through to the scene — a hotspot the
// actor stands on keeps working for plain taps. Pointer travel past
// DRAG_START_THRESHOLD then steals the actor (cancelling whatever walk the
// press started). While carried, her horizontal position is clamped to the
// grid's walkable extent (walk_grid_clamp_x), so she can be lifted up out of
// the area vertically but never dragged out of it sideways (e.g. Gina stays
// within the umbrella's shade before the sunscreen). The release then drops
// her onto the first walkable cell straight below the drop point (its grid
// column; no ground below falls back to walk_grid_nearest) — so a drop can
// never break a walkable-area invariant. Returns true when the event was the
// drag's (carry or release); consumed events must not reach the hotspots.
bool walk_actor_drag_event(Actor *actor, const WalkGrid *grid,
                           const SDL_Event *event);

#endif /* walk_h */
