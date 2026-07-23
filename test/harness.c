#include "harness.h"

#include <SDL2_image/SDL_image.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdio.h>
#include <string.h>

#include "asset.h"
#include "clock.h"
#include "constants.h"
#include "depth_demo.h"
#include "game.h"
#include "gina_hen_at_the_pool.h"
#include "hub.h"
#include "image.h"
#include "subtitle.h"
#include "vania_fox_the_slide.h"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

// Fixed timestep for the deterministic virtual clock (#155): every harness
// frame advances the simulation by exactly this many ms, so the scripted
// playthrough runs identically regardless of machine speed or load. 16 ms ≈
// 60 fps — fine enough for the animation/talk timers the assertions depend on.
#define HARNESS_STEP_MS 16

// Dialogue and messages go through SDL_Log; we install our own log sink so the
// tests can read the stream back and assert on it. Lines are appended here
// (newline-separated) and also teed to stderr so the CI log still shows the
// playthrough.
static char log_buf[1 << 16];
static size_t log_len = 0;

// ── lifecycle ────────────────────────────────────────────────────────────────

bool harness_init(void) {
  // Drive the whole simulation off a deterministic virtual clock (#155) instead
  // of the wall clock, so the scripted playthrough is reproducible. Set before
  // any actor/animation is created (game_load_media in harness_start_game), so
  // every start_time is on the virtual clock from the first frame.
  clock_set_virtual(true);

  // Offscreen video + dummy audio: no display server, no sound card. Must be
  // set before SDL_Init so the subsystems pick these drivers.
  SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);
  SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
                 SDL_GetError());
    return false;
  }

  window =
      SDL_CreateWindow("Vania Volpe (test)", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (!window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s",
                 SDL_GetError());
    return false;
  }

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s",
                 SDL_GetError());
    return false;
  }

  SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

  int imgFlags = IMG_INIT_PNG;
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IMG_Init failed: %s",
                 IMG_GetError());
    return false;
  }

  if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT,
                    MIX_DEFAULT_CHANNELS, AUDIO_CHUNK_SIZE) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Mix_OpenAudio failed: %s",
                 Mix_GetError());
    return false;
  }
  // Reserve channel 0 for dialogue so SFX can't steal or cut it off.
  Mix_AllocateChannels(MIXER_CHANNEL_COUNT);
  Mix_ReserveChannels(1);

  Mix_VolumeMusic(MUSIC_VOLUME);

  // Subtitles render offscreen like everything else; the overlay path runs
  // in the playthroughs too (default setting: on).
  subtitle_init(0, NULL, renderer);

  return true;
}

void harness_shutdown(void) {
  game_deinit();
  subtitle_deinit();
  Mix_CloseAudio();
  Mix_Quit();
  IMG_Quit();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

bool harness_start_game(void) {
  // Force the Italian source locale so asserted lines match regardless of the
  // host's $LANG.
  asset_set_locale("it_IT");

  vania_fox_the_slide_register();
  gina_hen_at_the_pool_register();
  depth_demo_register();

  static const Adventure *content[] = {&vania_fox_the_slide,
                                       &gina_hen_at_the_pool, &depth_demo};
  hub_register(content, LEN(content));

  static const Adventure *all[1 + LEN(content)];
  all[0] = &hub;
  for (int i = 0; i < (int)LEN(content); i++) {
    all[i + 1] = content[i];
  }
  register_adventures(&hub, all, LEN(all));

  game_init();
  if (!game_load_media(renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "game_load_media failed");
    return false;
  }
  adventure_switch_to(&hub);

  game.is_running = true;
  return true;
}

// ── loop ─────────────────────────────────────────────────────────────────────

static void process_input(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    game_process_input(&event);
    if (event.type == SDL_QUIT) {
      game.is_running = false;
    }
  }
}

void harness_step_frame(void) {
  process_input();

  // Fixed timestep on the virtual clock (#155): advance simulated time by
  // exactly HARNESS_STEP_MS and feed game_update the matching delta, so both
  // the frame-delta motion and the clock-based animation/talk timers step in
  // lockstep — no dependence on the wall clock.
  clock_advance(HARNESS_STEP_MS);
  game_update(HARNESS_STEP_MS / 1000.0F);

  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderClear(renderer);
  game_render(renderer);
}

void harness_pump_for(Uint32 ms) {
  // Run a fixed number of frames (ceil), not a wall-clock duration, so `ms` of
  // simulated time always advances the same amount however fast this runs.
  Uint32 elapsed = 0;
  while (game.is_running && elapsed < ms) {
    harness_step_frame();
    elapsed += HARNESS_STEP_MS;
  }
}

// ── scripted input ───────────────────────────────────────────────────────────

static int to_px_x(double fx) { return (int)(fx * WINDOW_WIDTH); }
static int to_px_y(double fy) { return (int)(fy * WINDOW_HEIGHT); }

void harness_mouse_move(double fx, double fy) {
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_MOUSEMOTION;
  e.motion.x = to_px_x(fx);
  e.motion.y = to_px_y(fy);
  e.motion.state = SDL_BUTTON_LMASK; // report the button as held (for brushing)
  SDL_PushEvent(&e);
}

static void mouse_button(Uint32 type, double fx, double fy) {
  SDL_Event e;
  SDL_zero(e);
  e.type = type;
  e.button.button = SDL_BUTTON_LEFT;
  e.button.x = to_px_x(fx);
  e.button.y = to_px_y(fy);
  SDL_PushEvent(&e);
}

void harness_mouse_down(double fx, double fy) {
  mouse_button(SDL_MOUSEBUTTONDOWN, fx, fy);
}

void harness_mouse_up(double fx, double fy) {
  mouse_button(SDL_MOUSEBUTTONUP, fx, fy);
}

void harness_click(double fx, double fy) {
  harness_mouse_move(fx, fy);
  harness_mouse_down(fx, fy);
  harness_mouse_up(fx, fy);
}

// ── assertions ───────────────────────────────────────────────────────────────

bool harness_frame_has_variation(void) {
  static unsigned char px[WINDOW_WIDTH * WINDOW_HEIGHT * 4];
  if (SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA32, px,
                           WINDOW_WIDTH * 4) != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "SDL_RenderReadPixels failed: %s", SDL_GetError());
    return false;
  }
  for (int i = 4; i < WINDOW_WIDTH * WINDOW_HEIGHT * 4; i += 4) {
    if (px[i] != px[0] || px[i + 1] != px[1] || px[i + 2] != px[2]) {
      return true;
    }
  }
  return false;
}

// SDL log sink: append each message to log_buf (newline-separated) and tee it
// to stderr so the CI log still shows the run. The tee — and the harness's own
// OK/MISS/PASS reporting — use fprintf on purpose, not SDL_Log: routing them
// through SDL_Log would recurse back into this sink and pollute the captured
// buffer the assertions read.
static void SDLCALL capture_log(void *userdata, int category,
                                SDL_LogPriority priority, const char *message) {
  (void)userdata;
  (void)category;
  (void)priority;
  size_t mlen = strlen(message);
  if (log_len + mlen + 2 < sizeof(log_buf)) {
    memcpy(log_buf + log_len, message, mlen);
    log_len += mlen;
    log_buf[log_len++] = '\n';
    log_buf[log_len] = '\0';
  }
  fprintf(stderr, "%s\n", message);
}

bool harness_capture_begin(void) {
  // Capture everything the app logs; dialogue is logged at INFO (the default
  // for SDL_LOG_CATEGORY_APPLICATION, set explicitly here so it can't be
  // filtered).
  SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
  SDL_LogSetOutputFunction(capture_log, NULL);
  return true;
}

int harness_check_lines_in_order(const char *const *expected, size_t count) {
  int missing = 0;
  size_t offset = 0; // enforce order: each match must come after the previous
  for (size_t i = 0; i < count; i++) {
    const char *hit = strstr(log_buf + offset, expected[i]);
    if (hit == NULL) {
      // Distinguish "missing" from merely "out of order" in the report.
      bool anywhere = strstr(log_buf, expected[i]) != NULL;
      fprintf(stderr, "MISS%s  %s\n", anywhere ? " (out of order)" : "",
              expected[i]);
      missing++;
    } else {
      fprintf(stderr, "OK    %s\n", expected[i]);
      offset = (size_t)(hit - log_buf) + strlen(expected[i]);
    }
  }
  return missing;
}
