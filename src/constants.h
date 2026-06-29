#ifndef CONSTANTS_H
#define CONSTANTS_H

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#define FPS 30
#define FRAME_TARGET_TIME (1000 / FPS)

#define AUDIO_CHUNK_SIZE 2048

// How close (in logical px) the actor must get to its target to count as
// arrived. Also the "tap landed on the actor" threshold in actor_walk_to.
#define ACTOR_ARRIVE_EPSILON 2.0f

#define LEN(x) (sizeof(x) / sizeof((x)[0]))

#endif
