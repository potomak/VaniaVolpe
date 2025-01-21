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

typedef enum fox_state {
  IDLE,
  WALKING,
  TALKING,
} FoxState;

typedef struct fox {
  AnimationData *walking;
  AnimationData *talking;
  SDL_FPoint current_position;
  SDL_FPoint target_position;
  SDL_FPoint direction;
  HorizontalOrientation horizontal_orientation;
  FoxState state;
  Uint32 started_talking_at;
  Uint32 talking_duration;
  bool has_key;
} Fox;

Fox *make_fox(SDL_FPoint initial_position);

bool fox_load_media(Fox *fox, SDL_Renderer *renderer);

void fox_update(Fox *fox, float delta_time);

void fox_render(Fox *fox, SDL_Renderer *renderer);

void fox_free(Fox *fox);

void fox_walk_to(Fox *fox, SDL_FPoint position, void (*on_end)(void));

void fox_talk_for(Fox *fox, Uint32 talking_duration);

#endif /* fox_h */
