//
//  playground.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <math.h>
#include <stdbool.h>

#include "constants.h"
#include "fox.h"
#include "game.h"
#include "image.h"
#include "sound.h"
#include "vania_fox_the_slide.h"

#include "playground.h"

static ImageData images[5] = {
    {NULL, "playground_background.png", "playground", 0, 0},
    {NULL, "squirrel.png", "playground", 0, 0},
    {NULL, "peg.png", "playground", 0, 0},
    {NULL, "fixed_peg.png", "playground", 0, 0},
    {NULL, "acorns.png", "playground", 0, 0},
};
static const ImageData *background = &images[0];
static const ImageData *squirrel = &images[1];
static const ImageData *peg = &images[2];
static const ImageData *fixed_peg = &images[3];
static const ImageData *acorns = &images[4];

static Fox *fox;

// Props the fox can stand behind or in front of (drawn y-sorted with the fox
// by render_action_layer; visibility mirrors the scene state each frame in
// render). The other peg/acorn placements (in the tree, carried, on the
// slide) never overlap the fox on the ground and stay manual draws.
static Prop props[2];
static Prop *acorn_pile_prop = &props[0];  // fallen acorns, on the ground
static Prop *dropped_peg_prop = &props[1]; // peg the squirrel dropped

// Music
static Mix_Music *music = NULL;

// Sound effects and dialog
static ChunkData chunks[9] = {
    {NULL, "acorns_falling.wav", "playground"},
    {NULL, "peg_falling.wav", "playground"},
    // TODO: Record new sound
    {NULL, "peg_falling.wav", "playground"},
    {NULL, "examine_fixed_slide.wav", "playground/dialog"},
    {NULL, "examine_slide_1.wav", "playground/dialog"},
    {NULL, "examine_slide_2.wav", "playground/dialog"},
    {NULL, "examine_squirrel_1.wav", "playground/dialog"},
    {NULL, "examine_squirrel_2.wav", "playground/dialog"},
    {NULL, "sliding_down.wav", "playground/dialog"},
};
static const ChunkData *acorns_falling_sound = &chunks[0];
static const ChunkData *peg_falling_sound = &chunks[1];
static const ChunkData *fix_slide_sound = &chunks[2];
static const ChunkData *examine_fixed_slide = &chunks[3];
static const ChunkData *examine_slide_1 = &chunks[4];
static const ChunkData *examine_slide_2 = &chunks[5];
static const ChunkData *examine_squirrel_1 = &chunks[6];
static const ChunkData *examine_squirrel_2 = &chunks[7];
static const ChunkData *sliding_down = &chunks[8];

// Mouse position
static SDL_Point m_pos;

// Hotspots
static const SDL_Rect SLIDE_HOTSPOT = {253, 157, 302, 285};
static const SDL_Rect SQUIRREL_HOTSPOT = {70, 147, 113, 97};
static const SDL_Rect ACORNS_TREE_HOTSPOT = {668, 201, 126, 118};
static const SDL_Rect ACORNS_FLOOR_HOTSPOT = {667, 500, 105, 98};
static const SDL_Rect PEG_HOTSPOT = {83, 460, 91, 94};
static SDL_Rect hotspots[5];

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

static void init(void) {
  fox = make_fox((SDL_FPoint){580, 457});

  walk_grid_init(&walk_grid, &WALK_AREA, "playground");

  // Baselines are set in on_scene_active, once the image heights are known
  // (the ground line is the bottom edge of each sprite).
  *acorn_pile_prop =
      (Prop){.image = &images[4] /* acorns */, .pos = ACORN_PILE_POS};
  *dropped_peg_prop =
      (Prop){.image = &images[2] /* peg */, .pos = DROPPED_PEG_POS};

  int i = 0;
  hotspots[i++] = SLIDE_HOTSPOT;
  hotspots[i++] = SQUIRREL_HOTSPOT;
  hotspots[i++] = ACORNS_TREE_HOTSPOT;
  hotspots[i++] = ACORNS_FLOOR_HOTSPOT;
  hotspots[i++] = PEG_HOTSPOT;

  i = 0;
  pois[i++] = SLIDE_POI;
  pois[i++] = SQUIRREL_POI;
  pois[i++] = ACORNS_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!fox_load_media(fox, renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load fox");
    return false;
  }

  // Load music
  char music_path[ASSET_PATH_MAX];
  asset_resolve(
      (Asset){
          .filename = "playground.wav",
          .directory = "music",
      },
      music_path, sizeof(music_path));
  music = Mix_LoadMUS(music_path);
  if (music == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load music: %s",
                 Mix_GetError());
    return false;
  }

  return true;
}

static void maybe_use_slide(void) {
  // If peg is in the inventory fix the slide
  if (has_peg) {
    has_peg = false;
    has_slide_been_fixed = true;
    Mix_PlayChannel(-1, fix_slide_sound->chunk, 0);
    // TODO: Wait for sound effect to end before starting to talk
    fox_talk(fox, examine_fixed_slide);
    return;
  }

  // Else if slide is working use: win the game
  if (has_slide_been_fixed) {
    fox->current_position = START_SLIDE_POS;
    fox_talk(fox, sliding_down);
    has_started_sliding = true;
    return;
  }

  // Else give hint to fix the slide
  if (examine_slide_count < 1) {
    fox_talk(fox, examine_slide_1);
  } else {
    fox_talk(fox, examine_slide_2);
  }
  examine_slide_count++;
}

static void maybe_get_peg(void) {
  // If acorns are in the inventory exchange them for the peg
  if (has_acorns) {
    has_acorns = false;
    has_peg_been_dropped = true;
    Mix_PlayChannel(-1, peg_falling_sound->chunk, 0);
    return;
  }

  // Else give hint to find acorns
  if (examine_squirrel_count < 1) {
    fox_talk(fox, examine_squirrel_1);
  } else {
    fox_talk(fox, examine_squirrel_2);
  }
  examine_squirrel_count++;
}

static void make_acorns_fall(void) {
  have_acorns_fallen = true;
  Mix_PlayChannel(-1, acorns_falling_sound->chunk, 0);
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
    // If has already slid three or more times go to outro
    if (slides_count > 2) {
      set_active_scene(OUTRO);
      break;
    }
    if (SDL_PointInRect(&m_pos, &SLIDE_HOTSPOT)) {
      // Walk to slide
      walk_actor_to(fox, &walk_grid, (SDL_FPoint){SLIDE_POI.x, SLIDE_POI.y},
                    true, maybe_use_slide);
      break;
    }
    // If the squirrel already dropped the peg skip this case
    if (!has_peg_been_dropped && SDL_PointInRect(&m_pos, &SQUIRREL_HOTSPOT)) {
      // Walk to squirrel
      walk_actor_to(fox, &walk_grid,
                    (SDL_FPoint){SQUIRREL_POI.x, SQUIRREL_POI.y}, true,
                    maybe_get_peg);
      break;
    }
    // If the acorns have already fallen skip this case
    if (!have_acorns_fallen && SDL_PointInRect(&m_pos, &ACORNS_TREE_HOTSPOT)) {
      // Walk to acorns
      walk_actor_to(fox, &walk_grid, (SDL_FPoint){ACORNS_POI.x, ACORNS_POI.y},
                    true, make_acorns_fall);
      break;
    }
    // If the acorns haven't fallen yet, or if fox has acorns, or if acorns have
    // been exchanged for the peg, skip this case
    if (have_acorns_fallen && !has_acorns && !has_peg_been_dropped &&
        SDL_PointInRect(&m_pos, &ACORNS_FLOOR_HOTSPOT)) {
      // Walk to acorns
      walk_actor_to(fox, &walk_grid, (SDL_FPoint){ACORNS_POI.x, ACORNS_POI.y},
                    true, pickup_acorns);
      break;
    }
    // If the peg hasn't been dropped yet, or if fox has the peg, or if the
    // slide has been fixed already, skip this case
    if (has_peg_been_dropped && !has_peg && !has_slide_been_fixed &&
        SDL_PointInRect(&m_pos, &PEG_HOTSPOT)) {
      // Walk to peg
      walk_actor_to(fox, &walk_grid,
                    (SDL_FPoint){SQUIRREL_POI.x, SQUIRREL_POI.y}, true,
                    pickup_peg);
      break;
    }
    // Walk to the clicked point (routed around the blocked area), or to the
    // nearest reachable point if the click is outside the walkable area.
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
  render_image(renderer, background, (SDL_Point){0, 0});
  render_image(renderer, squirrel, (SDL_Point){85, 160});

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

static void deinit(void) {
  fox_free(fox);

  Mix_FreeMusic(music);
  music = NULL;
}

static void on_scene_active(void) {
  Mix_PlayMusic(music, -1);

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

static void on_scene_inactive(void) { Mix_HaltMusic(); }

Scene playground_scene = {
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
    .walk_mask_dir = "playground",
    .images = images,
    .images_length = LEN(images),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
