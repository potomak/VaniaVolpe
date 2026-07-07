//
//  test_camera.c
//  Unit tests for the scrolling-scene camera (DEPTH_AND_CAMERA.md Phase 3):
//  snap centring, clamping at every scene edge, smoothing convergence, the
//  screen->scene transform, and a synthetic follow across a wide scene.
//  Everything is pure math — no window, renderer, or assets.
//

#include <stdio.h>

#include "actor.h"
#include "camera.h"
#include "constants.h"

#include "test_camera.h"

static int failures;

static void check(bool ok, const char *what) {
  if (ok) {
    fprintf(stderr, "OK    %s\n", what);
  } else {
    fprintf(stderr, "MISS  %s\n", what);
    failures++;
  }
}

// The camera only reads current_position, so a zeroed Actor is a fine target.
static Actor actor;

static const SDL_Point WIDE = {1600, 900};

int test_camera(void) {
  failures = 0;
  fprintf(stderr, "\n-- camera unit tests --\n");

  Camera camera;

  // ── snap centring + clamping at all four edges ────────────────────────────

  actor.current_position = (SDL_FPoint){800, 450};
  camera_init(&camera, WIDE, &actor);
  check(camera.pos.x == 400.0F && camera.pos.y == 150.0F,
        "init/snap centres the view on the actor");

  actor.current_position = (SDL_FPoint){0, 0};
  camera_snap(&camera);
  check(camera.pos.x == 0.0F && camera.pos.y == 0.0F,
        "snap clamps at the top-left scene corner");

  actor.current_position = (SDL_FPoint){1600, 900};
  camera_snap(&camera);
  check(camera.pos.x == 800.0F && camera.pos.y == 300.0F,
        "snap clamps at the bottom-right scene corner");

  // A window-sized scene can only ever show itself.
  actor.current_position = (SDL_FPoint){700, 100};
  camera_init(&camera, (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT}, &actor);
  check(camera.pos.x == 0.0F && camera.pos.y == 0.0F,
        "a window-sized scene pins the camera at the origin");

  // ── screen -> scene transform ─────────────────────────────────────────────

  actor.current_position = (SDL_FPoint){800, 450};
  camera_init(&camera, WIDE, &actor);
  SDL_Point scene_pt = camera_scene_of(&camera, (SDL_Point){10, 20});
  check(scene_pt.x == 410 && scene_pt.y == 170,
        "camera_scene_of shifts a screen point by the camera");

  // ── smoothing: monotone convergence, no overshoot ─────────────────────────

  actor.current_position = (SDL_FPoint){0, 0};
  camera_snap(&camera); // (0, 0)
  actor.current_position = (SDL_FPoint){800, 450};

  bool monotone = true;
  float previous = camera.pos.x;
  for (int i = 0; i < 200; i++) {
    camera_update(&camera, 1.0F / 30.0F);
    if (camera.pos.x < previous || camera.pos.x > 400.0F) {
      monotone = false;
      break;
    }
    previous = camera.pos.x;
  }
  check(monotone && camera.pos.x > 399.0F,
        "smoothing converges on the target without overshooting");

  camera_snap(&camera);
  camera.pos = (SDL_FPoint){0, 0};
  camera_update(&camera, 10.0F); // a huge frame: min(1, SMOOTHING * dt) == 1
  check(camera.pos.x == 400.0F && camera.pos.y == 150.0F,
        "a huge delta time lands exactly on the target");

  // ── synthetic follow across a wide, window-tall scene ─────────────────────

  actor.current_position = (SDL_FPoint){100, 400};
  camera_init(&camera, (SDL_Point){1600, WINDOW_HEIGHT}, &actor);
  bool follows = camera.pos.x == 0.0F && camera.pos.y == 0.0F;
  for (int x = 100; x <= 1500 && follows; x += 100) {
    actor.current_position.x = (float)x;
    for (int i = 0; i < 60; i++) {
      camera_update(&camera, 1.0F / 30.0F);
    }
    float want = SDL_clamp((float)x - WINDOW_WIDTH / 2.0F, 0.0F, 800.0F);
    follows = SDL_fabsf(camera.pos.x - want) < 1.0F && camera.pos.y == 0.0F;
  }
  check(follows, "the camera follows a walk across the scene, clamped");

  return failures;
}
