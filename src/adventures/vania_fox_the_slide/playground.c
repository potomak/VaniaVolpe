//
//  playground.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>

#include "constants.h"
#include "fox.h"
#include "game.h"
#include "image.h"
#include "sound.h"
#include "vania_fox_the_slide.h"

#include "playground.h"

// Asset declarations generated from the adventure manifest (ASSETS.md). The
// chunks table mixes directories (scene SFX and the dialog lines), so it
// lists per-entry _INIT rows instead of one table macro.
#include "vania_assets.h"

static ImageData images[VANIA_PLAYGROUND_IMAGES_COUNT] =
    VANIA_PLAYGROUND_IMAGES_INIT;
static const ImageData *background =
    &images[VANIA_PLAYGROUND_IMAGE_PLAYGROUND_BACKGROUND];
static const ImageData *squirrel = &images[VANIA_PLAYGROUND_IMAGE_SQUIRREL];
static const ImageData *peg = &images[VANIA_PLAYGROUND_IMAGE_PEG];
static const ImageData *fixed_peg = &images[VANIA_PLAYGROUND_IMAGE_FIXED_PEG];
static const ImageData *acorns = &images[VANIA_PLAYGROUND_IMAGE_ACORNS];

static Fox *fox;

// Props the fox can stand behind or in front of (drawn y-sorted with the fox
// by render_action_layer; visibility mirrors the scene state each frame in
// render). The other peg/acorn placements (in the tree, carried, on the
// slide) never overlap the fox on the ground and stay manual draws.
static Prop props[2];
static Prop *acorn_pile_prop = &props[0];  // fallen acorns, on the ground
static Prop *dropped_peg_prop = &props[1]; // peg the squirrel dropped

// The static sprite layer (SCENES.md milestone 2): backdrop and the squirrel.
// Everything else (the peg and acorns in their many states, the y-sorted
// props, the fox) is dynamic and stays in render().
static SceneSprite sprites[2];

// The scene's spoken lines, played via generated say_<name>() helpers
// (SCENES.md milestone 4). Sound effects (acorns_falling, peg_falling) are in
// the adventure's shared SFX bank (play_<name>()); music is declared on the
// Scene
// (.music); the framework plays and frees both.
static ChunkData chunks[VANIA_PLAYGROUND_DIALOG_CHUNKS_COUNT] =
    VANIA_PLAYGROUND_DIALOG_CHUNKS_INIT;

// Mouse position
static SDL_Point m_pos;

// Hotspots
static const SDL_Rect SLIDE_HOTSPOT = {253, 157, 302, 285};
static const SDL_Rect SQUIRREL_HOTSPOT = {70, 147, 113, 97};
static const SDL_Rect ACORNS_TREE_HOTSPOT = {668, 201, 126, 118};
static const SDL_Rect ACORNS_FLOOR_HOTSPOT = {667, 500, 105, 98};
static const SDL_Rect PEG_HOTSPOT = {83, 460, 91, 94};
static Hotspot hotspots[5];

// Walk geometry: the ground strip plus two side pockets; the slide/sandbox
// structure in the middle is blocked.
static const SDL_Rect WALKABLE_RECTS[] = {
    {63, 333, 645, 262},
    {707, 491, 88, 105},
    {4, 531, 68, 63},
};
static const SDL_Rect BLOCKED_RECTS[] = {{236, 131, 403, 312}};
static const WalkArea WALK_AREA = {WALKABLE_RECTS, LEN(WALKABLE_RECTS),
                                   BLOCKED_RECTS, LEN(BLOCKED_RECTS)};
static WalkGrid walk_grid;

// Points of interest
static const SDL_Point SLIDE_POI = {276, 454};
static const SDL_Point SQUIRREL_POI = {219, 455};
static const SDL_Point ACORNS_POI = {635, 498};
static SDL_Point pois[3];

// Objects position
static const SDL_FPoint START_SLIDE_POS = (SDL_FPoint){326, 142};
static const SDL_Point ACORN_PILE_POS = {687, 503};
static const SDL_Point DROPPED_PEG_POS = {109, 474};

// Other constants
static const float SLIDE_SIGMOID_HEIGHT = 240;
static const float SLIDE_X_VELOCITY = 100;

// Scene state
static bool has_slide_been_fixed;
static bool have_acorns_fallen;
static bool has_peg_been_dropped;
// Inventory (adventure state, formerly on the fox)
static bool has_peg;
static bool has_acorns;
static bool has_started_sliding;
static float slide_x;
static int examine_slide_count;
static int examine_squirrel_count;
static int slides_count;

// Interactions (bodies below the loaders).
static void maybe_use_slide(void);
static void maybe_get_peg(void);
static void make_acorns_fall(void);
static void pickup_acorns(void);
static void pickup_peg(void);

// Hotspot gating (see the table in init).
static bool squirrel_has_the_peg(void) { return !has_peg_been_dropped; }

static bool acorns_in_the_tree(void) { return !have_acorns_fallen; }

static bool acorns_on_the_ground(void) {
  return have_acorns_fallen && !has_acorns && !has_peg_been_dropped;
}

static bool peg_on_the_ground(void) {
  return has_peg_been_dropped && !has_peg && !has_slide_been_fixed;
}

static void init(void) {
  fox = make_fox((SDL_FPoint){580, 457});

  walk_grid_init(&walk_grid, &WALK_AREA,
                 (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT}, "playground");

  // Baselines are set in on_scene_active, once the image heights are known
  // (the ground line is the bottom edge of each sprite).
  *acorn_pile_prop = (Prop){.image = &images[VANIA_PLAYGROUND_IMAGE_ACORNS],
                            .pos = ACORN_PILE_POS};
  *dropped_peg_prop = (Prop){.image = &images[VANIA_PLAYGROUND_IMAGE_PEG],
                             .pos = DROPPED_PEG_POS};

  sprites[0] = (SceneSprite){.image = background, .at = {0, 0}};
  sprites[1] = (SceneSprite){.image = squirrel, .at = {85, 160}};

  int i = 0;
  hotspots[i++] = (Hotspot){
      .rect = SLIDE_HOTSPOT, .poi = SLIDE_POI, .on_arrive = maybe_use_slide};
  hotspots[i++] = (Hotspot){.rect = SQUIRREL_HOTSPOT,
                            .enabled = squirrel_has_the_peg,
                            .poi = SQUIRREL_POI,
                            .on_arrive = maybe_get_peg};
  hotspots[i++] = (Hotspot){.rect = ACORNS_TREE_HOTSPOT,
                            .enabled = acorns_in_the_tree,
                            .poi = ACORNS_POI,
                            .on_arrive = make_acorns_fall};
  hotspots[i++] = (Hotspot){.rect = ACORNS_FLOOR_HOTSPOT,
                            .enabled = acorns_on_the_ground,
                            .poi = ACORNS_POI,
                            .on_arrive = pickup_acorns};
  // The peg fell next to the squirrel's tree, so the walk target is the same.
  hotspots[i++] = (Hotspot){.rect = PEG_HOTSPOT,
                            .enabled = peg_on_the_ground,
                            .poi = SQUIRREL_POI,
                            .on_arrive = pickup_peg};

  i = 0;
  pois[i++] = SLIDE_POI;
  pois[i++] = SQUIRREL_POI;
  pois[i++] = ACORNS_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  // Music is loaded by the framework from .music; only the fox remains.
  return fox_load_media(fox, renderer);
}

static void maybe_use_slide(void) {
  // If peg is in the inventory fix the slide
  if (has_peg) {
    has_peg = false;
    has_slide_been_fixed = true;
    // Placeholder: reuses the peg-falling sound until a fix-slide SFX is
    // recorded (see the manifest).
    play_peg_falling();
    // TODO: Wait for sound effect to end before starting to talk
    say_examine_fixed_slide();
    return;
  }

  // Else if slide is working use: win the game
  if (has_slide_been_fixed) {
    fox->current_position = START_SLIDE_POS;
    say_sliding_down();
    has_started_sliding = true;
    return;
  }

  // Else give hint to fix the slide
  if (examine_slide_count < 1) {
    say_examine_slide_1();
  } else {
    say_examine_slide_2();
  }
  examine_slide_count++;
}

static void maybe_get_peg(void) {
  // If acorns are in the inventory exchange them for the peg
  if (has_acorns) {
    has_acorns = false;
    has_peg_been_dropped = true;
    play_peg_falling();
    return;
  }

  // Else give hint to find acorns
  if (examine_squirrel_count < 1) {
    say_examine_squirrel_1();
  } else {
    say_examine_squirrel_2();
  }
  examine_squirrel_count++;
}

static void make_acorns_fall(void) {
  have_acorns_fallen = true;
  play_acorns_falling();
}

static void pickup_acorns(void) { has_acorns = true; }

static void pickup_peg(void) { has_peg = true; }

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEMOTION:
    // Get mouse position
    m_pos.x = event->motion.x;
    m_pos.y = event->motion.y;
    break;
  case SDL_MOUSEBUTTONDOWN:
    // Hit-test the click's own coordinates (#64): the cached motion position
    // can be stale — e.g. a repeated tap with no motion in between while the
    // camera moved.
    m_pos.x = event->button.x;
    m_pos.y = event->button.y;
    // If has already slid three or more times go to outro
    if (slides_count > 2) {
      set_active_scene(OUTRO);
      break;
    }
    // The hotspot table says what each region does (see init). Otherwise:
    // walk to the clicked point (routed around the blocked area), or to the
    // nearest reachable point if the click is outside the walkable area.
    if (hotspots_handle_click(hotspots, LEN(hotspots), fox, &walk_grid,
                              m_pos)) {
      break;
    }
    walk_actor_to(fox, &walk_grid, (SDL_FPoint){m_pos.x, m_pos.y}, false, NULL);
    break;
  }
}

// Used to compute the y position of the fox sliding down
static float sigmoid(float x) {
  float steepness = -0.025;
  float center = 130;
  float c = steepness * (x - center);
  return SLIDE_SIGMOID_HEIGHT / (1 + expf(c));
}

static void update(float delta_time) {
  fox_update(fox, delta_time);

  float slide_y;
  if (has_started_sliding) {
    slide_y = sigmoid(slide_x);
    fox->current_position =
        (SDL_FPoint){START_SLIDE_POS.x + slide_x, START_SLIDE_POS.y + slide_y};
    slide_x += SLIDE_X_VELOCITY * delta_time;
    if (slide_x > 270) {
      slide_x = 0;
      has_started_sliding = false;
      slides_count++;
      // The slide drops the fox inside the blocked structure; the router
      // snaps the start point and walks her back out legally.
      walk_actor_to(fox, &walk_grid, (SDL_FPoint){SLIDE_POI.x, SLIDE_POI.y},
                    true, NULL);
    }
  }
}

static void render(SDL_Renderer *renderer) {
  // Backdrop and squirrel are static sprites (drawn by the framework). render()
  // draws the dynamic layer: the peg/acorns in their states and the y-sorted
  // props + fox.

  // The on-the-ground placements are props (y-sorted with the fox below);
  // every other placement never overlaps the fox and stays a manual draw.
  dropped_peg_prop->visible =
      has_peg_been_dropped && !has_peg && !has_slide_been_fixed;
  acorn_pile_prop->visible =
      have_acorns_fallen && !has_acorns && !has_peg_been_dropped;

  if (has_peg_been_dropped) {
    if (has_peg) {
      render_image(renderer, peg,
                   (SDL_Point){fox->current_position.x - 20,
                               fox->current_position.y - 100});
    } else if (has_slide_been_fixed) {
      render_image(renderer, fixed_peg, (SDL_Point){267, 263});
    }
  } else {
    render_image(renderer, peg, (SDL_Point){127, 175});
  }

  if (have_acorns_fallen) {
    if (has_acorns) {
      render_image(renderer, acorns,
                   (SDL_Point){fox->current_position.x - 50,
                               fox->current_position.y - 100});
    } else if (has_peg_been_dropped) {
      render_image(renderer, acorns, (SDL_Point){137, 135});
    }
  } else {
    render_image(renderer, acorns, (SDL_Point){698, 225});
  }

  render_action_layer(renderer, props, LEN(props), (Actor *[]){fox}, 1);
}

static void deinit(void) { fox_free(fox); }

static void on_scene_active(void) {
  // Anchor each prop's ground line to its sprite's bottom edge. Image sizes
  // are known here: the framework loads scene images right after load_media,
  // long before any scene becomes active.
  for (int i = 0; i < (int)LEN(props); i++) {
    props[i].baseline = props[i].pos.y + props[i].image->height;
  }

  // Initialize scene state
  has_slide_been_fixed = false;
  have_acorns_fallen = false;
  has_peg_been_dropped = false;
  has_started_sliding = false;
  slide_x = 0;
  examine_slide_count = 0;
  examine_squirrel_count = 0;
  slides_count = 0;

  // Initialize fox state
  has_peg = false;
  has_acorns = false;
}

static void on_scene_inactive(void) {}

Scene playground_scene = {
    .init = init,
    .load_media = load_media,
    // Custom process_input for the after-three-slides win check; .actor still
    // declared so the generated say_<name>() helpers can speak through it.
    .process_input = process_input,
    .actor = &fox,
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
    .walk_mask_dir = "playground",
    .sprites = sprites,
    .sprites_length = LEN(sprites),
    .images = images,
    .images_length = LEN(images),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
    .music = VANIA_MUSIC_CHUNK_PLAYGROUND_ASSET_INIT,
};
