#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "image.h"

// Scenes
#include "intro.h"

// Global variables
int game_is_running = false;
int last_frame_time = 0;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

// Walking animation
const int WALKING_ANIMATION_FRAMES = 4;
SDL_Rect gSpriteClips[WALKING_ANIMATION_FRAMES];
// The texture
ImageData animation = {NULL, 0, 0};
int frame = 0;

// The music that will be played
Mix_Music *gMusic = NULL;

// The sound effects that will be used
Mix_Chunk *gScratch = NULL;
Mix_Chunk *gHigh = NULL;
Mix_Chunk *gMedium = NULL;
Mix_Chunk *gLow = NULL;

// Mouse position
SDL_Point mPosition;

// Declare two game objects for the ball and the paddle
struct game_object {
  float x;
  float y;
  float width;
  float height;
  float vel_x;
  float vel_y;
} ball, paddle;

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

bool loadMedia(void) {
  // Load sprite sheet texture
  if (!load_from_file("foo.png", renderer, &animation)) {
    fprintf(stderr, "Failed to load walking animation texture!\n");
    return false;
  }

  // Set sprite clips
  gSpriteClips[0].x = 0;
  gSpriteClips[0].y = 0;
  gSpriteClips[0].w = 64;
  gSpriteClips[0].h = 205;

  gSpriteClips[1].x = 64;
  gSpriteClips[1].y = 0;
  gSpriteClips[1].w = 64;
  gSpriteClips[1].h = 205;

  gSpriteClips[2].x = 128;
  gSpriteClips[2].y = 0;
  gSpriteClips[2].w = 64;
  gSpriteClips[2].h = 205;

  gSpriteClips[3].x = 192;
  gSpriteClips[3].y = 0;
  gSpriteClips[3].w = 64;
  gSpriteClips[3].h = 205;

  // Load music
  gMusic = Mix_LoadMUS("beat.wav");
  if (gMusic == NULL) {
    fprintf(stderr, "Failed to load beat music! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  // Load sound effects
  gScratch = Mix_LoadWAV("scratch.wav");
  if (gScratch == NULL) {
    fprintf(stderr,
            "Failed to load scratch sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  gHigh = Mix_LoadWAV("high.wav");
  if (gHigh == NULL) {
    fprintf(stderr, "Failed to load high sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  gMedium = Mix_LoadWAV("medium.wav");
  if (gMedium == NULL) {
    fprintf(stderr, "Failed to load medium sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  gLow = Mix_LoadWAV("low.wav");
  if (gLow == NULL) {
    fprintf(stderr, "Failed to load low sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  return true;
}

// Function to poll SDL events and process keyboard input
void process_input(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
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
      // Play high sound effect
      case SDLK_1:
        Mix_PlayChannel(-1, gHigh, 0);
        break;
      // Play medium sound effect
      case SDLK_2:
        Mix_PlayChannel(-1, gMedium, 0);
        break;
      // Play low sound effect
      case SDLK_3:
        Mix_PlayChannel(-1, gLow, 0);
        break;
      // Play scratch sound effect
      case SDLK_4:
        Mix_PlayChannel(-1, gScratch, 0);
        break;
      // Play music
      case SDLK_9:
        // If there is no music playing
        if (Mix_PlayingMusic() == 0) {
          // Play the music
          Mix_PlayMusic(gMusic, -1);
        }
        // If music is being played
        else {
          // If the music is paused
          if (Mix_PausedMusic() == 1) {
            // Resume the music
            Mix_ResumeMusic();
          }
          // If the music is playing
          else {
            // Pause the music
            Mix_PauseMusic();
          }
        }
        break;
      // Stop the music
      case SDLK_0:
        Mix_HaltMusic();
        break;
      }
      break;

      // If mouse event happened
    case SDL_MOUSEMOTION:
      // Get mouse position
      SDL_GetMouseState(&mPosition.x, &mPosition.y);
      break;
    case SDL_MOUSEBUTTONDOWN:
      Mix_PlayChannel(-1, gScratch, 0);
      break;
    case SDL_MOUSEBUTTONUP:
      Mix_PlayChannel(-1, gLow, 0);
      break;
    }
  }

  // Process input for scenes
  intro_process_input();
}

// Setup function that runs once at the beginning of our program
void init(void) {
  // Initialize the ball object moving down at a constant velocity
  ball.x = 10;
  ball.y = 20;
  ball.width = 20;
  ball.height = 20;
  ball.vel_x = 180;
  ball.vel_y = 140;

  // Initialize the paddle object
  paddle.x = 10;
  paddle.y = 20;
  paddle.width = 100;
  paddle.height = 20;
  paddle.vel_x = 0;
  paddle.vel_y = 0;
}

// Update function with a fixed time step
void update(void) {
  // Get delta_time factor converted to seconds to be used to update objects
  float delta_time = (SDL_GetTicks() - last_frame_time) / 1000.0;

  // Store the milliseconds of the current frame to be used in the next one
  last_frame_time = SDL_GetTicks();

  // Move ball as a function of delta time
  ball.x += ball.vel_x * delta_time;
  ball.y += ball.vel_y * delta_time;

  // Check for ball collision with the window borders
  if (ball.x < 0) {
    ball.x = 0;
    ball.vel_x = -ball.vel_x;
  }
  if (ball.x + ball.height > WINDOW_WIDTH) {
    ball.x = WINDOW_WIDTH - ball.width;
    ball.vel_x = -ball.vel_x;
  }
  if (ball.y < 0) {
    ball.y = 0;
    ball.vel_y = -ball.vel_y;
  }
  if (ball.y + ball.height > WINDOW_HEIGHT) {
    ball.y = WINDOW_HEIGHT - ball.height;
    ball.vel_y = -ball.vel_y;
  }

  // Set paddle position based on mouse position
  paddle.x = mPosition.x;
  paddle.y = mPosition.y;

  // Update scenes
  intro_update();
}

void renderImage(int x, int y, SDL_Rect *clip) {
  // Set rendering space and render to screen
  SDL_Rect renderQuad = {x, y, animation.width, animation.height};

  // Set clip rendering dimensions
  if (clip != NULL) {
    renderQuad.w = clip->w;
    renderQuad.h = clip->h;
  }

  // Render to screen
  SDL_RenderCopy(renderer, animation.texture, clip, &renderQuad);
}

// Render function to draw game objects in the SDL window
void render(void) {
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderClear(renderer);

  // Render scenes
  intro_render(renderer);

  // Draw a rectangle for the ball object
  SDL_Rect ball_rect = {(int)ball.x, (int)ball.y, (int)ball.width,
                        (int)ball.height};
  SDL_SetRenderDrawColor(renderer, 0x00, 0xCC, 0xFF, 0xFF);
  SDL_RenderFillRect(renderer, &ball_rect);

  // Draw a rectangle for the paddle object
  SDL_Rect paddle_rect = {(int)paddle.x, (int)paddle.y, (int)paddle.width,
                          (int)paddle.height};
  SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0xCC, 0xFF);
  SDL_RenderFillRect(renderer, &paddle_rect);

  // Render current frame
  SDL_Rect *currentClip = &gSpriteClips[frame / 4];
  renderImage((WINDOW_WIDTH - currentClip->w) / 2,
              (WINDOW_HEIGHT - currentClip->h) / 2, currentClip);

  // Go to next frame
  ++frame;

  // Cycle animation
  if (frame / 4 >= WALKING_ANIMATION_FRAMES) {
    frame = 0;
  }

  // Update screen
  SDL_RenderPresent(renderer);
}

// Function to destroy SDL window and renderer
void destroy_window(void) {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void destroy_image(void) {
  free_image_texture(&animation);
  IMG_Quit();
}

void destroy_sound(void) {
  // Free the sound effects
  Mix_FreeChunk(gScratch);
  Mix_FreeChunk(gHigh);
  Mix_FreeChunk(gMedium);
  Mix_FreeChunk(gLow);
  gScratch = NULL;
  gHigh = NULL;
  gMedium = NULL;
  gLow = NULL;

  // Free the music
  Mix_FreeMusic(gMusic);
  gMusic = NULL;

  Mix_Quit();
}

// Main function
int main(int argc, char *args[]) {
  game_is_running = init_window();
  if (!game_is_running) {
    fprintf(stderr, "Failed to initialize window!\n");
  }

  init();
  intro_init();

  if (game_is_running) {
    game_is_running = loadMedia();
  }
  if (!game_is_running) {
    fprintf(stderr, "Failed to initialize image!\n");
  }

  if (game_is_running) {
    game_is_running = intro_load_media(renderer);
  }
  if (!game_is_running) {
    fprintf(stderr, "Failed to initialize intro!\n");
  }

  while (game_is_running) {
    process_input();
    update();
    render();
  }

  intro_deinit();

  destroy_window();
  destroy_image();
  destroy_sound();

  return 0;
}
