//
//  playground_entrance.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include <SDL2/SDL.h>
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

// The static sprite layer (SCENES.md milestone 2): backdrop, the three
// machines, and the key once it's on the ground (gated by key_on_the_ground).
// render() keeps the dynamic draws: the carried key (which follows the fox)
// and the fox.
static SceneSprite sprites[5];

static Fox *fox;

// The scene's spoken lines, played via generated say_<name>() helpers
// (SCENES.md milestone 4). Sound effects (excavator, shovel, key_reveal, and
// the peg-falling reused for the gate) are in the adventure's shared SFX bank
// (play_<name>()); music is declared on the Scene (.music).
static int excavator_sound_channel = -1;
static ChunkData chunks[VANIA_PLAYGROUND_ENTRANCE_DIALOG_CHUNKS_COUNT] =
    VANIA_PLAYGROUND_ENTRANCE_DIALOG_CHUNKS_INIT;

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
  // Retrigger from the top: stop a still-playing run before starting again
  // (scene_stop_channel ignores the initial -1 handle).
  scene_stop_channel(excavator_sound_channel);
  excavator_sound_channel = play_excavator();
}

static void init(void) {
  // The excavator (6) and shovel (3) loop counts are declared in the manifest
  // now and applied by the framework when it makes the animation (#150).
  excavator = animations[VANIA_PLAYGROUND_ENTRANCE_ANIM_EXCAVATOR];
  gate = animations[VANIA_PLAYGROUND_ENTRANCE_ANIM_GATE];
  shovel = animations[VANIA_PLAYGROUND_ENTRANCE_ANIM_SHOVEL];

  int s = 0;
  sprites[s++] = (SceneSprite){.image = background, .at = {0, 0}};
  sprites[s++] = (SceneSprite){.animation = excavator, .at = {180, 350}};
  sprites[s++] = (SceneSprite){.animation = gate, .at = {491, 152}};
  sprites[s++] = (SceneSprite){.animation = shovel, .at = {112, 380}};
  sprites[s++] = (SceneSprite){
      .image = key, .at = {20, 431}, .visible = key_on_the_ground};

  // The fox is made by the framework (actor_spec/actor_start below) before
  // init.
  walk_grid_init(&walk_grid, &WALK_AREA,
                 (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT},
                 "playground_entrance");

  int i = 0;
  hotspots[i++] = (Hotspot){
      .rect = GATE_HOTSPOT, .poi = GATE_POI, .on_arrive = maybe_open_gate};
  hotspots[i++] = (Hotspot){.rect = EXCAVATOR_HOTSPOT, .on_tap = run_excavator};
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

static void go_to_playground(void) { set_active_scene(PLAYGROUND); }

static void maybe_open_gate(void) {
  // If key is in inventory open gate then go to PLAYGROUND scene
  if (has_key) {
    // Placeholder: reuses the peg-falling sound for the gate opening.
    play_peg_falling();
    play_animation(gate, go_to_playground);
    return;
  }

  // Else give hint about where to find the key
  if (examine_gate_count < 1) {
    say_examine_gate_1();
  } else {
    say_examine_gate_2();
  }
  examine_gate_count++;
}

static void add_key_to_inventory(void) { has_key = true; }

static void reveal_key(void) {
  has_key_been_revealed = true;
  play_key_reveal();
}

static void maybe_dig_out_key(void) {
  // If key is in inventory don't do anything
  if (has_key) {
    return;
  }

  // Play shovel animation and reveal key
  play_animation(shovel, reveal_key);
  play_shovel();
}

static void examine_slide(void) {
  // Give hint about finding a key to open the gate
  say_examine_slide_from_outside();
}

// No update: the framework ticks the actor (#147). render stays custom to draw
// the carried key behind the fox.
static void render(SDL_Renderer *renderer) {
  // Backdrop, machines and the on-ground key are static sprites (drawn by the
  // framework). render() draws the dynamic layer: the key while the fox
  // carries it, and the fox.
  if (has_key_been_revealed && has_key) {
    render_image(renderer, key,
                 (SDL_Point){fox->current_position.x - 50,
                             fox->current_position.y - 80});
  }

  actor_render(fox, renderer);
}

static void on_scene_active(void) {
  // Initialize scene state
  has_key_been_revealed = false;
  examine_gate_count = 0;

  // Initialize fox state
  has_key = false;
}

static void on_scene_inactive(void) {}

Scene playground_entrance_scene = {
    .init = init,
    // No process_input: the framework's default drag/hit-test/walk handler
    // drives this scene (SCENES.md milestone 5). This is also what makes the
    // fox draggable here, like the hen — the default includes drag & drop.
    // The framework also owns the fox's lifecycle (#141).
    .actor = &fox,
    .actor_spec = &FOX_SPEC,
    .actor_start = {580, 457},
    // No update: the framework ticks the actor (#147). render is custom (the
    // carried key draws behind the fox).
    .render = render,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .hotspots = hotspots,
    .hotspots_length = LEN(hotspots),
    .pois = pois,
    .pois_length = LEN(pois),
    .walk_grid = &walk_grid,
    .walk_mask_dir = "playground_entrance",
    .sprites = sprites,
    .sprites_length = LEN(sprites),
    .images = images,
    .images_length = LEN(images),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
    .animations = animations,
    .animations_length = LEN(animations),
    .anim_specs = anim_specs,
    .anim_specs_length = LEN(anim_specs),
    .music = VANIA_MUSIC_CHUNK_PLAYGROUND_ASSET_INIT,
};
