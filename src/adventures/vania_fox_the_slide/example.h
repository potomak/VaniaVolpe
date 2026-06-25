//
//  example.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#ifndef example_h
#define example_h

#import "scene.h"

// Declare two game objects for the ball and the paddle
typedef struct game_object {
  float x;
  float y;
  float width;
  float height;
  float vel_x;
  float vel_y;
} GameObject;

extern Scene example_scene;

#endif /* example_h */
