//
//  actor.c
//  Generic version of the former fox.c: movement, talking, facing, and a small
//  state machine, parameterised by an ActorSpec.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <math.h>
#include <stdbool.h>

#include "actor.h"
#include "constants.h"
#include "image.h"
#include "sound.h"

// The active variant's animation table, indexed by ActorState.
static AnimationData **variant_animations(Actor *actor) {
  return actor->animations[actor->variant];
}

// Animation used to centre the sprite on the actor's position. Assumes all of
// an actor's frames share a size (as the original fox did) — per variant: a
// far variant's smaller frames give correspondingly nearer feet.
static AnimationData *reference_animation(Actor *actor) {
  AnimationData **animations = variant_animations(actor);
  if (animations[actor->spec->move_state]) {
    return animations[actor->spec->move_state];
  }
  if (animations[actor->spec->idle_state]) {
    return animations[actor->spec->idle_state];
  }
  for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
    if (animations[i]) {
      return animations[i];
    }
  }
  return NULL;
}

// Face every variant's animations, so a depth switch mid-walk keeps the
// actor pointing the same way.
static void actor_face(Actor *actor, HorizontalOrientation orientation) {
  SDL_RendererFlip flip =
      orientation == EAST ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
  for (int v = 0; v < actor->spec->variants_length; v++) {
    for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
      if (actor->animations[v][i]) {
        actor->animations[v][i]->flip = flip;
      }
    }
  }
}

// Begin moving toward `target`: face it and set the unit direction. A
// zero-length segment leaves direction {0, 0}; the arrive test resolves it on
// the next update.
static void start_segment(Actor *actor, SDL_FPoint target) {
  float dx = target.x - actor->current_position.x;
  float dy = target.y - actor->current_position.y;
  float dist = sqrtf(dx * dx + dy * dy);
  actor->target_position = target;
  if (dist < ACTOR_ARRIVE_EPSILON) {
    actor->direction = (SDL_FPoint){0, 0};
    return;
  }
  actor_face(actor, dx > 0 ? EAST : WEST);
  actor->direction = (SDL_FPoint){dx / dist, dy / dist};
}

// Stop an in-flight walk without firing its callback: the pending callback is
// dropped, exactly like a walk interrupted by a new destination.
static void cancel_walk(Actor *actor) {
  if (actor->state == WALKING) {
    if (variant_animations(actor)[actor->spec->move_state]) {
      stop_animation(variant_animations(actor)[actor->spec->move_state]);
    }
    actor->state = IDLE;
    if (actor->move_sound_channel >= 0) {
      Mix_HaltChannel(actor->move_sound_channel);
      actor->move_sound_channel = -1;
    }
  }
  actor->direction = (SDL_FPoint){0, 0};
  actor->target_position = actor->current_position;
  actor->waypoints_length = 0;
  actor->waypoint_index = 0;
  actor->on_end_walking = NULL;
}

Actor *make_actor(const ActorSpec *spec, SDL_FPoint initial_position) {
  Actor *actor = malloc(sizeof(Actor));
  actor->spec = spec;
  actor->variant = 0;
  for (int v = 0; v < ACTOR_MAX_VARIANTS; v++) {
    for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
      actor->animations[v][i] = NULL;
    }
  }
  for (int v = 0; v < spec->variants_length; v++) {
    const ActorVariantSpec *variant = &spec->variants[v];
    for (int i = 0; i < variant->anims_length; i++) {
      const ActorAnimSpec *anim = &variant->anims[i];
      AnimationData *animation = make_animation_data(anim->frames, anim->style);
      if (animation != NULL && anim->ms_per_frame > 0) {
        animation->ms_per_frame = anim->ms_per_frame;
      }
      actor->animations[v][anim->state] = animation;
    }
  }
  // Play the idle animation by default; it is also used for the IDLE state.
  if (variant_animations(actor)[spec->idle_state]) {
    play_animation(variant_animations(actor)[spec->idle_state], NULL);
  }
  actor->move_sound = NULL;
  actor->move_sound_channel = -1;
  actor->voice_channel = -1;
  actor->current_position = initial_position;
  actor->target_position = initial_position;
  actor->direction = (SDL_FPoint){0, 0};
  actor->waypoints_length = 0;
  actor->waypoint_index = 0;
  actor->state = IDLE;
  actor->started_talking_at = 0;
  actor->talking_duration = 0;
  actor->talking_cues = NULL;
  actor->cue_cursor = 0;
  actor->on_end_walking = NULL;
  actor_face(actor, WEST);
  return actor;
}

float actor_feet_y(const Actor *actor) {
  // reference_animation doesn't mutate; it just returns a mutable animation.
  AnimationData *reference = reference_animation((Actor *)actor);
  if (reference == NULL) {
    return actor->current_position.y;
  }
  return actor->current_position.y + reference->sprite_clips[0].h / 2.0F;
}

bool actor_load_media(Actor *actor, SDL_Renderer *renderer) {
  const ActorSpec *spec = actor->spec;

  for (int v = 0; v < spec->variants_length; v++) {
    // Every variant must provide the same set of states as variant 0, so
    // reference_animation and the state fallbacks behave identically at any
    // depth. Enforce loudly, before any file is touched.
    for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
      if ((actor->animations[v][i] == NULL) !=
          (actor->animations[0][i] == NULL)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "%s: variant %d and variant 0 disagree on state %d",
                     spec->id, v, i);
        return false;
      }
    }

    // A cue-driven talking sheet must have exactly one frame per mouth shape
    // (canonical order X A B C D E F) — in every variant.
    if (spec->talk_shape_frames == MOUTH_SHAPE_COUNT &&
        (actor->animations[v][TALKING] == NULL ||
         actor->animations[v][TALKING]->frames != MOUTH_SHAPE_COUNT)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "%s: talk_shape_frames is %d but variant %d's TALKING "
                   "animation has %d frames",
                   spec->id, MOUTH_SHAPE_COUNT, v,
                   actor->animations[v][TALKING]
                       ? actor->animations[v][TALKING]->frames
                       : 0);
      return false;
    }
  }

  for (int v = 0; v < spec->variants_length; v++) {
    const ActorVariantSpec *variant = &spec->variants[v];
    for (int i = 0; i < variant->anims_length; i++) {
      const ActorAnimSpec *anim = &variant->anims[i];
      if (!load_animation(renderer, actor->animations[v][anim->state],
                          (Asset){
                              .filename = anim->sprite_filename,
                              .directory = spec->assets_dir,
                          },
                          (Asset){
                              .filename = anim->data_filename,
                              .directory = spec->assets_dir,
                          })) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "Failed to load %s animation %s", spec->id,
                     anim->sprite_filename);
        return false;
      }
    }
  }

  if (spec->move_sound_filename) {
    char move_sound_path[ASSET_PATH_MAX];
    asset_resolve(
        (Asset){
            .filename = spec->move_sound_filename,
            .directory = spec->assets_dir,
        },
        move_sound_path, sizeof(move_sound_path));
    actor->move_sound = Mix_LoadWAV(move_sound_path);
    if (actor->move_sound == NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to load %s move sound: %s", spec->id,
                   Mix_GetError());
      return false;
    }
    SDL_assert(spec->move_sound_volume >= 0 && spec->move_sound_volume <= 128);
    Mix_VolumeChunk(actor->move_sound, spec->move_sound_volume);
  }

  return true;
}

void actor_update(Actor *actor, float delta_time) {
  // Advance every playing animation each frame (timing lives here now, not in
  // render). Only the active variant's animations ever play, but ticking all
  // of them is a cheap no-op on the stopped ones. All actor animations loop
  // with no end callback, so this never fires a stray callback.
  int now = SDL_GetTicks();
  for (int v = 0; v < actor->spec->variants_length; v++) {
    for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
      if (actor->animations[v][i] != NULL) {
        animation_update(actor->animations[v][i], now);
      }
    }
  }

  float dx = actor->target_position.x - actor->current_position.x;
  float dy = actor->target_position.y - actor->current_position.y;
  float ticks = SDL_GetTicks();

  switch (actor->state) {
  case IDLE:
  case SITTING:
  case WAVING:
  case ACTOR_STATE_COUNT:
    break;
  case WALKING: {
    // Stop when within 2px of target OR when we have passed it (dot product
    // of the original direction and the remaining vector turns negative).
    // The latter handles long frames where the actor overshoots the 2px window.
    bool close_enough =
        fabsf(dx) <= ACTOR_ARRIVE_EPSILON && fabsf(dy) <= ACTOR_ARRIVE_EPSILON;
    bool overshot = (actor->direction.x * dx + actor->direction.y * dy) <= 0;
    if (close_enough || overshot) {
      actor->current_position = actor->target_position;
      // More waypoints: continue into the next segment without stopping the
      // walk animation or the move sound.
      if (actor->waypoint_index + 1 < actor->waypoints_length) {
        actor->waypoint_index++;
        start_segment(actor, actor->waypoints[actor->waypoint_index]);
        return;
      }
      stop_animation(variant_animations(actor)[actor->spec->move_state]);
      actor->state = IDLE;
      actor->direction = (SDL_FPoint){0, 0};
      actor->waypoints_length = 0;
      actor->waypoint_index = 0;
      // Only halt our own walk-sound channel; -1 would halt every channel
      // (dialogue and SFX included).
      if (actor->move_sound_channel >= 0) {
        Mix_HaltChannel(actor->move_sound_channel);
        actor->move_sound_channel = -1;
      }
      // Clear the callback before firing it so a callback that starts a new
      // walk isn't immediately overwritten.
      void (*on_end)(void) = actor->on_end_walking;
      actor->on_end_walking = NULL;
      if (on_end != NULL) {
        on_end();
      }
      return;
    }

    // A far variant covers fewer scene px/s, so apparent speed stays
    // natural across depth bands.
    float velocity = actor->spec->velocity *
                     actor->spec->variants[actor->variant].speed_scale;
    actor->current_position =
        (SDL_FPoint){.x = actor->current_position.x +
                          actor->direction.x * velocity * delta_time,
                     .y = actor->current_position.y +
                          actor->direction.y * velocity * delta_time};
    break;
  }
  case TALKING: {
    AnimationData *talking = variant_animations(actor)[TALKING];
    // Cue-driven mouth: pick the frame for the active cue. The animation is
    // never "playing" in this mode, so animation_update leaves the frame
    // alone and no end callback is ever armed.
    if (actor->talking_cues != NULL && talking != NULL) {
      talking->current_frame = lipsync_shape_at(
          actor->talking_cues, (Uint32)now - actor->started_talking_at,
          &actor->cue_cursor);
    }
    if (ticks - actor->started_talking_at >= actor->talking_duration) {
      if (actor->talking_cues != NULL) {
        if (talking != NULL) {
          talking->current_frame = MOUTH_X;
        }
        actor->talking_cues = NULL;
      } else if (talking != NULL) {
        stop_animation(talking);
      }
      actor->state = IDLE;
    }
    break;
  }
  }
}

void actor_render(Actor *actor, SDL_Renderer *renderer) {
  AnimationData *reference = reference_animation(actor);
  if (reference == NULL) {
    return;
  }

  // Assumes that all animation frames have the same size.
  SDL_Point position = (SDL_Point){
      .x = (int)actor->current_position.x - reference->sprite_clips[0].w / 2,
      .y = (int)actor->current_position.y - reference->sprite_clips[0].h / 2};

  // IDLE renders the actor's idle animation (e.g. the fox's sitting sprite).
  ActorState state =
      actor->state == IDLE ? actor->spec->idle_state : actor->state;
  AnimationData *animation = variant_animations(actor)[state];
  if (animation == NULL) {
    animation = variant_animations(actor)[actor->spec->idle_state];
  }
  if (animation == NULL) {
    animation = reference;
  }
  render_animation(renderer, animation, position);
}

void actor_free(Actor *actor) {
  for (int v = 0; v < actor->spec->variants_length; v++) {
    for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
      if (actor->animations[v][i]) {
        free_animation(actor->animations[v][i]);
      }
    }
  }
  if (actor->move_sound) {
    // Stop the channel before freeing the chunk it may still be playing
    // (a use-after-free otherwise, especially on Emscripten's audio thread).
    if (actor->move_sound_channel >= 0) {
      Mix_HaltChannel(actor->move_sound_channel);
    }
    Mix_FreeChunk(actor->move_sound);
  }
  free(actor);
}

void actor_walk_path(Actor *actor, const SDL_FPoint *points, int points_length,
                     void (*on_end)(void)) {
  if (actor->state == TALKING) {
    return;
  }

  if (points_length > ACTOR_MAX_WAYPOINTS) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "actor_walk_path: %d waypoints truncated to %d", points_length,
                ACTOR_MAX_WAYPOINTS);
    points_length = ACTOR_MAX_WAYPOINTS;
  }

  // Skip leading points the actor is already standing on (tapping on or right
  // next to the actor; a normalised direction would be NaN there).
  int first = 0;
  while (first < points_length) {
    float dx = points[first].x - actor->current_position.x;
    float dy = points[first].y - actor->current_position.y;
    if (fabsf(dx) > ACTOR_ARRIVE_EPSILON || fabsf(dy) > ACTOR_ARRIVE_EPSILON) {
      break;
    }
    first++;
  }

  // Nowhere to walk: fully cancel any in-flight walk first — leaving it
  // running would fire its stale callback on the next update ("use the
  // slide" from anywhere) — then "arrive" immediately.
  if (points_length <= 0 || first >= points_length) {
    cancel_walk(actor);
    if (on_end != NULL) {
      on_end();
    }
    return;
  }

  actor->waypoints_length = 0;
  for (int i = first; i < points_length; i++) {
    actor->waypoints[actor->waypoints_length++] = points[i];
  }
  actor->waypoint_index = 0;
  actor->on_end_walking = on_end;

  if (actor->state != WALKING && actor->move_sound) {
    // Play walking sound
    actor->move_sound_channel = Mix_PlayChannel(-1, actor->move_sound, -1);
  }

  play_animation(variant_animations(actor)[actor->spec->move_state], NULL);
  actor->state = WALKING;
  start_segment(actor, actor->waypoints[0]);
}

void actor_set_variant(Actor *actor, int variant) {
  if (variant == actor->variant || variant < 0 ||
      variant >= actor->spec->variants_length) {
    return;
  }

  // Hand the current state's animation over mid-cycle: the new variant's
  // animation inherits the old one's timing, frame and facing, so a walk or
  // talk continues mid-stride instead of restarting — that continuity is
  // what hides the switch. The old animation is silenced directly rather
  // than via stop_animation, which would fire its end callback (see #35).
  ActorState state = actor->state;
  if (state == IDLE) {
    state = actor->spec->idle_state;
  } else if (state == WALKING) {
    state = actor->spec->move_state;
  }
  AnimationData *from = actor->animations[actor->variant][state];
  AnimationData *to = actor->animations[variant][state];
  if (from != NULL && to != NULL) {
    to->is_playing = from->is_playing;
    to->start_time = from->start_time;
    to->current_frame = from->current_frame;
    to->flip = from->flip;
    from->is_playing = false;
  }
  actor->variant = variant;
}

void actor_walk_to(Actor *actor, SDL_FPoint target_position,
                   void (*on_end)(void)) {
  actor_walk_path(actor, &target_position, 1, on_end);
}

void actor_talk(Actor *actor, const ChunkData *dialog, const char *text) {
  if (actor->state == WALKING) {
    return;
  }

  const char *line = text;
  if (line == NULL && dialog != NULL) {
    line = dialog->text;
  }
  Mix_Chunk *chunk = dialog != NULL ? dialog->chunk : NULL;
  if (chunk == NULL && line == NULL) {
    return;
  }

  // With no audio the duration is estimated from the text (~12.5 chars/s;
  // byte length slightly over-counts accented UTF-8, which is fine).
  Uint32 talking_duration = chunk != NULL
                                ? get_chunk_time_ms(chunk)
                                : (Uint32)SDL_max(1500, 80 * SDL_strlen(line));
  if (talking_duration == 0) {
    return;
  }

  // The log line is part of the headless-test contract (harness_check_*).
  if (line != NULL) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s: %s",
                actor->spec->display_name, line);
  }

  if (chunk != NULL) {
    // Playing on this fixed, reserved channel halts whatever line is still
    // playing there and takes its place, so a new line interrupts the
    // current one instead of overlapping it or being dropped for lack of a
    // free channel.
    actor->voice_channel = Mix_PlayChannel(DIALOG_CHANNEL, chunk, 0);
  } else {
    actor->voice_channel = -1;
  }

  // Cue mode needs cues and a canonical 7-frame talking sheet (validated in
  // actor_load_media); everything else keeps the classic looping animation.
  AnimationData *talking = variant_animations(actor)[TALKING];
  if (dialog != NULL && dialog->cues.length > 0 &&
      actor->spec->talk_shape_frames == MOUTH_SHAPE_COUNT && talking != NULL) {
    actor->talking_cues = &dialog->cues;
    actor->cue_cursor = 0;
    talking->current_frame = MOUTH_X;
  } else {
    actor->talking_cues = NULL;
    if (talking != NULL) {
      play_animation(talking, NULL);
    }
  }
  actor->state = TALKING;
  actor->talking_duration = talking_duration;
  actor->started_talking_at = SDL_GetTicks();
}

void actor_play_state(Actor *actor, ActorState state) {
  if (variant_animations(actor)[state]) {
    play_animation(variant_animations(actor)[state], NULL);
  }
  actor->state = state;
}
