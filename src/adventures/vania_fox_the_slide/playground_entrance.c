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
#include "vania_fox_the_slide.h"

#include "playground_entrance.h"

static ImageData images[2] = {
    {NULL, "playground_entrance_background.png", "playground_entrance", 0, 0},
    {NULL, "key.png", "playground_entrance", 0, 0},
};
static const ImageData *background = &images[0];
static const ImageData *key = &images[1];

// Animations (the framework ticks and frees these; the scene only declares
// them)
static AnimationData *excavator;
static AnimationData *gate;
static AnimationData *shovel;
static AnimationData *animations[3];

static Fox *fox;

// Music
static Mix_Music *music = NULL;

// Sound effects and dialog
static int excavator_sound_channel = -1;
static ChunkData chunks[7] = {
    {NULL, "excavator.wav", "playground_entrance"},
    {NULL, "shovel.wav", "playground_entrance"},
    {NULL, "key_reveal.wav", "playground_entrance"},
    // I'm reusing the sound effect of the peg falling
    {NULL, "peg_falling.wav", "playground"},
    {NULL, "examine_gate_1.wav", "playground_entrance/dialog"},
    {NULL, "examine_gate_2.wav", "playground_entrance/dialog"},
    {NULL, "examine_slide_from_outside.wav", "playground_entrance/dialog"},
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
static const SDL_Rect SHOVEL_HOTSPOT = {112, 378, 88, 72};
static const SDL_Rect KEY_HOTSPOT = {9, 404, 75, 69};
static const SDL_Rect SLIDE_HOTSPOT = {165, 47, 129, 107};
static SDL_Rect hotspots[5];

// Walk geometry: the ground below the horizon; the sandbox is blocked (the
// shovel POI sits inside it on purpose — the walk's final approach is exempt).
static const SDL_Rect WALKABLE_RECTS[] = {{1, 312, 795, 286}};
static const SDL_Rect BLOCKED_RECTS[] = {{80, 341, 250, 179}};
static const WalkArea WALK_AREA = {WALKABLE_RECTS, LEN(WALKABLE_RECTS),
                                   BLOCKED_RECTS, LEN(BLOCKED_RECTS)};
static WalkGrid walk_grid;

// Points of interest
static const SDL_Point GATE_POI = {554, 344};
static const SDL_Point SHOVEL_POI = {258, 507};
static const SDL_Point KEY_POI = {55, 494};
static const SDL_Point SLIDE_POI = {371, 333};
static SDL_Point pois[4];

// Scene state
static bool has_key_been_revealed;
// Inventory (adventure state, formerly on the fox)
static bool has_key;
static int examine_gate_count;

static void init(void) {
  excavator = animations[0] = make_animation_data(4, ONE_SHOT);
  // Loop the animation 6 times before stopping
  excavator->max_loop_count = 6;
  gate = animations[1] = make_animation_data(7, ONE_SHOT);
  shovel = animations[2] = make_animation_data(5, ONE_SHOT);
  // Loop the animation 3 times before stopping
  shovel->max_loop_count = 3;

  fox = make_fox((SDL_FPoint){580, 457});

  walk_grid_init(&walk_grid, &WALK_AREA,
                 (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT},
                 "playground_entrance");

  int i = 0;
  hotspots[i++] = GATE_HOTSPOT;
  hotspots[i++] = EXCAVATOR_HOTSPOT;
  hotspots[i++] = SHOVEL_HOTSPOT;
  hotspots[i++] = KEY_HOTSPOT;
  hotspots[i++] = SLIDE_HOTSPOT;

  i = 0;
  pois[i++] = GATE_POI;
  pois[i++] = SHOVEL_POI;
  pois[i++] = KEY_POI;
  pois[i++] = SLIDE_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!load_animation(renderer, excavator,
                      (Asset){
                          .filename = "excavator.png",
                          .directory = "playground_entrance",
                      },
                      (Asset){
                          .filename = "excavator.anim",
                          .directory = "playground_entrance",
                      })) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load excavator");
    return false;
  }

  if (!load_animation(renderer, gate,
                      (Asset){
                          .filename = "gate.png",
                          .directory = "playground_entrance",
                      },
                      (Asset){
                          .filename = "gate.anim",
                          .directory = "playground_entrance",
                      })) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load gate");
    return false;
  }

  if (!load_animation(renderer, shovel,
                      (Asset){
                          .filename = "shovel.png",
                          .directory = "playground_entrance",
                      },
                      (Asset){
                          .filename = "shovel.anim",
                          .directory = "playground_entrance",
                      })) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load shovel");
    return false;
  }

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

static void go_to_playground(void) { set_active_scene(PLAYGROUND); }

static void maybe_open_gate(void) {
  // If key is in inventory open gate then go to PLAYGROUND scene
  if (has_key) {
    Mix_PlayChannel(-1, open_gate_sound->chunk, 0);
    play_animation(gate, go_to_playground);
    return;
  }

  // Else give hint about where to find the key
  if (examine_gate_count < 1) {
    fox_talk(fox, examine_gate_1);
  } else {
    fox_talk(fox, examine_gate_2);
  }
  examine_gate_count++;
}

static void add_key_to_inventory(void) { has_key = true; }

static void reveal_key(void) {
  has_key_been_revealed = true;
  Mix_PlayChannel(-1, key_reveal_sound->chunk, 0);
}

static void maybe_dig_out_key(void) {
  // If key is in inventory don't do anything
  if (has_key) {
    return;
  }

  // Play shovel animation and reveal key
  play_animation(shovel, reveal_key);
  Mix_PlayChannel(-1, shovel_sound->chunk, 0);
}

static void examine_slide(void) {
  // Give hint about finding a key to open the gate
  fox_talk(fox, examine_slide_from_outside);
}

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
    if (SDL_PointInRect(&m_pos, &GATE_HOTSPOT)) {
      // Walk to gate
      walk_actor_to(fox, &walk_grid, (SDL_FPoint){GATE_POI.x, GATE_POI.y}, true,
                    maybe_open_gate);
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
      walk_actor_to(fox, &walk_grid, (SDL_FPoint){SHOVEL_POI.x, SHOVEL_POI.y},
                    true, maybe_dig_out_key);
      break;
    }
    // If key hasn't been revealed yet, or if key has been picked up already,
    // then skip this case
    if (has_key_been_revealed && !has_key &&
        SDL_PointInRect(&m_pos, &KEY_HOTSPOT)) {
      // Walk to key
      walk_actor_to(fox, &walk_grid, (SDL_FPoint){KEY_POI.x, KEY_POI.y}, true,
                    add_key_to_inventory);
      break;
    }
    if (SDL_PointInRect(&m_pos, &SLIDE_HOTSPOT)) {
      // Walk to slide
      walk_actor_to(fox, &walk_grid, (SDL_FPoint){SLIDE_POI.x, SLIDE_POI.y},
                    true, examine_slide);
      break;
    }
    // Walk to the clicked point (routed around the sandbox), or to the
    // nearest reachable point if the click is outside the walkable area.
    walk_actor_to(fox, &walk_grid, (SDL_FPoint){m_pos.x, m_pos.y}, false, NULL);
    break;
  }
}

static void update(float delta_time) { fox_update(fox, delta_time); }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});

  render_animation(renderer, excavator, (SDL_Point){180, 350});
  render_animation(renderer, gate, (SDL_Point){491, 152});
  render_animation(renderer, shovel, (SDL_Point){112, 380});

  if (has_key_been_revealed) {
    if (has_key) {
      render_image(renderer, key,
                   (SDL_Point){fox->current_position.x - 50,
                               fox->current_position.y - 80});
    } else {
      render_image(renderer, key, (SDL_Point){20, 431});
    }
  }

  fox_render(fox, renderer);
}

static void deinit(void) {
  fox_free(fox);

  Mix_FreeMusic(music);
  music = NULL;
}

static void on_scene_active(void) {
  Mix_PlayMusic(music, -1);

  // Initialize scene state
  has_key_been_revealed = false;
  examine_gate_count = 0;

  // Initialize fox state
  has_key = false;
}

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
    .walk_grid = &walk_grid,
    .walk_mask_dir = "playground_entrance",
    .images = images,
    .images_length = LEN(images),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
    .animations = animations,
    .animations_length = LEN(animations),
};
