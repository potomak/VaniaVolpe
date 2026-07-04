//
//  walk.c
//  Walkability grid + A* pathfinding (see MOVEMENT.md). Everything is
//  fixed-size and allocation-free: the grid is one byte per 10x10-px cell,
//  A* runs over static work arrays once per click, and paths are smoothed
//  with a line-of-sight pass so actors walk a few straight segments.
//

#include <math.h>
#include <stdlib.h>

#include "walk.h"

#define GRID_CELLS (WALK_GRID_W * WALK_GRID_H)

// Costs for A* steps (x10 so diagonals can be 14 ~= 10 * sqrt(2)).
#define WALK_COST_STRAIGHT 10
#define WALK_COST_DIAGONAL 14

// Distance between line-of-sight samples, in px.
#define WALK_SAMPLE_STEP (WALK_CELL_SIZE / 2)

static int cell_index(int cx, int cy) { return cy * WALK_GRID_W + cx; }

static SDL_FPoint cell_center(int cx, int cy) {
  return (SDL_FPoint){cx * WALK_CELL_SIZE + WALK_CELL_SIZE / 2,
                      cy * WALK_CELL_SIZE + WALK_CELL_SIZE / 2};
}

static bool cell_walkable(const WalkGrid *grid, int cx, int cy) {
  if (cx < 0 || cx >= WALK_GRID_W || cy < 0 || cy >= WALK_GRID_H) {
    return false;
  }
  return grid->cells[cy][cx] != 0;
}

void walk_grid_build(WalkGrid *grid, const WalkArea *area) {
  for (int cy = 0; cy < WALK_GRID_H; cy++) {
    for (int cx = 0; cx < WALK_GRID_W; cx++) {
      // Centre sampling keeps 1-px overlap seams between adjacent walkable
      // rects connected (see MOVEMENT.md).
      SDL_Point center = {cx * WALK_CELL_SIZE + WALK_CELL_SIZE / 2,
                          cy * WALK_CELL_SIZE + WALK_CELL_SIZE / 2};
      bool walkable = false;
      for (int i = 0; i < area->walkables_length; i++) {
        if (SDL_PointInRect(&center, &area->walkables[i])) {
          walkable = true;
          break;
        }
      }
      for (int i = 0; walkable && i < area->blocked_length; i++) {
        if (SDL_PointInRect(&center, &area->blocked[i])) {
          walkable = false;
        }
      }
      grid->cells[cy][cx] = walkable ? 1 : 0;
    }
  }
}

bool walk_grid_contains(const WalkGrid *grid, SDL_Point p) {
  if (p.x < 0 || p.x >= WINDOW_WIDTH || p.y < 0 || p.y >= WINDOW_HEIGHT) {
    return false;
  }
  return cell_walkable(grid, p.x / WALK_CELL_SIZE, p.y / WALK_CELL_SIZE);
}

// Clamp a value to the inclusive range [lo, hi].
static int clampi(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

SDL_FPoint walk_grid_nearest(const WalkGrid *grid, SDL_Point p) {
  if (walk_grid_contains(grid, p)) {
    return (SDL_FPoint){p.x, p.y};
  }

  // Breadth-first search outward from p's cell; the first walkable cell found
  // is (approximately) the nearest. Fixed neighbour order keeps results
  // deterministic.
  static Uint16 queue[GRID_CELLS];
  static Uint8 visited[GRID_CELLS];
  SDL_memset(visited, 0, sizeof(visited));

  int start_cx = clampi(p.x / WALK_CELL_SIZE, 0, WALK_GRID_W - 1);
  int start_cy = clampi(p.y / WALK_CELL_SIZE, 0, WALK_GRID_H - 1);
  int head = 0;
  int tail = 0;
  queue[tail++] = (Uint16)cell_index(start_cx, start_cy);
  visited[cell_index(start_cx, start_cy)] = 1;

  while (head < tail) {
    int idx = queue[head++];
    int cx = idx % WALK_GRID_W;
    int cy = idx / WALK_GRID_W;
    if (cell_walkable(grid, cx, cy)) {
      return cell_center(cx, cy);
    }
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        int nx = cx + dx;
        int ny = cy + dy;
        if ((dx == 0 && dy == 0) || nx < 0 || nx >= WALK_GRID_W || ny < 0 ||
            ny >= WALK_GRID_H) {
          continue;
        }
        int nidx = cell_index(nx, ny);
        if (!visited[nidx]) {
          visited[nidx] = 1;
          queue[tail++] = (Uint16)nidx;
        }
      }
    }
  }

  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
              "walk_grid_nearest: no walkable cell in the grid");
  return (SDL_FPoint){p.x, p.y};
}

// Octile-distance heuristic in WALK_COST units (admissible for 10/14 costs).
static Uint32 heuristic(int cx, int cy, int gx, int gy) {
  int dx = abs(cx - gx);
  int dy = abs(cy - gy);
  int min = dx < dy ? dx : dy;
  return (Uint32)(WALK_COST_STRAIGHT * (dx + dy) -
                  (2 * WALK_COST_STRAIGHT - WALK_COST_DIAGONAL) * min);
}

// Every sample along a->b sits in a walkable cell. Sampling sidesteps exact
// segment-vs-rect-union geometry; at WALK_SAMPLE_STEP px it cannot skip a
// cell.
static bool segment_is_walkable(const WalkGrid *grid, SDL_FPoint a,
                                SDL_FPoint b) {
  float dx = b.x - a.x;
  float dy = b.y - a.y;
  int steps = (int)(sqrtf(dx * dx + dy * dy) / WALK_SAMPLE_STEP) + 1;
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;
    SDL_Point p = {(int)(a.x + dx * t), (int)(a.y + dy * t)};
    if (!walk_grid_contains(grid, p)) {
      return false;
    }
  }
  return true;
}

int walk_grid_find_path(const WalkGrid *grid, SDL_FPoint from, SDL_FPoint to,
                        SDL_FPoint *out, int max_out) {
  // Endpoint normalisation: an illegal start (a POI on blocked ground, the
  // slide teleport) or goal is snapped to the nearest legal point first.
  SDL_Point from_i = {(int)from.x, (int)from.y};
  SDL_Point to_i = {(int)to.x, (int)to.y};
  SDL_FPoint from2 =
      walk_grid_contains(grid, from_i) ? from : walk_grid_nearest(grid, from_i);
  SDL_FPoint to2 =
      walk_grid_contains(grid, to_i) ? to : walk_grid_nearest(grid, to_i);

  // Degenerate grid (nearest found nothing legal): behave like today.
  if (!walk_grid_contains(grid, (SDL_Point){(int)from2.x, (int)from2.y}) ||
      !walk_grid_contains(grid, (SDL_Point){(int)to2.x, (int)to2.y})) {
    out[0] = to;
    return 1;
  }

  int start =
      cell_index((int)from2.x / WALK_CELL_SIZE, (int)from2.y / WALK_CELL_SIZE);
  int goal =
      cell_index((int)to2.x / WALK_CELL_SIZE, (int)to2.y / WALK_CELL_SIZE);
  if (start == goal) {
    out[0] = to2;
    return 1;
  }

  // A* over the grid, 8-connected, no corner cutting. The open "list" is a
  // plain scan: this runs once per click, not per frame, so worst case over
  // 4800 cells is still trivial.
  static Uint32 g_cost[GRID_CELLS];
  static Uint16 came_from[GRID_CELLS];
  static Uint8 in_open[GRID_CELLS];
  static Uint8 closed[GRID_CELLS];
  SDL_memset(in_open, 0, sizeof(in_open));
  SDL_memset(closed, 0, sizeof(closed));

  int gx = goal % WALK_GRID_W;
  int gy = goal / WALK_GRID_W;
  g_cost[start] = 0;
  came_from[start] = (Uint16)start;
  in_open[start] = 1;

  // Best-effort target: the visited cell closest (by heuristic) to the goal,
  // so an unreachable goal still routes as near as possible.
  int best = start;
  Uint32 best_h = heuristic(start % WALK_GRID_W, start / WALK_GRID_W, gx, gy);
  bool reached = false;

  for (;;) {
    // Pop the open node with the lowest f = g + h.
    int current = -1;
    Uint32 current_f = 0;
    for (int i = 0; i < GRID_CELLS; i++) {
      if (!in_open[i]) {
        continue;
      }
      Uint32 f =
          g_cost[i] + heuristic(i % WALK_GRID_W, i / WALK_GRID_W, gx, gy);
      if (current < 0 || f < current_f) {
        current = i;
        current_f = f;
      }
    }
    if (current < 0) {
      break; // open list empty: goal unreachable
    }
    in_open[current] = 0;
    closed[current] = 1;

    int cx = current % WALK_GRID_W;
    int cy = current / WALK_GRID_W;
    Uint32 h = heuristic(cx, cy, gx, gy);
    if (h < best_h) {
      best_h = h;
      best = current;
    }
    if (current == goal) {
      reached = true;
      break;
    }

    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) {
          continue;
        }
        int nx = cx + dx;
        int ny = cy + dy;
        if (!cell_walkable(grid, nx, ny)) {
          continue;
        }
        // A diagonal step is allowed only if both adjacent orthogonal cells
        // are walkable (no cutting a blocked corner).
        if (dx != 0 && dy != 0 &&
            (!cell_walkable(grid, cx + dx, cy) ||
             !cell_walkable(grid, cx, cy + dy))) {
          continue;
        }
        int nidx = cell_index(nx, ny);
        if (closed[nidx]) {
          continue;
        }
        Uint32 step =
            (dx != 0 && dy != 0) ? WALK_COST_DIAGONAL : WALK_COST_STRAIGHT;
        Uint32 tentative = g_cost[current] + step;
        if (!in_open[nidx] || tentative < g_cost[nidx]) {
          g_cost[nidx] = tentative;
          came_from[nidx] = (Uint16)current;
          in_open[nidx] = 1;
        }
      }
    }
  }

  int end = reached ? goal : best;
  if (end == start) {
    // Nowhere better to go (e.g. the goal is in a separate region and the
    // actor is already as close as it gets): stay put rather than walking
    // straight through blocked ground.
    out[0] = from2;
    return 1;
  }

  // Reconstruct the cell path start..end, then convert to points: the exact
  // start, the intermediate cell centres, and the exact goal when reached.
  static Uint16 cells_reversed[GRID_CELLS];
  int cells_length = 0;
  for (int idx = end; idx != start; idx = came_from[idx]) {
    cells_reversed[cells_length++] = (Uint16)idx;
  }

  static SDL_FPoint pts[GRID_CELLS + 1];
  int pts_length = 0;
  pts[pts_length++] = from2;
  for (int i = cells_length - 1; i >= 0; i--) {
    int idx = cells_reversed[i];
    pts[pts_length++] = cell_center(idx % WALK_GRID_W, idx / WALK_GRID_W);
  }
  if (reached) {
    pts[pts_length - 1] = to2;
  }

  // Greedy line-of-sight smoothing: from each point, jump to the furthest
  // path point still directly reachable. Adjacent cell centres always have
  // line of sight, so this terminates.
  int count = 0;
  int i = 0;
  while (i < pts_length - 1) {
    int j;
    for (j = pts_length - 1; j > i + 1; j--) {
      if (segment_is_walkable(grid, pts[i], pts[j])) {
        break;
      }
    }
    if (count >= max_out) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "walk_grid_find_path: path needs more than %d waypoints",
                  max_out);
      out[0] = to;
      return 1;
    }
    out[count++] = pts[j];
    i = j;
  }
  return count;
}

void walk_actor_to(Actor *actor, const WalkGrid *grid, SDL_FPoint goal,
                   bool exact_goal, void (*on_end)(void)) {
  SDL_Point goal_i = {(int)goal.x, (int)goal.y};
  SDL_FPoint snapped =
      walk_grid_contains(grid, goal_i) ? goal : walk_grid_nearest(grid, goal_i);

  SDL_FPoint path[ACTOR_MAX_WAYPOINTS];
  int count = walk_grid_find_path(grid, actor->current_position, snapped, path,
                                  ACTOR_MAX_WAYPOINTS);
  // The final approach to an exact goal is exempt from the walk area (POIs
  // like the shovel sit on blocked ground on purpose).
  if (exact_goal && (snapped.x != goal.x || snapped.y != goal.y)) {
    if (count < ACTOR_MAX_WAYPOINTS) {
      path[count++] = goal;
    } else {
      path[count - 1] = goal;
    }
  }
  actor_walk_path(actor, path, count, on_end);
}
