#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "game.h"
#include "image.h"
#include "terminal.h"

static int last_frame_time = 0;
static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;

static int init_window(void) {
  // Must be set before SDL_Init so the video subsystem uses the offscreen
  // driver instead of trying to connect to a display server.
  SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);

  // Use the dummy audio driver so SDL_mixer can open a device and load/query
  // WAV files even when no real audio hardware is available. Sounds will be
  // silently discarded.
  SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "Error initializing SDL: %s\n", SDL_GetError());
    return false;
  }

  window = SDL_CreateWindow("Vania Volpe",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (!window) {
    fprintf(stderr, "Error creating SDL window: %s\n", SDL_GetError());
    return false;
  }

  // Software renderer — no GPU or display required.
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    fprintf(stderr, "Error creating SDL renderer: %s\n", SDL_GetError());
    return false;
  }

  SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

  // PNG loading
  int imgFlags = IMG_INIT_PNG;
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    fprintf(stderr, "SDL_image error: %s\n", IMG_GetError());
    return false;
  }

  if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT,
                    MIX_DEFAULT_CHANNELS, AUDIO_CHUNK_SIZE) < 0) {
    fprintf(stderr, "SDL_mixer error: %s\n", Mix_GetError());
    return false;
  }
  Mix_VolumeMusic(30);

  if (!terminal_init(renderer)) {
    fprintf(stderr, "Error initializing terminal backend.\n");
    return false;
  }

  return true;
}

static void process_input(void) {
  // Translate terminal events into SDL events first so they are available
  // to SDL_PollEvent in the same iteration.
  terminal_poll_events();

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    game_process_input(&event);

    switch (event.type) {
    case SDL_QUIT:
      game.is_running = false;
      break;
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE)
        game.is_running = false;
      break;
    }
  }
}

static void update(void) {
  float delta_time = (SDL_GetTicks() - last_frame_time) / 1000.0f;
  last_frame_time  = SDL_GetTicks();
  game_update(delta_time);
}

static void render(void) {
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderClear(renderer);

  game_render(renderer);

  // Read back pixels and push to the terminal via libcaca instead of
  // presenting to a real window.
  terminal_present(renderer);
}

static void destroy_window(void) {
  terminal_deinit();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

int main(int argc, char *argv[]) {
  (void)argc; (void)argv;

  game.is_running = init_window();
  if (!game.is_running) {
    fprintf(stderr, "Failed to initialize terminal window.\n");
    return 1;
  }

  game_init();

  if (game.is_running) {
    game.is_running = game_load_media(renderer);
  }
  if (!game.is_running) {
    fprintf(stderr, "Failed to load media.\n");
    return 1;
  }

  set_active_scene(INTRO);

  last_frame_time = SDL_GetTicks();
  while (game.is_running) {
    process_input();
    update();
    render();
  }

  game_deinit();
  destroy_window();
  Mix_Quit();
  IMG_Quit();

  return 0;
}
