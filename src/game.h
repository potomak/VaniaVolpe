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

// Register every adventure that exists (init/load/deinit iterate all of them).
// `hub` is the back-to-hub target and must also appear in `registered` (it
// still needs its own init/load like any other adventure); it need not be
// registered[0]. The caller must keep both `hub` and the `registered` array
// alive for the whole run (game.c only retains the pointers) — pass static
// storage, as main.c does.
void register_adventures(const Adventure *hub, const Adventure **registered,
                         int count);

// Switch to another adventure: outgoing scene inactive (if one is active) ->
// new adventure/scene set -> adventure on_enter -> incoming entry scene
// active. Used both for the very first adventure at startup and later runtime
// switches (hub selection, back-to-hub). Safe to call from a scene's
// process_input, like set_active_scene.
void adventure_switch_to(const Adventure *adventure);

// The current adventure's scene at this index. Asserts current_adventure is
// set and the index is in range.
const Scene *scene_instance(int scene);

void set_active_scene(int scene);

// Play a sound effect from the current adventure's shared SFX bank
// (adventure.h) by its index, returning the channel it started on (or -1).
// Scenes don't call this directly — the generated play_<name>() helpers do,
// with a compile-checked <PREFIX>_SFX_<NAME> index (SCENES.md milestone 4).
int sfx_play(int index);

void exit_game(void);

void game_init(void);

bool game_load_media(SDL_Renderer *renderer);

void game_process_input(SDL_Event *event);

void game_update(float delta_time);

void game_render(SDL_Renderer *renderer);

void game_deinit(void);

#endif /* game_h */
