//
//  playground_entrance.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "constants.h"
#include "fox.h"
#include "game.h"
#include "image.h"
#include "sound.h"

#include "playground_entrance.h"

// Background image
static ImageData background = {NULL, 0, 0};
static ImageData key = {NULL, 0, 0};

// Animations
static AnimationData *excavator;
static AnimationData *gate;
static AnimationData *shovel;

static Fox *fox;

// Music
static Mix_Music *music = NULL;

// Sound effects and dialog
static int excavator_sound_channel = -1;
static ChunkData chunks[7] = {
    {NULL, "playground_entrance/excavator.wav"},
    {NULL, "playground_entrance/shovel.wav"},
    {NULL, "playground_entrance/key_reveal.wav"},
    // I'm reusing the sound effect of the peg falling
    {NULL, "playground/peg_falling.wav"},
    {NULL, "playground_entrance/dialog/examine_gate_1.wav"},
    {NULL, "playground_entrance/dialog/examine_gate_2.wav"},
    {NULL, "playground_entrance/dialog/examine_slide_from_outside.wav"},
};
static const ChunkData *excavator_sound = &chunks[0];
static const ChunkData *shovel_sound = &chunks[1];
static const ChunkData *key_reveal_sound = &chunks[2];
static const ChunkData *open_gate_sound = &chunks[3];
static const ChunkData *examine_gate_1 = &chunks[4];
static const ChunkData *examine_gate_2 = &chunks[5];
static const ChunkData *examine_slide_from_outside = &chunks[6];

// Mouse position
static SDL_Point m_pos;

// Hotspots
static const SDL_Rect GATE_HOTSPOT = {471, 144, 217, 184};
static const SDL_Rect EXCAVATOR_HOTSPOT = {197, 338, 108, 72};
static const SDL_Rect SHOVEL_HOTSPOT = {128, 385, 77, 65};
static const SDL_Rect KEY_HOTSPOT = {9, 404, 75, 69};
static const SDL_Rect SLIDE_HOTSPOT = {165, 47, 129, 107};
static const SDL_Rect WALKABLE_HOTSPOT = {1, 312, 795, 286};
static const SDL_Rect NON_WALKABLE_HOTSPOT = {80, 341, 250, 179};
static SDL_Rect hotspots[7];

// Points of interest
static const SDL_Point GATE_POI = {554, 344};
static const SDL_Point SHOVEL_POI = {258, 507};
static const SDL_Point KEY_POI = {55, 494};
static const SDL_Point SLIDE_POI = {371, 333};
static SDL_Point pois[4];

// Scene state
static bool has_key_been_revealed = false;
static int examine_gate_count = 0;

static void init(void) {
  excavator = make_animation_data(4, ONE_SHOT);
  // Loop the animation 6 times before stopping
  excavator->max_loop_count = 6;
  excavator->speed_multiplier = 1. / 5;
  gate = make_animation_data(7, ONE_SHOT);
  shovel = make_animation_data(5, ONE_SHOT);
  // Loop the animation 3 times before stopping
  shovel->max_loop_count = 3;

  fox = make_fox((SDL_FPoint){580, 457});

  int i = 0;
  hotspots[i++] = GATE_HOTSPOT;
  hotspots[i++] = EXCAVATOR_HOTSPOT;
  hotspots[i++] = SHOVEL_HOTSPOT;
  hotspots[i++] = KEY_HOTSPOT;
  hotspots[i++] = SLIDE_HOTSPOT;
  hotspots[i++] = WALKABLE_HOTSPOT;
  hotspots[i++] = NON_WALKABLE_HOTSPOT;

  i = 0;
  pois[i++] = GATE_POI;
  pois[i++] = SHOVEL_POI;
  pois[i++] = KEY_POI;
  pois[i++] = SLIDE_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!load_image(renderer, &background,
                  "playground_entrance/background.png")) {
    fprintf(stderr, "Failed to load background!\n");
    return false;
  }

  if (!load_image(renderer, &key, "playground_entrance/key.png")) {
    fprintf(stderr, "Failed to load key!\n");
    return false;
  }

  if (!load_animation(renderer, excavator, "playground_entrance/excavator.png",
                      "playground_entrance/excavator.anim")) {
    fprintf(stderr, "Failed to load excavator!\n");
    return false;
  }

  if (!load_animation(renderer, gate, "playground_entrance/gate.png",
                      "playground_entrance/gate.anim")) {
    fprintf(stderr, "Failed to load gate!\n");
    return false;
  }

  if (!load_animation(renderer, shovel, "playground_entrance/shovel.png",
                      "playground_entrance/shovel.anim")) {
    fprintf(stderr, "Failed to load shovel!\n");
    return false;
  }

  if (!fox_load_media(fox, renderer)) {
    fprintf(stderr, "Failed to load fox!\n");
    return false;
  }

  // Load music
  music = Mix_LoadMUS("music/playground.wav");
  if (music == NULL) {
    fprintf(stderr, "Failed to load music! SDL_mixer Error: %s\n",
            Mix_GetError());
    return false;
  }

  // Load sound effects and dialog
  for (int i = 0; i < LEN(chunks); i++) {
    chunks[i].chunk = Mix_LoadWAV(chunks[i].path);
    if (chunks[i].chunk == NULL) {
      fprintf(stderr, "Failed to load %s! SDL_mixer Error: %s\n",
              chunks[i].path, Mix_GetError());
      return false;
    }
  }

  return true;
}

static void go_to_playground(void) { set_active_scene(PLAYGROUND); }

static void maybe_open_gate(void) {
  // If key is in inventory open gate then go to PLAYGROUND scene
  if (fox->has_key) {
    Mix_PlayChannel(-1, open_gate_sound->chunk, 0);
    play_animation(gate, go_to_playground);
    return;
  }

  // Else give hint about where to find the key
  if (examine_gate_count < 1) {
    fox_talk(fox, examine_gate_1->chunk);
  } else {
    fox_talk(fox, examine_gate_2->chunk);
  }
  examine_gate_count++;
}

static void add_key_to_inventory(void) { fox->has_key = true; }

static void reveal_key(void) {
  has_key_been_revealed = true;
  Mix_PlayChannel(-1, key_reveal_sound->chunk, 0);
}

static void maybe_dig_out_key(void) {
  // If key is in inventory don't do anything
  if (fox->has_key) {
    return;
  }

  // Play shovel animation and reveal key
  play_animation(shovel, reveal_key);
  Mix_PlayChannel(-1, shovel_sound->chunk, 0);
}

static void examine_slide(void) {
  // Give hint about finding a key to open the gate
  fox_talk(fox, examine_slide_from_outside->chunk);
}

static void process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEMOTION:
    // Get mouse position
    SDL_GetMouseState(&m_pos.x, &m_pos.y);
    break;
  case SDL_MOUSEBUTTONDOWN:
    if (SDL_PointInRect(&m_pos, &GATE_HOTSPOT)) {
      // Walk to gate
      fox_walk_to(fox, (SDL_FPoint){GATE_POI.x, GATE_POI.y}, maybe_open_gate);
      break;
    }
    if (SDL_PointInRect(&m_pos, &EXCAVATOR_HOTSPOT)) {
      // Play excavator animation
      play_animation(excavator, NULL);
      if (excavator_sound_channel > -1) {
        Mix_HaltChannel(excavator_sound_channel);
      }
      excavator_sound_channel = Mix_PlayChannel(-1, excavator_sound->chunk, 0);
      break;
    }
    // If key has been revealed yet skip this case
    if (!has_key_been_revealed && SDL_PointInRect(&m_pos, &SHOVEL_HOTSPOT)) {
      // Walk to shovel
      fox_walk_to(fox, (SDL_FPoint){SHOVEL_POI.x, SHOVEL_POI.y},
                  maybe_dig_out_key);
      break;
    }
    // If key hasn't been revealed yet, or if key has been picked up already,
    // then skip this case
    if (has_key_been_revealed && !fox->has_key &&
        SDL_PointInRect(&m_pos, &KEY_HOTSPOT)) {
      // Walk to key
      fox_walk_to(fox, (SDL_FPoint){KEY_POI.x, KEY_POI.y},
                  add_key_to_inventory);
      break;
    }
    if (SDL_PointInRect(&m_pos, &SLIDE_HOTSPOT)) {
      // Walk to slide
      fox_walk_to(fox, (SDL_FPoint){SLIDE_POI.x, SLIDE_POI.y}, examine_slide);
      break;
    }
    if (SDL_PointInRect(&m_pos, &WALKABLE_HOTSPOT) &&
        !SDL_PointInRect(&m_pos, &NON_WALKABLE_HOTSPOT)) {
      // Walk to current position
      fox_walk_to(fox, (SDL_FPoint){m_pos.x, m_pos.y}, NULL);
    }
    break;
  }
}

static void update(float delta_time) { fox_update(fox, delta_time); }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, &background, (SDL_Point){0, 0});

  render_animation(renderer, excavator, (SDL_Point){180, 350});
  render_animation(renderer, gate, (SDL_Point){491, 152});
  render_animation(renderer, shovel, (SDL_Point){112, 380});

  if (has_key_been_revealed) {
    if (fox->has_key) {
      render_image(renderer, &key,
                   (SDL_Point){fox->current_position.x - 50,
                               fox->current_position.y - 80});
    } else {
      render_image(renderer, &key, (SDL_Point){20, 431});
    }
  }

  fox_render(fox, renderer);
}

static void deinit(void) {
  free_image_texture(&background);
  free_image_texture(&key);

  free_animation(excavator);
  free_animation(gate);
  free_animation(shovel);

  fox_free(fox);

  Mix_FreeMusic(music);
  music = NULL;

  for (int i = 0; i < LEN(chunks); i++) {
    Mix_FreeChunk(chunks[i].chunk);
  }
}

static void on_scene_active(void) { Mix_PlayMusic(music, -1); }

static void on_scene_inactive(void) { Mix_HaltMusic(); }

Scene playground_entrance_scene = {
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
};
