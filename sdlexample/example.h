//
//  example.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#ifndef example_h
#define example_h

// Declare two game objects for the ball and the paddle
struct game_object {
  float x;
  float y;
  float width;
  float height;
  float vel_x;
  float vel_y;
};

void example_init(void);

bool example_load_media(SDL_Renderer *renderer);

void example_process_input(SDL_Event *event);

void example_update(float delta_time);

void example_render(SDL_Renderer *renderer);

void example_deinit(void);

#endif /* example_h */
