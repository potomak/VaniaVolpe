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

#include "gina_hen_at_the_pool.h"
#include "gina_state.h"
#include "hen.h"
#include "pool.h"

// Images (background + objects), auto-loaded from the scene's images table.
static ImageData images[5] = {
    {NULL, "background.png", "pool", 0, 0}, {NULL, "water.png", "pool", 0, 0},
    {NULL, "sunscreen.png", "pool", 0, 0},  {NULL, "goggles.png", "pool", 0, 0},
    {NULL, "float.png", "pool", 0, 0},
};
static const ImageData *background = &images[0];
static const ImageData *water = &images[1];
static const ImageData *sunscreen = &images[2];
static const ImageData *goggles = &images[3];
static const ImageData *pool_float = &images[4];

// Sound effects and dialog (silent placeholders; lines are logged to stdout).
static ChunkData chunks[3] = {
    {NULL, "voice.wav", "pool"},
    {NULL, "wind.wav", "pool"},
    {NULL, "splash.wav", "pool"},
};
static Mix_Chunk *voice(void) { return chunks[0].chunk; }

static SDL_Point m_pos;

static Hen *gina;
static const SDL_FPoint HEN_START = {150, 480};

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
static SDL_Rect hotspots[6];

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
static void rebuild_walk_grid(void) {
  walk_grid_build(&walk_grid,
                  gina_state.has_sunscreen ? &POOLSIDE_AREA : &SHADE_AREA);
}

// Points of interest (where Gina stands to interact)
static const SDL_Point SUNSCREEN_POI = {150, 545};
static const SDL_Point GOGGLES_POI = {360, 525};
static const SDL_Point FLOAT_POI = {600, 545};
static const SDL_Point POOL_EDGE_POI = {400, 460};
static SDL_Point pois[4];

static void init(void) {
  gina = make_hen(HEN_START);

  rebuild_walk_grid();

  int i = 0;
  hotspots[i++] = POOL_WATER_HOTSPOT;
  hotspots[i++] = SUNSCREEN_HOTSPOT;
  hotspots[i++] = GOGGLES_HOTSPOT;
  hotspots[i++] = FLOAT_HOTSPOT;
  hotspots[i++] = VINE_NAV_HOTSPOT;
  hotspots[i++] = TREE_NAV_HOTSPOT;

  i = 0;
  pois[i++] = SUNSCREEN_POI;
  pois[i++] = GOGGLES_POI;
  pois[i++] = FLOAT_POI;
  pois[i++] = POOL_EDGE_POI;
}

static bool load_media(SDL_Renderer *renderer) {
  return hen_load_media(gina, renderer);
}

// ── interactions
// ──────────────────────────────────────────────────────────────

static void open_sunscreen_minigame(void) {
  set_active_scene(SUNSCREEN_MINIGAME);
}

static void collect_goggles(void) {
  gina_state.has_goggles = true;
  gina_say(gina, "Ho preso gli occhialini!", voice());
}

static void float_blows_away(void) {
  gina_state.float_state = FLOAT_STUCK_IN_TREE;
  Mix_PlayChannel(-1, chunks[1].chunk, 0); // wind
  gina_say(gina, "Oh no! Il vento ha portato il salvagente sull'albero!",
           voice());
}

static void dive(void) {
  Mix_PlayChannel(-1, chunks[2].chunk, 0); // splash
  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Gina: Che bello! Ancora!");
  // Replay the adventure in place. The reset happens without leaving the
  // scene (no on_scene_active), so the walkable area must be rebuilt here or
  // Gina could roam the whole poolside before reapplying the sunscreen.
  gina_state_reset();
  rebuild_walk_grid();
  gina->current_position = HEN_START;
  gina->target_position = HEN_START;
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
  switch (event->type) {
  case SDL_MOUSEMOTION:
    m_pos.x = event->motion.x;
    m_pos.y = event->motion.y;
    break;
  case SDL_MOUSEBUTTONDOWN:
    // Before sunscreen the walk grid covers only the umbrella's shadow: Gina
    // reaches for the lotion, wanders freely within the shade, and refuses
    // anything beyond it.
    if (!gina_state.has_sunscreen) {
      if (SDL_PointInRect(&m_pos, &SUNSCREEN_HOTSPOT)) {
        walk_actor_to(gina, &walk_grid,
                      (SDL_FPoint){SUNSCREEN_POI.x, SUNSCREEN_POI.y}, true,
                      open_sunscreen_minigame);
      } else if (walk_grid_contains(&walk_grid, m_pos)) {
        walk_actor_to(gina, &walk_grid, (SDL_FPoint){m_pos.x, m_pos.y}, false,
                      NULL);
      } else {
        gina_say(gina,
                 "Devo mettere la crema solare prima di uscire dall'ombra!",
                 voice());
      }
      break;
    }

    // Sunscreen applied: Gina can move and interact freely.
    if (SDL_PointInRect(&m_pos, &SUNSCREEN_HOTSPOT)) {
      gina_say(gina, "Ho gia' messo la crema.", voice());
      break;
    }
    if (!gina_state.has_goggles && SDL_PointInRect(&m_pos, &GOGGLES_HOTSPOT)) {
      walk_actor_to(gina, &walk_grid,
                    (SDL_FPoint){GOGGLES_POI.x, GOGGLES_POI.y}, true,
                    collect_goggles);
      break;
    }
    if (gina_state.float_state == FLOAT_AT_POOL &&
        SDL_PointInRect(&m_pos, &FLOAT_HOTSPOT)) {
      walk_actor_to(gina, &walk_grid, (SDL_FPoint){FLOAT_POI.x, FLOAT_POI.y},
                    true, float_blows_away);
      break;
    }
    if (SDL_PointInRect(&m_pos, &POOL_WATER_HOTSPOT)) {
      walk_actor_to(gina, &walk_grid,
                    (SDL_FPoint){POOL_EDGE_POI.x, POOL_EDGE_POI.y}, true,
                    try_dive);
      break;
    }
    if (SDL_PointInRect(&m_pos, &VINE_NAV_HOTSPOT)) {
      set_active_scene(VINE);
      break;
    }
    if (SDL_PointInRect(&m_pos, &TREE_NAV_HOTSPOT)) {
      set_active_scene(TREE);
      break;
    }
    // Otherwise walk toward the click, clamped to the walkable strip.
    walk_actor_to(gina, &walk_grid, (SDL_FPoint){m_pos.x, m_pos.y}, false,
                  NULL);
    break;
  }
}

static void update(float delta_time) { hen_update(gina, delta_time); }

static void render(SDL_Renderer *renderer) {
  render_image(renderer, background, (SDL_Point){0, 0});
  render_image(renderer, water, WATER_AT);
  render_image(renderer, sunscreen, SUNSCREEN_AT);
  if (!gina_state.has_goggles) {
    render_image(renderer, goggles, GOGGLES_AT);
  }
  if (gina_state.float_state == FLOAT_AT_POOL) {
    render_image(renderer, pool_float, FLOAT_AT);
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
    .chunks = chunks,
    .chunks_length = LEN(chunks),
};
