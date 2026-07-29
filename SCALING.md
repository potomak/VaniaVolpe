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
- **R8** — Scaled sprites must not visibly fringe. Edge artefacts under
  interpolation are the reason this approach was rejected once already; see
  *Fringing*, where the objection was measured and found not to apply to this
  art.

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
half was measured and **does not apply to this art** — see *Fringing*.
`DEPTH_AND_CAMERA.md` is amended to point here.

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
float actor_scale(const Actor *actor);   // reads actor->scale_ramp
```

Computed on demand — it is a clamp and a lerp. It is deliberately **not** cached
per frame: `actor_sprite_rect` is called from `walk_actor_drag_event` during
*input*, which would read a value cached by the previous frame's update, and go
stale after a teleport (`on_scene_active`, the slide, the dive tween).

All of it goes through one function, `actor_depth_y(a)`, so the scale and the
depth sort can never read different lines:

| state | depth read from |
|---|---|
| grounded (IDLE / WALKING / TALKING / ...) | `actor_feet_y(a)` |
| `DRAGGED` / `FALLING` / `LANDING` | `a->ground_y` — the shadow. Fixed at release: only the drag slews it |

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

`image.c`'s existing `scaled_quad` scales about the **centre**, so this needed a
bottom-anchored sibling: `scaled_quad_about`, plus the `render_*_scaled_about`
draws built on it.

#### Considered and not pursued: standardising pose sizes

Non-uniform sheets are handled, not required. Standardising every pose of an
actor onto one canvas with a common anchor is **not a prerequisite** for
scaling, but it would pay for itself elsewhere:

- `actor_sprite_rect` uses the *reference* (move-state) frame for every state,
  so the fox's grab box is 144x117 even while she is SITTING at 96x135 — 24px
  too wide on each side and 18px too short. That looseness exists today, in the
  intro, where she is sat the whole time.
- Drawn feet would equal logical feet in every pose, instead of the sitting
  sprite hanging ~18px below the walking feet line and sitting 24px left of
  centre.
- The anchor rule above collapses to "scale about the sprite's bottom edge",
  deleting the per-state overhang case entirely.

None of that blocks anything: the scaling shipped and looks right on the
existing sheets, so the art is left as it is.

### Fringing (R8)

The sprites blend correctly today, via a real alpha channel — 60 of the 61
adventure PNGs are RGBA (only `field/sky.png` is RGB), the fox's walking sheet
is 50% fully transparent with a further 5% partially transparent (authored
anti-aliased edges), and the hen's idle sheet is 73%/6%.

**The colour key is vestigial.** `image.c` called `SDL_SetColorKey` for cyan on
every loaded surface, and **not one sprite contains a cyan texel**. It was a
leftover of the original tutorial pipeline and did nothing; deleted.

**The theoretical artefact is real but invisible here, so nothing is done about
it.** Every fully transparent texel stores RGB `(0,0,0)`. SDL blends straight
(non-premultiplied) alpha and filters RGB and alpha independently, so a sample
straddling the sprite edge mixes in that black: in principle a dark halo on any
scaled sprite. Measured on the fox's walking frame downscaled to 0.6 — the
ramp's floor, and so the worst case in practice — dilating the edge colour
outward ("alpha bleeding") changes **214 of 6020** output pixels, by at most
40/255, and the two renders are **indistinguishable side by side at 6x zoom**.

The reason is the art style itself: these characters are drawn with a heavy
dark outline (`LIVELINESS.md`: "hand-drawn with squiggly lines"), so the pixels
the artefact darkens are already dark. A bleed pass was implemented, measured,
and then removed as ~75 lines of load-time work for no visible gain.

This is worth recording rather than just deleting, for two reasons: it is the
concrete half of the original art-direction objection, now shown not to apply;
and if a future asset ever *is* drawn without an outline, the fix is known —
bleed at load, or premultiplied alpha with a custom blend mode.

Note the filtering hint is set **only** in `main.c` — `main_terminal.c` and
`test/harness.c` never set it — so the headless harness would not catch a
fringing regression either way. Judge it by eye, in the desktop or web build.

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
// The grid is indexed by the actor's centre; ground_y is a ground line.
target       = walk_grid_clamp_ground(grid, x, feet + lift - foot_offset)
               + foot_offset;
ground_y     = slew(ground_y, target, GROUND_SLEW_PX_PER_S * dt);
scale        = ramp(ground_y);
```

**Two coordinate spaces meet here.** `current_position` — and therefore every
walk rect and the whole grid — is the sprite's *centre*; `actor_feet_y`, the
depth ramps and `ground_y` are *ground* lines, half a reference frame lower.
`foot_offset` above is `actor_feet_offset()`, that half-frame. Everything that
reads `ground_y` (the shadow, the depth sort, the scale ramp) wants the ground
line, and only the grid speaks centre space, so the conversion belongs at the
grid call and at the drop — not in the readers.

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

Release lands her on the shadow, not on a freshly scanned point — that is what
makes it authoritative (R4), and `walk_grid_clamp_ground` only ever returns
walkable cells, so R3 in `LIVELINESS.md` still holds.

The slew is **snapped to its target on release** rather than landed on as-is.
Slewing is there to smooth the shadow while she is *carried*; letting it also
decide where she comes down means a quick fling, released before the ease
converged, drops her short of where the player aimed. Snapping first keeps the
aim honest and still lands on a walkable cell. The scale can therefore step by
however much the ease was lagging — zero whenever the player pauses before
releasing, which is the normal case.

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
- Sorted on **`actor_depth_y`** — her feet when grounded, `ground_y` when
  airborne — which is the same value her scale reads, so size and sort cannot
  disagree. Sorting a lifted actor (or her shadow) on her *airborne* y would
  occlude her behind props she is plainly in front of: drag the fox high above
  the playground's acorn pile and both she and the shadow on the ground in
  front of it would draw behind it. The shadow is a third drawable kind in
  `action_layer_order`, inserted before the actors so the stable sort keeps a
  shadow just under its own actor.
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

1. ~~**Rendering foundation.**~~ *Shipped.* Alpha-bleed at load (R8);
   scale-about-feet draw; `ScaleRamp` + `actor_scale()`; scaled + floored grab
   rect. No scene declared a ramp yet, so it was provably an identity transform
   (R5).
2. ~~**Opt one scene in.**~~ *Shipped.* The depth demo has a ramp, scales its
   props, and its `DEPTH_BANDS` and generated far sheets are gone.
3. ~~**Drag & drop integration.**~~ *Shipped.* `walk_grid_clamp_ground`,
   `Actor.ground_y`, the lift model, slew, the shadow and its sort entry,
   mid-fall grab, NULL-grid no-op.
4. ~~**Speed scaling.**~~ *Shipped.*
5. ~~**Retire the variant system.**~~ *Shipped.* `ActorVariantSpec`, the
   `animations[VARIANT][STATE]` indirection, `actor_set_variant`,
   `depth_variant_for`, `DepthBand`, the per-variant fidget tables and parity
   validation, `tools/gen_depth_variants.py` and the generated `*_far` art are
   all gone. An `ActorSpec` now carries one flat `anims` / `fidgets` list.

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
