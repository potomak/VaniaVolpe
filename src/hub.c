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
    // camera moved.
    m_pos.x = event->button.x;
    m_pos.y = event->button.y;
    for (int i = 0; i < content_count; i++) {
      SDL_Rect rect = menu_button_rect(i);
      if (SDL_PointInRect(&m_pos, &rect)) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Hub: starting adventure '%s'",
                    content_adventures[i]->title);
        adventure_switch_to(content_adventures[i]);
        break;
      }
    }
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
  scenes[MENU] = (Scene){
      .init = init,
      .load_media = load_media,
      .process_input = process_input,
      .update = update,
      .render = render,
      .deinit = deinit,
      .on_scene_active = on_scene_active,
      .on_scene_inactive = on_scene_inactive,
  };
}
