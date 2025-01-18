//
//  fox.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/17/25.
//

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "image.h"

#include "fox.h"

Fox *make_fox(SDL_FPoint initial_position) {
  Fox *fox = malloc(sizeof(Fox));
  fox->animation = make_animation_data(4, LOOP);
  // Change default speed multiplier to make the animation slower
  fox->animation->speed_multiplier = 1. / 6;
  fox->current_position = initial_position;
  fox->target_position = initial_position;
  fox->direction = (SDL_FPoint){0, 0};
  fox->horizontal_orientation = WEST;
  fox->is_walking = false;
  fox->has_key = false;
  return fox;
}

bool fox_load_media(Fox *fox, SDL_Renderer *renderer) {
  // TODO: Move asset to a different directory
  if (!load_from_file("playground_entrance/fox.png", renderer,
                      &fox->animation->image)) {
    fprintf(stderr, "Failed to texture!\n");
    return false;
  }

  if (!load_animation_data(fox->animation, "playground_entrance/fox.anim")) {
    return false;
  }

  return true;
}

void fox_update(Fox *fox, float delta_time) {
  if (fox->is_walking) {
    float dx = fox->target_position.x - fox->current_position.x;
    float dy = fox->target_position.y - fox->current_position.y;

    // Stop walking after reaching the target position
    if (fabsf(dx) <= 2 && fabsf(dy) <= 2) {
      stop_animation(fox->animation);
      fox->is_walking = false;
      fox->direction = (SDL_FPoint){0, 0};
      fox->current_position = fox->target_position;
      return;
    }

    float vel = 2.2;
    fox->current_position =
        (SDL_FPoint){.x = fox->current_position.x + fox->direction.x * vel,
                     .y = fox->current_position.y + fox->direction.y * vel};
  }
}

void fox_render(Fox *fox, SDL_Renderer *renderer) {
  SDL_Point position = (SDL_Point){
      .x = fox->current_position.x - fox->animation->sprite_clips[0].w / 2,
      .y = fox->current_position.y - fox->animation->sprite_clips[0].h / 2};
  render_animation(renderer, fox->animation, position);
}

void fox_free(Fox *fox) {
  free_animation(fox->animation);
  free(fox);
}

void fox_walk_to(Fox *fox, SDL_FPoint target_position) {
  play_animation(fox->animation);
  fox->is_walking = true;
  fox->target_position = target_position;

  float dx = fox->target_position.x - fox->current_position.x;
  float dy = fox->target_position.y - fox->current_position.y;

  if (dx > 0) {
    fox->horizontal_orientation = EAST;
    fox->animation->flip = SDL_FLIP_HORIZONTAL;
  } else {
    fox->horizontal_orientation = WEST;
    fox->animation->flip = SDL_FLIP_NONE;
  }

  float dist = sqrtf(powf(dx, 2) + powf(dy, 2));
  fox->direction = (SDL_FPoint){dx / dist, dy / dist};
}
