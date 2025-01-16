//
//  game.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

// Special scene used as a playground
#include "example.h"

// Scenes
#include "intro.h"

#include "game.h"

Game game = {
    .current_scene = INTRO,
};

static Scene current_scene() {
  switch (game.current_scene) {
  case INTRO:
    return intro_scene;

  case EXAMPLE:
    return example_scene;

  default:
    fprintf(stderr, "Warning: scene not defined!\n");
    break;
  }
}

void game_init(void) {
  example_scene.init();
  intro_scene.init();
}

bool game_load_media(SDL_Renderer *renderer) {
  if (!example_scene.load_media(renderer)) {
    fprintf(stderr, "Failed to initialize example!\n");
    return false;
  }

  if (!intro_scene.load_media(renderer)) {
    fprintf(stderr, "Failed to initialize intro!\n");
    return false;
  }

  return true;
}

// Process input for scenes
void game_process_input(SDL_Event *event) {
  current_scene().process_input(event);
}

void game_update(float delta_time) { current_scene().update(delta_time); }

void game_render(SDL_Renderer *renderer) { current_scene().render(renderer); }

void game_deinit(void) {
  intro_scene.deinit();
  example_scene.deinit();
}
