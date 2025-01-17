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
  fox->current_position = initial_position;
  fox->target_position = initial_position;
  fox->direction = (SDL_Point){0, 0};
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

  fox->animation->sprite_clips[0].x = 0;
  fox->animation->sprite_clips[0].y = 0;
  fox->animation->sprite_clips[0].w = 211;
  fox->animation->sprite_clips[0].h = 163;

  fox->animation->sprite_clips[1].x = 0;
  fox->animation->sprite_clips[1].y = 163;
  fox->animation->sprite_clips[1].w = 211;
  fox->animation->sprite_clips[1].h = 163;

  fox->animation->sprite_clips[2].x = 0;
  fox->animation->sprite_clips[2].y = 326;
  fox->animation->sprite_clips[2].w = 211;
  fox->animation->sprite_clips[2].h = 163;

  fox->animation->sprite_clips[3].x = 0;
  fox->animation->sprite_clips[3].y = 489;
  fox->animation->sprite_clips[3].w = 211;
  fox->animation->sprite_clips[3].h = 163;

  return true;
}

void fox_update(Fox *fox, float delta_time) {
  if (fox->is_walking) {
    // TODO
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
  fox->target_position = target_position;
  // TODO: Hack
  fox->current_position = target_position;
  fox->is_walking = true;
}
