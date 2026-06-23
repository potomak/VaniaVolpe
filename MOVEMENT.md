# Movement & pathfinding

This note documents how the fox moves, a known limitation of that model, and the
direction for a future fix. It exists so the limitation isn't lost — there is no
planned change to the engine right now.

## Current behaviour

1. A click/tap is handled in the active scene's `process_input`
   (`sdlexample/playground.c`, `sdlexample/playground_entrance.c`). Object
   hotspots are checked first; anything else falls through to movement.
2. The destination is snapped onto legal ground by `nearest_walkable_point()`
   (`sdlexample/scene.c`): it takes the nearest point of the union of the
   scene's `WALKABLE_*` rectangles and, if that lands inside the
   `NON_WALKABLE_HOTSPOT` rectangle (e.g. the sandbox), pushes it just past the
   nearest edge.
3. `fox_walk_to(target)` (`sdlexample/fox.c`) then moves the fox in a **straight
   line** toward that target at `FOX_VELOCITY` (200 px/s), updated each frame in
   `fox_update`, stopping when within 2 px.

## Known limitation: straight-line movement, no collision/pathfinding

Movement is a single straight segment with no obstacle awareness, so the fox can
visibly walk **through** a non-walkable area whenever the start and the
destination are both walkable but the segment between them crosses the obstacle.

Snapping only the *destination* cannot fix this:

- **Nearest walkable point (current approach):** snaps the click to the closest
  legal point, but that point can be on the *far* side of the obstacle relative
  to the fox, so the straight path still cuts across the sandbox.
- **Ray-to-border (an alternative we discussed):** "draw the fox→click segment
  and stop at the point where it first crosses the obstacle's border." This only
  constrains clicks that land *inside* the obstacle. A perfectly legal
  destination whose straight path merely *clips* the obstacle is still
  unconstrained — so the fox would walk through it.

The root cause is that both approaches only adjust the endpoint; the **path**
itself is never routed around the obstacle.

## Future work: route around non-walkable areas

The real fix is obstacle-avoiding movement. Options, simplest first:

1. **Corner waypoint:** detect when the fox→target segment intersects the
   non-walkable rectangle and insert one or more waypoints at the obstacle's
   corners, so the fox walks around it. This is enough for the current scenes,
   which each have a single axis-aligned non-walkable rectangle.
2. **Grid / navmesh + A\*:** a general navigation mesh (or a coarse walkability
   grid) with A\* pathfinding. More work, but handles arbitrary layouts and
   multiple obstacles if the game grows.

Keep the engine simple (straight-line movement) until/unless this becomes a felt
gameplay problem.

## Relevant code

- `sdlexample/fox.c` — `fox_walk_to`, `fox_update` (the straight-line mover).
- `sdlexample/scene.c` — `nearest_walkable_point` (destination snapping).
- `sdlexample/playground.c`, `sdlexample/playground_entrance.c` — the
  `WALKABLE_*` and `NON_WALKABLE_HOTSPOT` rectangle definitions per scene.
