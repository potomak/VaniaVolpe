//
//  game.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#ifndef game_h
#define game_h

#include "adventure.h"
#include "scene.h"

typedef struct game {
  bool is_running;
  bool is_debugging;
  const Adventure *current_adventure;
  int current_scene;
} Game;

extern Game game;

// Select the adventure to run (sets the current scene to its entry scene).
void set_current_adventure(const Adventure *adventure);

// Register every adventure that exists (init/load/deinit iterate all of them).
// The first entry is treated as the hub (the back-to-hub button returns to it).
void register_adventures(const Adventure **registered, int count);

// Transition to another adventure at runtime (e.g. hub selection, back-to-hub).
// Safe to call from a scene's process_input, like set_active_scene.
void switch_to_adventure(const Adventure *adventure);

Scene scene_instance(int scene);

void set_active_scene(int scene);

void exit_game(void);

void game_init(void);

bool game_load_media(SDL_Renderer *renderer);

void game_process_input(SDL_Event *event);

void game_update(float delta_time);

void game_render(SDL_Renderer *renderer);

void game_deinit(void);

#endif /* game_h */
