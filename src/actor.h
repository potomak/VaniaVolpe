//
//  actor.h
//  Generic animated, positioned character (the player or an NPC). A concrete
//  character (fox, chicken, …) is described by an ActorSpec — data, not code —
//  so the engine carries no character-specific fields.
//

#ifndef actor_h
#define actor_h

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "image.h"
#include "lipsync.h"
#include "sound.h"

typedef enum horizontal_orientation {
  WEST = -1,
  EAST = 1,
} HorizontalOrientation;

// Maximum points in one walk (a smoothed path is 2-4 points in practice).
#define ACTOR_MAX_WAYPOINTS 8

typedef enum actor_state {
  IDLE,
  WALKING,
  TALKING,
  SITTING,
  WAVING,
  // Always last: number of states / size of the animation table.
  ACTOR_STATE_COUNT,
} ActorState;

// One animation in a character's spec (a sprite sheet + its .anim metadata).
typedef struct actor_anim_spec {
  ActorState state;
  const char *sprite_filename;
  const char *data_filename;
  int frames;
  AnimationPlaybackStyle style;
  // Milliseconds per frame; 0 means the engine default (DEFAULT_MS_PER_FRAME).
  int ms_per_frame;
} ActorAnimSpec;

// Static description of a character. Two characters differ only by their spec.
typedef struct actor_spec {
  const char *id;
  const char *display_name;        // "Vania", "Gina" — dialogue log prefix
  const char *assets_dir;          // e.g. "fox"
  float velocity;                  // walking speed, px/s
  const char *move_sound_filename; // looped while walking; NULL for none
  int move_sound_volume;
  ActorState idle_state; // animation shown while IDLE (e.g. SITTING)
  ActorState move_state; // animation shown while WALKING
  const ActorAnimSpec *anims;
  int anims_length;
  // MOUTH_SHAPE_COUNT: the TALKING sheet has one frame per mouth shape in
  // canonical order (X A B C D E F) and lines with .cues sidecars drive it.
  // 0: classic looping talking animation (see SPEECH.md).
  int talk_shape_frames;
} ActorSpec;

typedef struct actor {
  const ActorSpec *spec;
  // Indexed by ActorState; entries the spec doesn't provide stay NULL.
  AnimationData *animations[ACTOR_STATE_COUNT];
  Mix_Chunk *move_sound;
  int move_sound_channel;
  SDL_FPoint current_position;
  // The current segment's target; the remaining segments of a multi-point
  // walk are waypoints[waypoint_index + 1 ..].
  SDL_FPoint target_position;
  SDL_FPoint direction;
  SDL_FPoint waypoints[ACTOR_MAX_WAYPOINTS];
  int waypoints_length;
  int waypoint_index;
  ActorState state;
  Uint32 started_talking_at;
  Uint32 talking_duration;
  // Cue-driven talking (see SPEECH.md): the active line's mouth cues, or
  // NULL for the classic looping animation. cue_cursor caches the scan
  // position for lipsync_shape_at.
  const MouthCues *talking_cues;
  int cue_cursor;
  // Fired once when the current walk reaches its target; per-instance so two
  // actors walking at once don't clobber each other's callback. NULL when idle.
  void (*on_end_walking)(void);
} Actor;

Actor *make_actor(const ActorSpec *spec, SDL_FPoint initial_position);

bool actor_load_media(Actor *actor, SDL_Renderer *renderer);

void actor_update(Actor *actor, float delta_time);

void actor_render(Actor *actor, SDL_Renderer *renderer);

void actor_free(Actor *actor);

void actor_walk_to(Actor *actor, SDL_FPoint position, void (*on_end)(void));

// Walk through up to ACTOR_MAX_WAYPOINTS points in order (extra points are
// dropped with a warning); on_end fires once, on reaching the last one.
// Points the actor is already standing on are skipped; if nothing remains,
// any in-flight walk is cancelled (dropping its pending callback, like any
// interrupted walk) and on_end fires immediately.
void actor_walk_path(Actor *actor, const SDL_FPoint *points, int points_length,
                     void (*on_end)(void));

// Speak a line. `dialog` carries the audio and its optional sidecars (text,
// mouth cues, word timings); `text` overrides the sidecar transcript (used
// while a line's audio is still a placeholder). Either may be NULL, but not
// both. With no audio the talking duration is estimated from the text. The
// spoken line is logged as "<display_name>: <text>".
void actor_talk(Actor *actor, const ChunkData *dialog, const char *text);

// Play a one-off state animation (e.g. SITTING, WAVING) and hold it.
void actor_play_state(Actor *actor, ActorState state);

#endif /* actor_h */
