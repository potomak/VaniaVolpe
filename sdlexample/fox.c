//
//  fox.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/17/25.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "image.h"
#include "sound.h"

#include "fox.h"

static void (*on_end_walking)(void);

Fox *make_fox(SDL_FPoint initial_position) {
  Fox *fox = malloc(sizeof(Fox));
  fox->walking = make_animation_data(4, LOOP);
  // Change default speed multiplier to make the animation slower
  fox->walking->speed_multiplier = 1. / 6;
  fox->talking = make_animation_data(3, LOOP);
  fox->current_position = initial_position;
  fox->target_position = initial_position;
  fox->direction = (SDL_FPoint){0, 0};
  fox->horizontal_orientation = WEST;
  fox->state = IDLE;
  fox->started_talking_at = 0;
  fox->talking_duration = 0;
  fox->has_key = false;
  fox->has_peg = false;
  fox->has_acorns = false;
  return fox;
}

bool fox_load_media(Fox *fox, SDL_Renderer *renderer) {
  if (!load_animation(renderer, fox->walking, "fox/walking.png",
                      "fox/walking.anim")) {
    fprintf(stderr, "Failed to load fox walking!\n");
    return false;
  }

  if (!load_animation(renderer, fox->talking, "fox/talking.png",
                      "fox/talking.anim")) {
    fprintf(stderr, "Failed to load fox talking!\n");
    return false;
  }

  fox->walking_sound = Mix_LoadWAV("fox/walking.wav");
  if (fox->walking_sound == NULL) {
    fprintf(stderr, "Failed to load low sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }
  // Lower the sample volume
  fox->walking_sound->volume = 20;

  return true;
}

void fox_update(Fox *fox, float delta_time) {
  float dx = fox->target_position.x - fox->current_position.x;
  float dy = fox->target_position.y - fox->current_position.y;
  float vel = 2.2;
  float ticks = SDL_GetTicks();

  switch (fox->horizontal_orientation) {
  case EAST:
    fox->walking->flip = SDL_FLIP_HORIZONTAL;
    fox->talking->flip = SDL_FLIP_HORIZONTAL;
    break;
  case WEST:
    fox->walking->flip = SDL_FLIP_NONE;
    fox->talking->flip = SDL_FLIP_NONE;
    break;
  }

  switch (fox->state) {
  case IDLE:
    break;
  case WALKING:
    // Stop walking after reaching the target position
    if (fabsf(dx) <= 2 && fabsf(dy) <= 2) {
      stop_animation(fox->walking);
      fox->state = IDLE;
      fox->direction = (SDL_FPoint){0, 0};
      fox->current_position = fox->target_position;
      Mix_HaltChannel(fox->walking_sound_channel);
      if (on_end_walking != NULL) {
        on_end_walking();
      }
      return;
    }

    fox->current_position =
        (SDL_FPoint){.x = fox->current_position.x + fox->direction.x * vel,
                     .y = fox->current_position.y + fox->direction.y * vel};
    break;
  case TALKING:
    if (ticks - fox->started_talking_at >= fox->talking_duration) {
      stop_animation(fox->talking);
      fox->state = IDLE;
    }
    break;
  }
}

void fox_render(Fox *fox, SDL_Renderer *renderer) {
  // Assumes that all animation frames have the same size
  SDL_Point position = (SDL_Point){
      .x = fox->current_position.x - fox->walking->sprite_clips[0].w / 2,
      .y = fox->current_position.y - fox->walking->sprite_clips[0].h / 2};

  switch (fox->state) {
  case IDLE:
  case WALKING:
    render_animation(renderer, fox->walking, position);
    break;
  case TALKING:
    render_animation(renderer, fox->talking, position);
    break;
  }
}

void fox_free(Fox *fox) {
  free_animation(fox->walking);
  free_animation(fox->talking);
  Mix_FreeChunk(fox->walking_sound);
  free(fox);
}

void fox_walk_to(Fox *fox, SDL_FPoint target_position, void (*on_end)(void)) {
  if (fox->state == TALKING) {
    return;
  }

  on_end_walking = on_end;

  if (fox->state != WALKING) {
    // Play walking sound
    fox->walking_sound_channel = Mix_PlayChannel(-1, fox->walking_sound, -1);
  }

  play_animation(fox->walking, NULL);
  fox->state = WALKING;
  fox->target_position = target_position;

  float dx = fox->target_position.x - fox->current_position.x;
  float dy = fox->target_position.y - fox->current_position.y;

  if (dx > 0) {
    fox->horizontal_orientation = EAST;
  } else {
    fox->horizontal_orientation = WEST;
  }

  float dist = sqrtf(powf(dx, 2) + powf(dy, 2));
  fox->direction = (SDL_FPoint){dx / dist, dy / dist};
}

void fox_talk(Fox *fox, Mix_Chunk *dialog) {
  if (fox->state == WALKING) {
    return;
  }

  Uint32 talking_duration = get_chunk_time_ms(dialog);
  fprintf(stdout, "Sound duration: %d\n", talking_duration);

  Mix_PlayChannel(-1, dialog, 0);
  play_animation(fox->talking, NULL);
  fox->state = TALKING;
  fox->talking_duration = talking_duration;
  fox->started_talking_at = SDL_GetTicks();
}
