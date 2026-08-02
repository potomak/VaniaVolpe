//
//  hub.c
//  The adventure-selection screen: a home screen of cartridges.
//
//  Each adventure is a cartridge — one shared drawing, tinted with the
//  adventure's own colour, with a hole in the middle showing that adventure's
//  icon. Tapping one starts it. (The eventual look is an '80s kid's room with
//  an insert-into-the-console transition, tracked in #104; this is the grid it
//  starts from.)
//
//  The screen's own art is engine-owned: it belongs to no adventure, so it is
//  loaded by path like the subtitle font and the leave-confirmation buttons,
//  and listed in the repo-level assets/index.json. The icons are the exception
//  — each belongs to its adventure and carries that adventure's name, so it is
//  localized and resolved against its own root.
//

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "asset.h"
#include "clock.h"
#include "constants.h"
#include "game.h"
#include "image.h"
#include "scene.h"

#include "hub.h"

enum hub_scene { MENU };

// The selectable adventures (not including the hub itself).
static const Adventure **content_adventures = NULL;
static int content_count = 0;

// The grid. Three across and two down on the 800x600 screen, which makes a
// cartridge big enough to carry an icon with the adventure's name on it and to
// be hit by a toddler. More adventures than fit will need the screen to
// scroll.
#define GRID_COLS 3
#define GRID_ROWS 2
#define GRID_SLOTS (GRID_COLS * GRID_ROWS)

#define CARTRIDGE_W 220
#define CARTRIDGE_H 200
#define GRID_TOP 110
#define GRID_ROW_GAP 40
// Whatever is left over after the cartridges, split evenly into the outer
// margins and the gaps between columns.
#define GRID_COL_GAP                                                           \
  ((WINDOW_WIDTH - GRID_COLS * CARTRIDGE_W) / (GRID_COLS + 1))

// The hole in the cartridge, and so the size every adventure icon is drawn at.
// Must match ICON / HOLE_AT in tools/gen_hub_placeholders.py.
#define ICON_W 180
#define ICON_H 120
#define ICON_OFFSET_X 20
#define ICON_OFFSET_Y 22

// The corner buttons: settings, about, and quit. The first two do nothing yet.
#define BUTTON_SIZE 56
#define BUTTON_GAP 12
#define BUTTON_MARGIN 16
#define BUTTONS_COUNT 3

static SDL_Rect cartridge_rect(int index) {
  int col = index % GRID_COLS;
  int row = index / GRID_COLS;
  return (SDL_Rect){
      .x = GRID_COL_GAP + col * (CARTRIDGE_W + GRID_COL_GAP),
      .y = GRID_TOP + row * (CARTRIDGE_H + GRID_ROW_GAP),
      .w = CARTRIDGE_W,
      .h = CARTRIDGE_H,
  };
}

// Right-aligned along the top, in declaration order.
static SDL_Rect button_rect(int index) {
  int span = BUTTONS_COUNT * BUTTON_SIZE + (BUTTONS_COUNT - 1) * BUTTON_GAP;
  return (SDL_Rect){
      .x = WINDOW_WIDTH - BUTTON_MARGIN - span +
           index * (BUTTON_SIZE + BUTTON_GAP),
      .y = BUTTON_MARGIN,
      .w = BUTTON_SIZE,
      .h = BUTTON_SIZE,
  };
}

// A hotspot's on_tap carries no argument, so a fixed set of thunks maps a grid
// slot to its adventure.
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
#undef HUB_SELECT_THUNK

static void (*const SELECT_THUNKS[GRID_SLOTS])(void) = {
    select_0, select_1, select_2, select_3, select_4, select_5,
};

// Settings and About are drawn and tappable but do nothing yet: the buttons
// come first so the screen's shape is settled before there is anything behind
// them.
static void open_settings(void) {
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Hub: settings (not implemented)");
}

static void open_about(void) {
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Hub: about (not implemented)");
}

static void (*const BUTTON_TAPS[BUTTONS_COUNT])(void) = {
    open_settings,
    open_about,
    exit_game,
};

// One row per adventure plus the corner buttons. Ordinary hotspots, so they
// show in the debug overlay and dispatch like any scene's.
static Hotspot hotspots[GRID_SLOTS + BUTTONS_COUNT];
static int hotspots_count;

#define HUB_BACKGROUND_PATH "assets/hub/background.png"
#define BOIL_FRAMES 3

static ImageData background;
// One cartridge sheet for every slot: the drawing is shared and only its tint
// differs, so the same animation is drawn once per cartridge with a different
// colour mod. Same shape for the three corner buttons.
static AnimationData *cartridge_boil;
static AnimationData *button_boils[BUTTONS_COUNT];
static const char *const BUTTON_NAMES[BUTTONS_COUNT] = {"gear", "help", "exit"};

// Each adventure's icon, loaded against that adventure's own root — the hub
// cannot resolve them, since they live in another adventure's locale layers.
static ImageData icons[GRID_SLOTS];

static void init(void) {}

static bool load_boil(SDL_Renderer *renderer, AnimationData **animation,
                      const char *name) {
  char sheet[ASSET_PATH_MAX];
  char data[ASSET_PATH_MAX];
  SDL_snprintf(sheet, sizeof(sheet), "assets/hub/%s_boil.png", name);
  SDL_snprintf(data, sizeof(data), "assets/hub/%s_boil.anim", name);
  *animation = make_animation_data(BOIL_FRAMES, LOOP);
  return load_animation_from_path(renderer, *animation, sheet, data);
}

static bool load_media(SDL_Renderer *renderer) {
  bool ok = load_image_from_path(renderer, &background, HUB_BACKGROUND_PATH);

  ok = load_boil(renderer, &cartridge_boil, "cartridge") && ok;
  for (int i = 0; i < BUTTONS_COUNT; i++) {
    ok = load_boil(renderer, &button_boils[i], BUTTON_NAMES[i]) && ok;
  }

  // Each icon resolves under its own adventure's root and locale, so borrow
  // the root for the load and hand it back. The hub has none of its own (its
  // art is loaded by path), so restore whatever was set rather than assume.
  const char *hub_root = asset_get_root();
  for (int i = 0; i < content_count && i < GRID_SLOTS; i++) {
    const Adventure *adventure = content_adventures[i];
    icons[i].filename = adventure->hub_icon.filename;
    icons[i].directory = adventure->hub_icon.directory;
    asset_set_root(adventure->assets_root);
    ok = load_image(renderer, &icons[i]) && ok;
  }
  asset_set_root(hub_root);
  return ok;
}

static void update(float delta_time) {
  (void)delta_time;
  int now = clock_now_ms();
  animation_update(cartridge_boil, now);
  for (int i = 0; i < BUTTONS_COUNT; i++) {
    animation_update(button_boils[i], now);
  }
}

// Draw one boil tinted, then put the texture back the way it was: the colour
// mod is texture state, and every cartridge shares this one texture.
static void render_tinted(SDL_Renderer *renderer, AnimationData *animation,
                          SDL_Point at, SDL_Color color) {
  SDL_SetTextureColorMod(animation->image.texture, color.r, color.g, color.b);
  render_animation(renderer, animation, at);
  SDL_SetTextureColorMod(animation->image.texture, 0xFF, 0xFF, 0xFF);
}

static void render(SDL_Renderer *renderer) {
  render_image(renderer, &background, (SDL_Point){0, 0});

  for (int i = 0; i < content_count && i < GRID_SLOTS; i++) {
    SDL_Rect slot = cartridge_rect(i);
    // The icon goes down first and the cartridge over it: the shell's hole is
    // transparent, so it frames the art instead of covering it. Centred in the
    // hole rather than pinned to its corner, so art that comes back a little
    // off the declared size still sits square in the window.
    render_image(renderer, &icons[i],
                 (SDL_Point){
                     slot.x + ICON_OFFSET_X + (ICON_W - icons[i].width) / 2,
                     slot.y + ICON_OFFSET_Y + (ICON_H - icons[i].height) / 2,
                 });
    render_tinted(renderer, cartridge_boil, (SDL_Point){slot.x, slot.y},
                  content_adventures[i]->cartridge_color);
  }

  for (int i = 0; i < BUTTONS_COUNT; i++) {
    SDL_Rect rect = button_rect(i);
    render_animation(renderer, button_boils[i], (SDL_Point){rect.x, rect.y});
  }
}

static void deinit(void) {
  free_image_texture(&background);
  free_animation(cartridge_boil);
  cartridge_boil = NULL;
  for (int i = 0; i < BUTTONS_COUNT; i++) {
    free_animation(button_boils[i]);
    button_boils[i] = NULL;
  }
  for (int i = 0; i < GRID_SLOTS; i++) {
    free_image_texture(&icons[i]);
  }
}

// The boils run only while the selection screen is up; there is nothing to
// animate behind an adventure.
static void on_scene_active(void) {
  play_animation(cartridge_boil, NULL);
  for (int i = 0; i < BUTTONS_COUNT; i++) {
    play_animation(button_boils[i], NULL);
  }
}

static void on_scene_inactive(void) {
  stop_animation(cartridge_boil);
  for (int i = 0; i < BUTTONS_COUNT; i++) {
    stop_animation(button_boils[i]);
  }
}

static Scene scenes[1];

Adventure hub = {
    .id = "hub",
    .title = "Vania Volpe Adventures",
    // The screen's own art is engine-owned and loaded by path; the icons
    // resolve against the adventure that owns each.
    .assets_root = NULL,
    .scenes = scenes,
    .scenes_length = 1,
    .entry_scene = MENU,
};

void hub_register(const Adventure **content, int count) {
  content_adventures = content;
  content_count = count;
  SDL_assert(count <= GRID_SLOTS);

  int h = 0;
  for (int i = 0; i < count && i < GRID_SLOTS; i++) {
    hotspots[h++] =
        (Hotspot){.rect = cartridge_rect(i), .on_tap = SELECT_THUNKS[i]};
  }
  for (int i = 0; i < BUTTONS_COUNT; i++) {
    hotspots[h++] = (Hotspot){.rect = button_rect(i), .on_tap = BUTTON_TAPS[i]};
  }
  hotspots_count = h;

  scenes[MENU] = (Scene){
      .init = init,
      .load_media = load_media,
      .update = update,
      .render = render,
      .deinit = deinit,
      .on_scene_active = on_scene_active,
      .on_scene_inactive = on_scene_inactive,
      .hotspots = hotspots,
      .hotspots_length = hotspots_count,
  };
}
