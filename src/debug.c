//
//  debug.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "constants.h"
#include "game.h"
#include "scene.h"

#include "debug.h"

// Is mouse button down
static bool is_m_down = false;
// Mouse position (while moving)
static SDL_Point m_pos;
// Mouse down position
static SDL_Point m_pos_down;
// Mouse up position
static SDL_Point m_pos_up;

// Walk-mask paint mode (toggled with W while debugging; see TOOLS.md):
// left-drag paints cells walkable, right-drag paints them blocked, S saves
// the mask into the source tree. No undo and no brushes on purpose — cells
// are 10x10 px and a wrong stroke is one opposite-drag away.
static bool is_painting_walk = false;
// Mouse button held while painting (0 = none).
static Uint8 paint_button = 0;

static void paint_cell(WalkGrid *grid, int x, int y, Uint8 walkable) {
  // Coordinates arrive in scene space (the engine converts a camera scene's
  // input), so the bounds are the scene-sized grid's, not the window's.
  if (x < 0 || x >= grid->w * WALK_CELL_SIZE || y < 0 ||
      y >= grid->h * WALK_CELL_SIZE) {
    return;
  }
  grid->cells[y / WALK_CELL_SIZE][x / WALK_CELL_SIZE] = walkable;
}

bool debug_process_input(SDL_Event *event) {
  WalkGrid *grid = scene_instance(game.current_scene)->walk_grid;

  switch (event->type) {
  case SDL_KEYDOWN:
    if (event->key.repeat) {
      break;
    }
    switch (event->key.keysym.sym) {
    case SDLK_w:
      if (grid != NULL) {
        is_painting_walk = !is_painting_walk;
        paint_button = 0;
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Walk paint mode %s (left-drag: walkable, right-drag: "
                    "blocked, S: save)",
                    is_painting_walk ? "on" : "off");
        return true;
      }
      break;
    case SDLK_s:
      if (is_painting_walk && grid != NULL) {
        walk_grid_save(grid, scene_instance(game.current_scene)->walk_mask_dir);
        return true;
      }
      break;
    }
    break;
  case SDL_MOUSEMOTION:
    if (is_painting_walk) {
      if (grid != NULL && paint_button != 0) {
        paint_cell(grid, event->motion.x, event->motion.y,
                   paint_button == SDL_BUTTON_LEFT ? 1 : 0);
      }
      return true;
    }
    // Get mouse position
    if (is_m_down) {
      m_pos_up.x = event->motion.x;
      m_pos_up.y = event->motion.y;
    } else {
      m_pos.x = event->motion.x;
      m_pos.y = event->motion.y;
    }
    break;
  case SDL_MOUSEBUTTONDOWN:
    if (is_painting_walk) {
      if (grid != NULL) {
        paint_button = event->button.button;
        paint_cell(grid, event->button.x, event->button.y,
                   paint_button == SDL_BUTTON_LEFT ? 1 : 0);
      }
      return true;
    }
    is_m_down = true;
    m_pos_down = m_pos;
    m_pos_up = m_pos;
    break;
  case SDL_MOUSEBUTTONUP:
    if (is_painting_walk) {
      paint_button = 0;
      return true;
    }
    is_m_down = false;
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Rect: {%d, %d, %d, %d}",
                m_pos_down.x, m_pos_down.y, m_pos_up.x - m_pos_down.x,
                m_pos_up.y - m_pos_down.y);
    break;
  }
  return false;
}

void debug_render(SDL_Renderer *renderer) {
  // Marker to show that the debugging layer is active
  SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
  SDL_RenderFillRect(renderer, &((SDL_Rect){0, 0, 10, 10}));

  // Paint mode: a yellow marker next to the red one, and a yellow frame
  // around the screen as an unmissable "you are editing" affordance.
  if (is_painting_walk) {
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xDD, 0x00, 0xFF);
    SDL_RenderFillRect(renderer, &((SDL_Rect){12, 0, 10, 10}));
    SDL_RenderDrawRect(renderer,
                       &((SDL_Rect){0, 0, WINDOW_WIDTH, WINDOW_HEIGHT}));
  }

  // Everything below is scene geometry drawn through raw SDL rects — which
  // the render offset in image.c doesn't touch — so the camera shift is
  // applied here explicitly. Zero for static scenes.
  SDL_Point off = render_get_offset();

  SDL_SetRenderDrawColor(renderer, 0x00, 0xCC, 0xFF, 0xFF);
  SDL_RenderDrawRect(renderer, &((SDL_Rect){.x = m_pos_down.x + off.x,
                                            .y = m_pos_down.y + off.y,
                                            .w = m_pos_up.x - m_pos_down.x,
                                            .h = m_pos_up.y - m_pos_down.y}));

  const Scene *current_scene = scene_instance(game.current_scene);

  // Shade non-walkable cells so walk geometry is visible and clearly distinct
  // from the cyan hotspot outlines.
  if (current_scene->walk_grid != NULL) {
    const WalkGrid *grid = current_scene->walk_grid;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0xFF, 0x33, 0x33, 0x50);
    for (int cy = 0; cy < grid->h; cy++) {
      for (int cx = 0; cx < grid->w; cx++) {
        if (!grid->cells[cy][cx]) {
          SDL_RenderFillRect(renderer,
                             &((SDL_Rect){cx * WALK_CELL_SIZE + off.x,
                                          cy * WALK_CELL_SIZE + off.y,
                                          WALK_CELL_SIZE, WALK_CELL_SIZE}));
        }
      }
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
  }

  // Draw hotspots: enabled ones bright, gated-off ones dimmed, so the
  // overlay shows what a click would actually hit right now.
  for (int i = 0; i < current_scene->hotspots_length; i++) {
    const Hotspot *hotspot = &current_scene->hotspots[i];
    bool is_enabled = hotspot->enabled == NULL || hotspot->enabled();
    if (is_enabled) {
      SDL_SetRenderDrawColor(renderer, 0xCC, 0xFF, 0x00, 0xFF);
    } else {
      SDL_SetRenderDrawColor(renderer, 0x55, 0x66, 0x00, 0xFF);
    }
    SDL_Rect rect = hotspot->rect;
    rect.x += off.x;
    rect.y += off.y;
    SDL_RenderDrawRect(renderer, &rect);
  }

  // Draw points of interest
  for (int i = 0; i < current_scene->pois_length; i++) {
    SDL_SetRenderDrawColor(renderer, 0xCC, 0x00, 0xFF, 0xFF);
    SDL_RenderFillRect(renderer,
                       &((SDL_Rect){.x = current_scene->pois[i].x - 2 + off.x,
                                    .y = current_scene->pois[i].y - 2 + off.y,
                                    .w = 4,
                                    .h = 4}));
  }
}
