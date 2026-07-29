//
//  actor.c
//  Generic version of the former fox.c: movement, talking, facing, and a small
//  state machine, parameterised by an ActorSpec.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>

#include "actor.h"
#include "clock.h"
#include "constants.h"
#include "image.h"
#include "sound.h"
#include "subtitle.h"

// The frame the sprite is positioned and measured from. All of an actor's
// frames are assumed to share a size (as the original fox did).
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

// Face every animation, including the fidgets, so a state change mid-walk
// keeps the actor pointing the same way.
static void actor_face(Actor *actor, HorizontalOrientation orientation) {
  SDL_RendererFlip flip =
      orientation == EAST ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
  for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
    if (actor->animations[i]) {
      actor->animations[i]->flip = flip;
    }
  }
  for (int i = 0; i < ACTOR_MAX_FIDGETS; i++) {
    if (actor->fidget_anims[i]) {
      actor->fidget_anims[i]->flip = flip;
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

// Fidgets the spec actually carries (bounded by the animation array).
static int fidget_count(const ActorSpec *spec) {
  return SDL_min(spec->fidgets_length, ACTOR_MAX_FIDGETS);
}

// Every return to IDLE goes through here so the fidget timer is re-rolled
// (LIVELINESS.md Part 1): the next fidget fires a randomized few seconds
// after the actor last came to rest. Rolled whether or not the spec has
// fidgets — the trigger checks that. rand() is deliberately unseeded: the
// delays only need to *look* random, and determinism helps the tests.
static void enter_idle(Actor *actor) {
  actor->state = IDLE;
  Uint32 span = FIDGET_MAX_DELAY_MS - FIDGET_MIN_DELAY_MS;
  actor->next_fidget_at =
      clock_now_ms() + FIDGET_MIN_DELAY_MS + (Uint32)(rand() % (int)(span + 1));
}

// Interrupt a playing fidget: a tap always wins over a peck. The fidget was
// played with no callback, so stopping it is silent; callers usually
// overwrite the state right after.
static void stop_fidget(Actor *actor) {
  if (actor->state != FIDGETING) {
    return;
  }
  AnimationData *fidget = actor->fidget_anims[actor->active_fidget];
  if (fidget != NULL) {
    stop_animation(fidget);
  }
  enter_idle(actor);
}

// Stop an in-flight walk without firing its callback: the pending callback is
// dropped, exactly like a walk interrupted by a new destination.
static void cancel_walk(Actor *actor) {
  if (actor->state == WALKING) {
    if (actor->animations[actor->spec->move_state]) {
      stop_animation(actor->animations[actor->spec->move_state]);
    }
    enter_idle(actor);
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

// Depth's effect on how fast she moves: a far actor covers fewer screen px per
// second, so her apparent pace stays natural (SCUMM modulated step distance the
// same way). Floored so a walk at the ramp's far end still converges.
static float depth_speed_scale(const Actor *actor) {
  float scale = actor_scale(actor);
  return scale < ACTOR_MIN_SPEED_SCALE ? ACTOR_MIN_SPEED_SCALE : scale;
}

// Touchdown at the end of a drop: play the one-shot LANDING beat when the
// sheet exists (the LANDING case in actor_update returns to IDLE when it
// stops), else straight back to IDLE.
static void touch_down(Actor *actor) {
  AnimationData *falling = actor->animations[FALLING];
  if (falling != NULL) {
    stop_animation(falling);
  }
  AnimationData *landing = actor->animations[LANDING];
  if (landing != NULL) {
    play_animation(landing, NULL);
    actor->state = LANDING;
  } else {
    enter_idle(actor);
  }
}

Actor *make_actor(const ActorSpec *spec, SDL_FPoint initial_position,
                  const ScaleRamp *scale_ramp) {
  Actor *actor = malloc(sizeof(Actor));
  if (actor == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "make_actor: out of memory");
    return NULL;
  }
  // Each actor owns per-instance AnimationData (made below, not shared with the
  // spec or other actors), so actor_face flipping "all animations" only turns
  // this actor — two actors from one spec face independently.
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
  actor->drag_armed = false;
  actor->drag_grab = (SDL_FPoint){0, 0};
  actor->drag_offset = (SDL_FPoint){0, 0};
  actor->fall_target_y = 0;
  // No sprite is loaded yet, so the feet offset is 0 and these are just her
  // start; has_ground is false until a drag looks the ground up anyway.
  actor->ground_y = actor_feet_y(actor);
  actor->ground_target_y = actor->ground_y;
  actor->grab_ground_y = actor->ground_y;
  actor->lift_ceiling = DRAG_LIFT_MAX_PX;
  actor->has_ground = false;
  for (int i = 0; i < ACTOR_MAX_FIDGETS; i++) {
    actor->fidget_anims[i] = NULL;
  }
  for (int i = 0; i < fidget_count(spec); i++) {
    const ActorFidgetSpec *fidget = &spec->fidgets[i];
    AnimationData *animation = make_animation_data(fidget->frames, ONE_SHOT);
    if (animation != NULL && fidget->ms_per_frame > 0) {
      animation->ms_per_frame = fidget->ms_per_frame;
    }
    actor->fidget_anims[i] = animation;
  }
  actor->active_fidget = 0;
  actor->next_fidget_at = 0;
  actor->scale_ramp = scale_ramp;
  enter_idle(actor); // state IDLE + the first fidget timer roll
  actor_face(actor, WEST);
  return actor;
}

float actor_feet_offset(const Actor *actor) {
  // reference_animation doesn't mutate; it just returns a mutable animation.
  AnimationData *reference = reference_animation((Actor *)actor);
  if (reference == NULL) {
    return 0.0F;
  }
  return reference->sprite_clips[0].h / 2.0F;
}

float actor_feet_y(const Actor *actor) {
  return actor->current_position.y + actor_feet_offset(actor);
}

float actor_scale(const Actor *actor) {
  // Depth comes from the ground, never from how high she is held: standing,
  // that is her feet; airborne, it is the ground she is bound for (SCALING.md).
  // Reading FALLING from her live y instead would make her grow on the way
  // down; reading it from the landing keeps the whole gesture one size.
  float depth;
  switch (actor->state) {
  case DRAGGED:
  case FALLING:
  case LANDING:
    // ground_y holds the landing from the drop onward and the slew only runs
    // while DRAGGED, so it is the same line for all three.
    depth = actor->ground_y;
    break;
  default:
    depth = actor_feet_y(actor);
    break;
  }
  return scale_ramp_at(actor->scale_ramp, depth);
}

// How far she can be lifted before the lift stops being height and starts
// being depth. Expressed against the scene's ramp so the dead zone is the same
// proportion of the scene's depth range everywhere.
float actor_lift_ceiling(const Actor *actor) {
  if (actor->scale_ramp == NULL) {
    return DRAG_LIFT_MAX_PX;
  }
  float span = (float)(actor->scale_ramp->y_near - actor->scale_ramp->y_far);
  if (span < 0.0F) {
    span = -span;
  }
  return span * DRAG_LIFT_MAX_FRACTION;
}

bool actor_shadow_visible(const Actor *actor) {
  // Only while she is genuinely off the ground. LANDING is already down, so it
  // would just draw a shadow under her feet.
  return actor->has_ground &&
         (actor->state == DRAGGED || actor->state == FALLING);
}

void actor_render_shadow(const Actor *actor, SDL_Renderer *renderer) {
  AnimationData *reference = reference_animation((Actor *)actor);
  if (reference == NULL) {
    return;
  }
  float scale = actor_scale(actor);
  // An ellipse roughly the width of her footprint, flattened. Drawn as
  // scanlines: SDL has no ellipse primitive, and this needs no art to ship.
  int rx = (int)((float)reference->sprite_clips[0].w * 0.30F * scale);
  int ry = (int)((float)rx * 0.32F);
  if (rx <= 0 || ry <= 0) {
    return;
  }
  SDL_Point offset = render_get_offset();
  int cx = (int)actor->current_position.x + offset.x;
  int cy = (int)actor->ground_y + offset.y;
  // Fade with height, so the gap between her and the shadow reads as lift.
  float lift = actor->ground_y - actor_feet_y(actor);
  float fade = 1.0F - lift / 400.0F;
  if (fade < 0.35F) {
    fade = 0.35F;
  }
  if (fade > 1.0F) {
    fade = 1.0F;
  }
  SDL_BlendMode previous;
  SDL_GetRenderDrawBlendMode(renderer, &previous);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, (Uint8)(90.0F * fade));
  for (int dy = -ry; dy <= ry; dy++) {
    float t = (float)dy / (float)ry;
    int half = (int)((float)rx * sqrtf(1.0F - t * t));
    SDL_RenderDrawLine(renderer, cx - half, cy + dy, cx + half, cy + dy);
  }
  SDL_SetRenderDrawBlendMode(renderer, previous);
}

bool actor_load_media(Actor *actor, SDL_Renderer *renderer) {
  const ActorSpec *spec = actor->spec;

  // A cue-driven talking sheet must have exactly one frame per mouth shape
  // (canonical order X A B C D E F).
  if (spec->talk_shape_frames == MOUTH_SHAPE_COUNT &&
      (actor->animations[TALKING] == NULL ||
       actor->animations[TALKING]->frames != MOUTH_SHAPE_COUNT)) {
    SDL_LogError(
        SDL_LOG_CATEGORY_APPLICATION,
        "%s: talk_shape_frames is %d but the TALKING animation has %d frames",
        spec->id, MOUTH_SHAPE_COUNT,
        actor->animations[TALKING] ? actor->animations[TALKING]->frames : 0);
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

  if (spec->fidgets_length > ACTOR_MAX_FIDGETS) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "%s: %d fidgets declared, only the first %d are used", spec->id,
                spec->fidgets_length, ACTOR_MAX_FIDGETS);
  }
  for (int i = 0; i < fidget_count(spec); i++) {
    const ActorFidgetSpec *fidget = &spec->fidgets[i];
    if (!load_animation(renderer, actor->fidget_anims[i],
                        (Asset){
                            .filename = fidget->sprite_filename,
                            .directory = spec->assets_dir,
                        },
                        (Asset){
                            .filename = fidget->data_filename,
                            .directory = spec->assets_dir,
                        })) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load %s fidget %s",
                   spec->id, fidget->sprite_filename);
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
    SDL_assert(spec->move_sound_volume >= 0 && spec->move_sound_volume <= 128);
    Mix_VolumeChunk(actor->move_sound, spec->move_sound_volume);
  }

  return true;
}

void actor_update(Actor *actor, float delta_time) {
  // Advance every playing animation each frame (timing lives here, not in
  // render); ticking the stopped ones is a cheap no-op. Actor animations are
  // always played with no end callback (a ONE_SHOT LANDING sheet stops itself
  // silently), so this never fires a stray callback.
  // One Uint32 time base for the whole frame: the talk/fidget timers are Uint32
  // and subtract cleanly (wrapping past ~49 days) — a float lost precision. The
  // animation subsystem still keeps its own int clock, so it takes (int)now.
  Uint32 now = clock_now_ms();
  for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
    if (actor->animations[i] != NULL) {
      animation_update(actor->animations[i], (int)now);
    }
  }
  for (int i = 0; i < ACTOR_MAX_FIDGETS; i++) {
    if (actor->fidget_anims[i] != NULL) {
      animation_update(actor->fidget_anims[i], (int)now);
    }
  }

  float dx = actor->target_position.x - actor->current_position.x;
  float dy = actor->target_position.y - actor->current_position.y;

  switch (actor->state) {
  case IDLE: {
    // Idle fidgets (LIVELINESS.md Part 1): after the rolled delay, play one
    // of the spec's fidgets at random. An actor with no fidget art simply
    // never triggers.
    int fidgets = fidget_count(actor->spec);
    if (fidgets > 0 && now >= actor->next_fidget_at) {
      actor->active_fidget = rand() % fidgets;
      AnimationData *fidget = actor->fidget_anims[actor->active_fidget];
      if (fidget != NULL) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s fidgets (%d)",
                    actor->spec->id, actor->active_fidget);
        play_animation(fidget, NULL);
        actor->state = FIDGETING;
      } else {
        enter_idle(actor); // failed to allocate at startup: just re-roll
      }
    }
    break;
  }
  case FIDGETING: {
    // The one-shot fidget stops itself (animation_update); poll it rather
    // than arming a context-less end callback, like LANDING does.
    AnimationData *fidget = actor->fidget_anims[actor->active_fidget];
    if (fidget == NULL || !fidget->is_playing) {
      enter_idle(actor);
    }
    break;
  }
  case SITTING:
  case WAVING:
    break;
  case DRAGGED: {
    // Position follows the pointer (actor_drag_move); the landing shadow eases
    // toward wherever the drag last put it, so a step in the ground under a
    // sideways carry glides instead of teleporting.
    float remaining = actor->ground_target_y - actor->ground_y;
    float step = DRAG_GROUND_SLEW * delta_time;
    if (fabsf(remaining) <= step) {
      actor->ground_y = actor->ground_target_y;
    } else {
      actor->ground_y += remaining > 0 ? step : -step;
    }
    break;
  }
  case FALLING: {
    // Constant-speed descent to the landing target (LIVELINESS.md Part 2),
    // slowed with depth so a distant fall covers fewer screen px.
    float remaining = actor->fall_target_y - actor->current_position.y;
    float step = FALL_SPEED * depth_speed_scale(actor) * delta_time;
    if (remaining <= step) {
      actor->current_position.y = actor->fall_target_y;
      touch_down(actor);
    } else {
      actor->current_position.y += step;
    }
    break;
  }
  case LANDING: {
    // The one-shot landing beat stops itself (animation_update); poll it
    // rather than arming a context-less end callback, as the spec's fidgets
    // do too.
    AnimationData *landing = actor->animations[LANDING];
    if (landing == NULL || !landing->is_playing) {
      enter_idle(actor);
    }
    break;
  }
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
      enter_idle(actor);
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

    // A distant actor covers fewer scene px/s, so her apparent pace stays
    // natural (SCALING.md).
    float velocity = actor->spec->velocity * depth_speed_scale(actor);
    actor->current_position =
        (SDL_FPoint){.x = actor->current_position.x +
                          actor->direction.x * velocity * delta_time,
                     .y = actor->current_position.y +
                          actor->direction.y * velocity * delta_time};
    break;
  }
  case TALKING: {
    AnimationData *talking = actor->animations[TALKING];
    // Cue-driven mouth: pick the frame for the active cue. The animation is
    // never "playing" in this mode, so animation_update leaves the frame
    // alone and no end callback is ever armed.
    if (actor->talking_cues != NULL && talking != NULL) {
      talking->current_frame =
          lipsync_shape_at(actor->talking_cues, now - actor->started_talking_at,
                           &actor->cue_cursor);
    }
    if (now - actor->started_talking_at >= actor->talking_duration) {
      if (actor->talking_cues != NULL) {
        if (talking != NULL) {
          talking->current_frame = MOUTH_X;
        }
        actor->talking_cues = NULL;
      } else if (talking != NULL) {
        stop_animation(talking);
      }
      enter_idle(actor);
    }
    break;
  }
  }
}

void actor_render_with_shadow(Actor *actor, SDL_Renderer *renderer) {
  // Shadow first: it lies on the ground, so where the two overlap — a small
  // lift, her feet close to the mark — she belongs in front of it.
  if (actor_shadow_visible(actor)) {
    actor_render_shadow(actor, renderer);
  }
  actor_render(actor, renderer);
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

  // IDLE renders the actor's idle animation (e.g. the fox's sitting sprite);
  // FIDGETING renders the active fidget (they live outside the state table).
  ActorState state =
      actor->state == IDLE ? actor->spec->idle_state : actor->state;
  AnimationData *animation = actor->state == FIDGETING
                                 ? actor->fidget_anims[actor->active_fidget]
                                 : actor->animations[state];
  if (animation == NULL) {
    animation = actor->animations[actor->spec->idle_state];
  }
  if (animation == NULL) {
    animation = reference;
  }
  // Scale about the ground-contact point, so she keeps her feet planted as she
  // shrinks with depth and each state's frame keeps its offset relative to the
  // reference (the fox's sitting sheet is taller than her walking one and hangs
  // below the feet line; that overhang scales with her rather than snapping).
  // At scale 1 this is exactly the unscaled draw.
  SDL_Point anchor = {(int)actor->current_position.x, (int)actor_feet_y(actor)};
  render_animation_scaled_about(renderer, animation, position,
                                actor_scale(actor), anchor);
}

void actor_render_carried(const Actor *actor, SDL_Renderer *renderer,
                          const ImageData *image, SDL_Point offset) {
  SDL_Point at = {(int)actor->current_position.x + offset.x,
                  (int)actor->current_position.y + offset.y};
  // The same anchor and scale her sprite is drawn with, so the item keeps its
  // place on her body and its size relative to her at any depth. At scale 1
  // this is exactly render_image at `at`.
  SDL_Point anchor = {(int)actor->current_position.x, (int)actor_feet_y(actor)};
  render_image_scaled_about(renderer, image, at, actor_scale(actor), anchor);
}

void actor_free(Actor *actor) {
  for (int i = 0; i < ACTOR_STATE_COUNT; i++) {
    if (actor->animations[i]) {
      free_animation(actor->animations[i]);
    }
  }
  for (int i = 0; i < ACTOR_MAX_FIDGETS; i++) {
    if (actor->fidget_anims[i]) {
      free_animation(actor->fidget_anims[i]);
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
  // No walking while talking, held by the pointer, or mid-air. A LANDING
  // beat may be interrupted (she is on the ground; its one-shot just ticks
  // out silently).
  if (actor->state == TALKING || actor->state == DRAGGED ||
      actor->state == FALLING) {
    return;
  }
  // A tap always wins over a fidget.
  stop_fidget(actor);

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
  // A talking head mid-air would hang there: no lines while held or falling.
  if (actor->state == WALKING || actor->state == DRAGGED ||
      actor->state == FALLING) {
    return;
  }
  // A line always wins over a fidget.
  stop_fidget(actor);

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
    // Read-along overlay (SPEECH.md Part 3). Shown per the subtitles setting;
    // a line with no audio at all is always shown — the text is all there is.
    subtitle_show(line, dialog != NULL ? &dialog->words : NULL,
                  talking_duration, chunk == NULL);
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
  AnimationData *talking = actor->animations[TALKING];
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
  actor->started_talking_at = clock_now_ms();
}

bool actor_play_state(Actor *actor, ActorState state) {
  AnimationData *animation = actor->animations[state];
  if (animation == NULL) {
    return false;
  }
  stop_fidget(actor);
  play_animation(animation, NULL);
  actor->state = state;
  return true;
}

SDL_Rect actor_sprite_rect(const Actor *actor) {
  // reference_animation doesn't mutate; it just returns a mutable animation.
  AnimationData *reference = reference_animation((Actor *)actor);
  if (reference == NULL) {
    return (SDL_Rect){(int)actor->current_position.x,
                      (int)actor->current_position.y, 0, 0};
  }
  int w = reference->sprite_clips[0].w;
  int h = reference->sprite_clips[0].h;
  // The same natural origin, scale and ground anchor actor_render draws with,
  // so the grab box tracks the sprite instead of drifting off it by
  // (1 - scale) * h / 2. No render offset here: clicks arrive already
  // converted to scene coordinates.
  SDL_Point natural = {(int)actor->current_position.x - w / 2,
                       (int)actor->current_position.y - h / 2};
  SDL_Point anchor = {(int)actor->current_position.x, (int)actor_feet_y(actor)};
  float scale = actor_scale(actor);
  SDL_Rect rect = {
      (int)((float)anchor.x + (float)(natural.x - anchor.x) * scale),
      (int)((float)anchor.y + (float)(natural.y - anchor.y) * scale),
      (int)((float)w * scale), (int)((float)h * scale)};
  // Never let the target shrink below the natural size, though. Drag exists
  // because toddlers try it unprompted, so a grab box that got smaller with
  // depth would undercut the whole feature. Grow it about its own centre.
  if (rect.w < w) {
    rect.x -= (w - rect.w) / 2;
    rect.w = w;
  }
  if (rect.h < h) {
    rect.y -= (h - rect.h) / 2;
    rect.h = h;
  }
  return rect;
}

bool actor_begin_drag(Actor *actor) {
  if (actor->state == TALKING) {
    return false;
  }
  // Interrupt whatever she was doing: a fidget stops, a walk is cancelled
  // (dropping its callback, like any interrupted walk), a fall or landing
  // beat is caught mid-air. The airborne animations were played with no
  // callback, so stopping them is silent.
  stop_fidget(actor);
  cancel_walk(actor);
  AnimationData *falling = actor->animations[FALLING];
  if (falling != NULL) {
    stop_animation(falling);
  }
  AnimationData *landing = actor->animations[LANDING];
  if (landing != NULL) {
    stop_animation(landing);
  }
  AnimationData *dragged = actor->animations[DRAGGED];
  if (dragged != NULL) {
    play_animation(dragged, NULL);
  }
  // Remember the ground she came from, and start the gesture at whatever
  // height she already had. Catching her mid-fall would otherwise yank the
  // shadow up to the ceiling and rescale her on this very frame — the one case
  // this function exists for.
  float feet = actor_feet_y(actor);
  // fall_target_y is where her *centre* is headed; the ground fields are
  // ground lines, so it needs converting.
  float ground = (actor->state == FALLING || actor->state == LANDING)
                     ? actor->fall_target_y + actor_feet_offset(actor)
                     : feet;
  actor->grab_ground_y = ground;
  actor->ground_y = ground;
  actor->ground_target_y = ground;
  float initial_lift = ground - feet;
  float ceiling = actor_lift_ceiling(actor);
  actor->lift_ceiling = initial_lift > ceiling ? initial_lift : ceiling;
  actor->state = DRAGGED;
  return true;
}

void actor_drag_move(Actor *actor, SDL_FPoint pointer) {
  if (actor->state != DRAGGED) {
    return;
  }
  // The grab point stays under the pointer: the offset was taken at the
  // arming press, so she doesn't snap by half a sprite.
  actor->current_position.x = pointer.x + actor->drag_offset.x;
  actor->current_position.y = pointer.y + actor->drag_offset.y;
}

void actor_drop(Actor *actor, SDL_FPoint target) {
  if (actor->state != DRAGGED) {
    return;
  }
  AnimationData *dragged = actor->animations[DRAGGED];
  if (dragged != NULL) {
    stop_animation(dragged);
  }
  // The column scan preserves x; the nearest-ground fallback may nudge it.
  actor->current_position.x = target.x;
  // The landing is her depth from here on, so the fall and the landing beat
  // keep the size she already had while held — release changes nothing.
  // target is where her centre comes to rest; the ground fields are the ground
  // line she comes to rest *on*.
  actor->fall_target_y = target.y;
  actor->ground_y = target.y + actor_feet_offset(actor);
  actor->ground_target_y = actor->ground_y;
  if (target.y > actor->current_position.y + ACTOR_ARRIVE_EPSILON) {
    AnimationData *falling = actor->animations[FALLING];
    if (falling != NULL) {
      play_animation(falling, NULL);
    }
    actor->state = FALLING;
  } else {
    // Landing at or above the drop: place her there — a short hop, never an
    // upward "fall".
    actor->current_position.y = target.y;
    touch_down(actor);
  }
}
