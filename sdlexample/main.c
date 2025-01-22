#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "game.h"
#include "image.h"

// Global variables
int last_frame_time = 0;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

// Function to initialize our SDL window
int init_window(void) {
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    fprintf(stderr, "Error initializing SDL.\n");
    return false;
  }

  // Set texture filtering to linear
  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
    fprintf(stderr, "Warning: Linear texture filtering not enabled!");
  }

  window = SDL_CreateWindow("A simple game loop using C & SDL",
                            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            WINDOW_WIDTH, WINDOW_HEIGHT, 0);
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

// Main function
int main(int argc, char *args[]) {
  game.is_running = init_window();
  if (!game.is_running) {
    fprintf(stderr, "Failed to initialize window!\n");
  }

  game_init();

  if (game.is_running) {
    game.is_running = game_load_media(renderer);
  }
  if (!game.is_running) {
    fprintf(stderr, "Failed to load media!\n");
  }

  // Hack to execute lifecycle callbacks for the first scene
  set_active_scene(INTRO);

  while (game.is_running) {
    process_input();
    update();
    render();
  }

  game_deinit();

  destroy_window();
  destroy_image();
  destroy_sound();

  return 0;
}
