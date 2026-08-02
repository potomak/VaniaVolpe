//
//  tree.c
//  The tree where the pool float ends up stuck, and where Carla the crow
//  perches. Carla helps Gina get the float back — but only in exchange for
//  grapes, for which she first hands over a basket.
//

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#include "constants.h"
#include "game.h"
#include "image.h"
#include "sound.h"
#include "tween.h"

#include "gina_hen_at_the_pool.h"
#include "gina_state.h"
#include "gina_worn.h"
#include "hen.h"
#include "pool.h"
#include "tree.h"
#include "vine.h"

// Asset declarations generated from the adventure manifest (ASSETS.md).
#include "gina_assets.h"

static ImageData images[GINA_TREE_IMAGES_COUNT] = GINA_TREE_IMAGES_INIT;
static const ImageData *background = &images[GINA_TREE_IMAGE_BACKGROUND];

// The tappable objects boil (LIVELINESS.md Part 3): the stuck float squiggles
// while it can be examined, Carla while she can be talked to. The float plays
// the shared common/items sheet — it is the same float she loses at the pool
// and goes on to wear.
static AnimationData *float_boil;
static AnimationData *carla_boil;
// The same float, now lying on the grass. A second instance of the same boil,
// because one AnimationData carries one anim_at and this one squiggles in a
// different place from the stuck float above.
static AnimationData *ground_float_boil;
// The progress-reward burst over the float: plays once with the chime when
// Carla drops it back.
static AnimationData *celebration;
// Declared as data: the framework makes and loads these; init only aliases
// them. The tree dir's own sheets come first, then the sheets borrowed from
// other dirs — the float's, and the two destination tiles on the horizon: the
// poolside on the left, the vine on the right.
#define TREE_ANIM_FLOAT (GINA_TREE_ANIMS_COUNT)
#define TREE_ANIM_GROUND_FLOAT (GINA_TREE_ANIMS_COUNT + 1)
#define TREE_ANIM_TO_POOL (GINA_TREE_ANIMS_COUNT + 2)
#define TREE_ANIM_TO_VINE (GINA_TREE_ANIMS_COUNT + 3)
static AnimationData *animations[GINA_TREE_ANIMS_COUNT + 4];
static const SceneAnimSpec anim_specs[] = {
    GINA_TREE_ANIM_CELEBRATION_SPEC, GINA_TREE_ANIM_CARLA_BOIL_SPEC,
    GINA_ITEMS_ANIM_FLOAT_BOIL_SPEC, GINA_ITEMS_ANIM_FLOAT_BOIL_SPEC,
    GINA_NAV_ANIM_TO_POOL_BOIL_SPEC, GINA_NAV_ANIM_TO_VINE_BOIL_SPEC,
};

// Static sprite layer: just the backdrop. The stuck float and Carla are boils
// declared on their hotspots, which the framework plays and draws. render()
// keeps the dynamic draws: the falling float, the actor and her kit, and
// the reward burst.
static SceneSprite sprites[1];

// The scene's spoken lines: per-line dialogue chunks the framework speaks via
// generated say_<name>() helpers. Sound effects (caw, chime) live in the
// adventure's shared SFX bank (play_<name>()).
static ChunkData chunks[GINA_TREE_DIALOG_CHUNKS_COUNT] =
    GINA_TREE_DIALOG_CHUNKS_INIT;

static Hen *gina;
static const SDL_FPoint HEN_START = {400, 480};

static const SDL_Point FLOAT_AT = {500, 70};
static const SDL_Point CARLA_AT = {360, 150};
// The float-return reward burst, centred over the float's fall path.
static const SDL_Point CELEBRATION_AT = {425, 130};

// Where the float ends up once Carla knocks it loose: the top of the walkable
// strip, right under the branches it fell from.
static const SDL_Point GROUND_FLOAT_AT = {500, 430};

// The trade's two motions. Carla flaps up off her branch — that is what
// dislodges the float — and settles back; the float bounces down to the grass
// and stays there until Gina picks it up.
static Tween float_tween;
static bool float_falling;
static Tween carla_tween;
static bool carla_flying;

static const SDL_Rect FLOAT_HOTSPOT = {500, 70, 90, 60};
static const SDL_Rect GROUND_FLOAT_HOTSPOT = {500, 430, 90, 60};
static const SDL_Rect CARLA_HOTSPOT = {360, 150, 70, 70};
static Hotspot hotspots[5];

// Walk geometry: the ground under the tree, plus a path up either side to the
// tile above it. The paths overlap the near strip so the grid joins them into
// one region — a gap would leave a tile visible but unreachable.
static const SDL_Rect WALKABLE_RECTS[] = {
    {20, 430, 760, 150}, // the near ground, right across the scene
    {20, 250, 150, 190}, // up the left, toward the poolside
    {630, 250, 150, 190} // up the right, toward the vine
};
static const WalkArea WALK_AREA = {WALKABLE_RECTS, LEN(WALKABLE_RECTS), NULL,
                                   0};
static WalkGrid walk_grid;

// Depth (SCALING.md), in feet coordinates (the rects above are centre
// positions; feet sit half a walking frame lower). y_far is the feet line of
// the topmost walkable row, so she is smallest exactly where the paths run out.
// scale_far sits on the 0.6 floor scaling.h documents rather than below it.
static const ScaleRamp SCALE_RAMP = {
    .y_far = 310, .y_near = 639, .scale_far = 0.6F, .scale_near = 1.0F};

// The tiles, and the tappable areas around them — deliberately larger than the
// art, since a small finger aiming at a distant thing should not have to be
// precise.
static const SDL_Point POOL_TILE_AT = {30, 100};
static const SDL_Point VINE_TILE_AT = {680, 100};
static const SDL_Rect POOL_TILE_HOTSPOT = {10, 80, 130, 130};
static const SDL_Rect VINE_TILE_HOTSPOT = {660, 80, 130, 130};

static const SDL_Point FLOAT_LOOK_POI = {500, 470};
static const SDL_Point CARLA_POI = {400, 470};
// The far end of each path — one point per door, because it is both where she
// walks to use it and where she is standing when she comes through it. The
// hotspot POIs below are taken from these rather than repeating the numbers.
const SDL_FPoint GINA_TREE_ENTRY_FROM_POOL = {75, 280};
const SDL_FPoint GINA_TREE_ENTRY_FROM_VINE = {725, 280};

static SDL_Point door(SDL_FPoint at) {
  return (SDL_Point){(int)at.x, (int)at.y};
}
static SDL_Point pois[4];

// Interactions and hotspot gating (bodies below the loaders).
static bool float_is_stuck(void);
static bool float_is_on_ground(void);
static bool carla_is_perched(void);
static void examine_float(void);
static void pick_up_float(void);
static void talk_to_carla(void);
static void go_to_pool(void);
static void go_to_vine(void);

static void init(void) {
  walk_grid_init(&walk_grid, &WALK_AREA,
                 (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT}, "tree");

  float_boil = animations[TREE_ANIM_FLOAT];
  carla_boil = animations[GINA_TREE_ANIM_CARLA_BOIL];
  ground_float_boil = animations[TREE_ANIM_GROUND_FLOAT];
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
  hotspots[i++] = (Hotspot){.rect = GROUND_FLOAT_HOTSPOT,
                            .enabled = float_is_on_ground,
                            .poi = FLOAT_LOOK_POI,
                            .on_arrive = pick_up_float,
                            .active_anim = ground_float_boil,
                            .anim_at = GROUND_FLOAT_AT,
                            .anim_visible = float_is_on_ground};
  hotspots[i++] = (Hotspot){.rect = CARLA_HOTSPOT,
                            .poi = CARLA_POI,
                            .on_arrive = talk_to_carla,
                            .active_anim = carla_boil,
                            .anim_at = CARLA_AT,
                            .anim_visible = carla_is_perched};
  hotspots[i++] = (Hotspot){.rect = POOL_TILE_HOTSPOT,
                            .poi = door(GINA_TREE_ENTRY_FROM_POOL),
                            .on_arrive = go_to_pool,
                            .active_anim = animations[TREE_ANIM_TO_POOL],
                            .anim_at = POOL_TILE_AT};
  hotspots[i] = (Hotspot){.rect = VINE_TILE_HOTSPOT,
                          .poi = door(GINA_TREE_ENTRY_FROM_VINE),
                          .on_arrive = go_to_vine,
                          .active_anim = animations[TREE_ANIM_TO_VINE],
                          .anim_at = VINE_TILE_AT};

  i = 0;
  pois[i++] = FLOAT_LOOK_POI;
  pois[i++] = CARLA_POI;
  pois[i++] = door(GINA_TREE_ENTRY_FROM_POOL);
  pois[i] = door(GINA_TREE_ENTRY_FROM_VINE);
}

// ── interactions
// ──────────────────────────────────────────────────────────────

static bool float_is_stuck(void) {
  return gina_state.float_state == FLOAT_STUCK_IN_TREE && !float_falling;
}

static bool float_is_on_ground(void) {
  return gina_state.float_state == FLOAT_ON_GROUND;
}

// Carla is on her branch except while she is up in the air; the scene draws her
// flight itself, so the framework's boil draw stands down for it.
static bool carla_is_perched(void) { return !carla_flying; }

// Leaving: the destination says where its own door is, so this scene never
// assumes anything about the shape of the next one.
static void go_to_pool(void) {
  set_active_scene_at(GINA_POOL, GINA_POOL_ENTRY_FROM_TREE);
}

static void go_to_vine(void) {
  set_active_scene_at(GINA_VINE, GINA_VINE_ENTRY_FROM_TREE);
}

static void examine_float(void) {
  switch (gina_state.examine_float_count) {
  case 0:
    say_cant_reach();
    break;
  case 1:
    say_need_help();
    break;
  default:
    say_ask_carla();
    break;
  }
  gina_state.examine_float_count++;
}

// The float has settled on the grass. The puzzle is solved — Carla has been
// paid and the float is down — but it is not Gina's until she goes and gets it.
static void float_dropped(void) {
  float_falling = false;
  gina_state.float_state = FLOAT_ON_GROUND;
}

// Carla is back on her branch, so the framework resumes drawing her boil.
static void carla_landed(void) { carla_flying = false; }

// Gina walks over and takes it: from here she wears it (see gina_worn).
static void pick_up_float(void) { gina_state.float_state = FLOAT_RETRIEVED; }

static void talk_to_carla(void) {
  // While the float is mid-fall the trade already happened; ignore the tap.
  if (float_falling) {
    return;
  }
  play_caw();

  if (gina_state.float_state == FLOAT_STUCK_IN_TREE) {
    if (gina_state.has_grapes) {
      // Carla eats the grapes and drops the float back to Gina: it bounces down
      // from the branches while she says thanks, with a chime + confetti burst
      // over the float as it comes back.
      // The grapes go over in the basket, so both leave her at once.
      gina_state.has_grapes = false;
      gina_state.has_basket = false;
      say_carla_thanks();
      play_chime();
      play_animation(celebration, NULL);
      // Carla flaps up off her branch — that is what shakes the float loose —
      // and settles back. A tween from her perch to itself is a pure vertical
      // hop, since the arc is added on top of the (here zero) path.
      carla_flying = true;
      tween_start(&carla_tween, (SDL_FPoint){CARLA_AT.x, CARLA_AT.y},
                  (SDL_FPoint){CARLA_AT.x, CARLA_AT.y}, 800, TWEEN_EASE_OUT,
                  carla_landed);
      carla_tween.arc_height = 60;
      // The float bounces down to the grass and waits there to be picked up.
      float_falling = true;
      tween_start(&float_tween, (SDL_FPoint){FLOAT_AT.x, FLOAT_AT.y},
                  (SDL_FPoint){GROUND_FLOAT_AT.x, GROUND_FLOAT_AT.y}, 900,
                  TWEEN_BOUNCE, float_dropped);
      return;
    }
    if (!gina_state.has_basket) {
      gina_state.has_basket = true;
      say_carla_offer();
      return;
    }
    say_carla_reminder();
    return;
  }

  say_hi_carla();
}

static void update(float delta_time) {
  actor_update(gina, delta_time);
  if (float_falling) {
    tween_update(&float_tween, delta_time);
  }
  if (carla_flying) {
    tween_update(&carla_tween, delta_time);
  }
}

static void render(SDL_Renderer *renderer) {
  // Backdrop, the stuck float and Carla are static sprites (drawn by the
  // framework). render() draws the dynamic layer: the falling float, the actor
  // and whatever she is carrying, and the reward burst.
  if (float_falling) {
    // The drop: the float bounces down from the branches.
    SDL_FPoint p = tween_pos(&float_tween);
    render_animation(renderer, float_boil, (SDL_Point){(int)p.x, (int)p.y});
  }
  if (carla_flying) {
    // Her hop off the branch and back; the framework's boil draw is gated off
    // for the duration (carla_is_perched).
    SDL_FPoint p = tween_pos(&carla_tween);
    render_animation(renderer, carla_boil, (SDL_Point){(int)p.x, (int)p.y});
  }
  render_action_layer(renderer, &SCALE_RAMP, NULL, 0, &gina, 1);
  gina_render_worn(renderer, gina);
  // The reward burst over the returning float while the chime plays.
  if (celebration->is_playing) {
    render_animation(renderer, celebration, CELEBRATION_AT);
  }
}

static void on_scene_active(void) {
  // Her default spot. Arriving from another scene overrides this straight
  // after, with that scene passing one of the GINA_TREE_ENTRY_FROM_* points
  // above (set_active_scene_at).
  actor_place(gina, HEN_START);
}

static void on_scene_inactive(void) {}

Scene tree_scene = {
    .init = init,
    .actor = &gina,
    .actor_spec = &HEN_SPEC,
    .actor_start = {400, 480},
    .update = update,
    .render = render,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .hotspots = hotspots,
    .hotspots_length = LEN(hotspots),
    .pois = pois,
    .pois_length = LEN(pois),
    .scale_ramp = &SCALE_RAMP,
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
