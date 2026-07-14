//
//  tree.c
//  The tree where the pool float ends up stuck, and where Carla the crow
//  perches. Carla helps Gina get the float back — but only in exchange for
//  grapes, for which she first hands over a basket.
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
#include "hen.h"
#include "tree.h"

static ImageData images[2] = {
    {NULL, "background.png", "tree", 0, 0},
    {NULL, "basket.png", "tree", 0, 0},
};
static const ImageData *background = &images[0];
static const ImageData *basket = &images[1];

// The tappable objects boil (LIVELINESS.md Part 3): the stuck float squiggles
// while it can be examined, Carla while she can be talked to. Same size as the
// old still PNGs, so the render positions are unchanged.
static AnimationData *float_boil;
static AnimationData *carla_boil;
static AnimationData *animations[2];

static ChunkData chunks[2] = {
    {NULL, "voice.wav", "tree"},
    {NULL, "caw.wav", "tree"},
};
static const ChunkData *voice(void) { return &chunks[0]; }

static SDL_Point m_pos;

static Hen *gina;
static const SDL_FPoint HEN_START = {400, 480};

static const SDL_Point FLOAT_AT = {500, 70};
static const SDL_Point CARLA_AT = {360, 150};

static const SDL_Rect FLOAT_HOTSPOT = {500, 70, 90, 60};
static const SDL_Rect CARLA_HOTSPOT = {360, 150, 70, 70};
static const SDL_Rect POOL_NAV_HOTSPOT = {0, 200, 30, 250};
static const SDL_Rect VINE_NAV_HOTSPOT = {770, 200, 30, 250};
static Hotspot hotspots[4];

// Walk geometry: the ground strip under the tree; no blocked areas.
static const SDL_Rect WALKABLE_RECTS[] = {{20, 430, 760, 150}};
static const WalkArea WALK_AREA = {WALKABLE_RECTS, LEN(WALKABLE_RECTS), NULL,
                                   0};
static WalkGrid walk_grid;

static const SDL_Point FLOAT_LOOK_POI = {500, 470};
static const SDL_Point CARLA_POI = {400, 470};
static SDL_Point pois[2];

// Interactions and hotspot gating (bodies below the loaders).
static bool float_is_stuck(void);
static void examine_float(void);
static void talk_to_carla(void);
static void go_to_pool(void);
static void go_to_vine(void);

static void init(void) {
  gina = make_hen(HEN_START);

  walk_grid_init(&walk_grid, &WALK_AREA,
                 (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT}, "tree");

  float_boil = animations[0] = make_animation_data(3, LOOP);
  carla_boil = animations[1] = make_animation_data(3, LOOP);

  int i = 0;
  hotspots[i++] = (Hotspot){.rect = FLOAT_HOTSPOT,
                            .enabled = float_is_stuck,
                            .poi = FLOAT_LOOK_POI,
                            .on_arrive = examine_float,
                            .active_anim = float_boil};
  hotspots[i++] = (Hotspot){.rect = CARLA_HOTSPOT,
                            .poi = CARLA_POI,
                            .on_arrive = talk_to_carla,
                            .active_anim = carla_boil};
  hotspots[i++] = (Hotspot){
      .rect = POOL_NAV_HOTSPOT, .immediate = true, .on_arrive = go_to_pool};
  hotspots[i++] = (Hotspot){
      .rect = VINE_NAV_HOTSPOT, .immediate = true, .on_arrive = go_to_vine};

  i = 0;
  pois[i++] = FLOAT_LOOK_POI;
  pois[i++] = CARLA_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!hen_load_media(gina, renderer)) {
    return false;
  }
  return load_animation(renderer, float_boil, (Asset){"float_boil.png", "tree"},
                        (Asset){"float_boil.anim", "tree"}) &&
         load_animation(renderer, carla_boil, (Asset){"carla_boil.png", "tree"},
                        (Asset){"carla_boil.anim", "tree"});
}

// ── interactions
// ──────────────────────────────────────────────────────────────

static bool float_is_stuck(void) {
  return gina_state.float_state == FLOAT_STUCK_IN_TREE;
}

static void go_to_pool(void) { set_active_scene(POOL); }

static void go_to_vine(void) { set_active_scene(VINE); }

static void examine_float(void) {
  switch (gina_state.examine_float_count) {
  case 0:
    gina_say(gina, "Non ci arrivo!", voice());
    break;
  case 1:
    gina_say(gina, "Mi serve aiuto...", voice());
    break;
  default:
    gina_say(gina, "Potrei chiedere a Carla.", voice());
    break;
  }
  gina_state.examine_float_count++;
}

static void talk_to_carla(void) {
  Mix_PlayChannel(-1, chunks[1].chunk, 0); // caw

  if (gina_state.float_state == FLOAT_STUCK_IN_TREE) {
    if (gina_state.has_grapes) {
      // Carla eats the grapes, flies up and drops the float back to Gina.
      gina_state.float_state = FLOAT_RETRIEVED;
      gina_state.has_grapes = false;
      gina_say(gina, "Mmm, che uva buona! Ecco il tuo salvagente!", voice());
      return;
    }
    if (!gina_state.has_basket) {
      gina_state.has_basket = true;
      gina_say(gina, "Ti aiuto se mi porti dell'uva. Prendi questo cestino!",
               voice());
      return;
    }
    gina_say(gina, "Hai gia' il cestino? Portami l'uva dalla vigna!", voice());
    return;
  }

  gina_say(gina, "Ciao Carla!", voice());
}

static void process_input(SDL_Event *event) {
  // Drag & drop (LIVELINESS.md Part 2): dragging the pointer from a press on
  // Gina picks her up; plain taps fall through to the hotspots.
  if (walk_actor_drag_event(gina, &walk_grid, event)) {
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
    // The hotspot table says what each region does (see init); anything else
    // is a walk toward the click.
    if (hotspots_handle_click(hotspots, LEN(hotspots), gina, &walk_grid,
                              m_pos)) {
      break;
    }
    walk_actor_to(gina, &walk_grid, (SDL_FPoint){m_pos.x, m_pos.y}, false,
                  NULL);
    break;
  }
}

static void update(float delta_time) { hen_update(gina, delta_time); }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});
  if (gina_state.float_state == FLOAT_STUCK_IN_TREE) {
    render_animation(renderer, float_boil, FLOAT_AT);
  }
  render_animation(renderer, carla_boil, CARLA_AT);
  hen_render(gina, renderer);
  // Once Carla has handed it over, Gina carries the basket with her.
  if (gina_state.has_basket) {
    render_image(renderer, basket,
                 (SDL_Point){gina->current_position.x - 55,
                             gina->current_position.y - 70});
  }
}

static void deinit(void) { hen_free(gina); }

static void on_scene_active(void) {
  gina->current_position = HEN_START;
  gina->target_position = HEN_START;
}

static void on_scene_inactive(void) {}

Scene tree_scene = {
    .init = init,
    .load_media = load_media,
    .process_input = process_input,
    .update = update,
    .render = render,
    .deinit = deinit,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .hotspots = hotspots,
    .hotspots_length = LEN(hotspots),
    .pois = pois,
    .pois_length = LEN(pois),
    .walk_grid = &walk_grid,
    .walk_mask_dir = "tree",
    .images = images,
    .images_length = LEN(images),
    .animations = animations,
    .animations_length = LEN(animations),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
