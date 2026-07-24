//
//  pool.c
//  Poolside: the entry scene and the spine of the puzzle. Gina starts in the
//  umbrella shade and cannot leave it until she has put sunscreen on. Once she
//  can move, she collects goggles, loses the pool float to a gust of wind, and
//  — after the float is recovered (via the tree and the vine) — finally dives
//  in.
//

#include <SDL2/SDL.h>
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
// The progress-reward burst over the goggles (#118): plays once with the
// chime when she collects them, no input lock (this is a walking scene).
static AnimationData *celebration;
// Declared as data (SCENES.md milestone 1): the framework makes and loads
// these; init only aliases them, load_media no longer touches them. Order
// matches the generated indices.
static AnimationData *animations[GINA_POOL_ANIMS_COUNT];
static const SceneAnimSpec anim_specs[] = {
    GINA_POOL_ANIM_CELEBRATION_SPEC,
    GINA_POOL_ANIM_SUNSCREEN_BOIL_SPEC,
    GINA_POOL_ANIM_GOGGLES_BOIL_SPEC,
    GINA_POOL_ANIM_FLOAT_BOIL_SPEC,
};

// The static sprite layer (SCENES.md milestone 2): backdrop and water. The
// three object boils are declared on their hotspots (milestone 3) — the
// framework plays and draws each. render() keeps only the dynamic draws (the
// float mid-flight, the actor, the reward burst).
static SceneSprite sprites[2];

// The scene's spoken lines (SCENES.md milestone 4): each is a per-line dialogue
// chunk the framework speaks via a generated say_<name>() helper. Sound effects
// live in the adventure's shared SFX bank (play_<name>()).
static ChunkData chunks[GINA_POOL_DIALOG_CHUNKS_COUNT] =
    GINA_POOL_DIALOG_CHUNKS_INIT;

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
// Where Gina walks before a scene change: tapping a navigation arrow sends her
// to the edge first, and the scene switches when she arrives (not on the tap).
static const SDL_Point LEFT_EDGE_POI = {40, 500};
static const SDL_Point RIGHT_EDGE_POI = {760, 500};
static SDL_Point pois[4];

// Interactions and hotspot gating (bodies below the loaders). Before the
// sunscreen only the lotion is tappable; everything else unlocks after it.
static bool before_sunscreen(void);
static bool after_sunscreen(void);
static bool goggles_to_collect(void);
static bool float_at_the_pool(void);
static bool goggles_present(void);
static bool float_resting_at_pool(void);
static void open_sunscreen_minigame(void);
static void collect_goggles(void);
static void float_blows_away(void);
static void try_dive(void);
static void go_to_vine(void);
static void go_to_tree(void);

static void init(void) {
  // Gina is made by the framework (actor_spec/actor_start below) before init.
  rebuild_walk_grid();

  sunscreen_boil = animations[GINA_POOL_ANIM_SUNSCREEN_BOIL];
  goggles_boil = animations[GINA_POOL_ANIM_GOGGLES_BOIL];
  float_boil = animations[GINA_POOL_ANIM_FLOAT_BOIL];
  celebration = animations[GINA_POOL_ANIM_CELEBRATION];

  int s = 0;
  sprites[s++] = (SceneSprite){.image = background, .at = {0, 0}};
  sprites[s++] = (SceneSprite){.image = water, .at = WATER_AT};

  int i = 0;
  // The same bottle, two behaviours: reach for it before the sunscreen, a
  // gentle "already done" afterwards. Both carry the bottle's boil, so it
  // squiggles throughout (the sync ORs their enabled states).
  hotspots[i++] = (Hotspot){.rect = SUNSCREEN_HOTSPOT,
                            .enabled = before_sunscreen,
                            .poi = SUNSCREEN_POI,
                            .on_arrive = open_sunscreen_minigame,
                            .active_anim = sunscreen_boil,
                            .anim_at = SUNSCREEN_AT};
  hotspots[i++] = (Hotspot){.rect = SUNSCREEN_HOTSPOT,
                            .enabled = after_sunscreen,
                            .on_tap = say_sunscreen_done,
                            .active_anim = sunscreen_boil,
                            .anim_at = SUNSCREEN_AT};
  hotspots[i++] = (Hotspot){.rect = GOGGLES_HOTSPOT,
                            .enabled = goggles_to_collect,
                            .poi = GOGGLES_POI,
                            .on_arrive = collect_goggles,
                            .active_anim = goggles_boil,
                            .anim_at = GOGGLES_AT,
                            .anim_visible = goggles_present};
  hotspots[i++] = (Hotspot){.rect = FLOAT_HOTSPOT,
                            .enabled = float_at_the_pool,
                            .poi = FLOAT_POI,
                            .on_arrive = float_blows_away,
                            .active_anim = float_boil,
                            .anim_at = FLOAT_AT,
                            .anim_visible = float_resting_at_pool};
  hotspots[i++] = (Hotspot){.rect = POOL_WATER_HOTSPOT,
                            .enabled = after_sunscreen,
                            .poi = POOL_EDGE_POI,
                            .on_arrive = try_dive};
  hotspots[i++] = (Hotspot){.rect = VINE_NAV_HOTSPOT,
                            .enabled = after_sunscreen,
                            .poi = LEFT_EDGE_POI,
                            .on_arrive = go_to_vine};
  hotspots[i++] = (Hotspot){.rect = TREE_NAV_HOTSPOT,
                            .enabled = after_sunscreen,
                            .poi = RIGHT_EDGE_POI,
                            .on_arrive = go_to_tree};

  i = 0;
  pois[i++] = SUNSCREEN_POI;
  pois[i++] = GOGGLES_POI;
  pois[i++] = FLOAT_POI;
  pois[i++] = POOL_EDGE_POI;
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

// Render-visibility for the object boils (SceneSprite gates): the goggles show
// until collected, the float while it rests at the pool — both independent of
// the sunscreen, unlike the tap gates above (and the float sprite is off while
// it flies, when render() draws the tweened one).
static bool goggles_present(void) { return !gina_state.has_goggles; }
static bool float_resting_at_pool(void) {
  return gina_state.float_state == FLOAT_AT_POOL && !float_flying;
}

static void open_sunscreen_minigame(void) {
  set_active_scene(SUNSCREEN_MINIGAME);
}

// The "already applied" tap is the generated say_sunscreen_done() helper
// directly (see the hotspot table in init) — no wrapper needed.

static void go_to_vine(void) { set_active_scene(VINE); }

static void go_to_tree(void) { set_active_scene(TREE); }

static void collect_goggles(void) {
  gina_state.has_goggles = true;
  // Progress reward (#118): chime + confetti burst over the goggles while she
  // cheers. No input lock — she just picked something up, she isn't leaving.
  play_chime();
  play_animation(celebration, NULL);
  say_got_goggles();
}

// The float is gone: flip the state (which moves it to the tree scene) and
// only now let Gina react — the line reads as a response to what she saw.
static void float_gone(void) {
  float_flying = false;
  gina_state.float_state = FLOAT_STUCK_IN_TREE;
  say_float_blows_away();
}

static void float_blows_away(void) {
  play_wind();
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
  play_splash();
  say_dive_again();
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
    say_need_goggles();
    return;
  }
  switch (gina_state.float_state) {
  case FLOAT_AT_POOL:
    say_float_by_pool();
    return;
  case FLOAT_STUCK_IN_TREE:
    say_float_in_tree();
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
      say_shade_reminder();
      break;
    }
    // Otherwise walk toward the click, clamped to the walkable strip.
    walk_actor_to(gina, &walk_grid, (SDL_FPoint){m_pos.x, m_pos.y}, false,
                  NULL);
    break;
  }
}

static void update(float delta_time) {
  actor_update(gina, delta_time);
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

// The goggles reward burst, centred over the goggles (240x240 sheet over the
// 60x30 goggles at GOGGLES_AT).
static const SDL_Point CELEBRATION_AT = {240, 365};

static void render(SDL_Renderer *renderer) {
  // The backdrop, water and object boils are static sprites (drawn by the
  // framework). render() draws only the dynamic layer: the float mid-flight,
  // the actor, and the reward burst on top.
  if (gina_state.float_state == FLOAT_AT_POOL && float_flying) {
    // Mid-flight: the float follows its tween, shrinking as it recedes.
    SDL_FPoint p = tween_pos(&float_tween);
    render_animation_scaled(renderer, float_boil,
                            (SDL_Point){(int)p.x, (int)p.y},
                            tween_scale(&float_tween));
  }
  actor_render(gina, renderer);
  // The reward burst over the goggles spot while the chime plays.
  if (celebration->is_playing) {
    render_animation(renderer, celebration, CELEBRATION_AT);
  }
}

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
    say_sunscreen_ready();
  }
}

static void on_scene_inactive(void) {}

Scene pool_scene = {
    .init = init,
    // Custom process_input for the dive input-lock; .actor still declared so
    // the generated say_<name>() helpers can speak through it (SCENES.md
    // m4/m5), and the framework owns Gina's lifecycle (#141).
    .process_input = process_input,
    .actor = &gina,
    .actor_spec = &HEN_SPEC,
    .actor_start = {150, 480},
    .update = update,
    .render = render,
    .on_scene_active = on_scene_active,
    .on_scene_inactive = on_scene_inactive,
    .hotspots = hotspots,
    .hotspots_length = LEN(hotspots),
    .pois = pois,
    .pois_length = LEN(pois),
    .walk_grid = &walk_grid,
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
