# Backlog

Future improvements, mostly surfaced while play-testing with a toddler. Each task
is independent; some need art/audio assets that have to be authored separately —
those are flagged. Tasks are lettered for easy reference; A and B already shipped.

## Open

### Task C — Tap-the-fox → contextual voice hint
*Needs new Italian `.wav` voice lines.*

When the player taps the **fox** itself (a common toddler instinct), have her say
what to do next, by world state, plus a nudge that you move by tapping where you
want her to go. Hints are voice-only (the game has no on-screen text).

- **C1 (code):** add `bool fox_contains_point(Fox*, SDL_Point)` from the current
  animation frame's bounds around `current_position`; hit-test the fox at the top
  of each scene's `process_input` (before movement handling).
- **C2 (code + audio):** on a fox tap, a per-scene `give_hint()` picks a line by
  world state and plays it via `fox_talk()` (`sdlexample/fox.c`):
  - *playground_entrance:* key not revealed → "dig with the shovel"; key revealed
    but no key → "go get the key"; has key → "use the key on the gate".
  - *playground:* no acorns → "shake the acorns down"; has acorns → "give them to
    the squirrel"; has peg → "fix the slide"; slide fixed → "let's slide!".
- **Assets:** one Italian `.wav` per state above, placed under
  `playground/dialog/` and `playground_entrance/dialog/`.

### Task D — Drag-to-move the fox
*Optional polish, code-only.*

Today a press triggers movement to the press point on `MOUSEBUTTONDOWN`, so a drag
gesture just moves the fox to where the drag started. Make dragging intuitive: if
a press starts on the fox, treat the `MOUSEBUTTONUP` release point as the
destination (drag-to-move). Both scenes' `process_input`. Layer on after C.

### Task E — Climb-stairs & slide-down animations
*Needs new sprite sheets.*

The fox currently lacks dedicated animations for using the slide. Add two,
following the existing animation pattern (the ~10 steps mirroring
`walking`/`talking`/`sitting`/`waving` across `sdlexample/fox.h` and `fox.c`: new
struct field + `FoxState` + `make_fox` + `fox_load_media` + `fox_render` +
`fox_update` + `fox_face` + `fox_free` + a trigger helper).

- **Climb:** in the slide-fixed path of `maybe_use_slide` (`sdlexample/playground.c`),
  play a climb animation up the stairs before snapping to `START_SLIDE_POS`.
- **Slide:** during the sigmoid descent in `update()` the fox currently shows the
  *talking* animation; swap in a dedicated `sliding` animation for the descent
  (keep the audio).
- **Assets:** `fox/climbing.png` + `climbing.anim`, `fox/sliding.png` +
  `sliding.anim` (CSV `x,y,w,h` per frame, 12 FPS).

### Task F — Walk around non-walkable areas
*Future; see [`MOVEMENT.md`](MOVEMENT.md).*

Movement is a straight line with no collision/pathfinding, so the fox can walk
through non-walkable areas (e.g. the sandbox). Replace straight-line movement with
obstacle-avoiding routing — a corner waypoint around the rectangle for the simple
single-obstacle case, or a navigation grid / navmesh + A\* for the general case.
Keep the engine simple until this becomes a felt gameplay problem.

## Suggested sequencing

1. **C** — biggest remaining UX win; write C1 + the state machine, fill in lines
   as the `.wav`s arrive.
2. **E** — engine wiring can be written behind the new states; lands with the art.
3. **D** — optional polish.
4. **F** — only if the walk-through becomes a real problem.

## Asset checklist (blocks C2 and E; to be provided)

- Italian voice `.wav` per hint state (Task C2) → `playground*/dialog/`.
- `fox/climbing.png` + `climbing.anim`, `fox/sliding.png` + `sliding.anim` (Task E).

## Shipped

- **A** — enlarged the shovel hotspot (#5).
- **B** — clicks outside the walkable area now walk the fox to the nearest
  reachable point instead of being ignored (#5).
- Web build target + GitHub Pages deployment; custom minimal HTML shell; iOS
  Safari audio fix.
