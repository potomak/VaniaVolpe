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

#include "actor.h"
#include "image.h"
#include "sound.h"
#include "walk.h"

// A prop the actor can walk behind or in front of (DEPTH_AND_CAMERA.md
// Phase 1). Props live in scene-local tables and are drawn, y-sorted with the
// actors, by render_action_layer.
typedef struct prop {
  // Exactly one of image / animation is non-NULL. Both point into the scene's
  // existing images/animations tables, so loading/freeing is unchanged.
  ImageData *image;
  AnimationData *animation;
  SDL_Point pos; // top-left, scene coordinates
  // Scene y of the ground-contact line; the actor renders behind the prop
  // while actor_feet_y < baseline.
  int baseline;
  bool visible; // scenes toggle this (e.g. a picked-up item)
} Prop;

// Most drawables render_action_layer can sort in one call.
#define ACTION_LAYER_MAX 16

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
  // Scenes fill it once in init — from a committed walkable.walk mask when
  // one exists, else from their WalkArea rects (walk_grid_init). The debug
  // overlay shades its non-walkable cells, and its paint mode (see TOOLS.md)
  // edits the grid live, which is why the pointer is mutable.
  WalkGrid *walk_grid;

  // Asset directory the scene's walkable.walk mask lives in (under common/),
  // e.g. "playground"; NULL disables mask loading and the paint-mode save.
  const char *walk_mask_dir;

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

// Back-to-front draw order of the action layer: visible props and actors
// sorted by ascending baseline / feet y. On ties props draw first, so an
// actor standing exactly on a prop's ground line renders in front of it.
// out_order receives drawable indices: 0..props_length-1 name props,
// props_length + i names actors[i]. Returns how many entries were written.
// Split from render_action_layer so tests can assert the ordering without a
// renderer.
int action_layer_order(const Prop *props, int props_length,
                       Actor *const *actors, int actors_length,
                       int out_order[ACTION_LAYER_MAX]);

// Draw visible props and actors in y-order (see action_layer_order). Scenes
// call this once instead of hand-ordering "props, then the actor last";
// backgrounds and draws that never overlap an actor stay manual.
void render_action_layer(SDL_Renderer *renderer, Prop *props, int props_length,
                         Actor **actors, int actors_length);

bool load_scene_images(Scene *scene, SDL_Renderer *renderer);

bool load_scene_chunks(Scene *scene);

// Advance the scene's declared animations (called once per frame by the
// engine).
void update_scene_animations(Scene scene, int now_ms);

void free_scene_images(Scene *scene);

void free_scene_chunks(Scene *scene);

void free_scene_animations(Scene *scene);

#endif /* scene_h */
