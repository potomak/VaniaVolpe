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

// Asset declarations generated from the adventure manifest (ASSETS.md). The
// chunks table mixes directories (SFX here, one reused from the playground,
// the dialog lines), so it lists per-entry _INIT rows instead of one table
// macro.
#include "vania_assets.h"

static ImageData images[VANIA_PLAYGROUND_ENTRANCE_IMAGES_COUNT] =
    VANIA_PLAYGROUND_ENTRANCE_IMAGES_INIT;
static const ImageData *background =
    &images[VANIA_PLAYGROUND_ENTRANCE_IMAGE_PLAYGROUND_ENTRANCE_BACKGROUND];
static const ImageData *key = &images[VANIA_PLAYGROUND_ENTRANCE_IMAGE_KEY];

// Animations (declared as data, SCENES.md milestone 1: the framework makes,
// loads, ticks and frees these; the scene aliases the made objects and only
// sets their loop counts).
static AnimationData *excavator;
static AnimationData *gate;
static AnimationData *shovel;
static AnimationData *animations[VANIA_PLAYGROUND_ENTRANCE_ANIMS_COUNT];
static const SceneAnimSpec anim_specs[] = {
    VANIA_PLAYGROUND_ENTRANCE_ANIM_EXCAVATOR_SPEC,
    VANIA_PLAYGROUND_ENTRANCE_ANIM_GATE_SPEC,
    VANIA_PLAYGROUND_ENTRANCE_ANIM_SHOVEL_SPEC,
};

static Fox *fox;

// Music
static Mix_Music *music = NULL;

// Sound effects and dialog
static int excavator_sound_channel = -1;
static ChunkData chunks[7] = {
    VANIA_PLAYGROUND_ENTRANCE_CHUNK_EXCAVATOR_INIT,
    VANIA_PLAYGROUND_ENTRANCE_CHUNK_SHOVEL_INIT,
    VANIA_PLAYGROUND_ENTRANCE_CHUNK_KEY_REVEAL_INIT,
    // I'm reusing the sound effect of the peg falling
    VANIA_PLAYGROUND_CHUNK_PEG_FALLING_INIT,
    VANIA_PLAYGROUND_ENTRANCE_DIALOG_CHUNK_EXAMINE_GATE_1_INIT,
    VANIA_PLAYGROUND_ENTRANCE_DIALOG_CHUNK_EXAMINE_GATE_2_INIT,
    VANIA_PLAYGROUND_ENTRANCE_DIALOG_CHUNK_EXAMINE_SLIDE_FROM_OUTSIDE_INIT,
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
static Hotspot hotspots[5];

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

// Interactions (bodies below the loaders).
static void maybe_open_gate(void);
static void maybe_dig_out_key(void);
static void add_key_to_inventory(void);
static void examine_slide(void);

// Hotspot gating (see the table in init).
static bool key_still_buried(void) { return !has_key_been_revealed; }

static bool key_on_the_ground(void) {
  return has_key_been_revealed && !has_key;
}

static void run_excavator(void) {
  play_animation(excavator, NULL);
  if (excavator_sound_channel > -1) {
    Mix_HaltChannel(excavator_sound_channel);
  }
  excavator_sound_channel = Mix_PlayChannel(-1, excavator_sound->chunk, 0);
}

static void init(void) {
  excavator = animations[VANIA_PLAYGROUND_ENTRANCE_ANIM_EXCAVATOR];
  // Loop the animation 6 times before stopping
  excavator->max_loop_count = 6;
  gate = animations[VANIA_PLAYGROUND_ENTRANCE_ANIM_GATE];
  shovel = animations[VANIA_PLAYGROUND_ENTRANCE_ANIM_SHOVEL];
  // Loop the animation 3 times before stopping
  shovel->max_loop_count = 3;

  fox = make_fox((SDL_FPoint){580, 457});

  walk_grid_init(&walk_grid, &WALK_AREA,
                 (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT},
                 "playground_entrance");

  int i = 0;
  hotspots[i++] = (Hotspot){
      .rect = GATE_HOTSPOT, .poi = GATE_POI, .on_arrive = maybe_open_gate};
  hotspots[i++] = (Hotspot){
      .rect = EXCAVATOR_HOTSPOT, .immediate = true, .on_arrive = run_excavator};
  hotspots[i++] = (Hotspot){.rect = SHOVEL_HOTSPOT,
                            .enabled = key_still_buried,
                            .poi = SHOVEL_POI,
                            .on_arrive = maybe_dig_out_key};
  hotspots[i++] = (Hotspot){.rect = KEY_HOTSPOT,
                            .enabled = key_on_the_ground,
                            .poi = KEY_POI,
                            .on_arrive = add_key_to_inventory};
  hotspots[i++] = (Hotspot){
      .rect = SLIDE_HOTSPOT, .poi = SLIDE_POI, .on_arrive = examine_slide};

  i = 0;
  pois[i++] = GATE_POI;
  pois[i++] = SHOVEL_POI;
  pois[i++] = KEY_POI;
  pois[i++] = SLIDE_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  // The excavator, gate and shovel are loaded by the framework from
  // anim_specs.
  if (!fox_load_media(fox, renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load fox");
    return false;
  }

  // Load music
  char music_path[ASSET_PATH_MAX];
  asset_resolve(VANIA_MUSIC_CHUNK_PLAYGROUND_ASSET, music_path,
                sizeof(music_path));
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
    // The hotspot table says what each region does (see init). Otherwise:
    // walk to the clicked point (routed around the sandbox), or to the
    // nearest reachable point if the click is outside the walkable area.
    if (hotspots_handle_click(hotspots, LEN(hotspots), fox, &walk_grid,
                              m_pos)) {
      break;
    }
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
    .anim_specs = anim_specs,
    .anim_specs_length = LEN(anim_specs),
};
