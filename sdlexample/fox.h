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
  SITTING,
  WAVING,
} FoxState;

typedef struct fox {
  AnimationData *walking;
  Mix_Chunk *walking_sound;
  int walking_sound_channel;
  AnimationData *talking;
  AnimationData *sitting;
  AnimationData *waving;
  SDL_FPoint current_position;
  SDL_FPoint target_position;
  SDL_FPoint direction;
  HorizontalOrientation horizontal_orientation;
  FoxState state;
  Uint32 started_talking_at;
  Uint32 talking_duration;
  // Inventory
  bool has_key;
  bool has_peg;
  bool has_acorns;
} Fox;

Fox *make_fox(SDL_FPoint initial_position);

bool fox_load_media(Fox *fox, SDL_Renderer *renderer);

void fox_update(Fox *fox, float delta_time);

void fox_render(Fox *fox, SDL_Renderer *renderer);

void fox_free(Fox *fox);

void fox_walk_to(Fox *fox, SDL_FPoint position, void (*on_end)(void));

void fox_talk(Fox *fox, Mix_Chunk *dialog);

void fox_sit(Fox *fox);

void fox_wave(Fox *fox);

#endif /* fox_h */
