//
//  debug.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#include <SDL2/SDL.h>
#include <stdbool.h>

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

void debug_process_input(SDL_Event *event) {
  switch (event->type) {
  case SDL_MOUSEMOTION:
    // Get mouse position
    if (is_m_down) {
      SDL_GetMouseState(&m_pos_up.x, &m_pos_up.y);
    } else {
      SDL_GetMouseState(&m_pos.x, &m_pos.y);
    }
    break;
  case SDL_MOUSEBUTTONDOWN:
    is_m_down = true;
    m_pos_down = m_pos;
    m_pos_up = m_pos;
    break;
  case SDL_MOUSEBUTTONUP:
    is_m_down = false;
    fprintf(stdout, "Rect: {%d, %d, %d, %d}\n", m_pos_down.x, m_pos_down.y,
            m_pos_up.x - m_pos_down.x, m_pos_up.y - m_pos_down.y);
    break;
  }
}

void debug_render(SDL_Renderer *renderer) {
  // Marker to show that the debugging layer is active
  SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
  SDL_RenderFillRect(renderer, &((SDL_Rect){0, 0, 10, 10}));

  SDL_SetRenderDrawColor(renderer, 0x00, 0xCC, 0xFF, 0xFF);
  SDL_RenderDrawRect(renderer, &((SDL_Rect){.x = m_pos_down.x,
                                            .y = m_pos_down.y,
                                            .w = m_pos_up.x - m_pos_down.x,
                                            .h = m_pos_up.y - m_pos_down.y}));

  Scene current_scene = scene_instance(game.current_scene);
  // Draw hotspots
  for (int i = 0; i < current_scene.hotspots_length; i++) {
    SDL_SetRenderDrawColor(renderer, 0xCC, 0xFF, 0x00, 0xFF);
    SDL_RenderDrawRect(renderer, &current_scene.hotspots[i]);
  }

  // Draw points of interest
  for (int i = 0; i < current_scene.pois_length; i++) {
    SDL_SetRenderDrawColor(renderer, 0xCC, 0x00, 0xFF, 0xFF);
    SDL_RenderFillRect(renderer, &((SDL_Rect){.x = current_scene.pois[i].x - 2,
                                              .y = current_scene.pois[i].y - 2,
                                              .w = 4,
                                              .h = 4}));
  }
}
