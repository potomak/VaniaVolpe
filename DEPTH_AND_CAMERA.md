# Depth, camera & parallax

Spec for three engine features that share one foundation — a clean split
between *screen* and *scene* coordinates:

1. **Depth (simulated z axis):** the actor reads as nearer/farther as it moves
   up/down the scene, using per-depth **sprite variants** instead of
   render-time scaling.
2. **Scrollable scenes:** scenes larger than the 800×600 window, with a
   **camera** that follows the player.
3. **Planes & parallax:** the actor can walk **behind** props (a plant, a
   table), and backgrounds/foregrounds are composed of **planes** that scroll
   at different speeds.

Written to be implemented phase by phase (each phase is an independent PR that
leaves the game working); tracking issue
[#67](https://github.com/potomak/VaniaVolpe/issues/67). Interlocks with the
movement plan (`MOVEMENT.md`, #48) are collected in a section at the end.

## Glossary & coordinate systems

- **Screen coordinates** — the 800×600 logical render space
  (`SDL_RenderSetLogicalSize`). UI (the hub button) and the final render live
  here. Unchanged by this spec.
- **Scene coordinates** — the pixel space of a scene's world, of size
  `scene_size = {scene_w, scene_h}` with `scene_w >= 800`, `scene_h >= 600`.
  **All gameplay data is in scene coordinates**: actor positions, hotspots,
  POIs, props, the walk grid, plane anchors. For every existing scene,
  `scene_size == {800, 600}` and scene coordinates equal screen coordinates —
  existing data is untouched.
- **Camera** — the top-left corner of the visible 800×600 window, in scene
  coordinates, clamped to `0 <= x <= scene_w - 800`, `0 <= y <= scene_h - 600`
  (vertical scrolling falls out of the same math for free; a `scene_h` of 600
  simply clamps `y` to 0).
- Transform (parallax factor `p`; gameplay content has `p = 1`):
  `screen = scene_pos - camera * p`.
- **Actor feet** — the actor's ground-contact point.
  `actor->current_position` is the sprite **centre** (render subtracts half
  the frame size), and all existing walk data is authored against the centre;
  that stays. Depth bands and y-sorting need the ground point instead:
  `actor_feet_y(actor) = current_position.y + reference_frame_h / 2`. The two
  conventions coexist; the helper makes the distinction explicit.

## Requirements

- **R1** — An actor crossing scene-defined y thresholds switches between 1–3
  separately authored sprite sets ("variants"); no render-time scaling.
- **R2** — Walking speed scales per depth band (a far actor covers fewer
  scene px/s), so apparent speed stays natural.
- **R3** — A scene may be wider and/or taller than 800×600; the camera follows
  the player actor, smoothed and clamped to the scene bounds, and snaps (no
  smoothing) when a scene becomes active.
- **R4** — Scenes keep authoring everything in scene coordinates; the engine
  converts input (screen → scene) and rendering (scene → screen) centrally so
  scene code contains **no** camera math.
- **R5** — The actor renders behind or in front of props depending on
  y-order (feet vs. prop baseline), updated every frame.
- **R6** — A scene's background/foreground can be composed of multiple planes,
  each scrolling at its own parallax factor; planes behind the action layer
  and planes in front of it are both supported (a `p = 1` foreground plane is
  a cheap "walk-behind" strip).
- **R7** — Existing static scenes keep working with zero data changes at every
  phase; all four builds (`make`, `make terminal`, `make test`, `make web`)
  stay green.
- **R8** — The walk grid, `.walk` masks, the mask editor, and the debug
  overlay operate in scene coordinates and scene-sized grids.

Non-goals (out of scope, revisit only if felt): continuous zoom/scaling,
camera rotation or shake, dead-zone camera tuning, per-pixel occlusion masks,
animated planes, positional audio, different parallax factors per axis.

## Depth: approaches considered

- **Continuous render scaling** (classic SCUMM: `scale = f(y)`, one sprite
  set): smooth and art-cheap, but scaled sprites lose the hand-drawn
  crispness (and colour-keyed edges fringe under interpolation) — rejected by
  art direction.
- **Discrete depth bands + per-band sprite variants** *(chosen)*: each scene
  divides its floor into 1–3 horizontal bands; each actor provides a full
  sprite set per band, authored at the right size. Engine cost is trivial
  (pick the set by feet y); art stays exactly as drawn. Costs: art effort
  multiplies per variant (mitigated by a placeholder-generator tool, real art
  later — the `en_US` placeholder pattern), a visible "pop" at band
  boundaries (mitigated by placing boundaries where players cross quickly, or
  behind walk-behind props/planes), and more texture memory (negligible at
  this sprite size).
- **Hybrid** (discrete sets + slight scaling within a band): smoothest, most
  complex, inherits the scaling look partially — rejected for now.

## Phases

```
Phase 1: action layer   — y-sorted props + actor occlusion (no camera needed)
Phase 2: depth variants — bands + per-band sprite sets + speed scaling
Phase 3: camera         — scene coordinates, follow/clamp/smooth, input conversion
Phase 4: planes         — parallax rendering + the first scrolling scene (needs art)
```

Phases 1–3 are code-only and independently useful; Phase 4 depends on Phase 3
(and on Phase 1 for correct layering). Suggested order above front-loads the
phases that need no new art.

### Phase 1 — y-sorted action layer (walk-behind props) *(shipped)*

*Implementation note: the sort is split out as `action_layer_order()`
(`scene.h`) so the unit tests assert draw order without a renderer;
`render_action_layer` draws whatever order it returns. The spec'd cap of 16
was dropped in review: sort keys are computed on the fly and the order buffer
is sized `props_length + actors_length`, so there is nothing to overflow.*

New type in `scene.h` (or `src/prop.h` if `scene.h` gets crowded):

```c
typedef struct prop {
  // Exactly one of image / animation is non-NULL (points into the scene's
  // existing images/animations tables, so loading/freeing is unchanged).
  ImageData *image;
  AnimationData *animation;
  SDL_Point pos;    // top-left, scene coordinates
  int baseline;     // scene y of the ground-contact line; the actor renders
                    // behind the prop while actor_feet_y < baseline
  bool visible;     // scenes toggle this (e.g. a picked-up item)
} Prop;
```

New helpers:

```c
// actor.h
float actor_feet_y(const Actor *actor); // current_position.y + ref_frame_h/2

// scene.h / scene.c
void render_action_layer(SDL_Renderer *renderer, Prop *props, int props_length,
                         Actor **actors, int actors_length);
```

`render_action_layer` spec: build a small local array of drawables (visible
props + actors; cap at 16, `SDL_assert` the cap), each with a sort key —
`prop.baseline` for props, `(int)actor_feet_y(actor)` for actors. Sort
ascending with insertion sort (stable; on equal keys draw props before
actors). Draw in order: props via `render_image` / `render_animation` at
`prop.pos`, actors via `actor_render`.

Scene migration: a scene opts in by replacing its manual "draw props, then
always draw the fox last" block with a `Prop` table plus one
`render_action_layer` call, keeping any draws that never overlap the actor
(backgrounds, UI) manual as today. **Demo with zero new art:** in
`playground.c`, make the fallen-acorns pile and the dropped peg props — the
fox standing above (behind) the pile is then correctly overlapped by it.
Scenes that don't opt in are untouched.

Acceptance: all builds green; `make test` passes; manual playtest — walk the
fox above/below the acorn pile and see the overlap flip. Unit test
(`test/test_scene.c` or alongside existing tests): synthetic props/actor
lists assert the draw order, including the tie rule.

### Phase 2 — depth bands + actor sprite variants *(shipped)*

*Implementation notes: the same-states-as-variant-0 validation runs in
`actor_load_media` (which already has the loud-failure path scenes check),
not `make_actor`. Bands are per-scene data by construction — each scene
declares its own `DepthBand` table and maps its floor onto the actor's
variants however its art needs.*

`actor.h`:

```c
#define ACTOR_MAX_VARIANTS 3

// A full sprite set for one depth, plus how the actor moves at that depth.
typedef struct actor_variant_spec {
  const ActorAnimSpec *anims;
  int anims_length;
  float speed_scale; // 1.0 = spec velocity; far variants use < 1.0
} ActorVariantSpec;

// ActorSpec: REPLACE the flat `anims`/`anims_length` pair with:
  const ActorVariantSpec *variants; // at least one (variant 0 = nearest)
  int variants_length;              // 1..ACTOR_MAX_VARIANTS

// Actor: animations becomes 2D, plus the active variant:
  AnimationData *animations[ACTOR_MAX_VARIANTS][ACTOR_STATE_COUNT];
  int variant; // 0 initially
```

- `fox.c` / `hen.c`: wrap the existing anim tables in a one-element
  `ActorVariantSpec` array (`speed_scale = 1.0f`) — a small mechanical diff;
  no behaviour change.
- `make_actor` / `actor_load_media` / `actor_free` / `actor_face` /
  `actor_update`'s per-frame `animation_update` loop: iterate
  `variants_length × ACTOR_STATE_COUNT`. **Validation:** every variant must
  provide the same set of states as variant 0 — check at `make_actor`,
  `SDL_LogError` and fail loudly on mismatch (keeps `reference_animation` and
  state fallbacks sane). All state lookups (`actor_render`,
  `actor_play_state`, `actor_talk`, walk animation) index
  `animations[actor->variant][state]`.
- New API:

```c
void actor_set_variant(Actor *actor, int variant);
```

  No-op if unchanged or out of range. Otherwise: for the animation of the
  *current* state, start the new variant's animation and copy `start_time`
  and `flip` from the old one (the walk/talk cycle continues mid-stride
  instead of restarting — this is what hides the switch), stop the old one
  **without** firing callbacks (set `is_playing = false` directly rather than
  calling `stop_animation`, which fires the global end callback — see #35),
  and set `actor->variant`.
- **Speed:** in `actor_update`'s `WALKING` case, multiply by the active
  variant's factor:
  `velocity = spec->velocity * spec->variants[actor->variant].speed_scale`.

Scene side (`scene.h` / `scene.c`):

```c
// Bands sorted by ascending y_top; the active band is the last one with
// y_top <= feet y. Band 0 should start at 0.
typedef struct depth_band { int y_top; int variant; } DepthBand;

int depth_variant_for(const DepthBand *bands, int bands_length, float feet_y);
```

A scene that opts in declares `static const DepthBand DEPTH_BANDS[] = {…};`
and calls, once per frame in its `update` (before `fox_update`):

```c
actor_set_variant(fox, depth_variant_for(DEPTH_BANDS, LEN(DEPTH_BANDS),
                                         actor_feet_y(fox)));
```

Scenes without bands never call it — variant stays 0.

**Placeholder art tool** — `tools/gen_depth_variants.py` (mirrors the
`gen_en_us_placeholders.sh` pattern): given a variant-0 sprite sheet + its
`.anim`, emit a downscaled sheet (Pillow, `Image.NEAREST`, factor e.g. 0.6)
and a `.anim` with every `x,y,w,h` scaled by the same factor (rounded). These
are *placeholders to develop and test against*; real far/mid art replaces
them (file the art request as a `needs-art` issue when a scene adopts bands).

Acceptance: all builds green with fox/hen on one variant (zero behaviour
change); a branch-local test with generated placeholders shows the switch
mid-walk without animation restart. Unit tests: `depth_variant_for` band
selection (below first band, exact boundary, beyond last), and a walk-distance
test asserting `speed_scale` is applied.

### Phase 3 — camera + scene coordinates *(shipped)*

*Implementation notes: input conversion also folds in #64 — every scene reads
the click's own `event->button.x/y` on `SDL_MOUSEBUTTONDOWN` rather than a
cached motion position, so a repeated same-spot tap still routes correctly
while the camera moves. The debug overlay draws raw SDL rects (not through
the image offset), so it applies `render_get_offset()` to its scene-space
drawing explicitly. The depth demo field is now 1600×600 with a following
camera as the committed example.*

New files `src/camera.{c,h}` (add `camera.c` to `GAME_SRCS`):

```c
typedef struct camera {
  SDL_FPoint pos;        // top-left of the view, scene coordinates
  SDL_Point scene_size;  // >= {WINDOW_WIDTH, WINDOW_HEIGHT}
  const Actor *follow;   // player actor to track; NULL = fixed camera
} Camera;

#define CAMERA_SMOOTHING 5.0f // 1/s; higher snaps faster

void camera_init(Camera *cam, SDL_Point scene_size, const Actor *follow);
// Centre on the follow target immediately (scene entry; no smoothing).
void camera_snap(Camera *cam);
// Exponential follow + clamp: pos += (target - pos) * min(1, SMOOTHING * dt),
// where target centres the view on the actor; then clamp to scene bounds.
void camera_update(Camera *cam, float delta_time);
SDL_Point camera_scene_of(const Camera *cam, SDL_Point screen_pt);
```

Engine integration (this is the heart of R4 — scene code stays camera-free):

- `Scene` gets `Camera *camera;` (NULL for every existing scene). A scrolling
  scene owns a `static Camera camera;`, calls `camera_init` in `init` (after
  `make_fox`), and the engine calls `camera_snap` when the scene becomes
  active and `camera_update` right after the scene's `update` each frame
  (`game_update` in `game.c`).
- **Rendering:** `image.c` gets a module-static render offset applied inside
  `render_image` and `render_animation`:

  ```c
  void render_set_offset(SDL_Point offset); // engine-only; scenes never call
  ```

  In `game_render`: if the scene has a camera, set the offset to
  `{-(int)cam->pos.x, -(int)cam->pos.y}` (cast **once** so all draws share
  the same integer offset — no per-image jitter) before calling
  `scene.render` and the debug overlay, and reset it to `{0,0}` before
  drawing the hub button. `actor_render` and every existing `render_*` call
  inside scenes are automatically camera-correct with no signature change.
- **Input:** in `game_process_input`, *after* the screen-space hub-button
  check and *before* dispatching to the scene, if the scene has a camera,
  convert the event's coordinates in place:
  `event->button.x/y` and `event->motion.x/y` get `+(int)cam->pos.x/y`.
  Every existing `SDL_PointInRect` hit-test, POI walk, and the debug
  drag-rect then operate in scene coordinates unchanged — and the debug
  overlay's printed rects come out in scene coordinates, which is exactly
  what a scrolling scene's hotspot authoring needs. (Do #64 — read
  `event->button.x/y`, not the cached motion position — before or with this
  phase, so there is a single coordinate story.)
- **Walk grid:** becomes scene-sized — see *Interlocks with the movement
  plan* below.

Acceptance: all builds green; `make test` passes (no scene sets a camera
yet). Unit tests (`test/test_camera.c`): `camera_scene_of` round-trip;
clamping at all four scene edges; `camera_snap` centring; smoothing converges
monotonically toward a fixed target and never overshoots
(`min(1, SMOOTHING * dt)` guards big `dt`). Plus a synthetic integration
check: with a fake 1600×600 scene, feed actor positions and assert the camera
follows and clamps.

### Phase 4 — planes & parallax + the first scrolling scene

New type (in `scene.h`, next to the images table it mirrors):

```c
typedef struct plane {
  ImageData image;  // loaded/freed by the scene framework like scene images
  float parallax;   // 0 = fixed (sky), 1 = scene-locked, > 1 = nearer than
                    // the action layer (only meaningful for fg_planes)
  SDL_Point origin; // scene-coord position of the image's top-left when the
                    // camera is at (0,0); usually {0,0}
} Plane;
```

`Scene` gets two ordered tables, drawn in array order:

```c
Plane *bg_planes; int bg_planes_length; // behind the action layer
Plane *fg_planes; int fg_planes_length; // in front of the action layer
```

- **Engine rendering** (`game_render`; planes are drawn by the engine, not by
  `scene.render`, and *not* through the render offset — parallax needs its
  own offset per plane):

  ```
  for each bg plane:  draw at origin - camera.pos * plane.parallax
  render_set_offset(-camera)        // integer, computed once
  scene.render()                    // action layer, scene coordinates
  render_set_offset({0,0})
  for each fg plane:  draw at origin - camera.pos * plane.parallax
  set offset again → debug overlay → reset   // debug draws over everything
  hub button (screen space)
  ```

  A scene with planes moves its background image(s) out of `scene.render`
  into `bg_planes`; a static scene (no camera, no planes) renders exactly as
  today.
- **Coverage rule** (validate at load, `SDL_LogError` if violated): a plane
  must cover the whole view over the camera's travel —
  `image.w >= WINDOW_WIDTH + parallax * (scene_w - WINDOW_WIDTH)` (same
  formula for height). This is the formula to hand the artist: e.g. a
  1600-px-wide scene needs a `p = 0.5` hill layer
  `800 + 0.5 * 800 = 1200 px` wide and a `p = 1` gameplay layer 1600 px wide.
- **Loading/teardown:** extend the scene framework the same way images work —
  `load_scene_planes(Scene *, SDL_Renderer *)` / `free_scene_planes(Scene *)`
  called wherever `load_scene_images` / `free_scene_images` are (follow the
  `Scene *` convention from #38, not by-value).
- **Walk-behind without y-sorting:** a `p = 1` foreground strip (a fence, a
  bush row) occludes the actor with no `Prop` needed — use props (Phase 1)
  only for objects the actor must be able to stand *in front of* as well.

**First scrolling scene** (`needs-art`): a wide playground exterior or a new
Gina location, e.g. 1600×600 with three planes (sky `p = 0`, trees `p = 0.4`,
ground `p = 1`), one fg bush strip (`p = 1.15`), 2 depth bands, and a couple
of props. This is where R1–R6 are proven together; file it as its own
`backlog` + `needs-art` issue when Phase 4 starts, and extend the Vania/Gina
playthrough script to click at both scene ends so CI exercises the scrolling
path.

Acceptance: all builds green; unit test for the plane offset math and the
coverage validation; browser playtest screenshots at both scene ends.

## Interlocks with the movement plan (`MOVEMENT.md`, #48)

Scene coordinates change the walk layer's assumptions. Apply these deltas to
`MOVEMENT.md` (they are backward compatible — a static scene is just
`scene_size = {800, 600}`); if a phase of that plan is implemented *after*
Phase 3 of this one, build it scene-sized from the start:

1. **Grid size** derives from the scene, not the window:
   `grid->w = scene_w / WALK_CELL_SIZE`, `grid->h = scene_h / WALK_CELL_SIZE`,
   with static maximums `MAX_SCENE_W 2400`, `MAX_SCENE_H 1200` (`constants.h`)
   → worst-case grid 240×120 = 28,800 cells (~29 KB; A* work arrays ~200 KB
   static — fine, including wasm).
2. **A\* cost array** must be `Uint32`, not `Uint16` (a 240×120 grid's
   worst-case accumulated cost overflows 16 bits).
3. **`.walk` format** gains a header line — `walk <w> <h>` — so masks are
   scene-sized and self-describing; the strict parser checks the header
   against the scene's grid dimensions and falls back to rects on mismatch.
4. **Mask editor & debug overlay** already work in scene coordinates for free
   after Phase 3 (input conversion is central; overlay drawing happens inside
   the render offset). The editor paints whatever the camera currently shows —
   walk to the far end of the scene to paint it.
5. The walkability BFS/A\*/smoothing algorithms are untouched — they never
   assumed the window size, only the grid dimensions.

## Relevant code

- `src/actor.{c,h}` — anim tables → variants (Phase 2), `actor_feet_y`
  (Phase 1), speed scaling (Phase 2).
- `src/scene.{c,h}` — `Prop`, `render_action_layer`, `DepthBand`,
  `depth_variant_for`, plane tables.
- `src/camera.{c,h}` *(new)* — camera state, follow/clamp/smoothing,
  transforms.
- `src/image.c` — `render_set_offset` under `render_image` /
  `render_animation`.
- `src/game.c` — input conversion, camera update hook, plane rendering, the
  offset choreography in `game_render`.
- `src/walk.{c,h}` / `MOVEMENT.md` — scene-sized grid deltas above.
- `tools/gen_depth_variants.py` *(new)* — placeholder variant generator.
