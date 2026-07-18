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

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "vania_assets.h"

static ImageData images[VANIA_INTRO_IMAGES_COUNT] = VANIA_INTRO_IMAGES_INIT;
static const ImageData *background =
    &images[VANIA_INTRO_IMAGE_INTRO_BACKGROUND];

// Animations (the framework ticks and frees these; the scene only declares
// them)
static AnimationData *play_button;
static AnimationData *exit_button;
static AnimationData *animations[VANIA_INTRO_ANIMS_COUNT];

static Fox *fox;

// Music
static Mix_Music *music = NULL;

// Sound effects
static ChunkData chunks[VANIA_INTRO_CHUNKS_COUNT] = VANIA_INTRO_CHUNKS_INIT;

// Mouse position
static SDL_Point m_pos;

// Hotspots
static const SDL_Rect PLAY_BUTTON_HOTSPOT = {443, 285, 268, 118};
static const SDL_Rect EXIT_BUTTON_HOTSPOT = {436, 430, 277, 103};
static Hotspot hotspots[2];

static void start_playing(void) { set_active_scene(PLAYGROUND_ENTRANCE); }

static void init(void) {
  play_button = animations[VANIA_INTRO_ANIM_PLAY_BUTTON] = make_animation_data(
      VANIA_INTRO_ANIM_PLAY_BUTTON_FRAMES, VANIA_INTRO_ANIM_PLAY_BUTTON_STYLE);
  exit_button = animations[VANIA_INTRO_ANIM_EXIT_BUTTON] = make_animation_data(
      VANIA_INTRO_ANIM_EXIT_BUTTON_FRAMES, VANIA_INTRO_ANIM_EXIT_BUTTON_STYLE);

  fox = make_fox((SDL_FPoint){322, 317});
  fox_sit(fox);

  hotspots[0] = (Hotspot){.rect = PLAY_BUTTON_HOTSPOT,
                          .immediate = true,
                          .on_arrive = start_playing};
  hotspots[1] = (Hotspot){
      .rect = EXIT_BUTTON_HOTSPOT, .immediate = true, .on_arrive = exit_game};
}

static bool load_media(SDL_Renderer *renderer) {
  if (!load_animation(renderer, play_button,
                      VANIA_INTRO_ANIM_PLAY_BUTTON_SPRITE_ASSET,
                      VANIA_INTRO_ANIM_PLAY_BUTTON_DATA_ASSET)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load play button");
    return false;
  }

  if (!load_animation(renderer, exit_button,
                      VANIA_INTRO_ANIM_EXIT_BUTTON_SPRITE_ASSET,
                      VANIA_INTRO_ANIM_EXIT_BUTTON_DATA_ASSET)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load exit button");
    return false;
  }

  if (!fox_load_media(fox, renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load fox");
    return false;
  }

  // Load music
  char music_path[ASSET_PATH_MAX];
  asset_resolve(VANIA_MUSIC_CHUNK_INTRO_ASSET, music_path, sizeof(music_path));
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
    // Hit-test the click's own coordinates (#64): the cached motion position
    // can be stale — e.g. a repeated tap with no motion in between while the
    // camera moved.
    m_pos.x = event->button.x;
    m_pos.y = event->button.y;
    // Both buttons are immediate hotspots (no actor walk in the intro).
    hotspots_handle_click(hotspots, LEN(hotspots), NULL, NULL, m_pos);
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
