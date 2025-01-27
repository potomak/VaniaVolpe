//
//  example.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "constants.h"
#include "game.h"
#include "image.h"

#include "example.h"

// Walking animation
static AnimationData *walking;

// The music that will be played
static Mix_Music *gMusic = NULL;

// The sound effects that will be used
static Mix_Chunk *gScratch = NULL;
static Mix_Chunk *gHigh = NULL;
static Mix_Chunk *gMedium = NULL;
static Mix_Chunk *gLow = NULL;

// Mouse position
static SDL_Point mPosition;

// Game objects
static GameObject ball, paddle;

// Runs once at the beginning of our program
static void init(void) {
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

  walking = make_animation_data(4, LOOP);
}

static bool load_media(SDL_Renderer *renderer) {
  // Load sprite sheet texture
  if (!load_animation(renderer, walking,
                      (Asset){
                          .filename = "foo.png",
                          .directory = "example",
                      },
                      (Asset){
                          .filename = "foo.anim",
                          .directory = "example",
                      })) {
    fprintf(stderr, "Failed to load walking animation!\n");
    return false;
  }

  // Load music
  gMusic = Mix_LoadMUS(asset_path((Asset){
      .filename = "beat.wav",
      .directory = "example",
  }));
  if (gMusic == NULL) {
    fprintf(stderr, "Failed to load beat music! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  // Load sound effects
  gScratch = Mix_LoadWAV(asset_path((Asset){
      .filename = "scratch.wav",
      .directory = "example",
  }));
  if (gScratch == NULL) {
    fprintf(stderr,
            "Failed to load scratch sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  gHigh = Mix_LoadWAV(asset_path((Asset){
      .filename = "high.wav",
      .directory = "example",
  }));
  if (gHigh == NULL) {
    fprintf(stderr, "Failed to load high sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  gMedium = Mix_LoadWAV(asset_path((Asset){
      .filename = "medium.wav",
      .directory = "example",
  }));
  if (gMedium == NULL) {
    fprintf(stderr, "Failed to load medium sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  gLow = Mix_LoadWAV(asset_path((Asset){
      .filename = "low.wav",
      .directory = "example",
  }));
  if (gLow == NULL) {
    fprintf(stderr, "Failed to load low sound effect! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  return true;
}

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_KEYDOWN:
    switch (event->key.keysym.sym) {
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
    set_active_scene(INTRO);
    break;
  case SDL_MOUSEBUTTONUP:
    Mix_PlayChannel(-1, gLow, 0);
    break;
  }
}

static void update(float delta_time) {
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
}

static void render(SDL_Renderer *renderer) {
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

  // Render walking animation, assumes a constant frame image size
  render_animation(
      renderer, walking,
      (SDL_Point){(WINDOW_WIDTH - walking->sprite_clips[0].w) / 2,
                  (WINDOW_HEIGHT - walking->sprite_clips[0].h) / 2});
}

static void deinit(void) {
  free_animation(walking);

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
}

static void on_scene_active(void) { play_animation(walking, NULL); }

static void on_scene_inactive(void) {}

Scene example_scene = {
    .init = init,
    .load_media = load_media,
    .process_input = process_input,
    .update = update,
    .render = render,
    .deinit = deinit,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
};
