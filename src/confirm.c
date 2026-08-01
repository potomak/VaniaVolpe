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
//
// Macros rather than reads off PANEL: a const object is not a constant
// expression in C, so initializing one static rect from another's fields
// compiles only as a GNU extension. Clang (the Android NDK) rejects it.
#define PANEL_W 420
#define PANEL_H 220
#define PANEL_X ((WINDOW_WIDTH - PANEL_W) / 2)
#define PANEL_Y ((WINDOW_HEIGHT - PANEL_H) / 2)
#define ANSWER 120
#define ANSWER_INSET 36
#define ANSWER_Y (PANEL_Y + (PANEL_H - ANSWER) / 2)

static const SDL_Rect PANEL = {PANEL_X, PANEL_Y, PANEL_W, PANEL_H};
static const SDL_Rect YES = {PANEL_X + ANSWER_INSET, ANSWER_Y, ANSWER, ANSWER};
static const SDL_Rect NO = {PANEL_X + PANEL_W - ANSWER_INSET - ANSWER, ANSWER_Y,
                            ANSWER, ANSWER};

static bool is_open;
static void (*confirmed)(void);

// Engine art, not any adventure's: loaded by path like the subtitle font, and
// listed in the repo-level assets/index.json so it shows up as something to
// draw (ASSETS.md).
//
// The two answers boil while the question is up — the same "you can tap this"
// cue the scenes use (LIVELINESS.md Part 3). The panel behind them is not
// tappable, so it stays still.
#define CONFIRM_PANEL_PATH "assets/ui/confirm_panel.png"
#define CONFIRM_YES_SHEET "assets/ui/confirm_yes_boil.png"
#define CONFIRM_YES_ANIM "assets/ui/confirm_yes_boil.anim"
#define CONFIRM_NO_SHEET "assets/ui/confirm_no_boil.png"
#define CONFIRM_NO_ANIM "assets/ui/confirm_no_boil.anim"
#define BOIL_FRAMES 3

static ImageData panel_image;
static AnimationData *yes_boil;
static AnimationData *no_boil;

bool confirm_load_media(SDL_Renderer *renderer) {
  bool ok = load_image_from_path(renderer, &panel_image, CONFIRM_PANEL_PATH);
  yes_boil = make_animation_data(BOIL_FRAMES, LOOP);
  no_boil = make_animation_data(BOIL_FRAMES, LOOP);
  ok = load_animation_from_path(renderer, yes_boil, CONFIRM_YES_SHEET,
                                CONFIRM_YES_ANIM) &&
       ok;
  ok = load_animation_from_path(renderer, no_boil, CONFIRM_NO_SHEET,
                                CONFIRM_NO_ANIM) &&
       ok;
  return ok;
}

void confirm_free_media(void) {
  free_image_texture(&panel_image);
  free_animation(yes_boil);
  free_animation(no_boil);
  yes_boil = NULL;
  no_boil = NULL;
}

void confirm_open(void (*on_confirm)(void)) {
  is_open = true;
  confirmed = on_confirm;
  // Restart the boils with the question, so both answers wobble in step from
  // the moment they appear.
  if (yes_boil != NULL) {
    play_animation(yes_boil, NULL);
    play_animation(no_boil, NULL);
  }
}

void confirm_close(void) {
  is_open = false;
  confirmed = NULL;
  if (yes_boil != NULL) {
    stop_animation(yes_boil);
    stop_animation(no_boil);
  }
}

void confirm_update(int now_ms) {
  if (!is_open || yes_boil == NULL) {
    return;
  }
  animation_update(yes_boil, now_ms);
  animation_update(no_boil, now_ms);
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

// One answer, its boil centred in its hit target. A frame's size is the
// sheet's width by its clip height, so the art can be smaller than the target.
static void render_answer(SDL_Renderer *renderer, AnimationData *animation,
                          SDL_Rect target) {
  if (animation == NULL || animation->sprite_clips == NULL) {
    return;
  }
  const SDL_Rect *clip = &animation->sprite_clips[animation->current_frame];
  render_animation(renderer, animation,
                   (SDL_Point){target.x + (target.w - clip->w) / 2,
                               target.y + (target.h - clip->h) / 2});
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

  // Panel, then the two answers on it. Each is a whole picture — the engine
  // knows only where they go, so drawn art can change how they look. The rects
  // are the hit targets; the art is centred in them.
  render_image(renderer, &panel_image, (SDL_Point){PANEL.x, PANEL.y});
  render_answer(renderer, yes_boil, YES);
  render_answer(renderer, no_boil, NO);
}
