//
//  fox.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/17/25.
//

#ifndef fox_h
#define fox_h

#define FOX_X_VELOCITY = 1
#define FOX_Y_VELOCITY = 1

#import "image.h"

typedef enum horizontal_orientation {
  WEST = -1,
  EAST = 1,
} HorizontalOrientation;

typedef struct fox {
  AnimationData *animation;
  SDL_FPoint current_position;
  SDL_FPoint target_position;
  SDL_FPoint direction;
  HorizontalOrientation horizontal_orientation;
  bool is_walking;
  bool has_key;
} Fox;

Fox *make_fox(SDL_FPoint initial_position);

bool fox_load_media(Fox *fox, SDL_Renderer *renderer);

void fox_update(Fox *fox, float delta_time);

void fox_render(Fox *fox, SDL_Renderer *renderer);

void fox_free(Fox *fox);

void fox_walk_to(Fox *fox, SDL_FPoint position);

#endif /* fox_h */
