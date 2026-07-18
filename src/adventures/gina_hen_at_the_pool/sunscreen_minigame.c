//
//  sunscreen_minigame.c
//  Close-up of Gina: brush sunscreen over her (mouse-down + drag) until enough
//  of her is covered, then she is ready and we return to the pool. The covered
//  cells are drawn in a lighter colour so "Gina changes colour" as you paint.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "constants.h"
#include "game.h"
#include "image.h"
#include "sound.h"

#include "gina_hen_at_the_pool.h"
#include "gina_state.h"
#include "sunscreen_minigame.h"

// Asset declarations generated from the adventure manifest (ASSETS.md). The
// chunks table mixes directories (the chime is shared by both minigames), so
// it lists per-entry _INIT rows.
#include "gina_assets.h"

static ImageData images[GINA_SUNSCREEN_IMAGES_COUNT] =
    GINA_SUNSCREEN_IMAGES_INIT;
static const ImageData *background = &images[GINA_SUNSCREEN_IMAGE_BACKGROUND];
static const ImageData *gina_closeup = &images[GINA_SUNSCREEN_IMAGE_GINA];

// The completion reward (#116): a confetti burst over the close-up while the
// chime plays, then back to the pool where Gina explains.
static AnimationData *celebration;
static AnimationData *animations[GINA_SUNSCREEN_ANIMS_COUNT];

static ChunkData chunks[1] = {
    GINA_MINIGAMES_CHUNK_CHIME_INIT,
};

// The close-up rect Gina occupies, and the brush grid laid over it.
#define GINA_X 280
#define GINA_Y 180
#define GINA_W 240
#define GINA_H 240
#define COLS 8
#define ROWS 8
// Fraction of cells that must be covered to count as "done".
#define COVERAGE_THRESHOLD 0.7f

static bool painted[ROWS][COLS];
static int painted_count;
static bool brushing;
// Completion reached: the celebration is playing, input is ignored, and the
// scene switches when the burst ends.
static bool celebrating;

static void reset_grid(void) {
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      painted[r][c] = false;
    }
  }
  painted_count = 0;
  brushing = false;
  celebrating = false;
  stop_animation(celebration);
}

static void init(void) {
  celebration = animations[GINA_SUNSCREEN_ANIM_CELEBRATION] =
      make_animation_data(GINA_SUNSCREEN_ANIM_CELEBRATION_FRAMES,
                          GINA_SUNSCREEN_ANIM_CELEBRATION_STYLE);
  reset_grid();
}

static bool load_media(SDL_Renderer *renderer) {
  return load_animation(renderer, celebration,
                        GINA_SUNSCREEN_ANIM_CELEBRATION_SPRITE_ASSET,
                        GINA_SUNSCREEN_ANIM_CELEBRATION_DATA_ASSET);
}

static void back_to_pool(void) {
  // Gina explains back at the pool (the scene reads the flag on activation).
  gina_state.announce_sunscreen = true;
  set_active_scene(POOL);
}

// Mark the cell under (x, y), if it is over Gina and not already painted.
static void brush_at(int x, int y) {
  if (celebrating || x < GINA_X || x >= GINA_X + GINA_W || y < GINA_Y ||
      y >= GINA_Y + GINA_H) {
    return;
  }
  int c = (x - GINA_X) / (GINA_W / COLS);
  int r = (y - GINA_Y) / (GINA_H / ROWS);
  if (c < 0 || c >= COLS || r < 0 || r >= ROWS || painted[r][c]) {
    return;
  }
  painted[r][c] = true;
  painted_count++;

  if (painted_count >= (int)(COVERAGE_THRESHOLD * ROWS * COLS)) {
    gina_state.has_sunscreen = true;
    // The reward beat (#116): chime + confetti burst, then back to the pool.
    celebrating = true;
    Mix_PlayChannel(-1, chunks[0].chunk, 0);
    play_animation(celebration, back_to_pool);
  }
}

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEBUTTONDOWN:
    brushing = true;
    brush_at(event->button.x, event->button.y);
    break;
  case SDL_MOUSEBUTTONUP:
    brushing = false;
    break;
  case SDL_MOUSEMOTION:
    if (brushing || (event->motion.state & SDL_BUTTON_LMASK)) {
      brush_at(event->motion.x, event->motion.y);
    }
    break;
  }
}

static void update(float delta_time) { (void)delta_time; }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});
  render_image(renderer, gina_closeup, (SDL_Point){GINA_X, GINA_Y});

  // Painted cells: a pale sunscreen layer over Gina.
  SDL_SetRenderDrawColor(renderer, 0xFA, 0xF0, 0xD0, 0xFF);
  for (int r = 0; r < ROWS; r++) {
    for (int c = 0; c < COLS; c++) {
      if (painted[r][c]) {
        SDL_Rect cell = {GINA_X + c * (GINA_W / COLS),
                         GINA_Y + r * (GINA_H / ROWS), GINA_W / COLS,
                         GINA_H / ROWS};
        SDL_RenderFillRect(renderer, &cell);
      }
    }
  }

  // The reward burst over the close-up while the chime plays.
  if (celebrating) {
    render_animation(renderer, celebration, (SDL_Point){GINA_X, GINA_Y});
  }
}

static void deinit(void) {}

static void on_scene_active(void) { reset_grid(); }

static void on_scene_inactive(void) {}

Scene sunscreen_minigame_scene = {
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
    .animations = animations,
    .animations_length = LEN(animations),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
