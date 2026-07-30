//
//  intro.c
//  Gina's title screen: the adventure's entry scene. Play starts the
//  poolside; Exit goes back to the adventure selection.
//

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "constants.h"
#include "game.h"
#include "image.h"

#include "gina_hen_at_the_pool.h"
#include "hen.h"
#include "intro.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "gina_assets.h"

static ImageData images[GINA_INTRO_IMAGES_COUNT] = GINA_INTRO_IMAGES_INIT;
static const ImageData *background = &images[GINA_INTRO_IMAGE_INTRO_BACKGROUND];

// Animations declared as data: the framework makes, loads, ticks and frees
// them; the scene aliases the made objects.
static AnimationData *play_button;
static AnimationData *exit_button;
static AnimationData *animations[GINA_INTRO_ANIMS_COUNT];
static const SceneAnimSpec anim_specs[] = {
    GINA_INTRO_ANIM_PLAY_BUTTON_SPEC,
    GINA_INTRO_ANIM_EXIT_BUTTON_SPEC,
};

// Static sprite layer: backdrop and the two buttons (they animate on hover but
// are always drawn).
static SceneSprite sprites[3];

static Hen *hen;

// Mouse position
static SDL_Point m_pos;

// The buttons are drawn at their hotspot's origin, so what wiggles is exactly
// what a tap hits.
static const SDL_Rect PLAY_BUTTON_HOTSPOT = {430, 270, 280, 120};
static const SDL_Rect EXIT_BUTTON_HOTSPOT = {440, 430, 265, 96};
static Hotspot hotspots[2];

static void start_playing(void) {
  // The click plays over the transition into the poolside (SFX aren't halted
  // on a scene switch, only music).
  play_play_button_click();
  set_active_scene(GINA_POOL);
}

static void exit_to_hub(void) {
  // Exit returns to the adventure-selection menu; quitting the game is offered
  // by the hub itself. The click plays over the transition.
  play_exit_button_click();
  return_to_hub();
}

static void init(void) {
  play_button = animations[GINA_INTRO_ANIM_PLAY_BUTTON];
  exit_button = animations[GINA_INTRO_ANIM_EXIT_BUTTON];

  int s = 0;
  sprites[s++] = (SceneSprite){.image = background, .at = {0, 0}};
  sprites[s++] =
      (SceneSprite){.animation = play_button,
                    .at = {PLAY_BUTTON_HOTSPOT.x, PLAY_BUTTON_HOTSPOT.y}};
  sprites[s++] =
      (SceneSprite){.animation = exit_button,
                    .at = {EXIT_BUTTON_HOTSPOT.x, EXIT_BUTTON_HOTSPOT.y}};

  hotspots[0] = (Hotspot){.rect = PLAY_BUTTON_HOTSPOT, .on_tap = start_playing};
  hotspots[1] = (Hotspot){.rect = EXIT_BUTTON_HOTSPOT, .on_tap = exit_to_hub};
}

static void process_input(SDL_Event *event) {
  // Drag & drop (LIVELINESS.md): an upward pull on Gina lifts her, and letting
  // go sets her back on her spot. A drag is consumed here; plain taps fall
  // through to the buttons below.
  if (actor_drag_event(hen, event)) {
    return;
  }
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
    // Hit-test the click's own coordinates: the cached motion position can be
    // stale (a repeated tap with no motion between).
    m_pos.x = event->button.x;
    m_pos.y = event->button.y;
    // Tap-only hotspots; the intro has no actor to walk.
    hotspots_handle_click(hotspots, LEN(hotspots), NULL, NULL, m_pos);
    break;
  }
}

static void on_scene_active(void) {
  stop_animation(play_button);
  stop_animation(exit_button);
  // Re-pose Gina whenever the intro is shown, so replaying the adventure (or
  // dragging her away) always returns her to her spot. Matches .actor_start.
  hen->current_position = (SDL_FPoint){230, 400};
  actor_play_state(hen, IDLE);
}

static void on_scene_inactive(void) {}

Scene gina_intro_scene = {
    .init = init,
    .process_input = process_input,
    .actor = &hen,
    .actor_spec = &HEN_SPEC,
    .actor_start = {230, 400},
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .hotspots = hotspots,
    .hotspots_length = LEN(hotspots),
    .sprites = sprites,
    .sprites_length = LEN(sprites),
    .images = images,
    .images_length = LEN(images),
    .animations = animations,
    .animations_length = LEN(animations),
    .anim_specs = anim_specs,
    .anim_specs_length = LEN(anim_specs),
};
