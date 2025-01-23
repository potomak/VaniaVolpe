//
//  game.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#ifndef game_h
#define game_h

#import "scene.h"

typedef enum game_scene {
  INTRO,
  PLAYGROUND_ENTRANCE,
  PLAYGROUND,
  OUTRO,
  EXAMPLE,
  // This should always be the last value in the enum
  // It is a hack to iterate over all the scenes
  SCENES_LENGTH,
} GameScene;

typedef struct game {
  bool is_running;
  bool is_debugging;
  GameScene current_scene;
} Game;

extern Game game;

Scene scene_instance(GameScene scene);

void set_active_scene(GameScene scene);

void exit_game(void);

void game_init(void);

bool game_load_media(SDL_Renderer *renderer);

void game_process_input(SDL_Event *event);

void game_update(float delta_time);

void game_render(SDL_Renderer *renderer);

void game_deinit(void);

#endif /* game_h */
