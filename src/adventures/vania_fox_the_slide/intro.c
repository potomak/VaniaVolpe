//
//  intro.c
//  Tiny Adventures
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#include <SDL2/SDL.h>
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

// Animations declared as data: the framework makes, loads, ticks and frees
// them; the scene aliases the made objects.
static AnimationData *play_button;
static AnimationData *exit_button;
static AnimationData *animations[VANIA_INTRO_ANIMS_COUNT];
static const SceneAnimSpec anim_specs[] = {
    VANIA_INTRO_ANIM_PLAY_BUTTON_SPEC,
    VANIA_INTRO_ANIM_EXIT_BUTTON_SPEC,
};

// Static sprite layer: backdrop and the two buttons (they animate on hover but
// are always drawn).
static SceneSprite sprites[3];

static Fox *fox;

// Mouse position
static SDL_Point m_pos;

// Hotspots
static const SDL_Rect PLAY_BUTTON_HOTSPOT = {443, 285, 268, 118};
static const SDL_Rect EXIT_BUTTON_HOTSPOT = {436, 430, 277, 103};
static Hotspot hotspots[2];

static void start_playing(void) {
  // The click plays over the transition into the playground entrance (SFX
  // aren't halted on a scene switch, only music).
  play_play_button_click();
  set_active_scene(PLAYGROUND_ENTRANCE);
}

static void exit_to_hub(void) {
  // Exit returns to the adventure-selection menu; quitting the game is offered
  // by the hub itself. The click plays over the transition.
  play_exit_button_click();
  return_to_hub();
}

static void init(void) {
  play_button = animations[VANIA_INTRO_ANIM_PLAY_BUTTON];
  exit_button = animations[VANIA_INTRO_ANIM_EXIT_BUTTON];

  int s = 0;
  sprites[s++] = (SceneSprite){.image = background, .at = {0, 0}};
  sprites[s++] = (SceneSprite){.animation = play_button, .at = {410, 260}};
  sprites[s++] = (SceneSprite){.animation = exit_button, .at = {440, 450}};

  // Pose the fox sitting.
  actor_play_state(fox, SITTING);

  hotspots[0] = (Hotspot){.rect = PLAY_BUTTON_HOTSPOT, .on_tap = start_playing};
  hotspots[1] = (Hotspot){.rect = EXIT_BUTTON_HOTSPOT, .on_tap = exit_to_hub};
}

static void process_input(SDL_Event *event) {
  // Drag & drop (LIVELINESS.md): an upward pull on the fox lifts her, and
  // letting go sets her back in her seat. A drag is consumed here; plain taps
  // fall through to the buttons below.
  if (actor_drag_event(fox, event)) {
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
  case SDL_MOUSEBUTTONUP:
    // Hit-test the click's own coordinates: the cached motion position can be
    // stale (a repeated tap with no motion between, while the camera moved).
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
  // Re-pose the fox whenever the intro is shown, so replaying the adventure (or
  // dragging her away) always returns her to her seat. Matches .actor_start.
  fox->current_position = (SDL_FPoint){322, 317};
  actor_play_state(fox, SITTING);
}

static void on_scene_inactive(void) {}

Scene intro_scene = {
    .init = init,
    .process_input = process_input,
    .actor = &fox,
    .actor_spec = &FOX_SPEC,
    .actor_start = {322, 317},
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
    .music = VANIA_MUSIC_CHUNK_INTRO_ASSET_INIT,
};
