//
//  camera.h
//  The scrolling-scene camera (DEPTH_AND_CAMERA.md Phase 3): the top-left of
//  the visible 800x600 window, in scene coordinates. Scenes larger than the
//  window own a Camera and hand it to the engine via their Scene struct; the
//  engine snaps it on scene entry, updates it after the scene's update, and
//  converts input and rendering through it — scene code never does camera
//  math. Every existing static scene simply has no camera.
//

#ifndef camera_h
#define camera_h

#include <SDL2/SDL.h>

#include "actor.h"

typedef struct camera {
  SDL_FPoint pos;       // top-left of the view, scene coordinates
  SDL_Point scene_size; // >= {WINDOW_WIDTH, WINDOW_HEIGHT}
  const Actor *follow;  // player actor to track; NULL = fixed camera
} Camera;

// Exponential follow rate, 1/s; higher snaps faster.
#define CAMERA_SMOOTHING 5.0F

void camera_init(Camera *camera, SDL_Point scene_size, const Actor *follow);

// Centre on the follow target immediately (scene entry; no smoothing).
void camera_snap(Camera *camera);

// Exponential follow + clamp: pos += (target - pos) * min(1, SMOOTHING * dt),
// where target centres the view on the actor; then clamp to scene bounds.
void camera_update(Camera *camera, float delta_time);

// The scene coordinate under a screen point — the same integer offset the
// engine applies to input events.
SDL_Point camera_scene_of(const Camera *camera, SDL_Point screen_pt);

#endif /* camera_h */
