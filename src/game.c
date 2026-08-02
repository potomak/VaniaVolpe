//
//  game.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include "game.h"
#include "confirm.h"

// Asset path resolution (adventure assets root)
#include "asset.h"
// Simulation clock seam: a deterministic virtual clock under test
#include "clock.h"
#include "constants.h"
// Features for debugging the game
#include "debug.h"
// Engine-owned UI art (the back-to-hub button)
#include "image.h"
// Dialogue text overlay (SPEECH.md Part 3)
#include "subtitle.h"

Game game = {
    .is_running = false,
    .is_debugging = false,
};

// Every registered adventure (so init/load/deinit cover all of them up front)
// and the hub, which is the first registered adventure.
static const Adventure **adventures = NULL;
static int adventures_count = 0;
static const Adventure *hub_adventure = NULL;

// Engine-level "back to hub" button, drawn over any non-hub adventure. 64 px
// square: Apple's minimum tap target is 44 pt, and this is aimed at a
// two-year-old's finger, not an adult's.
#define HUB_BUTTON_SIZE 64
#define HUB_BUTTON_MARGIN 12
static const SDL_Rect HUB_BUTTON = {
    WINDOW_WIDTH - HUB_BUTTON_SIZE - HUB_BUTTON_MARGIN, HUB_BUTTON_MARGIN,
    HUB_BUTTON_SIZE, HUB_BUTTON_SIZE};

// Engine art, loaded by path like the confirmation's (see ASSETS.md). Alone
// among the tappable things it does *not* boil: it sits over a scene whose own
// hotspots are boiling to say "tap me", and a wobble in the corner would pull
// the eye away from them. Leaving is not what we want to advertise.
#define HUB_BUTTON_PATH "assets/ui/hub_button.png"
static ImageData hub_button_image;

// The renderer the game draws through, kept from the media pass so touch
// events can be mapped into logical coordinates (see game_process_input).
static SDL_Renderer *game_renderer = NULL;

#ifndef PROD
// Reaching the debug layer on a device with no keyboard: press and hold the
// top-left corner — where the debug marker itself appears — for two seconds.
// One finger, so it costs nothing to the "first finger owns the interaction"
// rule and works with a mouse too. Compiled out of a PROD build along with the
// D key, so a distributed build has no way in at all.
#define DEBUG_HOLD_MS 2000
#define DEBUG_HOLD_CORNER 64
static const SDL_Rect DEBUG_CORNER = {0, 0, DEBUG_HOLD_CORNER,
                                      DEBUG_HOLD_CORNER};
// 0 = not holding. Set on a press inside the corner, cleared by release or by
// leaving it, so a hold that wanders is not a hold.
static int debug_hold_started;
static bool debug_hold_fired;

// Watch a press-hold-release for the corner gesture. Returns true when the
// event should be swallowed: only the release that completes a hold, so an
// ordinary tap in that corner still reaches the scene.
static bool debug_hold_input(const SDL_Event *event) {
  SDL_Point point;
  switch (event->type) {
  case SDL_MOUSEBUTTONDOWN:
    point = (SDL_Point){event->button.x, event->button.y};
    if (SDL_PointInRect(&point, &DEBUG_CORNER)) {
      debug_hold_started = clock_now_ms();
      debug_hold_fired = false;
    }
    return false;
  case SDL_MOUSEMOTION:
    point = (SDL_Point){event->motion.x, event->motion.y};
    if (!SDL_PointInRect(&point, &DEBUG_CORNER)) {
      debug_hold_started = 0;
    }
    return false;
  case SDL_MOUSEBUTTONUP: {
    bool completed = debug_hold_fired;
    debug_hold_started = 0;
    debug_hold_fired = false;
    // The tap that toggled the overlay must not also walk the actor there.
    return completed;
  }
  default:
    return false;
  }
}

// Fire the gesture once the hold is long enough. Driven from game_update
// rather than the event loop: holding still produces no events.
static void debug_hold_update(void) {
  if (debug_hold_started == 0 || debug_hold_fired) {
    return;
  }
  if (clock_now_ms() - debug_hold_started >= DEBUG_HOLD_MS) {
    debug_hold_fired = true;
    game.is_debugging = !game.is_debugging;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Debug overlay %s (corner hold)",
                game.is_debugging ? "on" : "off");
  }
}
#endif /* PROD */

void register_adventures(const Adventure *hub, const Adventure **registered,
                         int count) {
  adventures = registered;
  adventures_count = count;
  hub_adventure = hub;
}

// A newly active scene's camera snaps straight onto its target (no easing
// across the scene on entry). After on_scene_active, which may reposition the
// followed actor.
static void snap_scene_camera(void) {
  Camera *camera = scene_instance(game.current_scene)->camera;
  if (camera != NULL) {
    camera_snap(camera);
  }
}

void adventure_switch_to(const Adventure *adventure) {
  if (game.current_adventure != NULL) {
    const Scene *previous = scene_instance(game.current_scene);
    previous->on_scene_inactive();
    scene_stop_music(previous);
  }
  // A line spoken in the old adventure must not linger over the new one.
  subtitle_clear();
  // Any question was about the adventure being left.
  confirm_close();
  game.current_adventure = adventure;
  game.current_scene = adventure->entry_scene;
  asset_set_root(adventure->assets_root);
  if (adventure->on_enter != NULL) {
    adventure->on_enter();
  }
  const Scene *current = scene_instance(game.current_scene);
  current->on_scene_active();
  scene_start_music(current);
  snap_scene_camera();
}

const Scene *scene_instance(int scene) {
  SDL_assert(game.current_adventure != NULL);
  SDL_assert(scene >= 0 && scene < game.current_adventure->scenes_length);
  return &game.current_adventure->scenes[scene];
}

// Sets a new scene as the current scene, optionally placing its actor.
static void activate_scene(int scene, const SDL_FPoint *actor_at) {
  const Scene *previous = scene_instance(game.current_scene);
  previous->on_scene_inactive();
  scene_stop_music(previous);
  subtitle_clear();
  game.current_scene = scene;
  const Scene *current = scene_instance(game.current_scene);
  current->on_scene_active();
  // After on_scene_active, so the caller's placement wins over the scene's own
  // idea of where the actor stands — the caller is the one who knows which way
  // the player came in.
  if (actor_at != NULL && current->actor != NULL && *current->actor != NULL) {
    actor_place(*current->actor, *actor_at);
  }
  scene_start_music(current);
  snap_scene_camera();
}

void set_active_scene(int scene) { activate_scene(scene, NULL); }

void set_active_scene_at(int scene, SDL_FPoint actor_at) {
  activate_scene(scene, &actor_at);
}

void return_to_hub(void) {
  if (hub_adventure != NULL && game.current_adventure != hub_adventure) {
    adventure_switch_to(hub_adventure);
  }
}

int sfx_play(int index) {
  const Adventure *adventure = game.current_adventure;
  SDL_assert(adventure != NULL);
  SDL_assert(index >= 0 && index < adventure->sfx_length);
  return Mix_PlayChannel(-1, adventure->sfx[index].chunk, 0);
}

void scene_say(int index) {
  const Scene *scene = scene_instance(game.current_scene);
  SDL_assert(scene->actor != NULL);
  SDL_assert(index >= 0 && index < scene->chunks_length);
  // NULL text: actor_talk reads the line from the chunk's text sidecar.
  actor_talk(*scene->actor, &scene->chunks[index], NULL);
}

void exit_game(void) { game.is_running = false; }

void game_init(void) {
  for (int a = 0; a < adventures_count; a++) {
    adventure_init(adventures[a]);
  }
}

bool game_load_media(SDL_Renderer *renderer) {
  game_renderer = renderer;
  if (!load_image_from_path(renderer, &hub_button_image, HUB_BUTTON_PATH)) {
    return false;
  }
  for (int a = 0; a < adventures_count; a++) {
    if (!adventure_load_media(adventures[a], renderer)) {
      return false;
    }
  }
  return true;
}

// The finger the game is currently following. SDL reports every touch, and a
// toddler puts down more than one; the first one down owns the interaction
// until it lifts, so a second palm can't drag what the first is holding.
static SDL_FingerID active_finger;
static bool has_active_finger;

// Rewrite a touch event as the mouse event the rest of the engine speaks, and
// drop the mouse events SDL synthesizes from the same touch — otherwise one tap
// arrives twice. Returns false when the event should be ignored entirely.
//
// Finger coordinates are normalized to the *window*, so they go through
// SDL_RenderWindowToLogical: with a logical size set, the window may be
// letterboxed, and scaling by WINDOW_WIDTH directly would land the tap in the
// wrong place on any aspect ratio but 4:3.
static bool normalize_touch(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEMOTION:
    return event->motion.which != SDL_TOUCH_MOUSEID;
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEBUTTONUP:
    return event->button.which != SDL_TOUCH_MOUSEID;
  case SDL_FINGERDOWN:
  case SDL_FINGERUP:
  case SDL_FINGERMOTION:
    break;
  default:
    return true;
  }

  if (event->type == SDL_FINGERDOWN) {
    if (has_active_finger) {
      return false;
    }
    active_finger = event->tfinger.fingerId;
    has_active_finger = true;
  } else if (!has_active_finger || event->tfinger.fingerId != active_finger) {
    return false;
  }

  float x = 0;
  float y = 0;
  if (game_renderer != NULL) {
    int w = WINDOW_WIDTH;
    int h = WINDOW_HEIGHT;
    SDL_GetRendererOutputSize(game_renderer, &w, &h);
    SDL_RenderWindowToLogical(game_renderer, (int)(event->tfinger.x * w),
                              (int)(event->tfinger.y * h), &x, &y);
  }

  Uint32 type = event->type;
  if (type == SDL_FINGERUP) {
    has_active_finger = false;
  }

  SDL_zerop(event);
  if (type == SDL_FINGERMOTION) {
    event->type = SDL_MOUSEMOTION;
    event->motion.x = (int)x;
    event->motion.y = (int)y;
    event->motion.state = SDL_BUTTON_LMASK;
  } else {
    event->type =
        type == SDL_FINGERDOWN ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event->button.button = SDL_BUTTON_LEFT;
    event->button.clicks = 1;
    event->button.x = (int)x;
    event->button.y = (int)y;
  }
  return true;
}

// Process input for scenes
void game_process_input(SDL_Event *event) {
  // Touch first: everything below this line deals in mouse events.
  if (!normalize_touch(event)) {
    return;
  }

  // A question is up: it owns the pointer until it is answered.
  if (confirm_process_input(event)) {
    return;
  }

#ifndef PROD
  // Two ways into the debug layer, neither of which ships in a PROD build: the
  // D key, and a long press in the top-left corner for devices with no
  // keyboard.
  if (debug_hold_input(event)) {
    return;
  }

  switch (event->type) {
  case SDL_KEYDOWN:
    // Auto-repeat would toggle once per repeat, strobing the overlay for as
    // long as the key is held.
    if (event->key.repeat) {
      break;
    }
    switch (event->key.keysym.sym) {
    // Toggle debugging features
    case SDLK_d:
      game.is_debugging = !game.is_debugging;
      break;
    }
    break;
  }
#endif /* PROD */

  // The back-to-hub button takes priority over scene input, except in the
  // hub. It lives in screen space, so it is tested before any camera
  // conversion.
  if (event->type == SDL_MOUSEBUTTONUP && hub_adventure != NULL &&
      game.current_adventure != hub_adventure) {
    SDL_Point point = {event->button.x, event->button.y};
    if (SDL_PointInRect(&point, &HUB_BUTTON)) {
      // Ask rather than leave: the button is easy to hit by accident and
      // leaving discards the adventure's progress (confirm.h).
      confirm_open(return_to_hub);
      return;
    }
  }

  // Scrolling scenes get their mouse events converted to scene coordinates in
  // place, so every hotspot test, POI walk and debug rect downstream operates
  // in scene coordinates without knowing a camera exists (R4). The cast
  // matches the render offset's, so input and drawing can't disagree.
  const Camera *camera = scene_instance(game.current_scene)->camera;
  if (camera != NULL) {
    switch (event->type) {
    case SDL_MOUSEMOTION:
      event->motion.x += (int)camera->pos.x;
      event->motion.y += (int)camera->pos.y;
      break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
      event->button.x += (int)camera->pos.x;
      event->button.y += (int)camera->pos.y;
      break;
    }
  }

  // The debug layer may consume the event (walk-paint mode swallows mouse
  // input so painting doesn't walk the actor).
  if (game.is_debugging && debug_process_input(event)) {
    return;
  }

  // A scene either handles input itself or leaves process_input NULL and gets
  // the framework's default drag/hit-test/walk handler.
  const Scene *scene = scene_instance(game.current_scene);
  if (scene->process_input != NULL) {
    scene->process_input(event);
  } else {
    scene_default_process_input(scene, event);
  }
}

void game_update(float delta_time) {
  // Match the hotspot active animations to their enabled state, then advance
  // the active scene's animations (which ticks any now playing) before its own
  // update. A ONE_SHOT end callback fired here may switch scene, so re-fetch
  // the current scene for the update() call (same re-entrancy as a scene switch
  // from process_input).
  sync_hotspot_active_anims(scene_instance(game.current_scene));
  update_scene_animations(*scene_instance(game.current_scene), clock_now_ms());
  // A scene either updates itself or leaves update NULL and the framework ticks
  // its actor — the same optional-with-default shape as process_input.
  const Scene *scene = scene_instance(game.current_scene);
  if (scene->update != NULL) {
    scene->update(delta_time);
  } else if (scene->actor != NULL) {
    scene_default_update(scene, delta_time);
  }

  // The camera eases after the scene has moved its actor, so it follows this
  // frame's position. Re-fetch: the update may have switched scene (a fresh
  // scene's camera was already snapped by the switch).
  Camera *camera = scene_instance(game.current_scene)->camera;
  if (camera != NULL) {
    camera_update(camera, delta_time);
  }

  // The modal is engine UI, outside any scene, so it ticks its own animations.
  confirm_update(clock_now_ms());

#ifndef PROD
  debug_hold_update();
#endif
}

void game_render(SDL_Renderer *renderer) {
  const Scene *scene = scene_instance(game.current_scene);
  const Camera *camera = scene->camera;
  // Scene content (and the debug overlay over it) draws shifted by the
  // camera; the cast to int happens once so every draw shares the same
  // offset. Parallax planes and screen-space UI carry their own offsets, so
  // they draw with the render offset reset to zero.
  const SDL_Point camera_offset =
      camera != NULL ? (SDL_Point){-(int)camera->pos.x, -(int)camera->pos.y}
                     : (SDL_Point){0, 0};

  // Background planes (behind the action layer), each at its own parallax.
  render_scene_planes(renderer, scene->bg_planes, scene->bg_planes_length,
                      camera);

  // The static sprite layer then the action layer, both in scene coordinates.
  // The framework draws the declared sprites first; the scene's own render
  // draws the dynamic action layer (the actor, tweens, overlays) on top.
  render_set_offset(camera_offset);
  render_scene_sprites(renderer, scene->sprites, scene->sprites_length);
  render_hotspot_anims(renderer, scene);
  // A scene either draws its dynamic layer or leaves render NULL and the
  // framework draws its actor, after the static sprites and inside the
  // same camera offset.
  if (scene->render != NULL) {
    scene->render(renderer);
  } else if (scene->actor != NULL) {
    scene_default_render(scene, renderer);
  }
  render_set_offset((SDL_Point){0, 0});

  // Foreground planes (in front of the action layer): a parallax > 1 strip
  // is a cheap walk-behind with no prop needed.
  render_scene_planes(renderer, scene->fg_planes, scene->fg_planes_length,
                      camera);

  // The debug overlay draws over everything, in scene coordinates.
  if (game.is_debugging) {
    render_set_offset(camera_offset);
    debug_render(renderer);
    render_set_offset((SDL_Point){0, 0});
  }

  // Draw the back-to-hub button over any non-hub adventure (screen space).
  if (hub_adventure != NULL && game.current_adventure != hub_adventure) {
    render_image(renderer, &hub_button_image,
                 (SDL_Point){HUB_BUTTON.x, HUB_BUTTON.y});
  }

  // The dialogue text overlay is screen-space UI, over everything.
  subtitle_render(renderer);

  // The modal is over even that: while it is up, nothing else is live.
  confirm_render(renderer);
}

void game_deinit(void) {
  free_image_texture(&hub_button_image);
  for (int a = 0; a < adventures_count; a++) {
    adventure_deinit(adventures[a]);
  }
}
