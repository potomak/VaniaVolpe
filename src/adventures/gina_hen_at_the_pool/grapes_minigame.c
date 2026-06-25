//
//  grapes_minigame.c
//  Tap every grape to pick it. When the bunch is empty Gina has her grapes and
//  we return to the vine.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "game.h"
#include "image.h"
#include "sound.h"

#include "gina_hen_at_the_pool.h"
#include "gina_state.h"
#include "grapes_minigame.h"

static ImageData images[2] = {
    {NULL, "background.png", "grapes", 0, 0},
    {NULL, "grape.png", "grapes", 0, 0},
};
static const ImageData *background = &images[0];
static const ImageData *grape = &images[1];

static ChunkData chunks[2] = {
    {NULL, "voice.wav", "grapes"},
    {NULL, "pop.wav", "grapes"},
};

#define GRAPE_SIZE 40
#define GRAPE_COUNT 6

// Where each grape sits (top-left).
static const SDL_Point GRAPE_AT[GRAPE_COUNT] = {
    {340, 150}, {400, 160}, {460, 150}, {360, 210}, {430, 210}, {395, 270},
};
static bool collected[GRAPE_COUNT];
static int collected_count;

static SDL_Point m_pos;

static void reset_grapes(void) {
  for (int i = 0; i < GRAPE_COUNT; i++) {
    collected[i] = false;
  }
  collected_count = 0;
}

static void init(void) { reset_grapes(); }

static bool load_media(SDL_Renderer *renderer) {
  (void)renderer;
  return true;
}

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEMOTION:
    m_pos.x = event->motion.x;
    m_pos.y = event->motion.y;
    break;
  case SDL_MOUSEBUTTONDOWN:
    for (int i = 0; i < GRAPE_COUNT; i++) {
      if (collected[i]) {
        continue;
      }
      SDL_Rect r = {GRAPE_AT[i].x, GRAPE_AT[i].y, GRAPE_SIZE, GRAPE_SIZE};
      if (SDL_PointInRect(&m_pos, &r)) {
        collected[i] = true;
        collected_count++;
        Mix_PlayChannel(-1, chunks[1].chunk, 0);  // pop
        break;
      }
    }
    if (collected_count == GRAPE_COUNT) {
      gina_state.has_grapes = true;
      fprintf(stdout, "Gina: Cestino pieno d'uva! Ora torno da Carla.\n");
      set_active_scene(VINE);
    }
    break;
  }
}

static void update(float delta_time) { (void)delta_time; }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});
  for (int i = 0; i < GRAPE_COUNT; i++) {
    if (!collected[i]) {
      render_image(renderer, grape, GRAPE_AT[i]);
    }
  }
}

static void deinit(void) {}

static void on_scene_active(void) { reset_grapes(); }

static void on_scene_inactive(void) {}

Scene grapes_minigame_scene = {
    .init = init,
    .load_media = load_media,
    .process_input = process_input,
    .update = update,
    .render = render,
    .deinit = deinit,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .images = images,
    .images_length = LEN(images),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
