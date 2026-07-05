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
#include "walk.h"

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

  // Walkability grid (see walk.h); NULL for scenes with no player movement.
  // Scenes build it once in init from their WalkArea rects; the debug overlay
  // shades its non-walkable cells.
  const WalkGrid *walk_grid;

  // Images
  ImageData *images;
  int images_length;

  // Chunks (sound effects)
  ChunkData *chunks;
  int chunks_length;

  // Animations the scene draws itself (buttons, props, …). The framework ticks
  // them each frame and frees them on teardown, so scenes only declare them —
  // they don't call animation_update. (An actor ticks its own animations.)
  AnimationData **animations;
  int animations_length;
} Scene;

bool load_scene_images(Scene *scene, SDL_Renderer *renderer);

bool load_scene_chunks(Scene *scene);

// Advance the scene's declared animations (called once per frame by the
// engine).
void update_scene_animations(Scene scene, int now_ms);

void free_scene_images(Scene *scene);

void free_scene_chunks(Scene *scene);

void free_scene_animations(Scene *scene);

#endif /* scene_h */
