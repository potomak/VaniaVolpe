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
  // Playing one of the spec's idle fidgets (LIVELINESS.md Part 1). Fidget
  // sheets live in Actor.fidget_anims, not the per-state animation table, so
  // this slot in that table is always NULL.
  FIDGETING,
  // Drag & drop (LIVELINESS.md Part 2), kept contiguous so
  // `state >= DRAGGED` reads as "airborne / off normal ground". All three
  // sheets are optional — a missing one falls back to the idle sheet.
  DRAGGED,
  FALLING,
  LANDING,
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

// Most sprite sets (depth variants) one actor can carry.
#define ACTOR_MAX_VARIANTS 3

// Most idle fidgets one actor can carry (LIVELINESS.md Part 1).
#define ACTOR_MAX_FIDGETS 4

// One idle fidget in a character's spec: a short one-shot animation (a peck,
// a blink) played after a randomized quiet delay, then back to IDLE. An
// ActorAnimSpec minus the state — fidgets are deliberately not per-character
// ActorStates: they are all the same behaviour with different art, and an
// open-ended list doesn't churn the enum for every new character.
typedef struct actor_fidget_spec {
  const char *sprite_filename;
  const char *data_filename;
  int frames;
  // Milliseconds per frame; 0 means the engine default (DEFAULT_MS_PER_FRAME).
  int ms_per_frame;
} ActorFidgetSpec;

// A full sprite set for one depth, plus how the actor moves at that depth
// (DEPTH_AND_CAMERA.md Phase 2). Variant 0 is the nearest; every variant must
// provide the same set of states as variant 0 (validated in
// actor_load_media). Which variant is active at a given scene y is scene
// data — see DepthBand in scene.h.
typedef struct actor_variant_spec {
  const ActorAnimSpec *anims;
  int anims_length;
  float speed_scale; // 1.0 = spec velocity; far variants use < 1.0
} ActorVariantSpec;

// Static description of a character. Two characters differ only by their spec.
typedef struct actor_spec {
  const char *id;
  const char *display_name;        // "Vania", "Gina" — dialogue log prefix
  const char *assets_dir;          // e.g. "fox"
  float velocity;                  // walking speed, px/s
  const char *move_sound_filename; // looped while walking; NULL for none
  int move_sound_volume;
  ActorState idle_state;            // animation shown while IDLE (e.g. SITTING)
  ActorState move_state;            // animation shown while WALKING
  const ActorVariantSpec *variants; // at least one (variant 0 = nearest)
  int variants_length;              // 1..ACTOR_MAX_VARIANTS
  // MOUTH_SHAPE_COUNT: the TALKING sheet has one frame per mouth shape in
  // canonical order (X A B C D E F) and lines with .cues sidecars drive it.
  // 0: classic looping talking animation (see SPEECH.md).
  int talk_shape_frames;
  // Idle fidgets (LIVELINESS.md Part 1); NULL/0 = the actor never fidgets.
  // Fidgets play on variant 0 only — far variants skip them rather than
  // requiring every fidget at every depth.
  const ActorFidgetSpec *fidgets;
  int fidgets_length;
} ActorSpec;

typedef struct actor {
  const ActorSpec *spec;
  // Indexed by [variant][ActorState]; entries a variant's spec doesn't
  // provide stay NULL.
  AnimationData *animations[ACTOR_MAX_VARIANTS][ACTOR_STATE_COUNT];
  // Active sprite set; 0 initially, switched by actor_set_variant.
  int variant;
  Mix_Chunk *move_sound;
  int move_sound_channel;
  // Channel dialogue last played on (DIALOG_CHANNEL when talking, -1
  // otherwise); see actor_talk.
  int voice_channel;
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
  // Drag & drop (LIVELINESS.md Part 2). A press over the sprite arms a drag
  // (managed by walk_actor_drag_event); drag_offset keeps the grab point
  // under the pointer while DRAGGED; fall_target_y is the centre y the actor
  // descends to while FALLING.
  bool drag_armed;
  SDL_FPoint drag_grab;   // pointer position at the arming press
  SDL_FPoint drag_offset; // current_position - drag_grab when the drag began
  float fall_target_y;
  // Idle fidgets (LIVELINESS.md Part 1): the spec's fidget sheets (variant 0
  // art, ONE_SHOT), which one is playing while FIDGETING, and when the next
  // one fires (re-rolled every time the actor enters IDLE).
  AnimationData *fidget_anims[ACTOR_MAX_FIDGETS];
  int active_fidget;
  Uint32 next_fidget_at;
} Actor;

Actor *make_actor(const ActorSpec *spec, SDL_FPoint initial_position);

// Scene y of the actor's ground-contact point. current_position is the sprite
// *centre* (all walk data is authored against it); y-sorting and depth bands
// need the feet instead: centre y + half the reference frame height. See
// DEPTH_AND_CAMERA.md.
float actor_feet_y(const Actor *actor);

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

// Switch the actor to another sprite set (depth variant). No-op if unchanged
// or out of range. The current state's animation carries on mid-cycle — the
// new variant's animation inherits the old one's timing, frame and facing —
// which is what hides the switch. Scenes drive this once per frame from
// their depth bands: see depth_variant_for in scene.h.
void actor_set_variant(Actor *actor, int variant);

// Speak a line. `dialog` carries the audio and its optional sidecars (text,
// mouth cues, word timings); `text` overrides the sidecar transcript (used
// while a line's audio is still a placeholder). Either may be NULL, but not
// both. With no audio the talking duration is estimated from the text. The
// spoken line is logged as "<display_name>: <text>".
void actor_talk(Actor *actor, const ChunkData *dialog, const char *text);

// Play a one-off state animation (e.g. SITTING, WAVING) and hold it.
void actor_play_state(Actor *actor, ActorState state);

// The sprite's screen-space rectangle (reference frame centred on
// current_position): the grab target for drag & drop, before padding.
SDL_Rect actor_sprite_rect(const Actor *actor);

// Start dragging (LIVELINESS.md Part 2): interrupts a walk (dropping its
// callback) or catches the actor mid-fall; refused while TALKING (returns
// false), like walks are. The actor then follows actor_drag_move until
// actor_drop. Scenes don't call these directly — walk_actor_drag_event does.
bool actor_begin_drag(Actor *actor);
void actor_drag_move(Actor *actor, SDL_FPoint pointer);
// Release at the landing target (centre coordinates, from the walk grid —
// see walk_actor_drag_event): below the actor starts a FALLING descent; at
// or above snaps there directly (a short hop, never an upward "fall").
void actor_drop(Actor *actor, SDL_FPoint target);

#endif /* actor_h */
