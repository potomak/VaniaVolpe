# Actor scaling by depth

Continuous pseudo-3D depth: an actor is drawn at a size that varies smoothly
with how far "into" the scene she stands, from one sprite set, the way the SCUMM
engine did it (`SCAL` slots + `SO_ACTOR_SCALE`). This supersedes the discrete
depth-band / per-band sprite-variant model in `DEPTH_AND_CAMERA.md` Phase 2 (see
*Reversal* below).

The hard part is not the scaling — it is that **scaling and drag & drop make
contradictory claims about what screen-y means**, and most of this document is
about resolving that.

## Requirements

- **R1** — One sprite set per actor. Apparent size is a continuous function of
  depth; no per-depth art.
- **R2** — Depth is scene data: a declared linear ramp from y to scale.
- **R3** — Scaling composes with drag & drop: **no discontinuity in scale** at
  any point of a drag gesture — grab, carry, release, fall, land.
- **R4** — While an actor is held, where she will land is **visible** (a
  shadow), and she lands exactly there. What you see is where she lands.
- **R5** — A scene that declares no ramp renders **exactly** as it does today.
  The change is an identity transform at scale 1.0.
- **R6** — Walk and fall speed scale with depth, so apparent motion stays
  natural (SCUMM modulated `SO_STEP_DIST` the same way).
- **R7** — The grab target never shrinks below a size a toddler can hit.
- **R8** — Scaled sprites must not fringe. Edge artefacts under interpolation
  are the reason this approach was rejected once already.

Non-goals: camera zoom or rotation, per-pixel occlusion masks, non-linear
(true 1/z) perspective, automatic scaling of props (opt-in only), per-cell
authored depth maps.

## Reversal

`DEPTH_AND_CAMERA.md` § *Depth: approaches considered* evaluated continuous
render scaling and **rejected it by art direction** — "scaled sprites lose the
hand-drawn crispness (and colour-keyed edges fringe under interpolation)" —
choosing discrete depth bands instead, and listing continuous scaling under
Non-goals ("revisit only if felt").

That decision is now reversed: continuous scaling is the chosen model. The
crispness half of the objection is an accepted aesthetic trade. The fringing
half is a solvable bug, and **R8** makes solving it a gate on Phase 1 rather
than a matter of taste — see *Fringing*. `DEPTH_AND_CAMERA.md` is amended to
point here.

## The depth map: `ScaleRamp`

One linear ramp per scene — SCUMM's `SCAL` slot, which maps an input y range
onto a scale range:

```c
typedef struct scale_ramp {
  int   y_far,     y_near;      // scene y bounds
  float scale_far, scale_near;  // e.g. 0.6 -> 1.0
} ScaleRamp;                    // Scene.scale_ramp; NULL = no scaling (R5)

// Clamped outside [y_far, y_near]; scale_ramp_at(NULL, y) == 1.0f.
float scale_ramp_at(const ScaleRamp *ramp, float y);
```

Four authored numbers, no new file format, no painting, guaranteed smooth and
monotonic — a per-cell map would be ~9600 values for a 1600x600 scene and lets
neighbouring cells disagree, which pops as the actor walks. SCUMM went the same
way historically: per-box constant scale first, `SCAL` added because it wasn't
enough.

The bounds are **declared, not derived** from the walk grid's walkable extent.
Depth is a property of the painted background; deriving it would couple
perspective to whatever area happens to be walkable right now (the pool swaps
between `SHADE_RECTS` and `POOLSIDE_RECTS` mid-puzzle — those happen to share a
y extent today, but nothing enforces that). Declared bounds are also the one
place an artist tunes the effect.

**`scale_far` floor.** Cue-driven mouth shapes (`SPEECH.md`) stop reading below
roughly 0.6 on a 144x117 sheet. A ramp that goes lower makes the whole lip-sync
investment invisible at depth. Treat 0.6 as the practical floor unless the scene
has no dialogue.

Growth path, if a scene ever needs a raised platform at odd depth: a second ramp
slot (SCUMM's answer) or a per-cell byte map beside `WalkGrid.cells`. Both sit
behind `actor_scale()` and need no actor-side change. Not built now.

## Where scale comes from

```c
float actor_scale(const Actor *a, const ScaleRamp *ramp);
```

Computed on demand — it is a clamp and a lerp. It is deliberately **not** cached
per frame: `actor_sprite_rect` is called from `walk_actor_drag_event` during
*input*, which would read a value cached by the previous frame's update, and go
stale after a teleport (`on_scene_active`, the slide, the dive tween).

| state | depth read from |
|---|---|
| grounded (IDLE / WALKING / TALKING / ...) | `actor_feet_y(a)` |
| `DRAGGED` | `a->ground_y` — the shadow |
| `FALLING` / `LANDING` | `a->ground_y` (the landing, fixed at release) |

`FALLING` must not read her live y, or she would grow as she descends. Because
the drag's final scale already equals the landing scale, **release changes
nothing visually** (R3).

## Rendering

### Anchor: scale about the ground-contact point

`current_position` stays the **sprite centre** and `actor_feet_y` stays a
function of the *natural* reference frame height:

```c
return actor->current_position.y + reference->sprite_clips[0].h / 2.0F;
```

This is deliberate. If `actor_feet_y` were made scale-aware it would feed the
ramp that produces the scale — a circular dependency. Keeping it on natural
height breaks the cycle, and it is the better sort key anyway: the ground-contact
point should not move because the sprite got smaller.

The visual requirement — her feet stay planted as she shrinks — is satisfied
entirely at draw time: **scale the sprite about `(current_position.x,
actor_feet_y(a))`**. Every drawn vertex `v` maps to `anchor + (v - anchor) * s`.

Two properties fall out:

- At `s == 1` this is the identity, so a scene with no ramp renders
  byte-identically (R5).
- Per-state frames of different sizes keep their relative composition. The fox's
  sheets are not uniform — walking/talking 144x117, sitting 96x135, waving
  93x132 — and `actor_render` positions *every* state from the **reference**
  frame's half-extent, so her sitting sprite already hangs ~18px below the
  walking feet line and sits 24px left of centre. Scaling the whole composition
  about the feet line scales those offsets with her, instead of snapping each
  state's own bottom edge to the ground (which would move her visibly whenever
  she changed pose).

`image.c`'s existing `scaled_quad` scales about the **centre** and is therefore
the wrong helper; this needs a bottom-anchored sibling.

#### Follow-up: standardising pose sizes (tracked in #169)

Non-uniform sheets are handled, not required. Standardising every pose of an
actor onto one canvas with a common anchor is **not a prerequisite** for
scaling, but it would pay for itself elsewhere:

- `actor_sprite_rect` uses the *reference* (move-state) frame for every state,
  so the fox's grab box is 144x117 even while she is SITTING at 96x135 — 24px
  too wide on each side and 18px too short. That looseness exists today, in the
  intro, where she is sat the whole time.
- Drawn feet would equal logical feet in every pose, instead of the sitting
  sprite hanging 9px below the walking feet line.
- The anchor rule above collapses to "scale about the sprite's bottom edge",
  deleting the per-state overhang case entirely.

Sequence it **after** the effect is validated (Phase 2), so the art isn't
re-exported before anyone knows the scaling looks good.

### Fringing (R8)

The sprites blend correctly today, via a real alpha channel — 60 of the 61
adventure PNGs are RGBA (only `field/sky.png` is RGB), the fox's walking sheet
is 50% fully transparent with a further 5% partially transparent (authored
anti-aliased edges), and the hen's idle sheet is 73%/6%.

Two things follow, both measured rather than assumed:

**The colour key is vestigial.** `image.c` calls `SDL_SetColorKey` for cyan on
every loaded surface, and **not one sprite contains a single cyan texel**. It is
a leftover of the original tutorial pipeline and does nothing. Delete it; the
alpha channel is what has been doing the work all along.

**The fringe is black, not cyan.** Every fully transparent texel stores RGB
`(0,0,0)` — transparent *black* — including the ~2700 that directly border the
fox. At 1:1 each screen pixel samples one texel, so an alpha-0 texel contributes
nothing and nothing fringes. Once scaled, linear filtering interpolates *between*
texels, and SDL's default `SDL_BLENDMODE_BLEND` is straight (non-premultiplied)
alpha, so RGB and A are averaged independently: an edge texel blends toward
black with partial alpha. The result is a **dark halo on every scaled sprite
edge**.

So the original art-direction objection was right that edges fringe, but wrong
about the cause. The fix is unchanged: at load, run an **alpha-bleed** pass —
dilate each transparent texel's RGB outward from its nearest non-transparent
neighbour. Interpolation then blends toward the sprite's own colour and the halo
disappears, with linear filtering retained for smoothness. (Premultiplied alpha
is the other standard fix, but it needs a custom SDL blend mode; bleeding works
with the default.)

Doing this at load rather than in the PNGs keeps assets as authored and covers
future art automatically, and it benefits the existing `render_*_scaled` paths
(tween scaling) too, not just actors.

Measured on the fox's walking sheet — comparing what straight-alpha bilinear
produces against the correct premultiplied result, over the 6899 sample
positions that straddle her edge — bleeding cuts the mean colour error by
**82%** (0.061 to 0.011). The residue is the ~5% of texels that are *partially*
transparent, whose exact fix would be premultiplied alpha; it is far below the
threshold that reads as a halo.

`SDL_SetTextureScaleMode(..., SDL_ScaleModeNearest)` on actor textures is the
fallback if bleeding still shimmers on hand-drawn art at small scales.

Note the filtering hint is set **only** in `main.c` — `main_terminal.c` and
`test/harness.c` never set it — so the headless harness cannot catch a fringing
regression. Verify it by eye, in the desktop or web build.

## Drag & drop

### The conflict

- Scaling says: **y is depth**, `scale = ramp(y)`.
- Drag & drop says (`LIVELINESS.md`, *"The z-axis question — there is no z"*):
  while held, **y is height** over column x, and release scans down that column
  for ground.

Both cannot own y. Two models were tried and rejected before the one below:

- **Scale from the raw column-scan landing** (`scale = ramp(drop_target(pos).y)`).
  Over walkable ground `drop_target` returns her *own* y, so the fall is always
  zero and lift is unrepresentable; dragging up shrinks her to the minimum and
  pins there.
- **Inject a constant lift at grab.** Creates a `LIFT` px translation the instant
  the 8px drag threshold is crossed — a discontinuity needing a tween to hide.

### The model: sprite is held, shadow is ground

The resolution is a second on-screen quantity. The **sprite** shows where she is
held; a drawn **shadow** shows where she will land. Depth and scale read off the
shadow; the gap between them reads as height. The shadow also makes the model
self-explanatory: while it sits still she is being lifted, once it slides back
she is moving away.

Per drag event, with `G` = the ground y captured at grab and held fixed for the
whole gesture:

```c
lift_ceiling = max(LIFT_MAX, G - feet_at_grab);   // see "caught mid-fall"
lift         = clamp(G - feet, 0, lift_ceiling);
target       = walk_grid_clamp_ground(grid, feet.x, feet + lift);
ground_y     = slew(ground_y, target, GROUND_SLEW_PX_PER_S * dt);
scale        = ramp(ground_y);
```

With `LIFT_MAX = 60` and a grab from standing (`lift_ceiling == LIFT_MAX`):

| drag | lift | ground | scale |
|---|---|---|---|
| none | 0 | `G` | unchanged |
| up 30 | 30 | **`G`** | **unchanged** |
| up 60 | 60 | `G` | unchanged |
| up 100 | 60 (capped) | `G - 40` | smaller |
| down 20 | 0 | `G + 20` | bigger |

No pop at grab; **no rescale at all until `LIFT_MAX` is exceeded**; `lift` and
`ground` are both continuous at the transition; the gesture is reversible with
no hysteresis in y; and the fall distance is whatever lift the player gave her —
a small hop, or a long flutter if she is carried high. Small accidental vertical
wobble cannot rescale her, which matters for an imprecise finger.

`LIFT_MAX` is expressed as a **fraction of the ramp's y span**, not a raw
constant: 60px is a 100% dead zone on a scene whose ramp spans 60px and a 10%
one on a scene spanning 600px. Keep it in scene px at a fixed fraction, not
scaled by the actor's current scale — that would reintroduce a `lift -> ground
-> scale -> lift` cycle needing a frame of lag to resolve.

### Landing is the shadow

Release lands her on `ground_y` — the slewed value the player has been looking
at, not a freshly computed one. `walk_grid_clamp_ground` only ever returns
walkable cells, so a lagging value is still a legal landing (R3 in
`LIVELINESS.md` holds), and using it makes the shadow authoritative (R4). No
snap is possible because the scale already matches.

### Clamping the shadow

A separate helper, so the release path's `drop_target` keeps its current
semantics:

```c
// Clamp a desired ground y into the walkable span of column x:
// the cell itself if walkable, else the first walkable below, else the first
// above. Returns false when the column has no walkable cell at all.
bool walk_grid_clamp_ground(const WalkGrid *g, float x, float desired_y,
                            float *out_y);
```

The upward scan is what lets her be dragged past the band's front edge without
`lift` going negative. When the column has **no** walkable cell, hold the last
valid `ground_y` and **hide the shadow** rather than inventing a ground point
somewhere she is not — `walk_grid_nearest` returns a cell in a *different*
column, which would put the shadow sideways of her and derive the scale from a
place she is not over. (This is unreachable in shipped scenes: every column
within the `#144` x-clamp is covered. It is specified so it stays harmless.)

### Slew

`ground_y` moves toward its target at a bounded rate (`GROUND_SLEW_PX_PER_S`,
around 600). This is not polish — the column scan makes `ground` a
*discontinuous* function of x, so horizontal motion across a step in ground
height would teleport the shadow and jump the scale with no vertical input at
all. In the playground, dragging across x = 710 at feet y = 400 steps the ground
from ~335 to ~495 between two adjacent columns. Slewing turns that into a ~270ms
glide, and covers the hidden-shadow case above on the way back.

Only `ground_y` is slewed. Scale and the shadow both derive from it, so they
stay consistent by construction.

### Caught mid-fall

`actor_begin_drag` deliberately catches an actor in mid-air (`LIVELINESS.md`
calls it "the better toy"; `test_walk.c` asserts it). A gesture that begins with
her already 300px above her landing point would, with a fixed `LIFT_MAX = 60`,
compute `lift = 60` and yank `ground` up 240px on the grab frame — a large
instant rescale, violating R3.

Hence `lift_ceiling = max(LIFT_MAX, G - feet_at_grab)`: the ceiling starts at
whatever height she already had, so the gesture begins continuous, and
`LIFT_MAX` governs only gestures that start on the ground. The same rule covers
re-grabbing after a lift-and-release.

### Scenes with no walk grid

The intro and outro pass a `NULL` grid to the drag helpers (poster scenes with
no walkable area). There is no ground map, so: no ramp, `scale == 1.0`, no
shadow, and the drop lands where released — exactly today's behaviour. All of
this must be a no-op there.

### Shadow rendering

- Drawn only while `state == DRAGGED || state == FALLING`. Not `>= DRAGGED`,
  which would include `LANDING`, where she is already on the ground.
- Sorted on **`ground_y`**, not `actor_feet_y`. A shadow drawn in the actor's
  sort slot uses her *airborne* y and would be occluded by props she is plainly
  in front of — drag the fox high above the playground's acorn pile and her
  shadow, on the ground in front of it, would draw behind it. This makes the
  shadow a third drawable kind in `action_layer_order`.
- Scaled by the same `ramp(ground_y)`, and offset by `render_get_offset()` if
  drawn as a raw primitive rather than through `render_image` — easy to miss,
  and wrong only in camera scenes.

Art: a soft ellipse. Procedurally drawable, or a placeholder PNG generated like
the boil sheets, so it need not block on an artist. An always-on shadow (not
just while dragging) would ground the actor at every depth and is a natural
follow-up, deliberately out of scope here.

## Speed (R6)

Walk velocity and fall speed multiply by the current scale, so a distant actor
covers fewer screen px/s and takes smaller steps — SCUMM's `SO_STEP_DIST`. This
subsumes `ActorVariantSpec.speed_scale`.

Safe against the arrival logic: the `overshot` dot-product test in `actor.c`
catches steps larger than `ACTOR_ARRIVE_EPSILON`, and the shipped 0.6 variant
already exercises this range. Apply a floor to the factor so an actor at extreme
depth still converges.

## Props

`Prop` gains an opt-in `bool scaled`. Default `false` preserves every shipped
scene, whose props are authored at final on-screen size. A prop that opts in is
drawn at `ramp(prop->baseline)` — its baseline *is* its depth — anchored on that
baseline, the same rule as the actor.

This is also SCUMM's "ignore scale" flag, inverted to make the safe case the
default. The depth demo needs it: its three bushes are the same `ImageData` at
three different depths, and a continuously scaling fox beside three
identically-sized bushes reads as broken.

## Grab target (R7)

`actor_sprite_rect` must use the **scaled** size and the **same anchor** as the
renderer, or the hit box drifts from the sprite by `(1-s)*h/2` — 23px at scale
0.6 on the fox, over twice `DRAG_GRAB_PADDING`.

It must also enforce a **floor**. `DRAG_GRAB_PADDING` is a flat 10px; at scale
0.6 the fox's grab box is 86x70, at 0.4 it is 58x47. Drag exists because
toddlers try it unprompted, so a grab target that shrinks with depth attacks the
feature's reason for existing. Never smaller than roughly the natural rect —
grow the padding as the sprite shrinks. This ships in Phase 1, not later.

## Phases

Each phase is a PR gated by `make test` plus a play-test of the scene it
touches.

1. **Rendering foundation.** Alpha-bleed at load (R8); scale-about-feet draw;
   `ScaleRamp` + `actor_scale()`; scaled + floored grab rect. No scene declares a
   ramp yet, so this is provably an identity transform (R5). Verify fringing by
   eye in the desktop build.
2. **Opt one scene in.** Give the depth demo a ramp and scale its props; delete
   its `DEPTH_BANDS`. First real look at the effect.
3. **Drag & drop integration.** `walk_grid_clamp_ground`, `Actor.ground_y`, the
   lift model, slew, the shadow and its sort entry, mid-fall grab, NULL-grid
   no-op.
4. **Speed scaling.**
5. **Retire the variant system** — its own PR with its own gate. This is not a
   freebie: `ActorVariantSpec` also carries per-variant fidget tables, the
   `animations[VARIANT][STATE]` indirection threaded through `actor_face` and
   `actor_load_media`'s parity validation, `actor_set_variant`,
   `depth_variant_for`, `DepthBand`, `test_scene.c`, `tools/gen_depth_variants.py`
   and the committed `*_far` art.

## Testing

The headless harness asserts dialogue and clicks fixed screen coordinates, so it
would stay green through a large visual regression. Scaling therefore needs unit
tests of its own, not just the playthrough:

- `scale_ramp_at`: endpoints, midpoint, clamping outside the range, NULL ramp
  returns exactly 1.0.
- The lift formula: the table above, including continuity at
  `feet == G - LIFT_MAX` and the mid-fall `lift_ceiling` case.
- `walk_grid_clamp_ground`: cell walkable; first walkable below; first walkable
  above (dragged past the front edge); empty column returns false.
- The render anchor: at `s == 1` the destination rect equals today's, for a
  state whose frame differs from the reference (sitting).
- Speed: a scaled walk still terminates at its target.
