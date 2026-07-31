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
#include "gina_nav.h"
#include "gina_state.h"
#include "gina_worn.h"
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
// framework ticks and frees them.
static AnimationData *sunscreen_boil;
// The goggles and the float are Gina's, not the poolside's: the same
// common/items art she goes on to wear (gina_worn.c), and the float's sheet is
// the one the tree plays too.
static AnimationData *goggles_boil;
static AnimationData *float_boil;
// The progress-reward burst over the goggles: plays once with the chime when
// she collects them, no input lock (this is a walking scene).
static AnimationData *celebration;
// Gina bobbing in the water after a dive; the scene draws it in place of her
// sprite while she floats.
static AnimationData *floating;
// The two exit arrows: one sheet, one instance per edge so each squiggles on
// its own cursor (gina_nav_render mirrors the left one).
static AnimationData *arrow_left;
static AnimationData *arrow_right;
// Declared as data: the framework makes and loads these; init only aliases
// them. The pool dir's own sheets come first, then the sheets borrowed from
// other dirs, which is why those get names here rather than generated indices.
#define POOL_ANIM_GOGGLES_BOIL (GINA_POOL_ANIMS_COUNT)
#define POOL_ANIM_FLOAT_BOIL (GINA_POOL_ANIMS_COUNT + 1)
#define POOL_ANIM_FLOATING (GINA_POOL_ANIMS_COUNT + 2)
#define POOL_ANIM_ARROW_LEFT (GINA_POOL_ANIMS_COUNT + 3)
#define POOL_ANIM_ARROW_RIGHT (GINA_POOL_ANIMS_COUNT + 4)
static AnimationData *animations[GINA_POOL_ANIMS_COUNT + 5];
static const SceneAnimSpec anim_specs[] = {
    GINA_POOL_ANIM_CELEBRATION_SPEC,   GINA_POOL_ANIM_SUNSCREEN_BOIL_SPEC,
    GINA_ITEMS_ANIM_GOGGLES_BOIL_SPEC, GINA_ITEMS_ANIM_FLOAT_BOIL_SPEC,
    GINA_HEN_ANIM_FLOATING_SPEC,       GINA_NAV_ANIM_ARROW_BOIL_SPEC,
    GINA_NAV_ANIM_ARROW_BOIL_SPEC,
};

// Static sprite layer: backdrop and water. The three object boils are declared
// on their hotspots — the framework plays and draws each. render() keeps only
// the dynamic draws (the float mid-flight, the actor, the reward burst).
static SceneSprite sprites[2];

// The scene's spoken lines: each is a per-line dialogue chunk the framework
// speaks via a generated say_<name>() helper. Sound effects live in the
// adventure's shared SFX bank (play_<name>()).
static ChunkData chunks[GINA_POOL_DIALOG_CHUNKS_COUNT] =
    GINA_POOL_DIALOG_CHUNKS_INIT;

static SDL_Point m_pos;

static Hen *gina;
static const SDL_FPoint HEN_START = {150, 480};

// Scene-object tweens. The float's flight into the tree, and Gina's dive; the
// flags below gate hotspots/input while each motion runs.
static Tween float_tween;
static bool float_flying;
static Tween dive_tween;

// The ending (#120). A dive is an arc into the water, a few seconds bobbing,
// then a hop back to the edge and the happy line — from there a tap dives
// again, until she has had DIVES_BEFORE_OUTRO of them and the end card takes
// over.
typedef enum dive_phase {
  DIVE_NONE,     // normal play
  DIVE_ENTERING, // the arc into the water
  DIVE_FLOATING, // bobbing, on a timer
  DIVE_CLIMBING, // the hop back to the edge
  DIVE_AGAIN,    // back on the edge: a tap goes again
} DivePhase;
static DivePhase dive_phase;
static float floating_seconds_left;
static int dive_count;

#define DIVES_BEFORE_OUTRO 3
#define FLOATING_SECONDS 2.0f
#define DIVE_ARC_MS 700
#define CLIMB_OUT_MS 600

// True while a dive is playing itself out and input should be ignored. The
// DIVE_AGAIN pause is deliberately not included: that is when a tap is wanted.
static bool dive_in_progress(void) {
  return dive_phase == DIVE_ENTERING || dive_phase == DIVE_FLOATING ||
         dive_phase == DIVE_CLIMBING;
}

// Object positions (top-left, matching each placeholder's size).
static const SDL_Point WATER_AT = {170, 40};
// Where she comes to rest in the water. Her 120px sheet is drawn centred here,
// so this sits high enough that she is inside the pool rect (WATER_AT +
// 460x180, i.e. y 40..220) instead of straddling its lower edge.
static const SDL_FPoint FLOAT_IN_WATER = {400, 160};
static const SDL_Point SUNSCREEN_AT = {120, 500};
static const SDL_Point GOGGLES_AT = {330, 470};
static const SDL_Point FLOAT_AT = {560, 470};

// Hotspots
static const SDL_Rect POOL_WATER_HOTSPOT = {170, 40, 460, 180};
static const SDL_Rect SUNSCREEN_HOTSPOT = {120, 500, 40, 60};
static const SDL_Rect GOGGLES_HOTSPOT = {330, 470, 60, 30};
static const SDL_Rect FLOAT_HOTSPOT = {560, 470, 90, 60};
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

// Depth (SCALING.md): a gentle ramp across the walkable strip, in feet
// coordinates (the walk rects above are centre positions; feet sit half a
// walking frame lower). scale_far is the knob — raise it toward 1 for a
// flatter backdrop, lower it for more perspective.
static const ScaleRamp SCALE_RAMP = {
    .y_far = 490, .y_near = 639, .scale_far = 0.85F, .scale_near = 1.0F};

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
static SDL_Point pois[6];

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

static void init(void) {
  rebuild_walk_grid();

  sunscreen_boil = animations[GINA_POOL_ANIM_SUNSCREEN_BOIL];
  goggles_boil = animations[POOL_ANIM_GOGGLES_BOIL];
  float_boil = animations[POOL_ANIM_FLOAT_BOIL];
  celebration = animations[GINA_POOL_ANIM_CELEBRATION];
  floating = animations[POOL_ANIM_FLOATING];
  arrow_left = animations[POOL_ANIM_ARROW_LEFT];
  arrow_right = animations[POOL_ANIM_ARROW_RIGHT];

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
  // Gated like the rest of the poolside: no wandering off before the sunscreen.
  gina_nav_hotspots(&hotspots[i], after_sunscreen);

  i = 0;
  pois[i++] = SUNSCREEN_POI;
  pois[i++] = GOGGLES_POI;
  pois[i++] = FLOAT_POI;
  pois[i++] = POOL_EDGE_POI;
  gina_nav_pois(&pois[i]);
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
  set_active_scene(GINA_SUNSCREEN_MINIGAME);
}

// The "already applied" tap is the generated say_sunscreen_done() helper
// directly (see the hotspot table in init) — no wrapper needed.

static void collect_goggles(void) {
  gina_state.has_goggles = true;
  // Progress reward: chime + confetti burst over the goggles while she
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
  // The gust carries the float up and off toward the tree: a hop off
  // the right edge, shrinking as it recedes. The state flips when it lands.
  float_flying = true;
  tween_start(&float_tween, (SDL_FPoint){FLOAT_AT.x, FLOAT_AT.y},
              (SDL_FPoint){WINDOW_WIDTH + 40, 120}, 900, TWEEN_EASE_IN,
              float_gone);
  float_tween.arc_height = 60;
  float_tween.to_scale = 0.5F;
}

// She has come back out and said her line. Either the card, or round again.
static void dive_finished(void) {
  dive_count++;
  if (dive_count >= DIVES_BEFORE_OUTRO) {
    set_active_scene(GINA_OUTRO);
    return;
  }
  dive_phase = DIVE_AGAIN;
  say_dive_again();
}

// Out of the water: a hop back to the edge she jumped from.
static void climb_out(void) {
  dive_phase = DIVE_CLIMBING;
  tween_start(&dive_tween, gina->current_position,
              (SDL_FPoint){POOL_EDGE_POI.x, POOL_EDGE_POI.y}, CLIMB_OUT_MS,
              TWEEN_EASE_OUT, dive_finished);
  dive_tween.arc_height = 70;
}

// She has hit the water: splash, then bob for a couple of seconds.
static void dive_entered(void) {
  play_splash();
  dive_phase = DIVE_FLOATING;
  floating_seconds_left = FLOATING_SECONDS;
  play_animation(floating, NULL);
}

static void dive(void) {
  // The dive arc: a tweened hop from the pool edge into the water. Input is
  // ignored until she is back out (see process_input).
  dive_phase = DIVE_ENTERING;
  tween_start(&dive_tween, gina->current_position, FLOAT_IN_WATER, DIVE_ARC_MS,
              TWEEN_EASE_IN, dive_entered);
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
  case FLOAT_ON_GROUND:
    // Either up in the branches or lying under them: still at the tree, still
    // to be fetched, so the same line covers both.
    say_float_in_tree();
    return;
  case FLOAT_RETRIEVED:
    dive();
    return;
  }
}

static void process_input(SDL_Event *event) {
  // Nothing is clickable while a dive plays itself out.
  if (dive_in_progress()) {
    return;
  }
  // After a dive, a tap anywhere goes again — she is already at the edge, so
  // there is nothing to walk to first.
  if (dive_phase == DIVE_AGAIN) {
    if (event->type == SDL_MOUSEBUTTONDOWN) {
      dive();
    }
    return;
  }
  // Drag & drop (LIVELINESS.md Part 2): an upward pull from a press on Gina
  // lifts her (plain taps fall through, so hotspots she stands on keep
  // working). She always comes back down where she was, so a pre-sunscreen
  // lift cannot carry her out of the shade and nobody lands in the pool.
  if (actor_drag_event(gina, event)) {
    return;
  }
  switch (event->type) {
  case SDL_MOUSEMOTION:
    m_pos.x = event->motion.x;
    m_pos.y = event->motion.y;
    break;
  case SDL_MOUSEBUTTONDOWN:
    // Hit-test the click's own coordinates: the cached motion position
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
  // The dive and climb arcs drive Gina's position directly, like Vania's
  // slide. tween_update returns false on the tick that fires the end callback,
  // so the assignment is skipped once the next phase has repositioned her.
  if ((dive_phase == DIVE_ENTERING || dive_phase == DIVE_CLIMBING) &&
      tween_update(&dive_tween, delta_time)) {
    gina->current_position = tween_pos(&dive_tween);
    gina->target_position = gina->current_position;
  }
  if (dive_phase == DIVE_FLOATING) {
    floating_seconds_left -= delta_time;
    if (floating_seconds_left <= 0.0F) {
      stop_animation(floating);
      climb_out();
    }
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
  // framework). render() draws only the dynamic layer: the exit arrows, the
  // float mid-flight, the actor, and the reward burst on top.
  gina_nav_render(renderer, arrow_left, arrow_right, after_sunscreen);
  if (gina_state.float_state == FLOAT_AT_POOL && float_flying) {
    // Mid-flight: the float follows its tween, shrinking as it recedes.
    SDL_FPoint p = tween_pos(&float_tween);
    render_animation_scaled(renderer, float_boil,
                            (SDL_Point){(int)p.x, (int)p.y},
                            tween_scale(&float_tween));
  }
  if (dive_phase == DIVE_FLOATING) {
    // Bobbing in the water: her own sheet, centred where the dive left her,
    // instead of the standing sprite the action layer would draw.
    render_animation(renderer, floating,
                     (SDL_Point){(int)gina->current_position.x - 60,
                                 (int)gina->current_position.y - 60});
  } else {
    render_action_layer(renderer, &SCALE_RAMP, NULL, 0, &gina, 1);
    gina_render_worn(renderer, gina);
  }
  // The reward burst over the goggles spot while the chime plays.
  if (celebration->is_playing) {
    render_animation(renderer, celebration, CELEBRATION_AT);
  }
}

static void on_scene_active(void) {
  // Stand where she walked in from, so leaving the tree by its left edge puts
  // her at this scene's right one (gina_nav.h); the shade start is the fallback
  // for arrivals that were not a walk. Cross-scene progress is preserved (it is
  // reset by the adventure's on_enter, not here), so navigating back from the
  // tree or vine keeps the puzzle state.
  SDL_FPoint at = gina_nav_entry(HEN_START);
  gina->current_position = at;
  gina->target_position = at;
  // The dive sequence is scene-local: entering the poolside always starts
  // outside it, with no dives counted.
  dive_phase = DIVE_NONE;
  dive_count = 0;
  // The sunscreen may have been applied since init (the minigame scene sets
  // it, then control returns here): pick the state-appropriate walk area.
  rebuild_walk_grid();
  // Fresh from the sunscreen minigame: explain what the reward means.
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
    // m4/m5), and the framework owns Gina's lifecycle.
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
    .scale_ramp = &SCALE_RAMP,
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
