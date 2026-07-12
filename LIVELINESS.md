# Liveliness: idle fidgets, dragging the actor, boiling hotspots

Spec for three features that make Gina's world feel alive and touchable:

1. **Idle fidgets:** leave Gina alone for a few seconds and she does
   something small — pecks the ground, shakes her head, blinks.
2. **Drag & drop the actor:** pick Gina up with the pointer and carry her; a
   special animation plays while she is held, while she falls, and when she
   lands. (Play-tested by the target audience: toddlers try this
   unprompted.)
3. **Boiling hotspots:** every *active* hotspot object keeps playing a
   subtle 2–3 frame "squiggle" loop — the hand-drawn line boil — so the
   things you can tap advertise themselves, without labels or glows.

All three mechanisms are engine-generic (any `Actor` / any scene), and all
three are art-gated: the engine work in each part is small, the sprite
sheets are what ship the feature. Each part is an independent PR that
leaves the game working; scoped to Gina's adventure first, Vania can adopt
any of them later. Tracking issues:
[#97](https://github.com/potomak/VaniaVolpe/issues/97) (fidgets),
[#96](https://github.com/potomak/VaniaVolpe/issues/96) (drag & drop),
[#95](https://github.com/potomak/VaniaVolpe/issues/95) (boiling hotspots).

## Requirements

- **R1** — An actor idle for a randomized few seconds plays one of its
  one-shot *fidget* animations, then returns to idle. Any interaction
  (walk, talk, drag) interrupts a fidget instantly.
- **R2** — Press-and-move on the actor picks her up: the sprite follows the
  pointer while held (DRAGGED), falls when released (FALLING), and plays a
  landing beat on touchdown (LANDING) before returning to IDLE.
- **R3** — A dropped actor always ends up on legal ground, per the scene's
  *current* walk grid — so the drop can never break a puzzle invariant
  (pre-sunscreen Gina lands back in the shade, nobody lands in the pool).
- **R4** — A hotspot may carry a *hint animation*; the engine plays it while
  the hotspot is enabled and freezes it while it is gated off, reusing the
  `Hotspot.enabled` predicates (#94). No per-scene sync code.
- **R5** — Every state/animation this spec adds is optional: an actor with
  no fidgets never fidgets, an actor with no drag sheets falls back to its
  idle sheet mid-air, a hotspot with no hint stays static. Vania's
  adventure keeps working with zero data changes.
- **R6** — All four builds (`make`, `make terminal`, `make test`,
  `make web`) stay green; the headless playthrough is extended where the
  mechanism is observable without a renderer.

Non-goals: physics beyond a straight vertical fall (no arcs, no bouncing,
no throw momentum), dragging NPCs or props, multi-touch, fidget/boil art
for Vania's adventure (mechanism-ready, art later), positional audio.

---

## Part 1 — Idle fidgets

### Design

A fidget is a short **one-shot** animation played from IDLE, chosen at
random after a randomized quiet delay. It is flavour, not state: the actor
is still IDLE for gameplay purposes the moment anything interrupts it.

Fidgets are deliberately *not* new `ActorState`s. States name behaviours
the engine treats differently (WALKING moves, TALKING times out); fidgets
are all the same behaviour with different art, and hens fidget differently
than foxes — a per-character enum would churn `actor.h` for every new
character. Instead the spec carries an open-ended list:

```c
// actor.h
#define ACTOR_MAX_FIDGETS 4

typedef struct actor_spec {
  ...
  // One-shot idle fidgets (peck, blink, …), same fields as an
  // ActorAnimSpec minus the state. NULL/0 = the actor never fidgets.
  const ActorFidgetSpec *fidgets;
  int fidgets_length;
} ActorSpec;
```

with `ActorFidgetSpec = {sprite_filename, data_filename, frames,
ms_per_frame}`. One new engine state covers all of them:

- `FIDGETING` is added to `ActorState`. `Actor` gains
  `AnimationData *fidget_anims[ACTOR_MAX_FIDGETS]`, `int active_fidget`,
  and `Uint32 next_fidget_at`.

### Behaviour

- Entering IDLE (from anywhere) rolls `next_fidget_at = now +
  rand_range(FIDGET_MIN_DELAY_MS, FIDGET_MAX_DELAY_MS)` — proposed
  4000–9000 ms (`constants.h`).
- In `actor_update`, `state == IDLE && now >= next_fidget_at` picks a
  random fidget, plays it as ONE_SHOT, sets `state = FIDGETING`.
- `state == FIDGETING && !anim->is_playing` → back to IDLE (which re-rolls
  the timer). Polling `is_playing` avoids the context-less `on_end`
  callback (an actor callback couldn't know *which* actor fired it).
- `actor_walk_path`, `actor_talk`, and drag-begin (Part 2) treat FIDGETING
  exactly like IDLE, stopping the fidget animation first — a tap always
  wins over a peck.
- Fidgets play only on **variant 0** (the near sprite set). Far variants
  skip them rather than requiring every fidget at every depth
  (`actor_load_media`'s variant-parity check does not apply to fidgets).
- Facing: fidget sheets inherit the actor's current flip, like every other
  animation (`actor_face` extends over `fidget_anims`).

### Art (needs-art)

For Gina, 2–3 sheets in `assets/common/hen/`, same format as her other
animations (sprite sheet + CSV `.anim`, frames sized like `idle.png`):

| sheet | frames | beat |
|---|---|---|
| `peck.png` | ~4 | head down, two pecks, head up |
| `headshake.png` | ~4 | quick left-right ruffle |
| `blink.png` | 2–3 | eyelids close and open |

The mechanism ships dormant until at least one sheet lands (R5), so the
engine PR and the art can go in either order.

### Testing

Headless: force `next_fidget_at` (test-only setter or a short-delay spec),
step the harness, assert IDLE → FIDGETING → IDLE and that
`actor_walk_path` mid-fidget cancels it. The state transitions are fully
observable without a renderer.

---

## Part 2 — Drag & drop the actor

### Design

Three new `ActorState`s, all with **optional** sheets (R5 — missing ones
fall back to the idle sheet, via the existing `actor_render` fallback):

- `DRAGGED` — held: Gina dangles from the pointer (legs paddling, art's
  call).
- `FALLING` — released: wings flap. A hen "flying" is the joke *and* the
  physics excuse (below).
- `LANDING` — one-shot touchdown beat (feather puff / little squat), then
  IDLE.

### Picking her up, putting her down

- **Grab:** `SDL_MOUSEBUTTONDOWN` inside the actor's sprite rect (the
  reference frame centred on `current_position`, padded ~10 px for toddler
  fingers) *arms* a drag; the first `SDL_MOUSEMOTION` beyond
  `DRAG_START_THRESHOLD` (~8 px) while held begins it. A press on the
  actor that never crosses the threshold does nothing on release — which
  is exactly what tapping the actor does today (she walks to where she
  already stands), so no behaviour is lost.
- **Held:** `current_position` follows the pointer 1:1 (with the grab-point
  offset, so she doesn't snap by half a sprite). An in-flight walk is
  cancelled at grab time, dropping its callback like any interrupted walk;
  TALKING refuses the grab, same as it refuses walks. Scene clicks and
  hotspots never see the press — the drag helper consumes it.
- **Release:** compute the landing point (below), enter FALLING.

Because scenes own `process_input`, the helper lives where actor and grid
already meet — `walk.c`:

```c
// walk.h — call first in process_input, before hotspot dispatch;
// returns true when the event belonged to the drag (grab, move, release).
bool walk_actor_drag_event(Actor *actor, const WalkGrid *grid,
                           const SDL_Event *event);
```

Camera scenes need nothing special: the engine already converts input to
scene coordinates before the scene sees the event, and the camera happily
follows a dragged actor. On the web, SDL translates single-finger touches
to mouse events, so this works on a tablet unchanged.

### The z-axis question — there is no z

The open question was how to compute the fall height of the dragged
position without a z axis. The answer: **the walk grid already is the
ground.** A drag position is interpreted as "in the air over column x",
and on release Gina falls straight down until her feet meet the ground:

- Landing target = the centre of the **first walkable cell at or below the
  drop point's feet y, in the drop point's grid column** (a straight scan
  down one column of `WalkGrid.cells`).
- No walkable cell below (dropped under the poolside strip, or in a
  column with no ground at all) → fall back to `walk_grid_nearest`; if the
  nearest legal point is at or above the drop point, skip FALLING and go
  straight to LANDING there (a short hop rather than an upward "fall").
- Fall distance = `landing_feet_y - drop_feet_y`, free from the same
  subtraction — available to scale the landing beat later (bigger puff,
  an "Ohi!" line past some height), which is polish, not v1.

This also makes **R3** free: the landing scan runs on the scene's *live*
grid, so pre-sunscreen Gina — whose grid covers only the umbrella's
shadow — always lands back in the shade no matter where she is dropped,
and nobody can be dropped into the pool (water is not walkable). The
puzzle rules and the toy obey the same source of truth.

### The fall

Constant `FALL_SPEED` (proposed ~420 px/s, `constants.h`), no gravity
integration: one line of math, and the flap art justifies the terminal-
velocity-from-frame-one look — a hen doesn't plummet, she flusters down.
On touchdown: snap feet to the landing y, play LANDING as ONE_SHOT (state
polls `is_playing`, as fidgets do), then IDLE. If the LANDING sheet is
missing, go straight to IDLE.

Depth-band scenes: scenes poll `depth_variant_for(feet_y)` every frame, so
a dragged actor would flip variants mid-air; scenes with bands should skip
the poll while `state >= DRAGGED` is airborne (one condition). Gina has
one variant, so this is a note, not work.

### Art (needs-art) & audio (needs-audio)

- `dragged.png` (~2 frames, LOOP), `falling.png` (~2–4 frames of flapping,
  LOOP), `landing.png` (~3 frames, ONE_SHOT) in `assets/common/hen/`.
- Optional spec-level sounds, plumbed like `move_sound_filename`: a
  flap/squawk loop while FALLING, a soft thud on LANDING.

### Testing

Headless: synthesize the down/motion/up events through
`walk_actor_drag_event`, assert the position follows, the landing column
scan (drop over the pool → lands on the strip; drop pre-sunscreen far
from the umbrella → lands inside the shade rects), and the state chain
DRAGGED → FALLING → LANDING → IDLE. Browser: extend the Puppeteer script
with a `mouse.down/move/up` drag once art exists.

---

## Part 3 — Boiling hotspots *(shipped)*

Implemented in #95. `Hotspot` carries an optional `hint` animation;
`sync_hotspot_hints` (in `scene.c`, called once per frame from `game_update`
before the scene's animations are ticked) plays a hint while any hotspot
carrying it is enabled and freezes it otherwise, ORing across hotspots that
share one hint. Gina's pool/tree/vine objects boil off placeholder sheets
generated by `tools/gen_boil_sheet.py` (a per-row/column sine wiggle over the
still PNGs, played at the standard animation rate); the still PNGs stay as the
boil source, like the depth-variant sheets. Real traced frames replace the
placeholders file-for-file. The design as built:

### Design

Gina's art style is hand-drawn with squiggly lines. In that style, the
classic way to say "this object is alive" is the **boil**: the same
drawing traced 2–3 times, cycled slowly, so the lines wobble while the
object stays put. The rule for the player (R4): **what squiggles is
tappable right now.** Objects whose hotspot is gated off hold still —
the same information the debug overlay's dimmed rects (#94) show
developers, delivered diegetically.

Everything needed nearly exists: scenes already declare animations the
framework ticks, so a boil is just another looping scene animation at the
standard rate. Two small pieces are new:

```c
// scene.h
typedef struct hotspot {
  ...
  // Boil loop for the hotspot's object; the engine plays it while the
  // hotspot is enabled and freezes it while gated off. NULL = static.
  AnimationData *hint;
} Hotspot;
```

and one engine-side sync (in `game.c`, next to the scene-animation tick):
once per frame, for each hotspot of the current scene with a `hint`,
`enabled()` → `play_animation` if not already playing, else
`stop_animation`-without-callback (direct `is_playing = false`, so no
stray `on_end` fires). Scenes render the object with `render_animation`
instead of `render_image`; conditional visibility (the goggles disappear
once collected) stays scene logic, exactly as today.

`hint` is a borrowed pointer into the scene's `animations` table — the
scene still owns loading/freeing; the table entry gives the framework the
tick, the hotspot entry gives it the enabled-sync. Two hotspots may share
one hint (the sunscreen bottle's two table entries point at the same
bottle boil; the engine ORs their enabled states by virtue of the last
writer being the enabled one — sync must OR explicitly: play if *any*
carrying hotspot is enabled).

### Art (needs-art)

Per object, 2–3 traced frames in place of today's single placeholder PNG,
packed as a sprite sheet + `.anim` (the packer, #82, fits here). Gina's
current inventory of hotspot objects:

| scene | objects |
|---|---|
| pool | sunscreen bottle, goggles, float, pool water |
| tree | float in branches, Carla |
| vine | grape bunch |

Navigation-edge hotspots have no object image and stay invisible (they
are affordances of the scene edges, not things). Water is left static for
now — jittering a large flat rect reads as noise, not a boil; the small
pickup objects are the ones that squiggle. Until real boil art lands,
`tools/gen_boil_sheet.py` generates the placeholder sheets that ship today:
a per-row/column sine wiggle over each still PNG (a flat placeholder boils
only at its silhouette; hand-drawn line art will boil throughout),
replaced frame-for-frame by real art later.

### Testing

Headless: a scene with a gated hint hotspot; flip the predicate, step,
assert `hint->is_playing` follows. Browser: screenshots two boil-frames
apart show the wobble; the existing playthrough already exercises the
gating predicates.

---

## Order & dependencies

The parts are independent; suggested order by joy-per-effort:

1. **Part 3** first *(shipped, #95)* — smallest engine delta, changes the
   feel of every scene, and the placeholder-boil tool made it visible
   without waiting for art.
2. **Part 2** second — the engine work is the bulk and the art can follow
   (idle-sheet fallbacks keep it playable end-to-end).
3. **Part 1** last — pure charm, fully art-gated.
