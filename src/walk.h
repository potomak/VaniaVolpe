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
#define WALK_GRID_W (WINDOW_WIDTH / WALK_CELL_SIZE)  // 80
#define WALK_GRID_H (WINDOW_HEIGHT / WALK_CELL_SIZE) // 60

// Runtime format: 1 byte per cell, 1 = walkable.
typedef struct walk_grid {
  Uint8 cells[WALK_GRID_H][WALK_GRID_W];
} WalkGrid;

// Rasterise the rects into the grid (called once from the scene's init). A
// cell is walkable iff its centre point is inside a walkable rect and inside
// no blocked rect.
void walk_grid_build(WalkGrid *grid, const WalkArea *area);

// Fill the grid from the scene's committed mask (<dir>/walkable.walk, painted
// with the debug overlay's paint mode — see TOOLS.md) when one exists and
// parses cleanly; else rasterise the rects. `dir` is the scene's asset
// directory, e.g. "playground"; NULL skips straight to the rects.
void walk_grid_init(WalkGrid *grid, const WalkArea *area, const char *dir);

// Strict .walk parser (see MOVEMENT.md): a "walk <w> <h>" header matching the
// engine's grid size, then <h> rows of <w> cells, '#' walkable / '.' blocked.
// Tolerates \r and a missing final newline; anything else rejects the file.
bool walk_grid_parse(const char *data, size_t size, WalkGrid *grid);

// Serialize the grid in the .walk format. Returns the byte length written
// (excluding the NUL), or -1 if out_size is too small. WALK_FILE_MAX bytes
// always suffice.
#define WALK_FILE_MAX (32 + WALK_GRID_H * (WALK_GRID_W + 1))
int walk_grid_serialize(const WalkGrid *grid, char *out, size_t out_size);

// Write the grid to <asset root>/common/<dir>/walkable.walk (the source tree,
// when running from the repo root). Native builds only; returns false (with a
// log) on the web build or on I/O failure.
bool walk_grid_save(const WalkGrid *grid, const char *dir);

// Is the cell containing p walkable? False outside the screen.
bool walk_grid_contains(const WalkGrid *grid, SDL_Point p);

// Nearest legal point to p: p itself if its cell is walkable, else the centre
// of the nearest walkable cell (BFS). Degenerate all-blocked grids return p
// with a warning.
SDL_FPoint walk_grid_nearest(const WalkGrid *grid, SDL_Point p);

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

#endif /* walk_h */
