//
//  field.c
//  Developer demo & reference implementation for the depth features
//  (DEPTH_AND_CAMERA.md): a 1600x600 field where clicking walks a
//  two-variant fox around three bushes, with a camera following her.
//
//  - Phase 1, y-sorted action layer: the bushes are Props drawn by
//    render_action_layer, so the fox passes behind or in front of each one
//    depending on where her feet are relative to its baseline.
//  - Phase 2, depth bands + sprite variants: the scene declares two
//    DepthBands; when the fox's feet cross y = 480 she switches between the
//    hand-drawn near set and a generated 0.6x far set (placeholders from
//    tools/gen_depth_variants.py), and her walking speed scales to match.
//  - Phase 3, camera + scene coordinates: the scene is twice the window's
//    width. Everything here is authored in scene coordinates — the Camera
//    handed to the engine below is the only camera-related line in the
//    scene, and clicking near a screen edge walks the fox toward ground the
//    camera then scrolls into view.
//  - Phase 4, planes + parallax: the field is built from planes instead of one
//    background — a fixed sky (parallax 0), hills that drift slowly (0.4), the
//    ground the fox walks on (1), and a foreground bush strip (1.15) she
//    passes behind. The engine draws them; the scene only declares the tables.
//
//  Toggle the debug overlay (D) to see the walk grid; the band boundary is
//  where the far lawn meets the near lawn in the ground plane.
//

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "actor.h"
#include "constants.h"
#include "image.h"

#include "field.h"

// The depth-band bushes (Phase 1/2 props) are the only scene image; the field
// itself is planes (below).
static ImageData images[1] = {
    {NULL, "bush.png", "field", 0, 0},
};

// Parallax layers, back to front. The engine draws bg_planes behind the action
// layer and fg_planes in front, each shifted by origin - camera * parallax.
// The coverage rule (image must span WINDOW + parallax * (scene - WINDOW))
// sets each image's width: sky 800, hills 1120, ground 1600, bushes 1720.
static Plane bg_planes[3] = {
    {.image = {NULL, "sky.png", "field", 0, 0}, .parallax = 0.0F},
    {.image = {NULL, "hills.png", "field", 0, 0}, .parallax = 0.4F},
    {.image = {NULL, "ground.png", "field", 0, 0}, .parallax = 1.0F},
};
static Plane fg_planes[1] = {
    // A parallax > 1 strip along the bottom: a walk-behind foreground with no
    // prop needed. origin.y drops it to the bottom of the window.
    {.image = {NULL, "bushes.png", "field", 0, 0},
     .parallax = 1.15F,
     .origin = {0, 460}},
};

// ── the demo actor: the fox in two depth variants ────────────────────────────
// The near set is the fox's own art; the far set is generated from it at 0.6
// scale. A far variant also walks at 0.6 speed, so her apparent pace doesn't
// change with her size.

static const ActorAnimSpec NEAR_ANIMS[] = {
    {WALKING, "walking.png", "walking.anim", 4, LOOP},
    {SITTING, "sitting.png", "sitting.anim", 3, LOOP},
};
static const ActorAnimSpec FAR_ANIMS[] = {
    {WALKING, "walking_far.png", "walking_far.anim", 4, LOOP},
    {SITTING, "sitting_far.png", "sitting_far.anim", 3, LOOP},
};
static const ActorVariantSpec DEMO_FOX_VARIANTS[] = {
    {.anims = NEAR_ANIMS, .anims_length = 2, .speed_scale = 1.0F},
    {.anims = FAR_ANIMS, .anims_length = 2, .speed_scale = 0.6F},
};
static const ActorSpec DEMO_FOX_SPEC = {
    .id = "demo_fox",
    .display_name = "Vania",
    .assets_dir = "fox",
    .velocity = 200,
    .idle_state = SITTING,
    .move_state = WALKING,
    .variants = DEMO_FOX_VARIANTS,
    .variants_length = 2,
};

static Actor *fox;
static const SDL_FPoint FOX_START = {600, 500};

// ── a scrolling scene: twice the window wide, camera follows the fox ─────────
static const SDL_Point SCENE_SIZE = {1600, 600};
static Camera camera;

// ── walk geometry: one open strip, nothing blocked
// ────────────────────────────
static const SDL_Rect WALKABLE_RECTS[] = {{40, 260, 1520, 300}};
static const WalkArea WALK_AREA = {WALKABLE_RECTS, LEN(WALKABLE_RECTS), NULL,
                                   0};
static WalkGrid walk_grid;

// ── depth bands (scene data): far set above, near set below ──────────────────
// y_top is compared against actor_feet_y. Band 0 starts at 0 by convention.
static const DepthBand DEPTH_BANDS[] = {
    {0, 1},   // feet above 480: variant 1, the far set
    {480, 0}, // feet at or below 480: variant 0, the near set
};

// ── props: bushes at both depths and both scene halves
// ────────────────────────
static Prop props[3];
static const SDL_Point FAR_BUSH_POS = {310, 310};
static const SDL_Point NEAR_BUSH_POS = {430, 460};
static const SDL_Point EAST_BUSH_POS = {1250, 420};

static SDL_Point m_pos;

static void init(void) {
  fox = make_actor(&DEMO_FOX_SPEC, FOX_START);

  camera_init(&camera, SCENE_SIZE, fox);

  walk_grid_init(&walk_grid, &WALK_AREA, SCENE_SIZE, NULL);

  // Baselines are set in on_scene_active, once the image heights are known
  // (the ground line is the bottom edge of each sprite). The props share one
  // ImageData — a Prop points into the images table, it doesn't own a copy.
  props[0] = (Prop){.image = &images[0], .pos = FAR_BUSH_POS, .visible = true};
  props[1] = (Prop){.image = &images[0], .pos = NEAR_BUSH_POS, .visible = true};
  props[2] = (Prop){.image = &images[0], .pos = EAST_BUSH_POS, .visible = true};
}

static bool load_media(SDL_Renderer *renderer) {
  return actor_load_media(fox, renderer);
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
    // Walk toward the click, clamped to the walkable strip.
    walk_actor_to(fox, &walk_grid, (SDL_FPoint){m_pos.x, m_pos.y}, false, NULL);
    break;
  }
}

static void update(float delta_time) {
  // The whole Phase 2 opt-in: pick the band under the fox's feet, before her
  // own update so this frame already renders (and moves) with the right set.
  actor_set_variant(
      fox, depth_variant_for(DEPTH_BANDS, LEN(DEPTH_BANDS), actor_feet_y(fox)));
  actor_update(fox, delta_time);
}

static void render(SDL_Renderer *renderer) {
  // Just the action layer: the engine draws the sky/hills/ground planes behind
  // this and the foreground bush strip in front. The whole Phase 1 opt-in:
  // props and actor draw y-sorted in one call.
  render_action_layer(renderer, props, LEN(props), (Actor *[]){fox}, 1);
}

static void deinit(void) { actor_free(fox); }

static void on_scene_active(void) {
  fox->current_position = FOX_START;
  fox->target_position = FOX_START;

  // Anchor each bush's ground line to its sprite's bottom edge. Image sizes
  // are known here: the framework loads scene images right after load_media,
  // long before any scene becomes active.
  for (int i = 0; i < (int)LEN(props); i++) {
    props[i].baseline = props[i].pos.y + props[i].image->height;
  }
}

static void on_scene_inactive(void) {}

Scene field_scene = {
    .init = init,
    .load_media = load_media,
    .process_input = process_input,
    .update = update,
    .render = render,
    .deinit = deinit,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .walk_grid = &walk_grid,
    .camera = &camera,
    .images = images,
    .images_length = LEN(images),
    .bg_planes = bg_planes,
    .bg_planes_length = LEN(bg_planes),
    .fg_planes = fg_planes,
    .fg_planes_length = LEN(fg_planes),
};
