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

// Animation used to centre the sprite on the actor's position. Assumes all of
// an actor's frames share a size (as the original fox did).
static AnimationData *reference_animation(Actor *actor) {
  if (actor->animations[actor->spec->move_state]) {
    return actor->animations[actor->spec->move_state];
  }
  if (actor->animations[actor->spec->idle_state]) {
    return actor->animations[actor->spec->idle_state];
  }
  for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
    if (actor->animations[i]) {
      return actor->animations[i];
    }
  }
  return NULL;
}

static void actor_face(Actor *actor, HorizontalOrientation orientation) {
  SDL_RendererFlip flip =
      orientation == EAST ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
  for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
    if (actor->animations[i]) {
      actor->animations[i]->flip = flip;
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
    if (actor->animations[actor->spec->move_state]) {
      stop_animation(actor->animations[actor->spec->move_state]);
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
  for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
    actor->animations[i] = NULL;
  }
  for (int i = 0; i < spec->anims_length; i++) {
    const ActorAnimSpec *anim = &spec->anims[i];
    AnimationData *animation = make_animation_data(anim->frames, anim->style);
    if (animation != NULL && anim->ms_per_frame > 0) {
      animation->ms_per_frame = anim->ms_per_frame;
    }
    actor->animations[anim->state] = animation;
  }
  // Play the idle animation by default; it is also used for the IDLE state.
  if (actor->animations[spec->idle_state]) {
    play_animation(actor->animations[spec->idle_state], NULL);
  }
  actor->move_sound = NULL;
  actor->move_sound_channel = -1;
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

bool actor_load_media(Actor *actor, SDL_Renderer *renderer) {
  const ActorSpec *spec = actor->spec;

  // A cue-driven talking sheet must have exactly one frame per mouth shape
  // (canonical order X A B C D E F) — enforce the contract loudly.
  if (spec->talk_shape_frames == MOUTH_SHAPE_COUNT &&
      (actor->animations[TALKING] == NULL ||
       actor->animations[TALKING]->frames != MOUTH_SHAPE_COUNT)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "%s: talk_shape_frames is %d but the TALKING animation has "
                 "%d frames",
                 spec->id, MOUTH_SHAPE_COUNT,
                 actor->animations[TALKING] ? actor->animations[TALKING]->frames
                                            : 0);
    return false;
  }

  for (int i = 0; i < spec->anims_length; i++) {
    const ActorAnimSpec *anim = &spec->anims[i];
    if (!load_animation(renderer, actor->animations[anim->state],
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
    Mix_VolumeChunk(actor->move_sound, spec->move_sound_volume);
  }

  return true;
}

void actor_update(Actor *actor, float delta_time) {
  // Advance every playing animation each frame (timing lives here now, not in
  // render). All actor animations loop with no end callback, so this never
  // fires a stray callback.
  int now = SDL_GetTicks();
  for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
    if (actor->animations[i] != NULL) {
      animation_update(actor->animations[i], now);
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
      stop_animation(actor->animations[actor->spec->move_state]);
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

    actor->current_position = (SDL_FPoint){
        .x = actor->current_position.x +
             actor->direction.x * actor->spec->velocity * delta_time,
        .y = actor->current_position.y +
             actor->direction.y * actor->spec->velocity * delta_time};
    break;
  }
  case TALKING:
    // Cue-driven mouth: pick the frame for the active cue. The animation is
    // never "playing" in this mode, so animation_update leaves the frame
    // alone and no end callback is ever armed.
    if (actor->talking_cues != NULL && actor->animations[TALKING] != NULL) {
      actor->animations[TALKING]->current_frame = lipsync_shape_at(
          actor->talking_cues, (Uint32)now - actor->started_talking_at,
          &actor->cue_cursor);
    }
    if (ticks - actor->started_talking_at >= actor->talking_duration) {
      if (actor->talking_cues != NULL) {
        actor->animations[TALKING]->current_frame = MOUTH_X;
        actor->talking_cues = NULL;
      } else if (actor->animations[TALKING] != NULL) {
        stop_animation(actor->animations[TALKING]);
      }
      actor->state = IDLE;
    }
    break;
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
  AnimationData *animation = actor->animations[state];
  if (animation == NULL) {
    animation = actor->animations[actor->spec->idle_state];
  }
  if (animation == NULL) {
    animation = reference;
  }
  render_animation(renderer, animation, position);
}

void actor_free(Actor *actor) {
  for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
    if (actor->animations[i]) {
      free_animation(actor->animations[i]);
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

  play_animation(actor->animations[actor->spec->move_state], NULL);
  actor->state = WALKING;
  start_segment(actor, actor->waypoints[0]);
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
    Mix_PlayChannel(-1, chunk, 0);
  }

  // Cue mode needs cues and a canonical 7-frame talking sheet (validated in
  // actor_load_media); everything else keeps the classic looping animation.
  if (dialog != NULL && dialog->cues.length > 0 &&
      actor->spec->talk_shape_frames == MOUTH_SHAPE_COUNT &&
      actor->animations[TALKING] != NULL) {
    actor->talking_cues = &dialog->cues;
    actor->cue_cursor = 0;
    actor->animations[TALKING]->current_frame = MOUTH_X;
  } else {
    actor->talking_cues = NULL;
    if (actor->animations[TALKING] != NULL) {
      play_animation(actor->animations[TALKING], NULL);
    }
  }
  actor->state = TALKING;
  actor->talking_duration = talking_duration;
  actor->started_talking_at = SDL_GetTicks();
}

void actor_play_state(Actor *actor, ActorState state) {
  if (actor->animations[state]) {
    play_animation(actor->animations[state], NULL);
  }
  actor->state = state;
}
