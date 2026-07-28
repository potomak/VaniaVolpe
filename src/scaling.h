//
//  scaling.h
//  Continuous depth scaling (see SCALING.md): a scene declares one linear ramp
//  from scene y to sprite scale, and whatever is drawn at a depth takes its
//  size from it.
//

#ifndef scaling_h
#define scaling_h

// A scene's depth map — SCUMM's SCAL slot: scale interpolated linearly between
// two scene y bounds, y_far at the back of the floor and y_near at the front.
//
// The bounds are declared rather than derived from the walk grid so that
// perspective is a property of the painted background, not of whichever area
// happens to be walkable at this point in the puzzle.
//
// scale_far has a practical floor around 0.6: below that the cue-driven mouth
// shapes (SPEECH.md) stop reading and the lip-sync is wasted at depth.
typedef struct scale_ramp {
  int y_far;
  int y_near;
  float scale_far;
  float scale_near;
} ScaleRamp;

// Scale at scene y, clamped outside [y_far, y_near]. A NULL ramp — a scene
// that declares no depth — returns exactly 1.0, which makes scaling an
// identity transform there.
float scale_ramp_at(const ScaleRamp *ramp, float y);

#endif /* scaling_h */
