# Movement, walkable areas & pathfinding

This note documents how actors move today, the problems with that model, and a
concrete, step-by-step design for obstacle-avoiding movement. It is written so
the plan can be implemented incrementally (each phase is an independent PR that
leaves the game working). The tracking issue is
[#48 — Walk around non-walkable areas](https://github.com/potomak/VaniaVolpe/issues/48).

## Current behaviour

1. A click/tap is handled in the active scene's `process_input`
   (`playground.c`, `playground_entrance.c`, `pool.c`, `tree.c`, `vine.c`).
   Object hotspots are checked first; anything else falls through to movement.
2. For a **fall-through click**, the destination is snapped onto legal ground by
   `nearest_walkable_point()` (`src/scene.c`): the nearest point of the union of
   the scene's `WALKABLE_*` rectangles; if that lands inside the (single)
   `NON_WALKABLE_HOTSPOT` rectangle it is pushed just past the nearest edge,
   then clamped back into the winning walkable rect.
3. For a **hotspot click**, the actor walks to a hand-placed POI with *no*
   snapping at all (some POIs are deliberately inside the non-walkable rect —
   see below).
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
  sandbox's non-walkable rect — the fox must be able to end a walk on
  technically-illegal ground when an interaction demands it.
- **P5 — the start can be illegal too.** The slide teleports the fox
  (`START_SLIDE_POS`, the sigmoid descent), and P4 walks end on illegal ground,
  so a router must tolerate an illegal start point.
- **P6 — `Actor` has no waypoint support.** `actor_walk_to` holds exactly one
  target; routing around anything needs a small queue.

## Design

Four pieces, each an independent PR, in this order. The game builds and plays
correctly after every phase.

```
Phase 1: WalkArea      — first-class walk geometry on Scene (+ fixes P2, P3)
Phase 2: waypoints     — Actor walks a short queue of points (P6)
Phase 3: router        — corner visibility graph + Dijkstra (P1, P4, P5)
Phase 4: tests/polish  — geometry unit tests, debug-overlay path drawing
```

### Phase 1 — `WalkArea`: first-class walk geometry

New files `src/walk.{c,h}` (add `src/walk.c` to `GAME_SRCS` in the Makefile so
all four builds pick it up):

```c
typedef struct walk_area {
  const SDL_Rect *walkables; // union of legal ground
  int walkables_length;
  const SDL_Rect *blocked;   // exclusion zones carved out of the union
  int blocked_length;        // 0 is fine
} WalkArea;

// Inside some walkable rect AND inside no blocked rect.
bool walk_area_contains(const WalkArea *area, SDL_Point p);

// Nearest legal point to p (replacement for nearest_walkable_point).
SDL_FPoint walk_area_nearest(const WalkArea *area, SDL_Point p);
```

`walk_area_nearest` spec (fixes P2 by *verifying* candidates instead of
trusting arithmetic):

1. If `walk_area_contains(p)` → return `p`.
2. Build a candidate list: for each walkable rect, `p` clamped into that rect;
   for every such candidate that sits inside a blocked rect `b`, add four
   push-outs (`x = b.x - 1`, `x = b.x + b.w`, `y = b.y - 1`, `y = b.y + b.h`,
   each re-clamped into the same walkable rect).
3. Keep only candidates passing `walk_area_contains`; return the one nearest to
   `p` (squared `int64_t` distance).
4. If none survives (degenerate authoring), return the plain nearest clamp and
   `SDL_LogWarn` once — same behaviour as today, but loudly.

Scene integration:

- Add `const WalkArea *walk_area;` to `Scene` (`scene.h`). `NULL` means the
  scene has no player movement (intro, outro, minigames, hub).
- Each movement scene declares `static const SDL_Rect walkables[] = {…};`,
  `static const SDL_Rect blocked[] = {…};` (may be empty) and a
  `static const WalkArea walk_area = {…};`, sets `.walk_area` in its `Scene`,
  and **removes** the walkable/non-walkable rects from `hotspots[]`.
- Delete `nearest_walkable_point` from `scene.{c,h}`; call sites use
  `walk_area_nearest(scene's area, m_pos)`.
- `debug.c`: if the current scene has a walk area, draw walkable rects in green
  and blocked rects in red, so they are distinguishable from the cyan hotspots.

Scenes to migrate (all five current `nearest_walkable_point` callers):
`playground.c` (3 walkable + 1 blocked), `playground_entrance.c` (1 + 1),
`pool.c`, `tree.c`, `vine.c` (1 + 0 each — the dummy zero rect in `pool.c`
disappears).

Acceptance: `make && make test && make web` pass; with the debug overlay on,
walk geometry renders in its own colours; clicking the sandbox still walks to
its edge.

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
  `ACTOR_MAX_WAYPOINTS` points; skips leading points within
  `ACTOR_ARRIVE_EPSILON` of the current position; starts the first segment.
  `actor_walk_to(a, p, cb)` becomes `actor_walk_path(a, &p, 1, cb)`.
- While touching this code, fix the stale-callback bug in `actor_walk_to`'s
  early-arrive branch (see the issue "stale walk callback fires after a tap on
  the actor mid-walk"): when the new target is within epsilon, the pending
  walk must be fully cancelled (clear `on_end_walking` and the queue, stop the
  walk animation/sound, state → `IDLE`) *before* the new `on_end` fires —
  today the old callback leaks and fires on the next frame.

Acceptance: `make test` passes (nothing produces multi-point paths yet).

### Phase 3 — router: corner visibility graph + Dijkstra

Why this algorithm: the geometry is a handful of axis-aligned rectangles. The
shortest obstacle-avoiding path between two points among rectangular obstacles
bends only at (inflated) obstacle corners, so a tiny visibility graph is exact,
allocation-free, and ~150 lines. It is the "corner waypoint" idea from the
original note, generalised to any number of blocked rects. A grid + A* is more
general (arbitrary masks) but is more code and needs tuning; keep it as the
escalation path if scenes ever outgrow rectangles.

In `walk.c`:

```c
// Fills out[] with up to max_out waypoints from `from` to `to` (excluding
// `from`, including the final destination). Returns the count (>= 1).
// Falls back to a single straight segment {to} when no route is found.
int walk_area_find_path(const WalkArea *area, SDL_FPoint from, SDL_FPoint to,
                        SDL_FPoint *out, int max_out);
```

Constants: `WALK_CORNER_MARGIN 8` (px a corner node sits outside its blocked
rect), `WALK_SAMPLE_STEP 4` (px between legality samples),
`WALK_MAX_BLOCKED 4` → max nodes `2 + 4*4 = 18` (fixed arrays, no malloc).

1. **Segment legality** — `segment_is_walkable(area, a, b)`: sample the
   segment every `WALK_SAMPLE_STEP` px (including both endpoints, rounding to
   int) and require `walk_area_contains` at every sample. Sampling sidesteps
   exact segment-vs-rect-union geometry (the union is not convex); at 800×600
   with 4 px steps it cannot miss any rect in this game's data.
2. **Endpoint normalisation** (handles P4/P5): `from2 = contains(from) ? from
   : walk_area_nearest(from)`; `to2` likewise. Route between `from2` and
   `to2`. If `to != to2` the caller may append the raw `to` as an exempt final
   hop (below).
3. **Nodes**: `[0] = from2`, `[1] = to2`, then for each blocked rect the four
   corners inflated by `WALK_CORNER_MARGIN`
   (`(r.x - M, r.y - M)`, `(r.x + r.w - 1 + M, r.y - M)`, …), keeping only
   those passing `walk_area_contains`. (In the playground the two top sandbox
   corners fall outside the walkable strip and are dropped; routes go around
   the bottom — correct.)
4. **Edges**: every node pair with a legal segment, weight = Euclidean
   distance. If `from2 → to2` is legal directly, Dijkstra returns it — no
   special-casing needed.
5. **Dijkstra** node 0 → node 1 over ≤ 18 nodes: plain O(n²) arrays, no heap.
6. **Fallback**: no path, or more waypoints than `max_out` → return `{to}` and
   `SDL_LogWarn`. The game never behaves *worse* than today.

Scene-facing wrapper (in `walk.c`; needs `actor.h`):

```c
// Route `actor` through `area` to `goal`. exact_goal = the walk must end at
// `goal` even if it is not legal ground (hotspot POIs, e.g. the shovel inside
// the sandbox); otherwise the goal is snapped with walk_area_nearest first
// (fall-through clicks).
void walk_actor_to(Actor *actor, const WalkArea *area, SDL_FPoint goal,
                   bool exact_goal, void (*on_end)(void));
```

Implementation: snap or keep `goal` per the flag; `walk_area_find_path`; if
`exact_goal` and the raw goal differs from the routed endpoint, append the raw
goal as the last waypoint; `actor_walk_path`.

Call-site migration in the five movement scenes:

- Fall-through clicks: `walk_actor_to(fox, area, fpoint(m_pos), false, NULL)`
  (replaces the `nearest_walkable_point` + `fox_walk_to` pair).
- Hotspot/POI clicks: `walk_actor_to(fox, area, POI, true, callback)`
  (replaces bare `fox_walk_to(fox, POI, callback)`).
- Scenes without a walk area (intro/outro/minigames) keep calling
  `fox_walk_to`/`hen_walk_to` directly.
- Direct position writes stay as they are: the slide's per-frame
  `fox->current_position = …` and the scene-entry teleports
  (`gina->current_position = HEN_START; gina->target_position = HEN_START;`)
  don't interact with the queue (`actor_walk_path` resets it on every walk).

Acceptance: `make test` passes (Gina's scenes have no blocked rects, so the
router returns direct segments and behaviour is unchanged). Manual playtest of
the playground: click across the sandbox from either side — the fox goes
around the bottom corners; clicking the shovel still ends exactly on
`SHOVEL_POI`.

### Phase 4 — tests & polish

- **Geometry unit tests** (pure functions, no SDL init — only `SDL_Rect`
  types): add `test/test_walk.c` to `TEST_SRCS`, called from `test/main_test.c`
  alongside the playthroughs. Using the real playground and
  playground_entrance geometry, assert:
  - every segment of every returned path passes `segment_is_walkable`;
  - left↔right of the playground sandbox is routed (path count > 1) and each
    waypoint is legal;
  - `exact_goal` routing to `SHOVEL_POI` ends on the POI and every *earlier*
    waypoint is legal;
  - `walk_area_nearest` postcondition on a sweep of points (e.g. every 10 px
    over the 800×600 grid): the result always satisfies `walk_area_contains`
    (or equals the logged-fallback clamp).
- **Debug overlay**: draw the player actor's remaining waypoints as a
  polyline. Needs the overlay to reach the actor — simplest is an optional
  `const Actor *debug_actor` field on `Scene` that movement scenes set.
  Nice-to-have; skip if it drags.
- The Vania play-test (#57) is worth doing around the same time: it exercises
  playground movement end-to-end and would catch routing regressions the Gina
  script cannot.

## Alternatives considered

- **Ray-to-border** (stop the fox→click segment at the obstacle border): only
  constrains clicks *inside* the obstacle; a legal destination whose path
  merely clips the obstacle is unaffected. Rejected — same class of endpoint-
  only fix as today's snapping.
- **Grid / navmesh + A\***: build a coarse walkability grid (e.g. 8 px cells)
  per scene, A* over it, then smooth with line-of-sight string pulling. More
  general (arbitrary masks, many obstacles) but more code and tuning. Not
  needed while geometry is a few axis-aligned rects; this is the escalation
  path, and Phases 1–2 (WalkArea, waypoints) are prerequisites for it too, so
  nothing in this plan is throwaway.

## Relevant code

- `src/actor.c` — `actor_walk_to`, `actor_update` (the straight-line mover;
  Phase 2 adds the waypoint queue).
- `src/scene.c` — `nearest_walkable_point` (Phase 1 replaces it with
  `walk_area_nearest` in `src/walk.c`).
- `src/adventures/vania_fox_the_slide/playground{,_entrance}.c`,
  `src/adventures/gina_hen_at_the_pool/{pool,tree,vine}.c` — the per-scene
  `WALKABLE_*` / `NON_WALKABLE_HOTSPOT` rectangles and both kinds of walk
  call sites.
- `src/debug.c` — the overlay that should render walk geometry distinctly.
