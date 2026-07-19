//
//  pool.c
//  Poolside: the entry scene and the spine of the puzzle. Gina starts in the
//  umbrella shade and cannot leave it until she has put sunscreen on. Once she
//  can move, she collects goggles, loses the pool float to a gust of wind, and
//  — after the float is recovered (via the tree and the vine) — finally dives
//  in.
//

#include <SDL2/SDL.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "constants.h"
#include "game.h"
#include "image.h"
#include "sound.h"
#include "tween.h"

#include "gina_hen_at_the_pool.h"
#include "gina_state.h"
#include "hen.h"
#include "pool.h"

// Asset declarations generated from the adventure manifest (ASSETS.md): the
// filenames, table order and animation frame counts below come from
// assets/index.json via gen_asset_decls.py, so the scene, the art pipeline
// and the estimator all read one source of truth.
#include "gina_assets.h"

// Backdrops, auto-loaded from the scene's images table. (The tappable
// objects render as boils below; their still PNGs are authoring-only.)
static ImageData images[GINA_POOL_IMAGES_COUNT] = GINA_POOL_IMAGES_INIT;
static const ImageData *background = &images[GINA_POOL_IMAGE_BACKGROUND];
static const ImageData *water = &images[GINA_POOL_IMAGE_WATER];

// The tappable objects boil (LIVELINESS.md Part 3): the engine plays each
// while its hotspot is enabled (see the active_anim wired in init) and freezes
// it otherwise, so what squiggles is what a tap would hit. Declared here so the
// framework ticks and frees them; each is the same size as the old still PNG,
// so the render positions below are unchanged.
static AnimationData *sunscreen_boil;
static AnimationData *goggles_boil;
static AnimationData *float_boil;
static AnimationData *animations[GINA_POOL_ANIMS_COUNT];

// Sound effects and dialog (silent placeholders; lines are logged to stdout).
static ChunkData chunks[GINA_POOL_CHUNKS_COUNT] = GINA_POOL_CHUNKS_INIT;
static const ChunkData *voice(void) { return &chunks[GINA_POOL_CHUNK_VOICE]; }

static SDL_Point m_pos;

static Hen *gina;
static const SDL_FPoint HEN_START = {150, 480};

// Scene-object tweens (#107/#119). The float's flight into the tree, and
// Gina's dive arc into the water; the flags gate hotspots/input while each
// motion runs.
static Tween float_tween;
static bool float_flying;
static Tween dive_tween;
static bool diving;

// Object positions (top-left, matching each placeholder's size).
static const SDL_Point WATER_AT = {170, 40};
static const SDL_Point SUNSCREEN_AT = {120, 500};
static const SDL_Point GOGGLES_AT = {330, 470};
static const SDL_Point FLOAT_AT = {560, 470};

// Hotspots
static const SDL_Rect POOL_WATER_HOTSPOT = {170, 40, 460, 180};
static const SDL_Rect SUNSCREEN_HOTSPOT = {120, 500, 40, 60};
static const SDL_Rect GOGGLES_HOTSPOT = {330, 470, 60, 30};
static const SDL_Rect FLOAT_HOTSPOT = {560, 470, 90, 60};
static const SDL_Rect VINE_NAV_HOTSPOT = {0, 200, 30, 250};
static const SDL_Rect TREE_NAV_HOTSPOT = {770, 200, 30, 250};
static Hotspot hotspots[7];

// Walk geometry. Before the sunscreen is applied Gina refuses to leave the
// umbrella's shadow, so the walkable area itself is a function of game state:
// the shade patch first, the whole poolside strip afterwards. (The shade rect
// is tuned to the current background art; toggle the debug overlay to see
// whichever area is active.)
static const SDL_Rect POOLSIDE_RECTS[] = {{20, 430, 760, 150}};
static const SDL_Rect SHADE_RECTS[] = {{60, 430, 200, 150}};
static const WalkArea POOLSIDE_AREA = {POOLSIDE_RECTS, LEN(POOLSIDE_RECTS),
                                       NULL, 0};
static const WalkArea SHADE_AREA = {SHADE_RECTS, LEN(SHADE_RECTS), NULL, 0};
static WalkGrid walk_grid;

// Rebuild the grid from the state-appropriate area. Called on scene entry and
// after any in-scene state change that affects movement (the replay reset in
// dive(); the sunscreen minigame re-enters through on_scene_active).
// State-switched areas stay rect-based: a walkable.walk mask describes one
// fixed grid, so this scene has no walk_mask_dir and paint edits are
// transient.
static void rebuild_walk_grid(void) {
  walk_grid_build(&walk_grid,
                  gina_state.has_sunscreen ? &POOLSIDE_AREA : &SHADE_AREA,
                  (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT});
}

// Points of interest (where Gina stands to interact)
static const SDL_Point SUNSCREEN_POI = {150, 545};
static const SDL_Point GOGGLES_POI = {360, 525};
static const SDL_Point FLOAT_POI = {600, 545};
static const SDL_Point POOL_EDGE_POI = {400, 460};
static SDL_Point pois[4];

// Interactions and hotspot gating (bodies below the loaders). Before the
// sunscreen only the lotion is tappable; everything else unlocks after it.
static bool before_sunscreen(void);
static bool after_sunscreen(void);
static bool goggles_to_collect(void);
static bool float_at_the_pool(void);
static void open_sunscreen_minigame(void);
static void say_sunscreen_done(void);
static void collect_goggles(void);
static void float_blows_away(void);
static void try_dive(void);
static void go_to_vine(void);
static void go_to_tree(void);

static void init(void) {
  gina = make_hen(HEN_START);

  rebuild_walk_grid();

  sunscreen_boil = animations[GINA_POOL_ANIM_SUNSCREEN_BOIL] =
      make_animation_data(GINA_POOL_ANIM_SUNSCREEN_BOIL_FRAMES,
                          GINA_POOL_ANIM_SUNSCREEN_BOIL_STYLE);
  goggles_boil = animations[GINA_POOL_ANIM_GOGGLES_BOIL] = make_animation_data(
      GINA_POOL_ANIM_GOGGLES_BOIL_FRAMES, GINA_POOL_ANIM_GOGGLES_BOIL_STYLE);
  float_boil = animations[GINA_POOL_ANIM_FLOAT_BOIL] = make_animation_data(
      GINA_POOL_ANIM_FLOAT_BOIL_FRAMES, GINA_POOL_ANIM_FLOAT_BOIL_STYLE);

  int i = 0;
  // The same bottle, two behaviours: reach for it before the sunscreen, a
  // gentle "already done" afterwards. Both carry the bottle's boil, so it
  // squiggles throughout (the sync ORs their enabled states).
  hotspots[i++] = (Hotspot){.rect = SUNSCREEN_HOTSPOT,
                            .enabled = before_sunscreen,
                            .poi = SUNSCREEN_POI,
                            .on_arrive = open_sunscreen_minigame,
                            .active_anim = sunscreen_boil};
  hotspots[i++] = (Hotspot){.rect = SUNSCREEN_HOTSPOT,
                            .enabled = after_sunscreen,
                            .immediate = true,
                            .on_arrive = say_sunscreen_done,
                            .active_anim = sunscreen_boil};
  hotspots[i++] = (Hotspot){.rect = GOGGLES_HOTSPOT,
                            .enabled = goggles_to_collect,
                            .poi = GOGGLES_POI,
                            .on_arrive = collect_goggles,
                            .active_anim = goggles_boil};
  hotspots[i++] = (Hotspot){.rect = FLOAT_HOTSPOT,
                            .enabled = float_at_the_pool,
                            .poi = FLOAT_POI,
                            .on_arrive = float_blows_away,
                            .active_anim = float_boil};
  hotspots[i++] = (Hotspot){.rect = POOL_WATER_HOTSPOT,
                            .enabled = after_sunscreen,
                            .poi = POOL_EDGE_POI,
                            .on_arrive = try_dive};
  hotspots[i++] = (Hotspot){.rect = VINE_NAV_HOTSPOT,
                            .enabled = after_sunscreen,
                            .immediate = true,
                            .on_arrive = go_to_vine};
  hotspots[i++] = (Hotspot){.rect = TREE_NAV_HOTSPOT,
                            .enabled = after_sunscreen,
                            .immediate = true,
                            .on_arrive = go_to_tree};

  i = 0;
  pois[i++] = SUNSCREEN_POI;
  pois[i++] = GOGGLES_POI;
  pois[i++] = FLOAT_POI;
  pois[i++] = POOL_EDGE_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  if (!hen_load_media(gina, renderer)) {
    return false;
  }
  return load_animation(renderer, sunscreen_boil,
                        GINA_POOL_ANIM_SUNSCREEN_BOIL_SPRITE_ASSET,
                        GINA_POOL_ANIM_SUNSCREEN_BOIL_DATA_ASSET) &&
         load_animation(renderer, goggles_boil,
                        GINA_POOL_ANIM_GOGGLES_BOIL_SPRITE_ASSET,
                        GINA_POOL_ANIM_GOGGLES_BOIL_DATA_ASSET) &&
         load_animation(renderer, float_boil,
                        GINA_POOL_ANIM_FLOAT_BOIL_SPRITE_ASSET,
                        GINA_POOL_ANIM_FLOAT_BOIL_DATA_ASSET);
}

// ── interactions
// ──────────────────────────────────────────────────────────────

static bool before_sunscreen(void) { return !gina_state.has_sunscreen; }

static bool after_sunscreen(void) { return gina_state.has_sunscreen; }

static bool goggles_to_collect(void) {
  return gina_state.has_sunscreen && !gina_state.has_goggles;
}

static bool float_at_the_pool(void) {
  return gina_state.has_sunscreen && gina_state.float_state == FLOAT_AT_POOL &&
         !float_flying;
}

static void open_sunscreen_minigame(void) {
  set_active_scene(SUNSCREEN_MINIGAME);
}

static void say_sunscreen_done(void) {
  gina_say(gina, "Ho gia' messo la crema.", voice());
}

static void go_to_vine(void) { set_active_scene(VINE); }

static void go_to_tree(void) { set_active_scene(TREE); }

static void collect_goggles(void) {
  gina_state.has_goggles = true;
  gina_say(gina, "Ho preso gli occhialini!", voice());
}

// The float is gone: flip the state (which moves it to the tree scene) and
// only now let Gina react — the line reads as a response to what she saw.
static void float_gone(void) {
  float_flying = false;
  gina_state.float_state = FLOAT_STUCK_IN_TREE;
  gina_say(gina, "Oh no! Il vento ha portato il salvagente sull'albero!",
           voice());
}

static void float_blows_away(void) {
  Mix_PlayChannel(-1, chunks[GINA_POOL_CHUNK_WIND].chunk, 0);
  // The gust carries the float up and off toward the tree (#107): a hop off
  // the right edge, shrinking as it recedes. The state flips when it lands.
  float_flying = true;
  tween_start(&float_tween, (SDL_FPoint){FLOAT_AT.x, FLOAT_AT.y},
              (SDL_FPoint){WINDOW_WIDTH + 40, 120}, 900, TWEEN_EASE_IN,
              float_gone);
  float_tween.arc_height = 60;
  float_tween.to_scale = 0.5F;
}

// Landing half of the dive (#119): splash, the happy line, and the in-place
// replay reset that used to happen on the tap.
static void dive_landed(void) {
  diving = false;
  Mix_PlayChannel(-1, chunks[GINA_POOL_CHUNK_SPLASH].chunk, 0);
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Gina: Che bello! Ancora!");
  // Replay the adventure in place. The reset happens without leaving the
  // scene (no on_scene_active), so the walkable area must be rebuilt here or
  // Gina could roam the whole poolside before reapplying the sunscreen.
  gina_state_reset();
  rebuild_walk_grid();
  gina->current_position = HEN_START;
  gina->target_position = HEN_START;
}

static void dive(void) {
  // The dive arc (#119): a tweened hop from the pool edge into the water.
  // Input is ignored until she lands (see process_input).
  diving = true;
  tween_start(&dive_tween, gina->current_position, (SDL_FPoint){400, 190}, 700,
              TWEEN_EASE_IN, dive_landed);
  dive_tween.arc_height = 100;
}

static void try_dive(void) {
  if (!gina_state.has_goggles) {
    gina_say(gina, "Mi servono gli occhialini per tuffarmi.", voice());
    return;
  }
  switch (gina_state.float_state) {
  case FLOAT_AT_POOL:
    gina_say(gina, "Mi serve il salvagente: e' li' vicino alla piscina.",
             voice());
    return;
  case FLOAT_STUCK_IN_TREE:
    gina_say(gina, "Il salvagente e' sull'albero! Devo recuperarlo.", voice());
    return;
  case FLOAT_RETRIEVED:
    dive();
    return;
  }
}

static void process_input(SDL_Event *event) {
  // Mid-dive nothing is clickable; the replay reset re-enables input.
  if (diving) {
    return;
  }
  // Drag & drop (LIVELINESS.md Part 2): dragging the pointer from a press on
  // Gina picks her up (plain taps fall through, so hotspots she stands on
  // keep working). The landing scan runs on the live grid, so a pre-sunscreen
  // drop always lands back in the shade and nobody lands in the pool.
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
    // The hotspot table says what each region does (see init).
    if (hotspots_handle_click(hotspots, LEN(hotspots), gina, &walk_grid,
                              m_pos)) {
      break;
    }
    // Before the sunscreen the walk grid covers only the umbrella's shadow:
    // Gina wanders freely within the shade and refuses anything beyond it.
    if (!gina_state.has_sunscreen && !walk_grid_contains(&walk_grid, m_pos)) {
      gina_say(gina, "Devo mettere la crema solare prima di uscire dall'ombra!",
               voice());
      break;
    }
    // Otherwise walk toward the click, clamped to the walkable strip.
    walk_actor_to(gina, &walk_grid, (SDL_FPoint){m_pos.x, m_pos.y}, false,
                  NULL);
    break;
  }
}

static void update(float delta_time) {
  hen_update(gina, delta_time);
  // The dive arc drives Gina's position directly, like Vania's slide; when
  // the final tick fires dive_landed the reset has already repositioned her,
  // so the assignment is skipped (tween_update returns false on that tick).
  if (diving && tween_update(&dive_tween, delta_time)) {
    gina->current_position = tween_pos(&dive_tween);
    gina->target_position = gina->current_position;
  }
  if (float_flying) {
    tween_update(&float_tween, delta_time);
  }
}

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});
  render_image(renderer, water, WATER_AT);
  render_animation(renderer, sunscreen_boil, SUNSCREEN_AT);
  if (!gina_state.has_goggles) {
    render_animation(renderer, goggles_boil, GOGGLES_AT);
  }
  if (gina_state.float_state == FLOAT_AT_POOL) {
    if (float_flying) {
      // Mid-flight: the float follows its tween, shrinking as it recedes.
      SDL_FPoint p = tween_pos(&float_tween);
      render_animation_scaled(renderer, float_boil,
                              (SDL_Point){(int)p.x, (int)p.y},
                              tween_scale(&float_tween));
    } else {
      render_animation(renderer, float_boil, FLOAT_AT);
    }
  }
  hen_render(gina, renderer);
}

static void deinit(void) { hen_free(gina); }

static void on_scene_active(void) {
  // Return Gina to her starting spot. Cross-scene progress is preserved (it is
  // reset by the adventure's on_enter, not here), so navigating back from the
  // tree or vine keeps the puzzle state.
  gina->current_position = HEN_START;
  gina->target_position = HEN_START;
  // The sunscreen may have been applied since init (the minigame scene sets
  // it, then control returns here): pick the state-appropriate walk area.
  rebuild_walk_grid();
  // Fresh from the sunscreen minigame (#116): explain what the reward means.
  if (gina_state.announce_sunscreen) {
    gina_state.announce_sunscreen = false;
    gina_say(gina, "Pronta! Ora posso uscire al sole!", voice());
  }
}

static void on_scene_inactive(void) {}

Scene pool_scene = {
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
    .images = images,
    .images_length = LEN(images),
    .animations = animations,
    .animations_length = LEN(animations),
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
