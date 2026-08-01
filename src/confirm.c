//
//  confirm.c
//  See confirm.h.
//

#include "confirm.h"

#include "constants.h"

// The panel, centred, and the two answers inside it. Both targets are far
// larger than the hub button that opens the question — the point is that
// answering is easy and mis-answering is hard, so they are pushed to opposite
// sides with a wide gap between them.
#define PANEL_W 420
#define PANEL_H 220
static const SDL_Rect PANEL = {(WINDOW_WIDTH - PANEL_W) / 2,
                               (WINDOW_HEIGHT - PANEL_H) / 2, PANEL_W, PANEL_H};
#define ANSWER 120
static const SDL_Rect YES = {PANEL.x + 36, PANEL.y + (PANEL_H - ANSWER) / 2,
                             ANSWER, ANSWER};
static const SDL_Rect NO = {PANEL.x + PANEL_W - 36 - ANSWER,
                            PANEL.y + (PANEL_H - ANSWER) / 2, ANSWER, ANSWER};

static bool is_open;
static void (*confirmed)(void);

void confirm_open(void (*on_confirm)(void)) {
  is_open = true;
  confirmed = on_confirm;
}

void confirm_close(void) {
  is_open = false;
  confirmed = NULL;
}

bool confirm_is_open(void) { return is_open; }

bool confirm_process_input(const SDL_Event *event) {
  if (!is_open) {
    return false;
  }
  switch (event->type) {
  case SDL_MOUSEBUTTONUP:
    break;
  case SDL_MOUSEBUTTONDOWN:
  case SDL_MOUSEMOTION:
    // Swallowed: the scene underneath must not walk the actor or start a drag
    // while the question is up.
    return true;
  default:
    return false;
  }

  SDL_Point p = {event->button.x, event->button.y};
  if (SDL_PointInRect(&p, &YES)) {
    void (*answer)(void) = confirmed;
    // Closed before answering: the callback leaves the adventure, and the
    // overlay must not still be up when the next one starts.
    confirm_close();
    if (answer != NULL) {
      answer();
    }
    return true;
  }
  // Anything else is "no" — the cross, the panel, or a miss outside it. The
  // safe answer is the easy one to give.
  confirm_close();
  return true;
}

// A thick line, as a rect, so the tick and cross are drawn from the same
// primitive the rest of this file uses.
static void bar(SDL_Renderer *renderer, int x, int y, int w, int h) {
  SDL_Rect r = {x, y, w, h};
  SDL_RenderFillRect(renderer, &r);
}

// A tick, built from two bars: a short one down-right, a long one up-right.
// Drawn as a staircase of small squares so it reads at a glance without
// needing rotation.
static void tick(SDL_Renderer *renderer, SDL_Rect box) {
  const int step = box.w / 12;
  for (int i = 0; i < 4; i++) {
    bar(renderer, box.x + box.w / 6 + i * step,
        box.y + box.h / 2 + i * step - step, step * 2, step * 2);
  }
  for (int i = 0; i < 6; i++) {
    bar(renderer, box.x + box.w / 6 + (3 + i) * step,
        box.y + box.h / 2 + (3 - i) * step - step, step * 2, step * 2);
  }
}

// A cross, two diagonals of the same squares.
static void cross(SDL_Renderer *renderer, SDL_Rect box) {
  const int step = box.w / 12;
  for (int i = 0; i < 7; i++) {
    bar(renderer, box.x + box.w / 4 + i * step, box.y + box.h / 4 + i * step,
        step * 2, step * 2);
    bar(renderer, box.x + box.w / 4 + i * step,
        box.y + box.h * 3 / 4 - i * step, step * 2, step * 2);
  }
}

void confirm_render(SDL_Renderer *renderer) {
  if (!is_open) {
    return;
  }
  // Dim the whole screen: the adventure is still there, just not listening.
  // The blend mode is restored below — it is renderer-wide state, and every
  // other draw in the game expects opaque.
  SDL_BlendMode blend;
  SDL_GetRenderDrawBlendMode(renderer, &blend);
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x99);
  SDL_Rect screen = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
  SDL_RenderFillRect(renderer, &screen);
  SDL_SetRenderDrawBlendMode(renderer, blend);

  SDL_SetRenderDrawColor(renderer, 0xF4, 0xF1, 0xE8, 0xFF);
  SDL_RenderFillRect(renderer, &PANEL);
  SDL_SetRenderDrawColor(renderer, 0x33, 0x33, 0x33, 0xFF);
  SDL_RenderDrawRect(renderer, &PANEL);

  SDL_SetRenderDrawColor(renderer, 0x3F, 0xA9, 0x55, 0xFF);
  SDL_RenderFillRect(renderer, &YES);
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  tick(renderer, YES);

  SDL_SetRenderDrawColor(renderer, 0xD1, 0x4B, 0x3F, 0xFF);
  SDL_RenderFillRect(renderer, &NO);
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  cross(renderer, NO);
}
