//
//  camera.c
//  Follow / clamp / smoothing for the scrolling-scene camera (see camera.h).
//

#include "camera.h"
#include "constants.h"

// Where the camera wants to be: the view centred on the follow target,
// clamped to the scene. A fixed camera (follow == NULL) targets its current
// clamped position.
static SDL_FPoint camera_target(const Camera *camera) {
  SDL_FPoint target = camera->pos;
  if (camera->follow != NULL) {
    target.x = camera->follow->current_position.x - WINDOW_WIDTH / 2.0F;
    target.y = camera->follow->current_position.y - WINDOW_HEIGHT / 2.0F;
  }
  float max_x = (float)(camera->scene_size.x - WINDOW_WIDTH);
  float max_y = (float)(camera->scene_size.y - WINDOW_HEIGHT);
  target.x = SDL_clamp(target.x, 0.0F, max_x);
  target.y = SDL_clamp(target.y, 0.0F, max_y);
  return target;
}

void camera_init(Camera *camera, SDL_Point scene_size, const Actor *follow) {
  SDL_assert(scene_size.x >= WINDOW_WIDTH && scene_size.x <= MAX_SCENE_W);
  SDL_assert(scene_size.y >= WINDOW_HEIGHT && scene_size.y <= MAX_SCENE_H);
  camera->scene_size = scene_size;
  camera->follow = follow;
  camera->pos = (SDL_FPoint){0, 0};
  camera_snap(camera);
}

void camera_snap(Camera *camera) { camera->pos = camera_target(camera); }

void camera_update(Camera *camera, float delta_time) {
  // Exponential ease toward the target; the min(1, ...) guard means a huge
  // delta_time steps exactly onto the target instead of overshooting it.
  SDL_FPoint target = camera_target(camera);
  float t = CAMERA_SMOOTHING * delta_time;
  if (t > 1.0F) {
    t = 1.0F;
  }
  camera->pos.x += (target.x - camera->pos.x) * t;
  camera->pos.y += (target.y - camera->pos.y) * t;
}

SDL_Point camera_scene_of(const Camera *camera, SDL_Point screen_pt) {
  return (SDL_Point){screen_pt.x + (int)camera->pos.x,
                     screen_pt.y + (int)camera->pos.y};
}
