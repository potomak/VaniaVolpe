#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "image.h"

// Special scene used as a playground
#include "example.h"

// Scenes
#include "intro.h"

// Global variables
int game_is_running = false;
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
  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0) {
    fprintf(stderr, "SDL_mixer could not initialize! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  return true;
}

void init(void) {
  example_scene.init();
  intro_scene.init();
}

bool load_media(void) {
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

// Function to poll SDL events and process keyboard input
void process_input(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    // Process input for scenes
    example_scene.process_input(&event);
    intro_scene.process_input(&event);

    switch (event.type) {
    case SDL_QUIT:
      game_is_running = false;
      break;
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      // Exit the game
      case SDLK_ESCAPE:
        game_is_running = false;
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

  // Update scenes
  example_scene.update(delta_time);
  intro_scene.update(delta_time);
}

// Render function to draw game objects in the SDL window
void render(void) {
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderClear(renderer);

  // Render scenes
  intro_scene.render(renderer);
  example_scene.render(renderer);

  // Update screen
  SDL_RenderPresent(renderer);
}

void deinit(void) {
  intro_scene.deinit();
  example_scene.deinit();
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
  game_is_running = init_window();
  if (!game_is_running) {
    fprintf(stderr, "Failed to initialize window!\n");
  }

  init();

  if (game_is_running) {
    game_is_running = load_media();
  }
  if (!game_is_running) {
    fprintf(stderr, "Failed to load media!\n");
  }

  while (game_is_running) {
    process_input();
    update();
    render();
  }

  deinit();

  destroy_window();
  destroy_image();
  destroy_sound();

  return 0;
}
