//
//  intro.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "constants.h"
#include "fox.h"
#include "game.h"
#include "image.h"
#include "vania_fox_the_slide.h"

#include "intro.h"

static ImageData images[1] = {
    {NULL, "intro_background.png", "intro", 0, 0},
};
static const ImageData *background = &images[0];

// Animations (the framework ticks and frees these; the scene only declares
// them)
static AnimationData *play_button;
static AnimationData *exit_button;
static AnimationData *animations[2];

static Fox *fox;

// Music
static Mix_Music *music = NULL;

// Sound effects
static ChunkData chunks[2] = {
    {NULL, "play_button_click.wav", "intro"},
    {NULL, "exit_button_click.wav", "intro"},
};

// Mouse position
static SDL_Point m_pos;

// Hotspots
static const SDL_Rect PLAY_BUTTON_HOTSPOT = {443, 285, 268, 118};
static const SDL_Rect EXIT_BUTTON_HOTSPOT = {436, 430, 277, 103};
static SDL_Rect hotspots[2];

static void init(void) {
  play_button = animations[0] = make_animation_data(3, LOOP);
  exit_button = animations[1] = make_animation_data(3, LOOP);

  fox = make_fox((SDL_FPoint){322, 317});
  fox_sit(fox);

  hotspots[0] = PLAY_BUTTON_HOTSPOT;
  hotspots[1] = EXIT_BUTTON_HOTSPOT;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!load_animation(renderer, play_button,
                      (Asset){
                          .filename = "play_button.png",
                          .directory = "intro",
                      },
                      (Asset){
                          .filename = "play_button.anim",
                          .directory = "intro",
                      })) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load play button");
    return false;
  }

  if (!load_animation(renderer, exit_button,
                      (Asset){
                          .filename = "exit_button.png",
                          .directory = "intro",
                      },
                      (Asset){
                          .filename = "exit_button.anim",
                          .directory = "intro",
                      })) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load exit button");
    return false;
  }

  if (!fox_load_media(fox, renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load fox");
    return false;
  }

  // Load music
  char music_path[ASSET_PATH_MAX];
  asset_resolve(
      (Asset){
          .filename = "intro.wav",
          .directory = "music",
      },
      music_path, sizeof(music_path));
  music = Mix_LoadMUS(music_path);
  if (music == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load music: %s",
                 Mix_GetError());
    return false;
  }

  return true;
}

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEMOTION:
    // Get mouse position
    m_pos.x = event->motion.x;
    m_pos.y = event->motion.y;
    if (SDL_PointInRect(&m_pos, &PLAY_BUTTON_HOTSPOT)) {
      play_animation(play_button, NULL);
    } else {
      stop_animation(play_button);
    }
    if (SDL_PointInRect(&m_pos, &EXIT_BUTTON_HOTSPOT)) {
      play_animation(exit_button, NULL);
    } else {
      stop_animation(exit_button);
    }
    break;
  case SDL_MOUSEBUTTONDOWN:
    if (SDL_PointInRect(&m_pos, &PLAY_BUTTON_HOTSPOT)) {
      set_active_scene(PLAYGROUND_ENTRANCE);
    }
    if (SDL_PointInRect(&m_pos, &EXIT_BUTTON_HOTSPOT)) {
      exit_game();
    }
    break;
  }
}

static void update(float delta_time) { fox_update(fox, delta_time); }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});

  render_animation(renderer, play_button, (SDL_Point){410, 260});
  render_animation(renderer, exit_button, (SDL_Point){440, 450});

  fox_render(fox, renderer);
}

static void deinit(void) {
  fox_free(fox);

  Mix_FreeMusic(music);
  music = NULL;
}

static void on_scene_active(void) {
  stop_animation(play_button);
  stop_animation(exit_button);

  Mix_PlayMusic(music, -1);
}

static void on_scene_inactive(void) { Mix_HaltMusic(); }

Scene intro_scene = {
    .init = init,
    .load_media = load_media,
    .process_input = process_input,
    .update = update,
    .render = render,
    .deinit = deinit,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .hotspots = hotspots,
    .hotspots_length = LEN(hotspots),
    .images = images,
    .images_length = LEN(images),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
    .animations = animations,
    .animations_length = LEN(animations),
};
