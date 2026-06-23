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

#include "image.h"
#include "sound.h"

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

  // Points of interest in the scene
  SDL_Point *pois;
  int pois_length;

  // Images
  ImageData *images;
  int images_length;

  // Chunks (sound effects)
  ChunkData *chunks;
  int chunks_length;
} Scene;

// Nearest point that is inside one of the walkable rects and outside the
// non-walkable rect, to the given click.
SDL_FPoint nearest_walkable_point(SDL_Point click, const SDL_Rect *walkables,
                                  int walkables_length, SDL_Rect non_walkable);

bool load_scene_images(Scene scene, SDL_Renderer *renderer);

bool load_scene_chunks(Scene scene);

void free_scene_images(Scene scene);

void free_scene_chunks(Scene scene);

#endif /* scene_h */
