# Movement, walkable areas & pathfinding

This note documents how actors move today, the problems with that model, and a
concrete, step-by-step design for obstacle-avoiding movement based on a
low-resolution walkability grid + A*. It is written so the plan can be
implemented incrementally (each phase is an independent PR that leaves the
game working). The tracking issue is
[#48 — Walk around non-walkable areas](https://github.com/potomak/VaniaVolpe/issues/48).

## Current behaviour

1. A click/tap is handled in the active scene's `process_input`
   (`playground.c`, `playground_entrance.c`, `pool.c`, `tree.c`, `vine.c`).
   Object hotspots are checked first; anything else falls through to movement.
   A hotspot is a touch area that triggers an action: some actions walk the
   actor to a hand-placed POI first (the shovel, the gate), some play in place
   with no walk at all (the excavator).
2. For a **fall-through click**, the destination is snapped onto legal ground by
   `nearest_walkable_point()` (`src/scene.c`): the nearest point of the union of
   the scene's `WALKABLE_*` rectangles; if that lands inside the (single)
   `NON_WALKABLE_HOTSPOT` rectangle it is pushed just past the nearest edge,
   then clamped back into the winning walkable rect.
3. For a **hotspot walk**, the actor walks to the POI with *no* snapping at all
   (some POIs are deliberately on non-walkable ground — see below).
4. `actor_walk_to(target)` (`src/actor.c`) then moves the actor in a **straight
   line** toward the target at `spec->velocity`, updated each frame in
   `actor_update`, stopping within `ACTOR_ARRIVE_EPSILON`.

## Problems

- **P1 — the path is never routed.** Movement is one straight segment with no
  obstacle awareness, so the actor can walk *through* a non-walkable area
  whenever start and destination are both legal but the segment between them
  crosses the obstacle. Concrete repro (playground): stand left of the sandbox
  at ~(150, 400), click right of it at ~(700, 400) — the segment crosses
  `NON_WALKABLE_HOTSPOT = {236, 131, 403, 312}`. Snapping only the endpoint
  (current approach) or stopping the segment at the obstacle border (an
  alternative we discussed) cannot fix this: both only adjust the *endpoint*.
- **P2 — the snap itself has a re-entry bug.** After pushing the point out of
  the non-walkable rect, `nearest_walkable_point` clamps it back into the
  winning walkable rect, which can land it back *inside* the non-walkable rect.
  Example: walkable `{300,300,300,100}`, blocked `{250,250,200,200}`, click
  `(320,380)` → nearest exit is left (`x = 249`), clamp back to `x = 300` →
  `(300,380)`, inside the blocked rect. Current scene data happens not to
  trigger it, but the function does not guarantee its own postcondition.
- **P3 — walk geometry is not engine data.** Each scene stashes its walkable /
  non-walkable rects inside its `hotspots[]` array (purely so the debug overlay
  draws them), and hand-passes them to `nearest_walkable_point`. Semantically
  they are not hotspots; the overlay draws them in the same colour as clickable
  regions; the API supports only *one* blocked rect; and `pool.c` has to pass a
  dummy `(SDL_Rect){0,0,0,0}` because it has none.
- **P4 — POI walks bypass the constraint entirely,** and must keep working:
  `SHOVEL_POI = {258, 507}` (playground_entrance) is intentionally *inside* the
  sandbox's non-walkable rect — the fox walks close, the dig animation plays,
  the key is revealed. A router must let a walk *end* on non-walkable ground
  when the interaction demands it (only the final approach is exempt).
- **P5 — the start can be illegal too.** The slide teleports the fox
  (`START_SLIDE_POS`, the sigmoid descent), and P4 walks end on illegal ground,
  so a router must tolerate an illegal start point.
- **P6 — `Actor` has no waypoint support.** `actor_walk_to` holds exactly one
  target; routing around anything needs a small queue.

## Design

The core idea: rasterise each scene's walk geometry **once** into a
low-resolution walkability bitmap — one cell per 10×10 logical pixels, so
**80×60 cells** for the 800×600 screen — and do everything on that grid:

- *"Is this point legal?"* → one cell lookup.
- *"Nearest legal point to a click"* → breadth-first search from the clicked
  cell (fixes P2 **by construction**: the result is a legal cell, never the
  output of edge arithmetic).
- *"Route from A to B"* → A* over the cells, then a line-of-sight smoothing
  pass so the actor walks a few straight segments instead of a staircase.

Everything is fixed-size and allocation-free: the grid is 4,800 bytes, A*'s
work arrays are static, and a smoothed path in these scenes is 2–4 points.

The grid has two fill sources: the existing rect tables (scenes keep declaring
`SDL_Rect`s; the grid is rasterised from them at scene init) and, once Phase 5
lands, a per-scene **mask file** (`walkable.walk`) painted with an in-game
editor over the real scene background — the natural authoring format for
organic shapes, and what new scenes should use. Rects remain supported so the
migration is per-scene and optional.

Five phases, each an independent PR, in this order (Phase 5 only needs
Phase 1). The game builds and plays correctly after every phase.

```
Phase 1: WalkArea + grid  — first-class walk geometry, grid-based nearest point (P2, P3)
Phase 2: waypoints        — Actor walks a short queue of points (P6)
Phase 3: A* + smoothing   — routed movement, all call sites migrated (P1, P4, P5)
Phase 4: tests/polish     — geometry unit tests, debug-overlay grid & path drawing
Phase 5: mask authoring   — .walk file format + in-game paint editor
```

### Phase 1 — `WalkArea` + walkability grid

New files `src/walk.{c,h}` (add `src/walk.c` to `GAME_SRCS` in the Makefile so
all four builds pick it up):

```c
// Authoring format: rectangles, as scenes define them today.
typedef struct walk_area {
  const SDL_Rect *walkables; // union of legal ground
  int walkables_length;
  const SDL_Rect *blocked;   // exclusion zones carved out of the union
  int blocked_length;        // 0 is fine
} WalkArea;

#define WALK_CELL_SIZE 10
#define WALK_GRID_W (WINDOW_WIDTH / WALK_CELL_SIZE)  // 80
#define WALK_GRID_H (WINDOW_HEIGHT / WALK_CELL_SIZE) // 60

// Runtime format: 1 byte per cell, 1 = walkable.
typedef struct walk_grid {
  Uint8 cells[WALK_GRID_H][WALK_GRID_W];
} WalkGrid;
```

> **Scrollable scenes** (`DEPTH_AND_CAMERA.md`, #67) make the grid
> *scene*-sized rather than window-sized: dimensions become `WalkGrid` fields
> derived from the scene size (static maximums `MAX_SCENE_W`/`MAX_SCENE_H`),
> the A* cost array becomes `Uint32`, and the `.walk` format gains a
> `walk <w> <h>` header line. See that spec's *Interlocks with the movement
> plan* for the exact deltas; if this plan is implemented after camera
> support lands, build it scene-sized from the start.

```c

// Rasterise the rects into the grid (called once from the scene's init).
void walk_grid_build(WalkGrid *grid, const WalkArea *area);

// Cell lookup for a point in logical coordinates (false outside the screen).
bool walk_grid_contains(const WalkGrid *grid, SDL_Point p);

// Nearest legal point to p: p itself if its cell is walkable, else the centre
// of the nearest walkable cell (BFS).
SDL_FPoint walk_grid_nearest(const WalkGrid *grid, SDL_Point p);
```

Specs:

- **Rasterisation:** cell `(cx, cy)` is walkable iff its centre point
  `(cx*10 + 5, cy*10 + 5)` is inside some walkable rect and inside no blocked
  rect. Centre sampling (rather than requiring the whole cell to be legal)
  keeps the 1-px overlap seams between adjacent walkable rects connected —
  verified against the playground data, where `WALKABLE_HOTSPOT_1` and `_2`
  overlap by a single column at x ≈ 707 and stay connected at 10-px cells. The
  smallest walkable rect in the game (`{4, 531, 68, 63}`) is ~6×6 cells, so the
  resolution is safe for all current scenes.
- **`walk_grid_nearest`:** if `walk_grid_contains(p)` → return `p` **unchanged**
  (walk to the exact click, not a cell centre). Otherwise BFS outward from
  `p`'s cell over 8-connected neighbours (fixed ring buffer of `W*H` `Uint16`
  indices, `visited` bitmap) and return the centre of the first walkable cell
  found; ties resolved by a fixed neighbour order, so results are
  deterministic. If no walkable cell exists at all (degenerate authoring),
  return `p` and `SDL_LogWarn`.

Scene integration:

- Add `const WalkGrid *walk_grid;` to `Scene` (`scene.h`). `NULL` means the
  scene has no player movement (intro, outro, minigames, hub).
- Each movement scene declares its rect arrays and a `static const WalkArea`,
  owns a `static WalkGrid walk_grid;`, calls `walk_grid_build(&walk_grid,
  &walk_area)` in `init`, sets `.walk_grid` on its `Scene`, and **removes**
  the walkable/non-walkable rects from `hotspots[]`.
- Delete `nearest_walkable_point` from `scene.{c,h}`; fall-through clicks use
  `walk_grid_nearest(&walk_grid, m_pos)`.
- `debug.c`: if the current scene has a walk grid, shade non-walkable cells
  (e.g. translucent red 10×10 fills) so walk geometry is visible and clearly
  distinct from the cyan hotspots. (Cheap: at most 4,800 tiny fills, debug
  mode only.)

Scenes to migrate (all five current `nearest_walkable_point` callers):
`playground.c` (3 walkable + 1 blocked), `playground_entrance.c` (1 + 1),
`pool.c`, `tree.c`, `vine.c` (1 + 0 each — the dummy zero rect in `pool.c`
disappears).

Acceptance: `make && make test && make web` pass; with the debug overlay on,
non-walkable cells are shaded; clicking the sandbox still walks to its edge.

### Phase 2 — waypoint queue in `Actor`

`actor.h`:

```c
#define ACTOR_MAX_WAYPOINTS 8

// in struct actor:
SDL_FPoint waypoints[ACTOR_MAX_WAYPOINTS];
int waypoints_length;
int waypoint_index;
```

- Keep `target_position` as "the current segment's target" so `actor_update`'s
  arrive test is untouched. On arrival: if `waypoint_index + 1 <
  waypoints_length`, advance — set `target_position` to the next point,
  recompute `direction` and facing, and **do not** stop the walk animation or
  the move sound; otherwise finish exactly as today (stop animation, halt
  sound, state → `IDLE`, fire `on_end_walking`).
- New API:

```c
void actor_walk_path(Actor *actor, const SDL_FPoint *points, int points_length,
                     void (*on_end)(void));
```

  Same guards as `actor_walk_to` (refuse while `TALKING`); copies up to
  `ACTOR_MAX_WAYPOINTS` points (log a warning if truncated — smoothed paths in
  these scenes are 2–4 points, so 8 is generous); skips leading points within
  `ACTOR_ARRIVE_EPSILON` of the current position; starts the first segment.
  `actor_walk_to(a, p, cb)` becomes `actor_walk_path(a, &p, 1, cb)`.
- While touching this code, fix the stale-callback bug in `actor_walk_to`'s
  early-arrive branch (#63): when the new target is within epsilon, the
  pending walk must be fully cancelled (clear `on_end_walking` and the queue,
  stop the walk animation/sound, state → `IDLE`) *before* the new `on_end`
  fires — today the old callback leaks and fires on the next frame.

Acceptance: `make test` passes (nothing produces multi-point paths yet).

### Phase 3 — A* + smoothing, and call-site migration

In `walk.c`:

```c
// Fills out[] with up to max_out waypoints from `from` to `to` (excluding
// `from`, including the final destination). Returns the count (>= 1).
int walk_grid_find_path(const WalkGrid *grid, SDL_FPoint from, SDL_FPoint to,
                        SDL_FPoint *out, int max_out);
```

1. **Endpoint normalisation** (handles P4/P5): `from2 = walk_grid_nearest(from)`
   and `to2 = walk_grid_nearest(to)` — both are identity when already legal.
2. **A\*** from `from2`'s cell to `to2`'s cell, 8-connected: orthogonal step
   cost 10, diagonal 14, octile-distance heuristic; a diagonal step is allowed
   only if **both** adjacent orthogonal cells are walkable (no corner
   cutting). Static work arrays (`g[H][W]` as `Uint16`, `came_from[H][W]` as
   `Uint16` cell index, open/closed bitmaps); the open "list" can be a plain
   scan over 4,800 cells — this runs once per click, not per frame, so O(n²)
   worst case (~a few ms, realistically far less) is fine. No allocation.
3. **Best-effort goal:** while searching, remember the visited cell with the
   smallest heuristic distance to the goal. If the goal cell is unreachable
   (separate walkable region), path to that closest cell instead — the actor
   walks as near as it can, which is friendlier than today's
   straight-line-through-the-obstacle. Only if there is no path at all
   (degenerate) return the single point `{to}` (today's behaviour) with a
   `SDL_LogWarn`.
4. **Reconstruct** the cell path, convert cells to centre points, then replace
   the first point with the exact `from2` and the last with the exact `to2`.
5. **Smooth** (mandatory — raw grid paths are staircases and the actor would
   visibly zigzag): greedy line-of-sight string pulling. `los(a, b)` samples
   the segment every `WALK_CELL_SIZE / 2` px (both endpoints included) and
   requires `walk_grid_contains` at every sample. Starting at the first point,
   find the furthest path point still in line of sight, emit it, and repeat
   from there. Output ≤ a handful of points; write into `out[]`.

Scene-facing wrapper (in `walk.c`; needs `actor.h`):

```c
// Route `actor` through `grid` to `goal`. exact_goal = the walk must end at
// `goal` even if it is on non-walkable ground (hotspot POIs, e.g. the shovel
// inside the sandbox: the fox walks close along legal ground, then the final
// approach is exempt); otherwise the goal is snapped with walk_grid_nearest
// (fall-through clicks).
void walk_actor_to(Actor *actor, const WalkGrid *grid, SDL_FPoint goal,
                   bool exact_goal, void (*on_end)(void));
```

Implementation: `walk_grid_find_path(grid, actor->current_position, goal, …)`
routes to the *snapped* goal; if `exact_goal` and the snapped endpoint differs
from the raw `goal`, append the raw `goal` as one extra final waypoint;
`actor_walk_path`.

Call-site migration in the five movement scenes:

- Fall-through clicks: `walk_actor_to(fox, &walk_grid, fpoint(m_pos), false,
  NULL)` (replaces the `nearest_walkable_point` + `fox_walk_to` pair).
- Hotspot walks to a POI: `walk_actor_to(fox, &walk_grid, POI, true,
  callback)` (replaces bare `fox_walk_to(fox, POI, callback)`). Hotspots that
  don't walk (the excavator) are untouched.
- Scenes without a walk grid (intro/outro/minigames) keep calling
  `fox_walk_to`/`hen_walk_to` directly.
- Direct position writes stay as they are: the slide's per-frame
  `fox->current_position = …` and the scene-entry teleports
  (`gina->current_position = HEN_START; gina->target_position = HEN_START;`)
  don't interact with the queue (`actor_walk_path` resets it on every walk).

Acceptance: `make test` passes (Gina's scenes have no blocked rects, so A*
returns a path the smoother collapses to one segment and behaviour is
unchanged). Manual playtest of the playground: click across the sandbox from
either side — the fox goes around it; clicking the shovel still ends exactly
on `SHOVEL_POI` with the dig animation playing as before.

### Phase 4 — tests & polish

- **Geometry unit tests** (pure functions, no SDL init — only `SDL_Rect`
  types): add `test/test_walk.c` to `TEST_SRCS`, called from `test/main_test.c`
  alongside the playthroughs. Using the real playground and
  playground_entrance geometry, assert:
  - `walk_grid_nearest` postcondition on a sweep of points (e.g. every 10 px
    over the 800×600 grid): the result's cell is always walkable, or the
    input is returned with the logged degenerate fallback;
  - left↔right of the playground sandbox is routed (path count > 1), every
    returned waypoint's cell is walkable, and every consecutive pair passes
    the same `los` sampling the smoother uses;
  - `exact_goal` routing to `SHOVEL_POI` ends on the POI and every *earlier*
    waypoint is on walkable cells;
  - determinism: the same inputs produce the same path twice.
- **Debug overlay**: besides the shaded non-walkable cells from Phase 1, draw
  the player actor's remaining waypoints as a polyline. Needs the overlay to
  reach the actor — simplest is an optional `const Actor *debug_actor` field
  on `Scene` that movement scenes set. Nice-to-have; skip if it drags.
- The Vania play-test (#57) is worth doing around the same time: it exercises
  playground movement end-to-end and would catch routing regressions the Gina
  script cannot.

### Phase 5 — mask authoring: `.walk` files + in-game paint editor

Rect tables are fine for boxy playgrounds, but painting the walkable area over
the actual background is the right authoring model — especially for organic
scenes. Two parts: a tiny on-disk format, and an editor built into the debug
overlay (the same place `debug.c` already prints drag-selected `SDL_Rect`s for
authoring hotspots).

**File format — `walkable.walk`:** plain text, 60 lines of 80 characters,
`#` = walkable, `.` = blocked, newline-terminated. One character per grid
cell, so the file *is* the grid — human-readable, git-diffable, and trivially
parseable. Lives in the scene's asset dir under `common/` (masks are not
localized), e.g.
`src/adventures/vania_fox_the_slide/assets/common/playground/walkable.walk`.
The web build picks it up automatically (the `common/` dirs are already
`--preload-file`d).

**Loading:** extend Phase 1's init to try the file first:

```c
// Fill the grid from <dir>/walkable.walk if it exists (via asset_resolve),
// else rasterise the rects. `dir` is the scene's asset directory
// (e.g. "playground").
void walk_grid_init(WalkGrid *grid, const WalkArea *area, const char *dir);
```

The parser must be strict from day one (the `.anim` parser's lesson, #44):
exactly `WALK_GRID_W` chars per line, exactly `WALK_GRID_H` lines, only
`#`/`.`/`\r` accepted; on any violation, `SDL_LogError` and fall back to the
rects. A scene whose mask has shipped can drop its rect tables (pass an empty
`WalkArea`).

**Editor — walk-mask paint mode in the debug overlay** (`debug.c`, native
desktop build; `#ifndef __EMSCRIPTEN__` for the file write):

- With debug mode on (`D`), press `W` to toggle paint mode in a scene that has
  a walk grid. The overlay already shades non-walkable cells (Phase 1); paint
  mode additionally outlines the grid and swallows mouse input so painting
  doesn't walk the fox — give `debug_process_input` a `bool` return
  ("event consumed") and have `game_process_input` skip the scene dispatch
  when it returns true.
- **Left-drag** paints cells walkable, **right-drag** paints them blocked
  (cell = cursor position / `WALK_CELL_SIZE`). The scene keeps rendering
  underneath, so you paint over the live background with the POIs and
  hotspots visible.
- Press `S` to save: serialize the grid in the `.walk` format to
  `<assets_root>/common/<dir>/walkable.walk` (running from the repo root the
  asset paths are relative, so this writes straight into the source tree;
  `SDL_Log` the path). If the engine can't know the scene's asset dir, add an
  optional `const char *walk_mask_dir` next to `Scene.walk_grid` — the scene
  that opts into masks sets both.
- No undo, no brush sizes: cells are 10×10 and a wrong stroke is one
  right-drag away. Keep it dumb.

**Migration & tests:** convert one scene (the playground, the most complex
geometry) by saving its rasterised rects as a first `.walk`, hand-touch it up,
and drop the scene's rect tables; other scenes migrate whenever convenient.
Unit-test the format with a serialize → parse round-trip on the playground
grid, plus a truncated/corrupt-input case asserting the rect fallback.

## Alternatives considered

- **Ray-to-border** (stop the fox→click segment at the obstacle border): only
  constrains clicks *inside* the obstacle; a legal destination whose path
  merely clips the obstacle is unaffected. Rejected — same class of endpoint-
  only fix as today's snapping.
- **Corner visibility graph + Dijkstra** (nodes at inflated blocked-rect
  corners, edges where the connecting segment stays legal): exact for
  axis-aligned rects and very small, but it concentrates the subtlety in
  exactly the places that are easy to get wrong — corner inflation/filtering,
  segment legality over a *non-convex union* of rects, and a separately
  hand-verified nearest-point fix — and it needs an explicit fallback when
  corner filtering disconnects the graph. The grid replaces all of that with
  one uniform representation, fixes the nearest-point bug by construction,
  gives best-effort routing to unreachable goals for free, and makes
  hand-painted walkable masks (Phase 5) a natural extension rather than a
  redesign. The grid's own cost — mandatory
  path smoothing and 4.8 KB per scene — is small and mechanical. Rejected in
  favour of the grid.

## Relevant code

- `src/actor.c` — `actor_walk_to`, `actor_update` (the straight-line mover;
  Phase 2 adds the waypoint queue).
- `src/scene.c` — `nearest_walkable_point` (Phase 1 replaces it with the grid
  helpers in `src/walk.c`).
- `src/adventures/vania_fox_the_slide/playground{,_entrance}.c`,
  `src/adventures/gina_hen_at_the_pool/{pool,tree,vine}.c` — the per-scene
  `WALKABLE_*` / `NON_WALKABLE_HOTSPOT` rectangles and both kinds of walk
  call sites.
- `src/debug.c` — the overlay that should render walk geometry distinctly.
