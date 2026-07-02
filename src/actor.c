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
  actor->state = IDLE;
  actor->started_talking_at = 0;
  actor->talking_duration = 0;
  actor->on_end_walking = NULL;
  actor_face(actor, WEST);
  return actor;
}

bool actor_load_media(Actor *actor, SDL_Renderer *renderer) {
  const ActorSpec *spec = actor->spec;
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
      stop_animation(actor->animations[actor->spec->move_state]);
      actor->state = IDLE;
      actor->direction = (SDL_FPoint){0, 0};
      actor->current_position = actor->target_position;
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
    if (ticks - actor->started_talking_at >= actor->talking_duration) {
      stop_animation(actor->animations[TALKING]);
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

void actor_walk_to(Actor *actor, SDL_FPoint target_position,
                   void (*on_end)(void)) {
  if (actor->state == TALKING) {
    return;
  }

  float dx = target_position.x - actor->current_position.x;
  float dy = target_position.y - actor->current_position.y;
  float dist = sqrtf(dx * dx + dy * dy);

  // Tapping on (or right next to) the actor leaves dist ~= 0; dividing by it
  // would make the direction NaN. There's nowhere to walk, so just "arrive".
  if (dist < ACTOR_ARRIVE_EPSILON) {
    actor->target_position = actor->current_position;
    actor->direction = (SDL_FPoint){0, 0};
    if (on_end != NULL) {
      on_end();
    }
    return;
  }

  actor->on_end_walking = on_end;

  if (actor->state != WALKING && actor->move_sound) {
    // Play walking sound
    actor->move_sound_channel = Mix_PlayChannel(-1, actor->move_sound, -1);
  }

  play_animation(actor->animations[actor->spec->move_state], NULL);
  actor->state = WALKING;
  actor->target_position = target_position;

  actor_face(actor, dx > 0 ? EAST : WEST);

  actor->direction = (SDL_FPoint){dx / dist, dy / dist};
}

void actor_talk(Actor *actor, Mix_Chunk *dialog) {
  if (actor->state == WALKING || dialog == NULL) {
    return;
  }

  Uint32 talking_duration = get_chunk_time_ms(dialog);

  Mix_PlayChannel(-1, dialog, 0);
  play_animation(actor->animations[TALKING], NULL);
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
