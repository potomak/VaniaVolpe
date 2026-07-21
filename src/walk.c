//
//  walk.c
//  Walkability grid + A* pathfinding (see MOVEMENT.md). Everything is
//  fixed-size and allocation-free: the grid is one byte per 10x10-px cell,
//  A* runs over static work arrays once per click, and paths are smoothed
//  with a line-of-sight pass so actors walk a few straight segments. Grids
//  are scene-sized (DEPTH_AND_CAMERA.md); the static arrays are sized for
//  the largest scrollable scene.
//

#include <math.h>
#include <stdlib.h>

#include "walk.h"

#define GRID_CELLS_MAX (WALK_GRID_MAX_W * WALK_GRID_MAX_H)

// Costs for A* steps (x10 so diagonals can be 14 ~= 10 * sqrt(2)).
#define WALK_COST_STRAIGHT 10
#define WALK_COST_DIAGONAL 14

// Distance between line-of-sight samples, in px (half a cell).
#define WALK_SAMPLE_STEP (WALK_CELL_SIZE / 2.0F)

static int cell_index(const WalkGrid *grid, int cx, int cy) {
  return cy * grid->w + cx;
}

static SDL_FPoint cell_center(int cx, int cy) {
  int x = cx * WALK_CELL_SIZE + WALK_CELL_SIZE / 2;
  int y = cy * WALK_CELL_SIZE + WALK_CELL_SIZE / 2;
  return (SDL_FPoint){x, y};
}

static bool cell_walkable(const WalkGrid *grid, int cx, int cy) {
  if (cx < 0 || cx >= grid->w || cy < 0 || cy >= grid->h) {
    return false;
  }
  return grid->cells[cy][cx] != 0;
}

void walk_grid_build(WalkGrid *grid, const WalkArea *area,
                     SDL_Point scene_size) {
  grid->w = scene_size.x / WALK_CELL_SIZE;
  grid->h = scene_size.y / WALK_CELL_SIZE;
  SDL_assert(grid->w > 0 && grid->w <= WALK_GRID_MAX_W);
  SDL_assert(grid->h > 0 && grid->h <= WALK_GRID_MAX_H);
  for (int cy = 0; cy < grid->h; cy++) {
    for (int cx = 0; cx < grid->w; cx++) {
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

bool walk_grid_parse(const char *data, size_t size, WalkGrid *grid) {
  // Header: "walk <w> <h>" — the mask is self-describing, so a scene-sized
  // grid can be checked against the scene it is loaded for. Scan a bounded,
  // NUL-terminated copy: `data` need not be NUL-terminated.
  char header[32];
  size_t header_max = size < sizeof(header) - 1 ? size : sizeof(header) - 1;
  SDL_memcpy(header, data, header_max);
  header[header_max] = '\0';
  int w = 0;
  int h = 0;
  int header_length = 0;
  if (sscanf(header, "walk %d %d%n", &w, &h, &header_length) != 2 ||
      header_length <= 0 || (size_t)header_length > size) {
    return false;
  }
  if (w <= 0 || w > WALK_GRID_MAX_W || h <= 0 || h > WALK_GRID_MAX_H) {
    return false;
  }
  size_t i = (size_t)header_length;
  if (i < size && data[i] == '\r') {
    i++;
  }
  if (i >= size || data[i] != '\n') {
    return false;
  }
  i++;

  // Parse into a scratch grid so a mid-file failure leaves *grid untouched
  // (static: a WalkGrid is too large for comfort on the wasm stack).
  static WalkGrid parsed;
  parsed.w = w;
  parsed.h = h;
  for (int cy = 0; cy < h; cy++) {
    for (int cx = 0; cx < w; cx++) {
      if (i >= size) {
        return false;
      }
      char c = data[i++];
      if (c == '#') {
        parsed.cells[cy][cx] = 1;
      } else if (c == '.') {
        parsed.cells[cy][cx] = 0;
      } else {
        return false;
      }
    }
    if (i < size && data[i] == '\r') {
      i++;
    }
    if (cy < h - 1) {
      if (i >= size || data[i] != '\n') {
        return false;
      }
      i++;
    } else if (i < size && data[i] == '\n') {
      i++; // the final newline is optional
    }
  }
  if (i != size) {
    return false; // trailing junk
  }
  *grid = parsed;
  return true;
}

void walk_grid_init(WalkGrid *grid, const WalkArea *area, SDL_Point scene_size,
                    const char *dir) {
  if (dir != NULL) {
    char path[ASSET_PATH_MAX];
    if (asset_try_resolve(
            (Asset){.filename = "walkable.walk", .directory = dir}, path,
            sizeof(path))) {
      size_t size = 0;
      char *data = SDL_LoadFile(path, &size);
      if (data != NULL) {
        bool ok = walk_grid_parse(data, size, grid) &&
                  grid->w == scene_size.x / WALK_CELL_SIZE &&
                  grid->h == scene_size.y / WALK_CELL_SIZE;
        SDL_free(data);
        if (ok) {
          return;
        }
      }
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Malformed or wrong-size walk mask %s; falling back to "
                   "the rects",
                   path);
    }
  }
  walk_grid_build(grid, area, scene_size);
}

int walk_grid_serialize(const WalkGrid *grid, char *out, size_t out_size) {
  int written = snprintf(out, out_size, "walk %d %d\n", grid->w, grid->h);
  if (written < 0 || (size_t)written >= out_size) {
    return -1;
  }
  size_t at = (size_t)written;
  if (at + (size_t)grid->h * (grid->w + 1) + 1 > out_size) {
    return -1;
  }
  for (int cy = 0; cy < grid->h; cy++) {
    for (int cx = 0; cx < grid->w; cx++) {
      out[at++] = grid->cells[cy][cx] ? '#' : '.';
    }
    out[at++] = '\n';
  }
  out[at] = '\0';
  return (int)at;
}

bool walk_grid_save(const WalkGrid *grid, const char *dir) {
#ifdef __EMSCRIPTEN__
  (void)grid;
  (void)dir;
  SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
              "walk_grid_save: not available on the web build (use the web "
              "walk editor's export instead)");
  return false;
#else
  if (dir == NULL) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "walk_grid_save: this scene has no walk_mask_dir");
    return false;
  }
  char path[ASSET_PATH_MAX];
  if (!asset_common_path((Asset){.filename = "walkable.walk", .directory = dir},
                         path, sizeof(path))) {
    return false;
  }
  static char buffer[WALK_FILE_MAX];
  int length = walk_grid_serialize(grid, buffer, sizeof(buffer));
  if (length < 0) {
    return false;
  }
  SDL_RWops *rw = SDL_RWFromFile(path, "wb");
  if (rw == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "walk_grid_save: %s: %s", path,
                 SDL_GetError());
    return false;
  }
  bool ok = SDL_RWwrite(rw, buffer, 1, (size_t)length) == (size_t)length;
  SDL_RWclose(rw);
  if (ok) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Saved walk mask: %s", path);
  } else {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "walk_grid_save: short write to %s", path);
  }
  return ok;
#endif
}

bool walk_grid_contains(const WalkGrid *grid, SDL_Point p) {
  if (p.x < 0 || p.x >= grid->w * WALK_CELL_SIZE || p.y < 0 ||
      p.y >= grid->h * WALK_CELL_SIZE) {
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
  static Uint16 queue[GRID_CELLS_MAX];
  static Uint8 visited[GRID_CELLS_MAX];
  SDL_memset(visited, 0, sizeof(visited));

  int start_cx = clampi(p.x / WALK_CELL_SIZE, 0, grid->w - 1);
  int start_cy = clampi(p.y / WALK_CELL_SIZE, 0, grid->h - 1);
  int head = 0;
  int tail = 0;
  queue[tail++] = (Uint16)cell_index(grid, start_cx, start_cy);
  visited[cell_index(grid, start_cx, start_cy)] = 1;

  while (head < tail) {
    int idx = queue[head++];
    int cx = idx % grid->w;
    int cy = idx / grid->w;
    if (cell_walkable(grid, cx, cy)) {
      return cell_center(cx, cy);
    }
    for (int dy = -1; dy <= 1; dy++) {
      for (int dx = -1; dx <= 1; dx++) {
        int nx = cx + dx;
        int ny = cy + dy;
        if ((dx == 0 && dy == 0) || nx < 0 || nx >= grid->w || ny < 0 ||
            ny >= grid->h) {
          continue;
        }
        int nidx = cell_index(grid, nx, ny);
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

// Clamps to the whole grid's walkable-column extent, which equals the origin
// zone's extent only while the walkable area is a single connected region (as
// every shipped scene's is). With disconnected zones the interval spans the
// gap between them, so a drag could cross it; confining the clamp to the zone
// the drag started in (flood fill on grab) is tracked in #145.
float walk_grid_clamp_x(const WalkGrid *grid, float x) {
  int min_cx = -1;
  int max_cx = -1;
  for (int cx = 0; cx < grid->w; cx++) {
    bool any = false;
    for (int cy = 0; cy < grid->h; cy++) {
      if (grid->cells[cy][cx]) {
        any = true;
        break;
      }
    }
    if (any) {
      if (min_cx < 0) {
        min_cx = cx;
      }
      max_cx = cx;
    }
  }
  if (min_cx < 0) {
    return x; // no walkable cell: nothing to clamp to
  }
  float lo = min_cx * WALK_CELL_SIZE + WALK_CELL_SIZE / 2.0F;
  float hi = max_cx * WALK_CELL_SIZE + WALK_CELL_SIZE / 2.0F;
  if (x < lo) {
    return lo;
  }
  if (x > hi) {
    return hi;
  }
  return x;
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

  int start = cell_index(grid, (int)from2.x / WALK_CELL_SIZE,
                         (int)from2.y / WALK_CELL_SIZE);
  int goal = cell_index(grid, (int)to2.x / WALK_CELL_SIZE,
                        (int)to2.y / WALK_CELL_SIZE);
  if (start == goal) {
    out[0] = to2;
    return 1;
  }

  // A* over the grid, 8-connected, no corner cutting. The open "list" is a
  // plain scan: this runs once per click, not per frame, so the worst case
  // (28,800 cells in the largest scrollable scene) is still trivial.
  static Uint32 g_cost[GRID_CELLS_MAX];
  static Uint16 came_from[GRID_CELLS_MAX];
  static Uint8 in_open[GRID_CELLS_MAX];
  static Uint8 closed[GRID_CELLS_MAX];
  int cells = grid->w * grid->h;
  SDL_memset(in_open, 0, (size_t)cells);
  SDL_memset(closed, 0, (size_t)cells);

  int gx = goal % grid->w;
  int gy = goal / grid->w;
  g_cost[start] = 0;
  came_from[start] = (Uint16)start;
  in_open[start] = 1;

  // Best-effort target: the visited cell closest (by heuristic) to the goal,
  // so an unreachable goal still routes as near as possible.
  int best = start;
  Uint32 best_h = heuristic(start % grid->w, start / grid->w, gx, gy);
  bool reached = false;

  for (;;) {
    // Pop the open node with the lowest f = g + h.
    int current = -1;
    Uint32 current_f = 0;
    for (int i = 0; i < cells; i++) {
      if (!in_open[i]) {
        continue;
      }
      Uint32 f = g_cost[i] + heuristic(i % grid->w, i / grid->w, gx, gy);
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

    int cx = current % grid->w;
    int cy = current / grid->w;
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
        int nidx = cell_index(grid, nx, ny);
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
  static Uint16 cells_reversed[GRID_CELLS_MAX];
  int cells_length = 0;
  for (int idx = end; idx != start; idx = came_from[idx]) {
    cells_reversed[cells_length++] = (Uint16)idx;
  }

  static SDL_FPoint pts[GRID_CELLS_MAX + 1];
  int pts_length = 0;
  pts[pts_length++] = from2;
  for (int i = cells_length - 1; i >= 0; i--) {
    int idx = cells_reversed[i];
    pts[pts_length++] = cell_center(idx % grid->w, idx / grid->w);
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

// Landing target for a drop at `from`. Coordinates are the sprite centre —
// the walk grid is authored against the centre (see MOVEMENT.md), so the
// landing target is computed like every walk target. The drop falls straight
// down: the centre of the first walkable cell at or below `from` in its grid
// column; a column with no ground below (or a drop outside the scene) falls
// back to the nearest walkable point overall.
static SDL_FPoint drop_target(const WalkGrid *grid, SDL_FPoint from) {
  int cx = (int)from.x / WALK_CELL_SIZE;
  if (from.x >= 0 && cx < grid->w) {
    int start = (int)from.y / WALK_CELL_SIZE;
    if (start < 0) {
      start = 0;
    }
    for (int cy = start; cy < grid->h; cy++) {
      if (grid->cells[cy][cx]) {
        return (SDL_FPoint){from.x,
                            cy * WALK_CELL_SIZE + WALK_CELL_SIZE / 2.0F};
      }
    }
  }
  return walk_grid_nearest(grid, (SDL_Point){(int)from.x, (int)from.y});
}

bool walk_actor_drag_event(Actor *actor, const WalkGrid *grid,
                           const SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEBUTTONDOWN: {
    SDL_Point p = {event->button.x, event->button.y};
    SDL_Rect grab = actor_sprite_rect(actor);
    grab.x -= DRAG_GRAB_PADDING;
    grab.y -= DRAG_GRAB_PADDING;
    grab.w += 2 * DRAG_GRAB_PADDING;
    grab.h += 2 * DRAG_GRAB_PADDING;
    // TALKING refuses the grab like it refuses walks.
    if (actor->state != TALKING && SDL_PointInRect(&p, &grab)) {
      actor->drag_armed = true;
      actor->drag_grab = (SDL_FPoint){(float)p.x, (float)p.y};
    }
    // The press always falls through: a hotspot the actor happens to stand
    // on keeps working for plain taps. If the pointer then travels, the
    // drag steals the actor, cancelling whatever walk the press started.
    return false;
  }
  case SDL_MOUSEMOTION: {
    SDL_FPoint p = {(float)event->motion.x, (float)event->motion.y};
    if (actor->state == DRAGGED) {
      actor_drag_move(actor, p);
      actor->current_position.x =
          walk_grid_clamp_x(grid, actor->current_position.x);
      return true;
    }
    if (actor->drag_armed) {
      // A stale press: the button is no longer held (e.g. the fallen-through
      // press hit a navigation hotspot and the release went to another
      // scene). Disarm instead of phantom-grabbing on a later motion.
      if ((event->motion.state & SDL_BUTTON_LMASK) == 0) {
        actor->drag_armed = false;
        return false;
      }
      float dx = p.x - actor->drag_grab.x;
      float dy = p.y - actor->drag_grab.y;
      if (dx * dx + dy * dy >= DRAG_START_THRESHOLD * DRAG_START_THRESHOLD) {
        actor->drag_armed = false;
        // The offset is taken against the arming press, so the grab point
        // stays under the pointer without a snap, wherever the fallen-
        // through press briefly walked her meanwhile.
        actor->drag_offset =
            (SDL_FPoint){actor->current_position.x - actor->drag_grab.x,
                         actor->current_position.y - actor->drag_grab.y};
        if (actor_begin_drag(actor)) {
          actor_drag_move(actor, p);
          actor->current_position.x =
              walk_grid_clamp_x(grid, actor->current_position.x);
          return true;
        }
      }
    }
    return false;
  }
  case SDL_MOUSEBUTTONUP:
    actor->drag_armed = false;
    if (actor->state == DRAGGED) {
      actor_drop(actor, drop_target(grid, actor->current_position));
      return true;
    }
    return false;
  default:
    return false;
  }
}
