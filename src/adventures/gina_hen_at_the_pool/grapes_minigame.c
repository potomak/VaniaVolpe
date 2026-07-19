//
//  grapes_minigame.c
//  Tap every grape to pick it. When the bunch is empty Gina has her grapes and
//  we return to the vine.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "constants.h"
#include "game.h"
#include "image.h"
#include "sound.h"
#include "tween.h"

#include "gina_hen_at_the_pool.h"
#include "gina_state.h"
#include "grapes_minigame.h"

// Asset declarations generated from the adventure manifest (ASSETS.md). The
// chunks table mixes directories (the chime is shared by both minigames), so
// it lists per-entry _INIT rows.
#include "gina_assets.h"

static ImageData images[GINA_GRAPES_IMAGES_COUNT] = GINA_GRAPES_IMAGES_INIT;
static const ImageData *background = &images[GINA_GRAPES_IMAGE_BACKGROUND];
static const ImageData *grape = &images[GINA_GRAPES_IMAGE_GRAPE];

// The completion reward (#116): a confetti burst over the picked bunch while
// the chime plays, then back to the vine where Gina explains. Declared as data
// (SCENES.md milestone 1): the framework makes and loads it.
static AnimationData *celebration;
static AnimationData *animations[GINA_GRAPES_ANIMS_COUNT];
static const SceneAnimSpec anim_specs[] = {
    GINA_GRAPES_ANIM_CELEBRATION_SPEC,
};

static ChunkData chunks[2] = {
    GINA_GRAPES_CHUNK_POP_INIT,
    GINA_MINIGAMES_CHUNK_CHIME_INIT,
};
static const ChunkData *pop_sound = &chunks[0];
static const ChunkData *chime_sound = &chunks[1];

#define GRAPE_SIZE 40
#define GRAPE_COUNT 6
// How long a picked grape takes to drop off the bottom of the screen (#108).
#define GRAPE_FALL_MS 550

// Where each grape sits (top-left).
static const SDL_Point GRAPE_AT[GRAPE_COUNT] = {
    {340, 150}, {400, 160}, {460, 150}, {360, 210}, {430, 210}, {395, 270},
};

// A tapped grape falls off-screen before it's gone (#108), so each has three
// phases and its own fall tween.
typedef enum grape_phase {
  GRAPE_ON_VINE, // at rest, tappable
  GRAPE_FALLING, // picked: dropping under its tween
  GRAPE_GONE,    // off-screen; no longer drawn
} GrapePhase;
static GrapePhase phase[GRAPE_COUNT];
static Tween fall[GRAPE_COUNT];

// Completion reached: the celebration is playing, input is ignored, and the
// scene switches when the burst ends.
static bool celebrating;

static SDL_Point m_pos;

static void reset_grapes(void) {
  for (int i = 0; i < GRAPE_COUNT; i++) {
    phase[i] = GRAPE_ON_VINE;
  }
  celebrating = false;
  stop_animation(celebration);
}

static void init(void) {
  celebration = animations[GINA_GRAPES_ANIM_CELEBRATION];
  reset_grapes();
}

static bool load_media(SDL_Renderer *renderer) {
  // The celebration is loaded by the framework from anim_specs.
  (void)renderer;
  return true;
}

static void back_to_vine(void) {
  // Gina explains back at the vine (the scene reads the flag on activation).
  gina_state.announce_grapes = true;
  set_active_scene(VINE);
}

static void process_input(SDL_Event *event) {
  if (celebrating) {
    return;
  }
  switch (event->type) {
  case SDL_MOUSEMOTION:
    m_pos.x = event->motion.x;
    m_pos.y = event->motion.y;
    break;
  case SDL_MOUSEBUTTONDOWN:
    // Hit-test the click's own coordinates (#64): the cached motion position
    // can be stale — e.g. a repeated tap with no motion in between while the
    // camera moved.
    m_pos.x = event->button.x;
    m_pos.y = event->button.y;
    for (int i = 0; i < GRAPE_COUNT; i++) {
      if (phase[i] != GRAPE_ON_VINE) {
        continue;
      }
      SDL_Rect r = {GRAPE_AT[i].x, GRAPE_AT[i].y, GRAPE_SIZE, GRAPE_SIZE};
      if (SDL_PointInRect(&m_pos, &r)) {
        // Pick it: it drops off the bottom of the screen (#108). Completion
        // is checked in update, once every grape has finished falling.
        phase[i] = GRAPE_FALLING;
        tween_start(&fall[i], (SDL_FPoint){GRAPE_AT[i].x, GRAPE_AT[i].y},
                    (SDL_FPoint){GRAPE_AT[i].x, WINDOW_HEIGHT}, GRAPE_FALL_MS,
                    TWEEN_EASE_IN, NULL);
        Mix_PlayChannel(-1, pop_sound->chunk, 0);
        break;
      }
    }
    break;
  }
}

static void update(float delta_time) {
  if (celebrating) {
    return; // the framework ticks the celebration animation
  }
  int gone = 0;
  for (int i = 0; i < GRAPE_COUNT; i++) {
    if (phase[i] == GRAPE_FALLING && !tween_update(&fall[i], delta_time)) {
      phase[i] = GRAPE_GONE;
    }
    if (phase[i] == GRAPE_GONE) {
      gone++;
    }
  }
  // Every grape picked and landed: the reward beat (#116) — chime + confetti
  // burst, then back to the vine.
  if (gone == GRAPE_COUNT) {
    gina_state.has_grapes = true;
    celebrating = true;
    Mix_PlayChannel(-1, chime_sound->chunk, 0);
    play_animation(celebration, back_to_vine);
  }
}

// Where the reward burst plays: centred over the picked bunch.
static const SDL_Point CELEBRATION_AT = {275, 100};

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});
  for (int i = 0; i < GRAPE_COUNT; i++) {
    if (phase[i] == GRAPE_ON_VINE) {
      render_image(renderer, grape, GRAPE_AT[i]);
    } else if (phase[i] == GRAPE_FALLING) {
      // Mid-fall: draw it at its tween position.
      SDL_FPoint p = tween_pos(&fall[i]);
      render_image(renderer, grape, (SDL_Point){(int)p.x, (int)p.y});
    }
  }

  // The reward burst over the bunch while the chime plays.
  if (celebrating) {
    render_animation(renderer, celebration, CELEBRATION_AT);
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
    .animations = animations,
    .animations_length = LEN(animations),
    .anim_specs = anim_specs,
    .anim_specs_length = LEN(anim_specs),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
