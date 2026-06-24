//
//  game.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include "game.h"

// Features for debugging the game
#include "debug.h"

Game game = {
    .is_running = false,
    .is_debugging = false,
};

void set_current_adventure(const Adventure *adventure) {
  game.current_adventure = adventure;
  game.current_scene = adventure->entry_scene;
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
  for (int i = 0; i < game.current_adventure->scenes_length; i++) {
    scene_instance(i).init();
  }
}

bool game_load_media(SDL_Renderer *renderer) {
  for (int i = 0; i < game.current_adventure->scenes_length; i++) {
    if (!scene_instance(i).load_media(renderer)) {
      return false;
    }

    if (!load_scene_images(scene_instance(i), renderer)) {
      return false;
    }

    if (!load_scene_chunks(scene_instance(i))) {
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

  if (game.is_debugging) {
    debug_process_input(event);
  }

  scene_instance(game.current_scene).process_input(event);
}

void game_update(float delta_time) {
  scene_instance(game.current_scene).update(delta_time);
}

void game_render(SDL_Renderer *renderer) {
  scene_instance(game.current_scene).render(renderer);

  if (game.is_debugging) {
    debug_render(renderer);
  }
}

void game_deinit(void) {
  for (int i = 0; i < game.current_adventure->scenes_length; i++) {
    scene_instance(i).deinit();
    free_scene_images(scene_instance(i));
    free_scene_chunks(scene_instance(i));
  }
}
