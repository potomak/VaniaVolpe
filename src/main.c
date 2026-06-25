#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "game.h"
#include "hub.h"
#include "image.h"
#include "vania_fox_the_slide.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Global variables
int last_frame_time = 0;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

// Function to initialize our SDL window
int init_window(void) {
  // Initialize only the subsystems the game needs. SDL_INIT_EVERYTHING also
  // pulls in haptic/joystick/sensor, which Emscripten's SDL2 port does not
  // implement, causing SDL_Init to fail in the browser.
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    fprintf(stderr, "Error initializing SDL.\n");
    return false;
  }

  // Set texture filtering to linear
  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
    fprintf(stderr, "Warning: Linear texture filtering not enabled!");
  }

  window = SDL_CreateWindow("Vania Volpe - Lo Scivolo", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT,
                            SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) {
    fprintf(stderr, "Error creating SDL Window.\n");
    return false;
  }
  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    fprintf(stderr, "Error creating SDL Renderer.\n");
    return false;
  }

  // Scale
  SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

  // Initialize renderer color
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

  // Initialize PNG loading
  int imgFlags = IMG_INIT_PNG;
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    fprintf(stderr, "SDL_image could not initialize! SDL_image Error: %s\n",
            IMG_GetError());
    return false;
  }

  // Initialize SDL_mixer
  if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT,
                    MIX_DEFAULT_CHANNELS, AUDIO_CHUNK_SIZE) < 0) {
    fprintf(stderr, "SDL_mixer could not initialize! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }
  // Lower the music volume
  Mix_VolumeMusic(30);

  return true;
}

// Function to poll SDL events and process keyboard input
void process_input(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    game_process_input(&event);

    switch (event.type) {
    case SDL_QUIT:
      game.is_running = false;
      break;
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      // Exit the game
      case SDLK_ESCAPE:
        game.is_running = false;
        break;
      }
    }
  }
}

// Update function with a fixed time step
void update(void) {
  // Get delta_time factor converted to seconds to be used to update objects
  float delta_time = (SDL_GetTicks() - last_frame_time) / 1000.0;

  // Store the milliseconds of the current frame to be used in the next one
  last_frame_time = SDL_GetTicks();

  game_update(delta_time);
}

// Render function to draw game objects in the SDL window
void render(void) {
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderClear(renderer);

  game_render(renderer);

  // Update screen
  SDL_RenderPresent(renderer);
}

// Function to destroy SDL window and renderer
void destroy_window(void) {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void destroy_image(void) { IMG_Quit(); }

void destroy_sound(void) { Mix_Quit(); }

// One iteration of the game loop, shared by the native and web entry points.
static void main_loop(void) {
  process_input();
  update();
  render();
#ifdef __EMSCRIPTEN__
  if (!game.is_running) {
    emscripten_cancel_main_loop();
  }
#endif
}

// Main function
int SDL_main(int argc, char *argv[]) {
  game.is_running = init_window();
  if (!game.is_running) {
    fprintf(stderr, "Failed to initialize window!\n");
  }

  // Build each adventure's scene table before initializing scenes.
  vania_fox_the_slide_register();
  static const Adventure *content[] = {&vania_fox_the_slide};
  hub_register(content, 1);

  // Register the hub first (it is the start screen and the back-to-hub target),
  // then the content adventures. All of them are initialized and loaded up front.
  static const Adventure *all[] = {&hub, &vania_fox_the_slide};
  register_adventures(all, 2);
  set_current_adventure(&hub);

  game_init();

  if (game.is_running) {
    game.is_running = game_load_media(renderer);
  }
  if (!game.is_running) {
    fprintf(stderr, "Failed to load media!\n");
  }

  // Hack to execute lifecycle callbacks for the first scene
  set_active_scene(hub.entry_scene);

#ifdef __EMSCRIPTEN__
  // The browser owns the event loop; drive the game via requestAnimationFrame
  // (fps = 0) and never return (simulate_infinite_loop = 1).
  emscripten_set_main_loop(main_loop, 0, 1);
#else
  while (game.is_running) {
    main_loop();
  }

  game_deinit();

  destroy_window();
  destroy_image();
  destroy_sound();
#endif

  return 0;
}

/* Include the SDL main definition header */
#include <SDL2/SDL_main.h>

#if defined(__IPHONEOS__) || defined(__TVOS__)

#ifndef SDL_MAIN_HANDLED
#ifdef main
#undef main
#endif

int main(int argc, char *argv[]) {
  return SDL_UIKitRunApp(argc, argv, SDL_main);
}

#endif /* !SDL_MAIN_HANDLED */

#else

int main(int argc, char *argv[]) { return SDL_main(argc, argv); }

#endif /* __IPHONEOS__ || __TVOS__ */
