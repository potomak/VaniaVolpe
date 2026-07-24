//
//  intro.c
//  sdlexample
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

// Animations (declared as data, SCENES.md milestone 1: the framework makes,
// loads, ticks and frees these; the scene only declares them and aliases the
// made objects).
static AnimationData *play_button;
static AnimationData *exit_button;
static AnimationData *animations[VANIA_INTRO_ANIMS_COUNT];
static const SceneAnimSpec anim_specs[] = {
    VANIA_INTRO_ANIM_PLAY_BUTTON_SPEC,
    VANIA_INTRO_ANIM_EXIT_BUTTON_SPEC,
};

// The static sprite layer (SCENES.md milestone 2): backdrop and the two
// buttons (which animate on hover but are always drawn). render() draws only
// the fox.
static SceneSprite sprites[3];

static Fox *fox;

// The button-click SFX live in the adventure's shared bank (play via
// play_<name>()); music is declared on the Scene (.music, SCENES.md
// milestone 4). So the intro carries no per-scene chunk table.

// Mouse position
static SDL_Point m_pos;

// Hotspots
static const SDL_Rect PLAY_BUTTON_HOTSPOT = {443, 285, 268, 118};
static const SDL_Rect EXIT_BUTTON_HOTSPOT = {436, 430, 277, 103};
static Hotspot hotspots[2];

static void start_playing(void) {
  // The click plays over the transition into the playground entrance (SFX
  // aren't halted on a scene switch, only music). The Exit button's click
  // (play_exit_button_click) is deliberately not played: exit_game quits the
  // loop immediately, so a click there would be cut off before it's heard.
  play_play_button_click();
  set_active_scene(PLAYGROUND_ENTRANCE);
}

static void init(void) {
  play_button = animations[VANIA_INTRO_ANIM_PLAY_BUTTON];
  exit_button = animations[VANIA_INTRO_ANIM_EXIT_BUTTON];

  int s = 0;
  sprites[s++] = (SceneSprite){.image = background, .at = {0, 0}};
  sprites[s++] = (SceneSprite){.animation = play_button, .at = {410, 260}};
  sprites[s++] = (SceneSprite){.animation = exit_button, .at = {440, 450}};

  // The framework made the fox (actor_spec/actor_start below) before init; the
  // intro just poses her sitting.
  actor_play_state(fox, SITTING);

  hotspots[0] = (Hotspot){.rect = PLAY_BUTTON_HOTSPOT,
                          .immediate = true,
                          .on_arrive = start_playing};
  hotspots[1] = (Hotspot){
      .rect = EXIT_BUTTON_HOTSPOT, .immediate = true, .on_arrive = exit_game};
}

static void process_input(SDL_Event *event) {
  // Drag & drop (LIVELINESS.md Part 2, #41): a press-drag on the fox picks her
  // up here too. The intro has no walkable area, so the drag takes a NULL grid
  // — she is set back down where released and settles to her sitting idle. A
  // press that becomes a drag is consumed here; plain taps fall through to the
  // buttons below.
  if (walk_actor_drag_event(fox, NULL, event)) {
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

// No update/render: the fox is the only dynamic thing, so the framework ticks
// and draws her (#147); the backdrop and buttons are static sprites.

static void on_scene_active(void) {
  stop_animation(play_button);
  stop_animation(exit_button);
}

static void on_scene_inactive(void) {}

Scene intro_scene = {
    .init = init,
    .process_input = process_input,
    // No update/render: the framework ticks and draws the actor (#147). The
    // framework also owns the fox's lifecycle (#141): it makes her at
    // actor_start before init, loads her media, and frees her on teardown.
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
