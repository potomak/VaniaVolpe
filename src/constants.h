#ifndef CONSTANTS_H
#define CONSTANTS_H

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

// Largest scene a camera can scroll over (DEPTH_AND_CAMERA.md). Sizes the
// walk layer's static arrays: 240x120 cells worst case, ~29 KB per grid.
#define MAX_SCENE_W 2400
#define MAX_SCENE_H 1200

#define FPS 30
#define FRAME_TARGET_TIME (1000 / FPS)

#define AUDIO_CHUNK_SIZE 2048

// Channel 0 is reserved for actor dialogue so sound effects —
// played on whatever channel is free via Mix_PlayChannel(-1, …) — can't steal
// or cut off a voice line mid-sentence.
#define MIXER_CHANNEL_COUNT 16
#define DIALOG_CHANNEL 0

#define MUSIC_VOLUME 30

// How close (in logical px) the actor must get to its target to count as
// arrived. Also the "tap landed on the actor" threshold in actor_walk_to.
#define ACTOR_ARRIVE_EPSILON 2.0f

// Drag & drop (LIVELINESS.md Part 2). A press within GRAB_PADDING px of the
// actor's sprite arms a drag; pointer travel beyond START_THRESHOLD begins it
// (anything less is a tap). A dropped actor descends at FALL_SPEED — constant,
// no gravity integration: a hen flutters down, she doesn't plummet.
#define DRAG_GRAB_PADDING 10
#define DRAG_START_THRESHOLD 8.0f
#define FALL_SPEED 420.0f

// Depth scaling while dragging (SCALING.md). Lifting the actor changes her
// height, not her depth, until the lift passes a ceiling — so a small vertical
// wobble can't rescale her. The ceiling is a fraction of the scene's ramp span
// rather than a flat pixel count, because 60px is the whole dead zone on a
// short ramp and a tenth of a tall one; DRAG_LIFT_MAX_PX applies only to a
// scene with no ramp, where nothing scales anyway.
#define DRAG_LIFT_MAX_FRACTION 0.25f
#define DRAG_LIFT_MAX_PX 60.0f
// How fast the landing shadow eases toward its target, px/s. A column scan is
// discontinuous in x, so a sideways drag across a step in the ground would
// otherwise teleport the shadow and jump the scale.
#define DRAG_GROUND_SLEW 600.0f
// Depth never slows the actor to a crawl: a floor on the speed factor so a
// walk at the ramp's far end still converges.
#define ACTOR_MIN_SPEED_SCALE 0.25f

// Idle fidgets (LIVELINESS.md Part 1): after a randomized quiet delay in
// [MIN, MAX] ms of IDLE, an actor with fidgets plays one at random.
#define FIDGET_MIN_DELAY_MS 4000
#define FIDGET_MAX_DELAY_MS 9000

#define LEN(x) (sizeof(x) / sizeof((x)[0]))

#endif
