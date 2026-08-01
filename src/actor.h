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
#include "scaling.h"
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
  LANDING, // keep last (ACTOR_STATE_COUNT below tracks it)
} ActorState;

// Number of states / size of the per-state animation table. A macro rather than
// the enum's last member, so a `switch (state)` built with -Wswitch flags a
// newly added ActorState instead of silently matching a no-op ACTOR_STATE_COUNT
// case (see actor_update). Point it at whatever state stays last.
#define ACTOR_STATE_COUNT (LANDING + 1)

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
  // One sprite set, drawn at whatever size the scene's depth ramp gives it
  // (SCALING.md).
  const ActorAnimSpec *anims;
  int anims_length;
  // Idle fidgets (LIVELINESS.md Part 1); NULL/0 = this actor never fidgets.
  const ActorFidgetSpec *fidgets;
  int fidgets_length;
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
  // Drag & drop (LIVELINESS.md Part 2), managed by actor_drag_event. A press
  // over the sprite arms a drag; an upward pull past the threshold starts one.
  // A drag is purely vertical: only current_position.y changes, and never
  // below ground_y, so the actor cannot be *moved* by dragging — only lifted
  // and let go.
  bool drag_armed;
  SDL_FPoint drag_grab; // pointer position at the arming press
  float drag_offset_y;  // current_position.y - pointer.y when the drag began
  // The ground she was picked up from, in feet space (actor_feet_y). Fixed for
  // the whole gesture: the shadow is drawn on it, she falls back to it, and her
  // depth — and so her size — is read from it, which is why a drag never
  // rescales her (SCALING.md).
  float ground_y;
  // Idle fidgets (LIVELINESS.md Part 1): the spec's fidget sheets (ONE_SHOT),
  // which one is playing while FIDGETING, and when the next one fires
  // (re-rolled every time the actor enters IDLE).
  AnimationData *fidget_anims[ACTOR_MAX_FIDGETS];
  int active_fidget;
  Uint32 next_fidget_at;
  // The depth ramp of the scene this actor belongs to (SCALING.md), borrowed
  // from Scene.scale_ramp. NULL (the poster scenes and minigames) means she is
  // always drawn at scale 1.
  const ScaleRamp *scale_ramp;
} Actor;

// scale_ramp is borrowed, not copied: it must outlive the actor. Pass NULL for
// a scene that declares no depth ramp.
Actor *make_actor(const ActorSpec *spec, SDL_FPoint initial_position,
                  const ScaleRamp *scale_ramp);

// Scene y of the actor's ground-contact point. current_position is the sprite
// *centre* (all walk data is authored against it); y-sorting and depth need
// the feet instead: centre y + half the reference frame height. Deliberately
// on the *natural* frame height, not the drawn one — otherwise it would depend
// on the scale it feeds (SCALING.md).
float actor_feet_y(const Actor *actor);

// The gap actor_feet_y adds to current_position.y — half the reference frame
// height. Subtract it from a ground line to get the centre y at which her feet
// rest on it (what a drag clamps against and a drop falls to).
float actor_feet_offset(const Actor *actor);

// The one y an actor's depth is read from (SCALING.md): her feet on the
// ground, or — while she is airborne — the ground she is bound for. Her size
// and her place in the depth sort both come from this, so they cannot
// disagree.
float actor_depth_y(const Actor *actor);

// How large the actor is drawn right now, from her scene's depth ramp
// (SCALING.md). Exactly 1.0 for a scene that declares none. Cheap enough to
// call per draw — deliberately not cached, since the grab hit-test reads it
// during input, before the frame's update has run.
float actor_scale(const Actor *actor);

// Is the actor being held above the ground she came from? Then the framework
// draws her landing shadow there — the gap between her and it is the lift.
bool actor_shadow_visible(const Actor *actor);
void actor_render_shadow(const Actor *actor, SDL_Renderer *renderer);

bool actor_load_media(Actor *actor, SDL_Renderer *renderer);

void actor_update(Actor *actor, float delta_time);

// Draws the actor alone. Scenes go through render_action_layer (scene.h)
// instead, which depth-sorts her together with her landing shadow and the
// scene's props — drawing her with this directly loses the shadow.
void actor_render(Actor *actor, SDL_Renderer *renderer);

// Where a carried item's top-left lands, before scaling. `offset` is authored
// against the unflipped (west-facing) sheet, so this mirrors it about the
// actor's centre once she turns east. Exposed for tests.
SDL_Point actor_carried_at(const Actor *actor, SDL_Point offset, int width);

// Draw something the actor is carrying. `offset` is authored against her
// natural size, relative to current_position, and is scaled and anchored
// exactly like her sprite — so a carried item stays on her body instead of
// floating full-size beside a shrunken actor (SCALING.md). Identical to a
// plain render_image at the offset when her scene has no depth ramp. The item
// mirrors with her, so it stays on the side she is facing.
void actor_render_carried(const Actor *actor, SDL_Renderer *renderer,
                          const ImageData *image, SDL_Point offset);

void actor_free(Actor *actor);

// Put the actor down at `at`, cancelling any walk in progress. Scene changes
// and scripted placement go through this rather than assigning
// current_position, so a walk still running does not immediately drag her back
// off the spot.
void actor_place(Actor *actor, SDL_FPoint at);

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

// Play a one-off state animation (e.g. SITTING, WAVING) and hold it. Refuses
// (returns false, leaving the actor unchanged) when the actor has no
// animation for `state` — entering a state it can't render would be a bug —
// mirroring how actor_talk / actor_walk_to refuse rather than enter a bad
// state.
bool actor_play_state(Actor *actor, ActorState state);

// The sprite's screen-space rectangle (reference frame centred on
// current_position): the grab target for drag & drop, before padding.
SDL_Rect actor_sprite_rect(const Actor *actor);

// Drag & drop (LIVELINESS.md Part 2). A press over the sprite arms a drag; an
// upward pull past DRAG_START_THRESHOLD starts one, so a tap or a sideways
// swipe still reaches the hotspots underneath. Returns true once it has taken
// the actor over, which is a scene's cue to stop handling the event.
//
// The gesture only lifts her: x never moves and y never goes below the ground
// she was picked up from, so releasing always sets her back down exactly where
// she was. That is the whole model — there is no landing to search for, and a
// drag needs no walkable area at all, which is why the poster scenes can use it
// too.
bool actor_drag_event(Actor *actor, const SDL_Event *event);

// The pieces actor_drag_event drives; separate so tests can step a gesture.
// actor_begin_drag interrupts a walk (dropping its callback) or catches the
// actor mid-fall, and is refused while TALKING (returns false), like walks are.
bool actor_begin_drag(Actor *actor);
// Lift her to follow the pointer, clamped so she never sinks below ground_y.
void actor_drag_move(Actor *actor, float pointer_y);
// Let go: she falls back to ground_y, or lands straight away if she is already
// there (a release with no lift, never an upward "fall").
void actor_drop(Actor *actor);

#endif /* actor_h */
