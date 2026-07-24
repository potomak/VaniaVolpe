//
//  hub.c
//  The adventure-selection menu (placeholder: one button rect per adventure).
//

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "constants.h"
#include "game.h"
#include "scene.h"

#include "hub.h"

enum hub_scene { MENU };

// The selectable adventures (not including the hub itself).
static const Adventure **content_adventures = NULL;
static int content_count = 0;

static SDL_Point m_pos;

// Vertical list of placeholder buttons, one per content adventure.
#define MENU_BUTTON_WIDTH 440
#define MENU_BUTTON_HEIGHT 80
#define MENU_BUTTON_GAP 28
#define MENU_TOP 150

static SDL_Rect menu_button_rect(int index) {
  return (SDL_Rect){
      .x = (WINDOW_WIDTH - MENU_BUTTON_WIDTH) / 2,
      .y = MENU_TOP + index * (MENU_BUTTON_HEIGHT + MENU_BUTTON_GAP),
      .w = MENU_BUTTON_WIDTH,
      .h = MENU_BUTTON_HEIGHT,
  };
}

// A placeholder Exit button below the adventure list: the hub is the one place
// the game is quit from, now that an adventure's own exit returns here instead.
// Real art is tracked with the rest of the hub in #47.
#define EXIT_BUTTON_WIDTH 200
#define EXIT_BUTTON_HEIGHT 60
#define EXIT_BUTTON_BOTTOM_MARGIN 40

static SDL_Rect exit_button_rect(void) {
  return (SDL_Rect){
      .x = (WINDOW_WIDTH - EXIT_BUTTON_WIDTH) / 2,
      .y = WINDOW_HEIGHT - EXIT_BUTTON_HEIGHT - EXIT_BUTTON_BOTTOM_MARGIN,
      .w = EXIT_BUTTON_WIDTH,
      .h = EXIT_BUTTON_HEIGHT,
  };
}

// A hotspot's on_tap carries no argument, so a fixed set of thunks maps a menu
// index to its adventure. HUB_MAX_ADVENTURES caps how many rows the menu can
// show; hub_register builds one row per registered adventure.
#define HUB_MAX_ADVENTURES 8

static void start_content(int index) {
  if (index < content_count) {
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Hub: starting adventure '%s'",
                content_adventures[index]->title);
    adventure_switch_to(content_adventures[index]);
  }
}

#define HUB_SELECT_THUNK(n)                                                    \
  static void select_##n(void) { start_content(n); }
HUB_SELECT_THUNK(0)
HUB_SELECT_THUNK(1)
HUB_SELECT_THUNK(2)
HUB_SELECT_THUNK(3)
HUB_SELECT_THUNK(4)
HUB_SELECT_THUNK(5)
HUB_SELECT_THUNK(6)
HUB_SELECT_THUNK(7)
#undef HUB_SELECT_THUNK

static void (*const SELECT_THUNKS[HUB_MAX_ADVENTURES])(void) = {
    select_0, select_1, select_2, select_3,
    select_4, select_5, select_6, select_7,
};

// One row per adventure (on_tap = its thunk) plus the Exit row (on_tap =
// exit_game). Built in hub_register and dispatched like any scene's hotspots,
// so they show in the debug overlay and can carry a boil once art exists.
static Hotspot hotspots[HUB_MAX_ADVENTURES + 1];
static int hotspots_count;

static void init(void) {}

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
    // Hit-test the click's own coordinates (#64): the cached motion position
    // can be stale — e.g. a repeated tap with no motion in between while the
    // camera moved. The hub has no actor, so these tap-only hotspots take a
    // NULL actor/grid.
    m_pos.x = event->button.x;
    m_pos.y = event->button.y;
    hotspots_handle_click(hotspots, hotspots_count, NULL, NULL, m_pos);
    break;
  }
}

static void update(float delta_time) { (void)delta_time; }

static void render(SDL_Renderer *renderer) {
  // Background
  SDL_SetRenderDrawColor(renderer, 0x20, 0x30, 0x50, 0xFF);
  SDL_RenderFillRect(renderer,
                     &((SDL_Rect){0, 0, WINDOW_WIDTH, WINDOW_HEIGHT}));

  // One placeholder button per adventure (distinct colour each).
  for (int i = 0; i < content_count; i++) {
    SDL_Rect rect = menu_button_rect(i);
    SDL_SetRenderDrawColor(renderer, 0xC0, (Uint8)(0x60 + i * 0x50), 0x40,
                           0xFF);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    SDL_RenderDrawRect(renderer, &rect);
  }

  // The placeholder Exit button (a darker red, set apart from the adventures).
  SDL_Rect exit_rect = exit_button_rect();
  SDL_SetRenderDrawColor(renderer, 0x80, 0x28, 0x28, 0xFF);
  SDL_RenderFillRect(renderer, &exit_rect);
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderDrawRect(renderer, &exit_rect);
}

static void deinit(void) {}

static void on_scene_active(void) {}

static void on_scene_inactive(void) {}

static Scene scenes[1];

Adventure hub = {
    .id = "hub",
    .title = "Vania Volpe Adventures",
    .assets_root = NULL, // drawn with placeholder rects; no asset files
    .scenes = scenes,
    .scenes_length = 1,
    .entry_scene = MENU,
};

void hub_register(const Adventure **content, int count) {
  content_adventures = content;
  content_count = count;
  SDL_assert(count <= HUB_MAX_ADVENTURES);

  // One tap-only hotspot per adventure, then the Exit row.
  int h = 0;
  for (int i = 0; i < count && i < HUB_MAX_ADVENTURES; i++) {
    hotspots[h++] =
        (Hotspot){.rect = menu_button_rect(i), .on_tap = SELECT_THUNKS[i]};
  }
  hotspots[h++] = (Hotspot){.rect = exit_button_rect(), .on_tap = exit_game};
  hotspots_count = h;

  scenes[MENU] = (Scene){
      .init = init,
      .load_media = load_media,
      .process_input = process_input,
      .update = update,
      .render = render,
      .deinit = deinit,
      .on_scene_active = on_scene_active,
      .on_scene_inactive = on_scene_inactive,
      .hotspots = hotspots,
      .hotspots_length = hotspots_count,
  };
}
