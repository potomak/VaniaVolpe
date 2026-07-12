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

// Channel 0 is reserved for actor dialogue (see #33/#34) so sound effects —
// played on whatever channel is free via Mix_PlayChannel(-1, …) — can't steal
// or cut off a voice line mid-sentence.
#define MIXER_CHANNEL_COUNT 16
#define DIALOG_CHANNEL 0

#define MUSIC_VOLUME 30

// How close (in logical px) the actor must get to its target to count as
// arrived. Also the "tap landed on the actor" threshold in actor_walk_to.
#define ACTOR_ARRIVE_EPSILON 2.0f

#define LEN(x) (sizeof(x) / sizeof((x)[0]))

#endif
