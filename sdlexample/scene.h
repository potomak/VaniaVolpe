//
//  scene.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#ifndef scene_h
#define scene_h

typedef struct scene {
  void (*init)(void);
  bool (*load_media)(SDL_Renderer *renderer);
  void (*process_input)(SDL_Event *event);
  void (*update)(float delta_time);
  void (*render)(SDL_Renderer *renderer);
  void (*deinit)(void);
} Scene;

#endif /* scene_h */
