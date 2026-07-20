//
//  game.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include "game.h"

// Asset path resolution (adventure assets root)
#include "asset.h"
#include "constants.h"
// Features for debugging the game
#include "debug.h"
// Dialogue text overlay (SPEECH.md Part 3)
#include "subtitle.h"

Game game = {
    .is_running = false,
    .is_debugging = false,
};

// Every registered adventure (so init/load/deinit cover all of them up front)
// and the hub, which is the first registered adventure.
static const Adventure **adventures = NULL;
static int adventures_count = 0;
static const Adventure *hub_adventure = NULL;

// Engine-level "back to hub" button, drawn over any non-hub adventure.
static const SDL_Rect HUB_BUTTON = {WINDOW_WIDTH - 48, 8, 40, 40};

void register_adventures(const Adventure *hub, const Adventure **registered,
                         int count) {
  adventures = registered;
  adventures_count = count;
  hub_adventure = hub;
}

// A newly active scene's camera snaps straight onto its target (no easing
// across the scene on entry). After on_scene_active, which may reposition the
// followed actor.
static void snap_scene_camera(void) {
  Camera *camera = scene_instance(game.current_scene)->camera;
  if (camera != NULL) {
    camera_snap(camera);
  }
}

void adventure_switch_to(const Adventure *adventure) {
  if (game.current_adventure != NULL) {
    const Scene *previous = scene_instance(game.current_scene);
    previous->on_scene_inactive();
    scene_stop_music(previous);
  }
  // A line spoken in the old adventure must not linger over the new one.
  subtitle_clear();
  game.current_adventure = adventure;
  game.current_scene = adventure->entry_scene;
  asset_set_root(adventure->assets_root);
  if (adventure->on_enter != NULL) {
    adventure->on_enter();
  }
  const Scene *current = scene_instance(game.current_scene);
  current->on_scene_active();
  scene_start_music(current);
  snap_scene_camera();
}

const Scene *scene_instance(int scene) {
  SDL_assert(game.current_adventure != NULL);
  SDL_assert(scene >= 0 && scene < game.current_adventure->scenes_length);
  return &game.current_adventure->scenes[scene];
}

// Sets a new scene as the current scene
void set_active_scene(int scene) {
  const Scene *previous = scene_instance(game.current_scene);
  previous->on_scene_inactive();
  scene_stop_music(previous);
  subtitle_clear();
  game.current_scene = scene;
  const Scene *current = scene_instance(game.current_scene);
  current->on_scene_active();
  scene_start_music(current);
  snap_scene_camera();
}

int sfx_play(int index) {
  const Adventure *adventure = game.current_adventure;
  SDL_assert(adventure != NULL);
  SDL_assert(index >= 0 && index < adventure->sfx_length);
  return Mix_PlayChannel(-1, adventure->sfx[index].chunk, 0);
}

void exit_game(void) { game.is_running = false; }

void game_init(void) {
  for (int a = 0; a < adventures_count; a++) {
    adventure_init(adventures[a]);
  }
}

bool game_load_media(SDL_Renderer *renderer) {
  for (int a = 0; a < adventures_count; a++) {
    if (!adventure_load_media(adventures[a], renderer)) {
      return false;
    }
  }
  return true;
}

// Process input for scenes
void game_process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_KEYDOWN:
    switch (event->key.keysym.sym) {
    // Toggle debugging features
    case SDLK_d:
      game.is_debugging = !game.is_debugging;
      break;
    }
    break;
  }

  // The back-to-hub button takes priority over scene input, except in the
  // hub. It lives in screen space, so it is tested before any camera
  // conversion.
  if (event->type == SDL_MOUSEBUTTONDOWN && hub_adventure != NULL &&
      game.current_adventure != hub_adventure) {
    SDL_Point point = {event->button.x, event->button.y};
    if (SDL_PointInRect(&point, &HUB_BUTTON)) {
      adventure_switch_to(hub_adventure);
      return;
    }
  }

  // Scrolling scenes get their mouse events converted to scene coordinates in
  // place, so every hotspot test, POI walk and debug rect downstream operates
  // in scene coordinates without knowing a camera exists (R4). The cast
  // matches the render offset's, so input and drawing can't disagree.
  const Camera *camera = scene_instance(game.current_scene)->camera;
  if (camera != NULL) {
    switch (event->type) {
    case SDL_MOUSEMOTION:
      event->motion.x += (int)camera->pos.x;
      event->motion.y += (int)camera->pos.y;
      break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      event->button.x += (int)camera->pos.x;
      event->button.y += (int)camera->pos.y;
      break;
    }
  }

  // The debug layer may consume the event (walk-paint mode swallows mouse
  // input so painting doesn't walk the actor).
  if (game.is_debugging && debug_process_input(event)) {
    return;
  }

  scene_instance(game.current_scene)->process_input(event);
}

void game_update(float delta_time) {
  // Match the hotspot active animations to their enabled state, then advance
  // the active scene's animations (which ticks any now playing) before its own
  // update. A ONE_SHOT end callback fired here may switch scene, so re-fetch
  // the current scene for the update() call (same re-entrancy as a scene switch
  // from process_input).
  sync_hotspot_active_anims(scene_instance(game.current_scene));
  update_scene_animations(*scene_instance(game.current_scene), SDL_GetTicks());
  scene_instance(game.current_scene)->update(delta_time);

  // The camera eases after the scene has moved its actor, so it follows this
  // frame's position. Re-fetch: the update may have switched scene (a fresh
  // scene's camera was already snapped by the switch).
  Camera *camera = scene_instance(game.current_scene)->camera;
  if (camera != NULL) {
    camera_update(camera, delta_time);
  }
}

void game_render(SDL_Renderer *renderer) {
  const Scene *scene = scene_instance(game.current_scene);
  const Camera *camera = scene->camera;
  // Scene content (and the debug overlay over it) draws shifted by the
  // camera; the cast to int happens once so every draw shares the same
  // offset. Parallax planes and screen-space UI carry their own offsets, so
  // they draw with the render offset reset to zero.
  const SDL_Point camera_offset =
      camera != NULL ? (SDL_Point){-(int)camera->pos.x, -(int)camera->pos.y}
                     : (SDL_Point){0, 0};

  // Background planes (behind the action layer), each at its own parallax.
  render_scene_planes(renderer, scene->bg_planes, scene->bg_planes_length,
                      camera);

  // The static sprite layer then the action layer, both in scene coordinates.
  // The framework draws the declared sprites first; the scene's own render
  // draws the dynamic action layer (the actor, tweens, overlays) on top.
  render_set_offset(camera_offset);
  render_scene_sprites(renderer, scene->sprites, scene->sprites_length);
  render_hotspot_anims(renderer, scene);
  scene->render(renderer);
  render_set_offset((SDL_Point){0, 0});

  // Foreground planes (in front of the action layer): a parallax > 1 strip
  // is a cheap walk-behind with no prop needed.
  render_scene_planes(renderer, scene->fg_planes, scene->fg_planes_length,
                      camera);

  // The debug overlay draws over everything, in scene coordinates.
  if (game.is_debugging) {
    render_set_offset(camera_offset);
    debug_render(renderer);
    render_set_offset((SDL_Point){0, 0});
  }

  // Draw the back-to-hub button over any non-hub adventure (screen space).
  if (hub_adventure != NULL && game.current_adventure != hub_adventure) {
    SDL_SetRenderDrawColor(renderer, 0x33, 0x33, 0x33, 0xFF);
    SDL_RenderFillRect(renderer, &HUB_BUTTON);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderDrawRect(renderer, &HUB_BUTTON);
  }

  // The dialogue text overlay is screen-space UI, over everything.
  subtitle_render(renderer);
}

void game_deinit(void) {
  for (int a = 0; a < adventures_count; a++) {
    adventure_deinit(adventures[a]);
  }
}
