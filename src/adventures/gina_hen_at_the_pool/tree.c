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
#include "tween.h"

#include "gina_hen_at_the_pool.h"
#include "gina_state.h"
#include "hen.h"
#include "tree.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "gina_assets.h"

static ImageData images[GINA_TREE_IMAGES_COUNT] = GINA_TREE_IMAGES_INIT;
static const ImageData *background = &images[GINA_TREE_IMAGE_BACKGROUND];
static const ImageData *basket = &images[GINA_TREE_IMAGE_BASKET];

// The tappable objects boil (LIVELINESS.md Part 3): the stuck float squiggles
// while it can be examined, Carla while she can be talked to. Same size as the
// old still PNGs, so the render positions are unchanged.
static AnimationData *float_boil;
static AnimationData *carla_boil;
// The progress-reward burst over the float (#118): plays once with the chime
// when Carla drops it back.
static AnimationData *celebration;
// Declared as data (SCENES.md milestone 1): the framework makes and loads
// these; init only aliases them. Order matches the generated indices.
static AnimationData *animations[GINA_TREE_ANIMS_COUNT];
static const SceneAnimSpec anim_specs[] = {
    GINA_TREE_ANIM_CELEBRATION_SPEC,
    GINA_TREE_ANIM_FLOAT_BOIL_SPEC,
    GINA_TREE_ANIM_CARLA_BOIL_SPEC,
};

// The static sprite layer (SCENES.md milestone 2): just the backdrop. The
// stuck float and Carla are boils declared on their hotspots (milestone 3),
// which the framework plays and draws. render() keeps the dynamic draws: the
// falling float, the actor, the carried basket and the reward burst.
static SceneSprite sprites[1];

// The scene's own chunks (voice/caw) plus the shared completion chime (#118),
// which lives in another dir — so the table lists per-entry _INIT rows and
// appends the chime after the tree's own chunks.
static ChunkData chunks[GINA_TREE_CHUNKS_COUNT + 1] = {
    GINA_TREE_CHUNK_VOICE_INIT,
    GINA_TREE_CHUNK_CAW_INIT,
    GINA_MINIGAMES_CHUNK_CHIME_INIT,
};
static const ChunkData *voice(void) { return &chunks[GINA_TREE_CHUNK_VOICE]; }
static const ChunkData *chime_sound = &chunks[GINA_TREE_CHUNKS_COUNT];

static SDL_Point m_pos;

static Hen *gina;
static const SDL_FPoint HEN_START = {400, 480};

static const SDL_Point FLOAT_AT = {500, 70};
static const SDL_Point CARLA_AT = {360, 150};
// The float-return reward burst (#118), centred over the float's fall path.
static const SDL_Point CELEBRATION_AT = {425, 130};

// The float's drop from the branches after the trade (#107): a bouncing fall
// to the ground; the state flips to retrieved when it settles.
static Tween float_tween;
static bool float_falling;

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

  float_boil = animations[GINA_TREE_ANIM_FLOAT_BOIL];
  carla_boil = animations[GINA_TREE_ANIM_CARLA_BOIL];
  celebration = animations[GINA_TREE_ANIM_CELEBRATION];

  sprites[0] = (SceneSprite){.image = background, .at = {0, 0}};

  int i = 0;
  hotspots[i++] = (Hotspot){.rect = FLOAT_HOTSPOT,
                            .enabled = float_is_stuck,
                            .poi = FLOAT_LOOK_POI,
                            .on_arrive = examine_float,
                            .active_anim = float_boil,
                            .anim_at = FLOAT_AT,
                            .anim_visible = float_is_stuck};
  hotspots[i++] = (Hotspot){.rect = CARLA_HOTSPOT,
                            .poi = CARLA_POI,
                            .on_arrive = talk_to_carla,
                            .active_anim = carla_boil,
                            .anim_at = CARLA_AT};
  hotspots[i++] = (Hotspot){
      .rect = POOL_NAV_HOTSPOT, .immediate = true, .on_arrive = go_to_pool};
  hotspots[i++] = (Hotspot){
      .rect = VINE_NAV_HOTSPOT, .immediate = true, .on_arrive = go_to_vine};

  i = 0;
  pois[i++] = FLOAT_LOOK_POI;
  pois[i++] = CARLA_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  // The boils and the celebration are loaded by the framework from anim_specs;
  // only the actor remains for the scene to load.
  return hen_load_media(gina, renderer);
}

// ── interactions
// ──────────────────────────────────────────────────────────────

static bool float_is_stuck(void) {
  return gina_state.float_state == FLOAT_STUCK_IN_TREE && !float_falling;
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

// The float has settled on the ground: only now is it truly retrieved.
static void float_dropped(void) {
  float_falling = false;
  gina_state.float_state = FLOAT_RETRIEVED;
}

static void talk_to_carla(void) {
  // While the float is mid-fall the trade already happened; ignore the tap.
  if (float_falling) {
    return;
  }
  Mix_PlayChannel(-1, chunks[GINA_TREE_CHUNK_CAW].chunk, 0);

  if (gina_state.float_state == FLOAT_STUCK_IN_TREE) {
    if (gina_state.has_grapes) {
      // Carla eats the grapes and drops the float back to Gina (#107): it
      // bounces down from the branches while she says thanks. Progress reward
      // (#118): chime + confetti burst over the float as it comes back.
      gina_state.has_grapes = false;
      gina_say(gina, "Mmm, che uva buona! Ecco il tuo salvagente!", voice());
      Mix_PlayChannel(-1, chime_sound->chunk, 0);
      play_animation(celebration, NULL);
      float_falling = true;
      tween_start(&float_tween, (SDL_FPoint){FLOAT_AT.x, FLOAT_AT.y},
                  (SDL_FPoint){FLOAT_AT.x, 430}, 900, TWEEN_BOUNCE,
                  float_dropped);
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

static void update(float delta_time) {
  hen_update(gina, delta_time);
  if (float_falling) {
    tween_update(&float_tween, delta_time);
  }
}

static void render(SDL_Renderer *renderer) {
  // Backdrop, the stuck float and Carla are static sprites (drawn by the
  // framework). render() draws the dynamic layer: the falling float, the
  // actor, the carried basket and the reward burst.
  if (float_falling) {
    // The drop: the float bounces down from the branches.
    SDL_FPoint p = tween_pos(&float_tween);
    render_animation(renderer, float_boil, (SDL_Point){(int)p.x, (int)p.y});
  }
  hen_render(gina, renderer);
  // Once Carla has handed it over, Gina carries the basket with her.
  if (gina_state.has_basket) {
    render_image(renderer, basket,
                 (SDL_Point){gina->current_position.x - 55,
                             gina->current_position.y - 70});
  }
  // The reward burst over the returning float while the chime plays.
  if (celebration->is_playing) {
    render_animation(renderer, celebration, CELEBRATION_AT);
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
    .sprites = sprites,
    .sprites_length = LEN(sprites),
    .images = images,
    .images_length = LEN(images),
    .animations = animations,
    .animations_length = LEN(animations),
    .anim_specs = anim_specs,
    .anim_specs_length = LEN(anim_specs),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
