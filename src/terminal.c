#include <SDL2/SDL.h>
#include <caca.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "constants.h"
#include "terminal.h"

// Pixel masks for SDL_PIXELFORMAT_RGBA32 on little-endian (ARM64 / x86-64):
// memory layout [R][G][B][A] = uint32 with R at bits 0-7
#define RMASK 0x000000FF
#define GMASK 0x0000FF00
#define BMASK 0x00FF0000
#define AMASK 0xFF000000

static caca_canvas_t  *cv     = NULL;
static caca_display_t *dp     = NULL;
static caca_dither_t  *dither = NULL;
static void           *pixels = NULL;
static int             pitch  = 0;

bool terminal_init(SDL_Renderer *renderer) {
  pitch  = WINDOW_WIDTH * 4; // 4 bytes per pixel (RGBA32)
  pixels = malloc(WINDOW_HEIGHT * pitch);
  if (!pixels) {
    fprintf(stderr, "terminal: failed to allocate pixel buffer\n");
    return false;
  }

  cv = caca_create_canvas(0, 0);
  if (!cv) {
    fprintf(stderr, "terminal: failed to create caca canvas\n");
    free(pixels);
    return false;
  }

  dp = caca_create_display(cv);
  if (!dp) {
    fprintf(stderr, "terminal: failed to create caca display\n");
    fprintf(stderr, "terminal: is TERM set and is a tty attached?\n");
    caca_free_canvas(cv);
    cv = NULL;
    free(pixels);
    return false;
  }

  caca_set_display_title(dp, "Vania Volpe - Lo Scivolo");
  caca_set_mouse(dp, 1);
  caca_set_cursor(dp, 0);

  // Cap terminal refresh at ~10 fps (100 ms). caca_refresh_display will
  // sleep to honour this, keeping CPU usage low while the game logic runs
  // at its own speed.
  caca_set_display_time(dp, 100000);

  dither = caca_create_dither(32, WINDOW_WIDTH, WINDOW_HEIGHT, pitch,
                              RMASK, GMASK, BMASK, AMASK);
  if (!dither) {
    fprintf(stderr, "terminal: failed to create caca dither\n");
    caca_free_display(dp);
    caca_free_canvas(cv);
    dp = NULL;
    cv = NULL;
    free(pixels);
    return false;
  }

  return true;
}

void terminal_present(SDL_Renderer *renderer) {
  if (!dp) return;

  if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA32,
                           pixels, pitch) != 0) {
    fprintf(stderr, "terminal: SDL_RenderReadPixels failed: %s\n",
            SDL_GetError());
    return;
  }

  int cw = caca_get_canvas_width(cv);
  int ch = caca_get_canvas_height(cv);
  caca_dither_bitmap(cv, 0, 0, cw, ch, dither, pixels);
  caca_refresh_display(dp);
}

// Scale a caca character-cell coordinate to game logical pixels.
static int scale_x(int cx) {
  int cw = caca_get_canvas_width(cv);
  if (cw <= 0) return 0;
  return (cx * WINDOW_WIDTH) / cw;
}

static int scale_y(int cy) {
  int ch = caca_get_canvas_height(cv);
  if (ch <= 0) return 0;
  return (cy * WINDOW_HEIGHT) / ch;
}

// Push a synthetic SDL mouse button event into the SDL event queue.
static void push_mouse_button(Uint32 type, int game_x, int game_y,
                              Uint8 button) {
  SDL_Event ev;
  SDL_memset(&ev, 0, sizeof(ev));
  ev.type               = type;
  ev.button.button      = button;
  ev.button.state       = (type == SDL_MOUSEBUTTONDOWN) ? SDL_PRESSED
                                                        : SDL_RELEASED;
  ev.button.x           = game_x;
  ev.button.y           = game_y;
  SDL_PushEvent(&ev);
}

// Push a synthetic SDL mouse motion event.
static void push_mouse_motion(int game_x, int game_y) {
  SDL_Event ev;
  SDL_memset(&ev, 0, sizeof(ev));
  ev.type         = SDL_MOUSEMOTION;
  ev.motion.x     = game_x;
  ev.motion.y     = game_y;
  SDL_PushEvent(&ev);
}

// Push a synthetic SDL key event.
static void push_key(Uint32 type, SDL_Keycode sym) {
  SDL_Event ev;
  SDL_memset(&ev, 0, sizeof(ev));
  ev.type            = type;
  ev.key.state       = (type == SDL_KEYDOWN) ? SDL_PRESSED : SDL_RELEASED;
  ev.key.keysym.sym  = sym;
  SDL_PushEvent(&ev);
}

void terminal_poll_events(void) {
  if (!dp) return;

  caca_event_t cev;
  while (caca_get_event(dp, CACA_EVENT_ANY, &cev, 0) > 0) {
    switch (caca_get_event_type(&cev)) {

    case CACA_EVENT_MOUSE_PRESS: {
      int gx = scale_x(caca_get_event_mouse_x(&cev));
      int gy = scale_y(caca_get_event_mouse_y(&cev));
      // caca button 1 = left, 2 = middle, 3 = right
      Uint8 btn = (Uint8)caca_get_event_mouse_button(&cev);
      push_mouse_button(SDL_MOUSEBUTTONDOWN, gx, gy, btn);
      break;
    }

    case CACA_EVENT_MOUSE_RELEASE: {
      int gx = scale_x(caca_get_event_mouse_x(&cev));
      int gy = scale_y(caca_get_event_mouse_y(&cev));
      Uint8 btn = (Uint8)caca_get_event_mouse_button(&cev);
      push_mouse_button(SDL_MOUSEBUTTONUP, gx, gy, btn);
      break;
    }

    case CACA_EVENT_MOUSE_MOTION: {
      int gx = scale_x(caca_get_event_mouse_x(&cev));
      int gy = scale_y(caca_get_event_mouse_y(&cev));
      push_mouse_motion(gx, gy);
      break;
    }

    case CACA_EVENT_KEY_PRESS: {
      int ch = caca_get_event_key_ch(&cev);
      switch (ch) {
      case CACA_KEY_ESCAPE:
        push_key(SDL_KEYDOWN, SDLK_ESCAPE);
        break;
      case 'd':
      case 'D':
        push_key(SDL_KEYDOWN, SDLK_d);
        break;
      case 'q':
      case 'Q':
      case CACA_KEY_CTRL_C:  // Ctrl+C — libcaca raw mode intercepts SIGINT
      case CACA_KEY_CTRL_D: {
        SDL_Event quit;
        SDL_memset(&quit, 0, sizeof(quit));
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
        break;
      }
      default:
        break;
      }
      break;
    }

    case CACA_EVENT_RESIZE:
      // Canvas is automatically resized by libcaca; nothing to do here
      // except allow the new dimensions to be picked up on the next
      // scale_x / scale_y call.
      break;

    case CACA_EVENT_QUIT: {
      SDL_Event quit;
      SDL_memset(&quit, 0, sizeof(quit));
      quit.type = SDL_QUIT;
      SDL_PushEvent(&quit);
      break;
    }

    default:
      break;
    }
  }
}

void terminal_deinit(void) {
  if (dither) { caca_free_dither(dither); dither = NULL; }
  if (dp)     { caca_free_display(dp);    dp     = NULL; }
  if (cv)     { caca_free_canvas(cv);     cv     = NULL; }
  if (pixels) { free(pixels);             pixels = NULL; }
}
