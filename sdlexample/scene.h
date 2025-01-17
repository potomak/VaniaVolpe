//
//  scene.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#ifndef scene_h
#define scene_h

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct scene {
  void (*init)(void);
  bool (*load_media)(SDL_Renderer *renderer);
  void (*process_input)(SDL_Event *event);
  void (*update)(float delta_time);
  void (*render)(SDL_Renderer *renderer);
  void (*deinit)(void);

  // Called before the scene is set as the current scene
  void (*on_scene_active)(void);
  // Called before the scene is unset as the current scene
  void (*on_scene_inactive)(void);

  // Hotspots in the scene
  SDL_Rect *hotspots;
  int hotspots_length;
} Scene;

#endif /* scene_h */
