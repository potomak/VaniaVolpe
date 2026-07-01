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

void register_adventures(const Adventure **registered, int count) {
  adventures = registered;
  adventures_count = count;
  hub_adventure = count > 0 ? registered[0] : NULL;
}

void set_current_adventure(const Adventure *adventure) {
  game.current_adventure = adventure;
  game.current_scene = adventure->entry_scene;
  asset_set_root(adventure->assets_root);
  if (adventure->on_enter != NULL) {
    adventure->on_enter();
  }
}

void switch_to_adventure(const Adventure *adventure) {
  if (game.current_adventure != NULL) {
    scene_instance(game.current_scene).on_scene_inactive();
  }
  game.current_adventure = adventure;
  game.current_scene = adventure->entry_scene;
  asset_set_root(adventure->assets_root);
  if (adventure->on_enter != NULL) {
    adventure->on_enter();
  }
  scene_instance(game.current_scene).on_scene_active();
}

Scene scene_instance(int scene) {
  return game.current_adventure->scenes[scene];
}

// Sets a new scene as the current scene
void set_active_scene(int scene) {
  scene_instance(game.current_scene).on_scene_inactive();
  scene_instance(scene).on_scene_active();
  game.current_scene = scene;
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

  // The back-to-hub button takes priority over scene input, except in the hub.
  if (event->type == SDL_MOUSEBUTTONDOWN && hub_adventure != NULL &&
      game.current_adventure != hub_adventure) {
    SDL_Point point = {event->button.x, event->button.y};
    if (SDL_PointInRect(&point, &HUB_BUTTON)) {
      switch_to_adventure(hub_adventure);
      return;
    }
  }

  if (game.is_debugging) {
    debug_process_input(event);
  }

  scene_instance(game.current_scene).process_input(event);
}

void game_update(float delta_time) {
  // Advance the active scene's animations before its own update. A ONE_SHOT end
  // callback fired here may switch scene, so re-fetch the current scene for the
  // update() call (same re-entrancy as a scene switch from process_input).
  update_scene_animations(scene_instance(game.current_scene), SDL_GetTicks());
  scene_instance(game.current_scene).update(delta_time);
}

void game_render(SDL_Renderer *renderer) {
  scene_instance(game.current_scene).render(renderer);

  // Draw the back-to-hub button over any non-hub adventure.
  if (hub_adventure != NULL && game.current_adventure != hub_adventure) {
    SDL_SetRenderDrawColor(renderer, 0x33, 0x33, 0x33, 0xFF);
    SDL_RenderFillRect(renderer, &HUB_BUTTON);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderDrawRect(renderer, &HUB_BUTTON);
  }

  if (game.is_debugging) {
    debug_render(renderer);
  }
}

void game_deinit(void) {
  for (int a = 0; a < adventures_count; a++) {
    adventure_deinit(adventures[a]);
  }
}
