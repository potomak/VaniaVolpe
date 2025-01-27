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
static const SDL_Rect WALKABLE_HOTSPOT_1 = {63, 333, 645, 262};
static const SDL_Rect WALKABLE_HOTSPOT_2 = {707, 491, 88, 105};
static const SDL_Rect WALKABLE_HOTSPOT_3 = {4, 531, 68, 63};
static const SDL_Rect NON_WALKABLE_HOTSPOT = {236, 131, 403, 312};
static SDL_Rect hotspots[9];

// Points of interest
static const SDL_Point SLIDE_POI = {276, 454};
static const SDL_Point SQUIRREL_POI = {219, 455};
static const SDL_Point ACORNS_POI = {635, 498};
static SDL_Point pois[3];

// Objects position
static const SDL_FPoint START_SLIDE_POS = (SDL_FPoint){326, 142};

// Other constants
static const float SLIDE_SIGMOID_HEIGHT = 240;

// Scene state
static bool has_slide_been_fixed = false;
static bool have_acorns_fallen = false;
static bool has_peg_been_dropped = false;
static bool has_started_sliding = false;
static float slide_x = 0;
static int examine_slide_count = 0;
static int examine_squirrel_count = 0;
static int slides_count = 0;

static void init(void) {
  fox = make_fox((SDL_FPoint){580, 457});

  int i = 0;
  hotspots[i++] = SLIDE_HOTSPOT;
  hotspots[i++] = SQUIRREL_HOTSPOT;
  hotspots[i++] = ACORNS_TREE_HOTSPOT;
  hotspots[i++] = ACORNS_FLOOR_HOTSPOT;
  hotspots[i++] = PEG_HOTSPOT;
  hotspots[i++] = WALKABLE_HOTSPOT_1;
  hotspots[i++] = WALKABLE_HOTSPOT_2;
  hotspots[i++] = WALKABLE_HOTSPOT_3;
  hotspots[i++] = NON_WALKABLE_HOTSPOT;

  i = 0;
  pois[i++] = SLIDE_POI;
  pois[i++] = SQUIRREL_POI;
  pois[i++] = ACORNS_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!fox_load_media(fox, renderer)) {
    fprintf(stderr, "Failed to load fox!\n");
    return false;
  }

  // Load music
  music = Mix_LoadMUS(asset_path((Asset){
      .filename = "playground.wav",
      .directory = "music",
  }));
  if (music == NULL) {
    fprintf(stderr, "Failed to load music! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  return true;
}

static void maybe_use_slide(void) {
  // If peg is in the inventory fix the slide
  if (fox->has_peg) {
    fox->has_peg = false;
    has_slide_been_fixed = true;
    Mix_PlayChannel(-1, fix_slide_sound->chunk, 0);
    // TODO: Wait for sound effect to end before starting to talk
    fox_talk(fox, examine_fixed_slide->chunk);
    return;
  }

  // Else if slide is working use: win the game
  if (has_slide_been_fixed) {
    fox->current_position = START_SLIDE_POS;
    fox_talk(fox, sliding_down->chunk);
    has_started_sliding = true;
    return;
  }

  // Else give hint to fix the slide
  if (examine_slide_count < 1) {
    fox_talk(fox, examine_slide_1->chunk);
  } else {
    fox_talk(fox, examine_slide_2->chunk);
  }
  examine_slide_count++;
}

static void maybe_get_peg(void) {
  // If acorns are in the inventory exchange them for the peg
  if (fox->has_acorns) {
    fox->has_acorns = false;
    has_peg_been_dropped = true;
    Mix_PlayChannel(-1, peg_falling_sound->chunk, 0);
    return;
  }

  // Else give hint to find acorns
  if (examine_squirrel_count < 1) {
    fox_talk(fox, examine_squirrel_1->chunk);
  } else {
    fox_talk(fox, examine_squirrel_2->chunk);
  }
  examine_squirrel_count++;
}

static void make_acorns_fall(void) {
  have_acorns_fallen = true;
  Mix_PlayChannel(-1, acorns_falling_sound->chunk, 0);
}

static void pickup_acorns(void) { fox->has_acorns = true; }

static void pickup_peg(void) { fox->has_peg = true; }

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
      fox_walk_to(fox, (SDL_FPoint){SLIDE_POI.x, SLIDE_POI.y}, maybe_use_slide);
      break;
    }
    // If the squirrel already dropped the peg skip this case
    if (!has_peg_been_dropped && SDL_PointInRect(&m_pos, &SQUIRREL_HOTSPOT)) {
      // Walk to squirrel
      fox_walk_to(fox, (SDL_FPoint){SQUIRREL_POI.x, SQUIRREL_POI.y},
                  maybe_get_peg);
      break;
    }
    // If the acorns have already fallen skip this case
    if (!have_acorns_fallen && SDL_PointInRect(&m_pos, &ACORNS_TREE_HOTSPOT)) {
      // Walk to acorns
      fox_walk_to(fox, (SDL_FPoint){ACORNS_POI.x, ACORNS_POI.y},
                  make_acorns_fall);
      break;
    }
    // If the acorns haven't fallen yet, or if fox has acorns, or if acorns have
    // been exchanged for the peg, skip this case
    if (have_acorns_fallen && !fox->has_acorns && !has_peg_been_dropped &&
        SDL_PointInRect(&m_pos, &ACORNS_FLOOR_HOTSPOT)) {
      // Walk to acorns
      fox_walk_to(fox, (SDL_FPoint){ACORNS_POI.x, ACORNS_POI.y}, pickup_acorns);
      break;
    }
    // If the peg hasn't been dropped yet, or if fox has the peg, or if the
    // slide has been fixed already, skip this case
    if (has_peg_been_dropped && !fox->has_peg && !has_slide_been_fixed &&
        SDL_PointInRect(&m_pos, &PEG_HOTSPOT)) {
      // Walk to peg
      fox_walk_to(fox, (SDL_FPoint){SQUIRREL_POI.x, SQUIRREL_POI.y},
                  pickup_peg);
      break;
    }
    if ((SDL_PointInRect(&m_pos, &WALKABLE_HOTSPOT_1) ||
         SDL_PointInRect(&m_pos, &WALKABLE_HOTSPOT_2) ||
         SDL_PointInRect(&m_pos, &WALKABLE_HOTSPOT_3)) &&
        !SDL_PointInRect(&m_pos, &NON_WALKABLE_HOTSPOT)) {
      // Walk to current position
      fox_walk_to(fox, (SDL_FPoint){m_pos.x, m_pos.y}, NULL);
    }
    break;
  }
}

// Used to compute the y position of the fox sliding down
static float sigmoid(float x) {
  float steepness = -0.025;
  float center = 130;
  float c = steepness * (x - center);
  return SLIDE_SIGMOID_HEIGHT / (1 + exp(c));
}

static void update(float delta_time) {
  fox_update(fox, delta_time);

  float slide_y;
  if (has_started_sliding) {
    slide_y = sigmoid(slide_x);
    fox->current_position =
        (SDL_FPoint){START_SLIDE_POS.x + slide_x, START_SLIDE_POS.y + slide_y};
    slide_x += 1.4;
    if (slide_x > 270) {
      slide_x = 0;
      has_started_sliding = false;
      slides_count++;
      fox_walk_to(fox, (SDL_FPoint){SLIDE_POI.x, SLIDE_POI.y}, NULL);
    }
  }
}

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});
  render_image(renderer, squirrel, (SDL_Point){85, 160});

  if (has_peg_been_dropped) {
    if (fox->has_peg) {
      render_image(renderer, peg,
                   (SDL_Point){fox->current_position.x - 20,
                               fox->current_position.y - 100});
    } else {
      if (has_slide_been_fixed) {
        render_image(renderer, fixed_peg, (SDL_Point){267, 263});
      } else {
        render_image(renderer, peg, (SDL_Point){109, 474});
      }
    }
  } else {
    render_image(renderer, peg, (SDL_Point){127, 175});
  }

  if (have_acorns_fallen) {
    if (fox->has_acorns) {
      render_image(renderer, acorns,
                   (SDL_Point){fox->current_position.x - 50,
                               fox->current_position.y - 100});
    } else {
      if (has_peg_been_dropped) {
        render_image(renderer, acorns, (SDL_Point){137, 135});
      } else {
        render_image(renderer, acorns, (SDL_Point){687, 503});
      }
    }
  } else {
    render_image(renderer, acorns, (SDL_Point){698, 225});
  }

  fox_render(fox, renderer);
}

static void deinit(void) {
  fox_free(fox);

  Mix_FreeMusic(music);
  music = NULL;
}

static void on_scene_active(void) { Mix_PlayMusic(music, -1); }

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
    .images = images,
    .images_length = LEN(images),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
