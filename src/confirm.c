//
//  confirm.c
//  See confirm.h.
//

#include "confirm.h"

#include "constants.h"
#include "image.h"

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

// Engine art, not any adventure's: loaded by path like the subtitle font, and
// listed in the repo-level assets/index.json so it shows up as something to
// draw (ASSETS.md).
#define CONFIRM_YES_PATH "assets/ui/confirm_yes.png"
#define CONFIRM_NO_PATH "assets/ui/confirm_no.png"
static ImageData yes_image;
static ImageData no_image;

bool confirm_load_media(SDL_Renderer *renderer) {
  bool ok = load_image_from_path(renderer, &yes_image, CONFIRM_YES_PATH);
  ok = load_image_from_path(renderer, &no_image, CONFIRM_NO_PATH) && ok;
  return ok;
}

void confirm_free_media(void) {
  free_image_texture(&yes_image);
  free_image_texture(&no_image);
}

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

  // The buttons are whole images — background and glyph together — so drawn
  // art can change how they look without the engine knowing anything but where
  // they go. Their rects are the hit targets; the art is centred in them.
  render_image(renderer, &yes_image,
               (SDL_Point){YES.x + (ANSWER - yes_image.width) / 2,
                           YES.y + (ANSWER - yes_image.height) / 2});
  render_image(renderer, &no_image,
               (SDL_Point){NO.x + (ANSWER - no_image.width) / 2,
                           NO.y + (ANSWER - no_image.height) / 2});
}
