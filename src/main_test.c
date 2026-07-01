// Headless smoke test: drives a scripted playthrough of "Gina la Gallina in
// Piscina" with no display server or audio hardware, then asserts the expected
// dialogue appeared in order. This is the native analog of the browser
// scratchpad/gina_smoke.js — same choreography, no puppeteer.
//
// It reuses the offscreen-rendering trick from the terminal build: set
// SDL_VIDEODRIVER=offscreen + a software renderer before SDL_Init, so every
// frame is drawn into an ordinary RGBA buffer instead of onto a screen. Input
// is a table of scripted mouse events pushed onto SDL's own queue, so the
// game's process_input can't tell it apart from a real click.
//
// Exit code: 0 = all expected lines seen (and frames rendered non-blank),
// 2 = a step is missing, 1 = a setup/build failure.

#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "asset.h"
#include "constants.h"
#include "game.h"
#include "gina_hen_at_the_pool.h"
#include "hub.h"
#include "image.h"
#include "locale.h"
#include "vania_fox_the_slide.h"

// Dialogue (fprintf(stdout, …) / gina_say) is redirected here so the harness
// can read it back and assert on it; the run's own summary goes to stderr.
#define CAPTURE_PATH "vaniavolpe_test_stdout.log"

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

static bool init_window(void) {
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
  Mix_VolumeMusic(30);

  return true;
}

static void destroy_window(void) {
  Mix_CloseAudio();
  Mix_Quit();
  IMG_Quit();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

// ── game loop pieces ─────────────────────────────────────────────────────────

static void process_input(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    game_process_input(&event);
    if (event.type == SDL_QUIT) {
      game.is_running = false;
    }
  }
}

static Uint32 last_frame_time = 0;

static void step_frame(void) {
  process_input();

  Uint32 now = SDL_GetTicks();
  float delta_time = (now - last_frame_time) / 1000.0f;
  last_frame_time = now;
  game_update(delta_time);

  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderClear(renderer);
  game_render(renderer);
}

// Pump the real loop for ms milliseconds so walks and spoken lines have time to
// finish, the same waits scratchpad/gina_smoke.js uses.
static void pump_for(Uint32 ms) {
  Uint32 until = SDL_GetTicks() + ms;
  while (game.is_running && SDL_GetTicks() < until) {
    step_frame();
    SDL_Delay(8);
  }
}

// ── scripted input ───────────────────────────────────────────────────────────

static int gx(float fx) { return (int)(fx * WINDOW_WIDTH); }
static int gy(float fy) { return (int)(fy * WINDOW_HEIGHT); }

static void push_motion(int x, int y) {
  SDL_Event e;
  SDL_zero(e);
  e.type = SDL_MOUSEMOTION;
  e.motion.x = x;
  e.motion.y = y;
  e.motion.state = SDL_BUTTON_LMASK; // report the button as held (for brushing)
  SDL_PushEvent(&e);
}

static void push_button(Uint32 type, int x, int y) {
  SDL_Event e;
  SDL_zero(e);
  e.type = type;
  e.button.button = SDL_BUTTON_LEFT;
  e.button.x = x;
  e.button.y = y;
  SDL_PushEvent(&e);
}

// A click: move (so scenes that track m_pos see the cursor), then
// press/release.
static void click(float fx, float fy) {
  int x = gx(fx), y = gy(fy);
  push_motion(x, y);
  push_button(SDL_MOUSEBUTTONDOWN, x, y);
  push_button(SDL_MOUSEBUTTONUP, x, y);
}

// The sunscreen minigame: hold the button and serpentine over the close-up so
// every cell gets painted (mirrors gina_smoke.js's brush()).
static void brush(void) {
  int x0 = gx(0.36f), x1 = gx(0.64f);
  push_button(SDL_MOUSEBUTTONDOWN, x0, gy(0.31f));
  for (int row = 0; row < 8; row++) {
    int y = gy(0.31f + row * (0.38f / 7));
    int a = row % 2 == 0 ? x0 : x1;
    int b = row % 2 == 0 ? x1 : x0;
    for (int s = 0; s <= 16; s++) {
      push_motion(a + (b - a) * s / 16, y);
    }
    step_frame(); // let the paint register progressively
  }
  push_button(SDL_MOUSEBUTTONUP, x1, gy(0.31f));
  pump_for(1500);
}

// ── render sanity ────────────────────────────────────────────────────────────

// True if the current frame has more than one distinct colour — a cheap guard
// against a black-screen / missing-texture regression, without pinning exact
// pixels.
static bool frame_has_variation(void) {
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

// ── the scripted Gina playthrough ────────────────────────────────────────────

// Runs the full adventure. A render-sanity flag is set once, after the pool
// scene is on screen.
static bool g_frame_ok = false;

static void play_gina(void) {
  // Hub → Gina (the second button).
  click(0.5f, 0.497f);
  pump_for(1500);

  g_frame_ok = frame_has_variation(); // pool scene should be a real image

  // Try to leave the shade before sunscreen → refused.
  click(0.5f, 0.5f);
  pump_for(1200);
  // Tap the sunscreen bottle → walk → sunscreen minigame.
  click(0.175f, 0.883f);
  pump_for(2500);
  brush();

  // Collect goggles.
  click(0.45f, 0.808f);
  pump_for(2800);
  // Tap pool → needs the float.
  click(0.5f, 0.217f);
  pump_for(3000);
  // Tap the float → wind blows it into the tree.
  click(0.756f, 0.833f);
  pump_for(3000);
  // Tap pool again → float is now in the tree.
  click(0.5f, 0.217f);
  pump_for(3000);

  // Go to the tree (right edge).
  click(0.981f, 0.542f);
  pump_for(1200);
  // Examine the stuck float.
  click(0.681f, 0.167f);
  pump_for(3000);
  // Talk to Carla → she gives the basket.
  click(0.494f, 0.308f);
  pump_for(3000);

  // Go to the vine (right edge).
  click(0.981f, 0.542f);
  pump_for(1200);
  // Tap grapes → grapes minigame.
  click(0.5f, 0.4f);
  pump_for(2800);
  // Pick every grape.
  const float grapes[6][2] = {{0.45f, 0.283f},   {0.525f, 0.3f},
                              {0.6f, 0.283f},    {0.475f, 0.383f},
                              {0.5625f, 0.383f}, {0.519f, 0.483f}};
  for (int i = 0; i < 6; i++) {
    click(grapes[i][0], grapes[i][1]);
    pump_for(400);
  }
  pump_for(1000);

  // Back to the tree (left edge) and give Carla the grapes.
  click(0.019f, 0.542f);
  pump_for(1200);
  click(0.494f, 0.308f);
  pump_for(3000);

  // Back to the pool (left edge) and dive.
  click(0.019f, 0.542f);
  pump_for(1200);
  click(0.5f, 0.217f);
  pump_for(3500);
}

// ── assertions ───────────────────────────────────────────────────────────────

// Expected dialogue substrings, in order (the same set gina_smoke.js checks).
static const char *EXPECTED[] = {
    "Hub: starting adventure 'Gina la Gallina in Piscina'",
    "Devo mettere la crema",
    "Pronta! Ora posso uscire al sole",
    "Ho preso gli occhialini",
    "Mi serve il salvagente",
    "Oh no! Il vento",
    "Il salvagente e' sull'albero",
    "Non ci arrivo",
    "Prendi questo cestino",
    "Cestino pieno d'uva",
    "Ecco il tuo salvagente",
    "Che bello! Ancora",
};

// Read the captured stdout back into buf; returns bytes read (buf is
// NUL-terminated).
static size_t read_capture(char *buf, size_t cap) {
  FILE *f = fopen(CAPTURE_PATH, "r");
  if (f == NULL) {
    return 0;
  }
  size_t n = fread(buf, 1, cap - 1, f);
  buf[n] = '\0';
  fclose(f);
  return n;
}

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // Redirect dialogue (stdout) to a file so we can assert on it; SDL_Log stays
  // on stderr and remains visible for diagnostics.
  if (freopen(CAPTURE_PATH, "w", stdout) == NULL) {
    fprintf(stderr, "test: could not redirect stdout to %s\n", CAPTURE_PATH);
    return 1;
  }

  if (!init_window()) {
    fprintf(stderr, "test: window/media init failed\n");
    return 1;
  }
  game.is_running = true;

  // Force the Italian source locale so the asserted lines match regardless of
  // the CI host's $LANG.
  asset_set_locale("it_IT");
  (void)detect_locale; // (kept for symmetry with the other entry points)

  vania_fox_the_slide_register();
  gina_hen_at_the_pool_register();

  static const Adventure *content[] = {&vania_fox_the_slide,
                                       &gina_hen_at_the_pool};
  hub_register(content, LEN(content));

  static const Adventure *all[1 + LEN(content)];
  all[0] = &hub;
  for (int i = 0; i < (int)LEN(content); i++) {
    all[i + 1] = content[i];
  }
  register_adventures(all, LEN(all));
  set_current_adventure(&hub);

  game_init();
  if (!game_load_media(renderer)) {
    fprintf(stderr, "test: game_load_media failed\n");
    return 1;
  }
  set_active_scene(hub.entry_scene);

  last_frame_time = SDL_GetTicks();
  play_gina();

  game_deinit();
  destroy_window();

  // ── verify ────────────────────────────────────────────────────────────────
  fflush(stdout);
  static char captured[1 << 16];
  read_capture(captured, sizeof(captured));

  int missing = 0;
  size_t offset = 0; // enforce order: each match must come after the previous
  for (size_t i = 0; i < LEN(EXPECTED); i++) {
    const char *hit = strstr(captured + offset, EXPECTED[i]);
    if (hit == NULL) {
      // Fall back to an unordered check so the report distinguishes "missing"
      // from merely "out of order".
      bool anywhere = strstr(captured, EXPECTED[i]) != NULL;
      fprintf(stderr, "MISS%s  %s\n", anywhere ? " (out of order)" : "",
              EXPECTED[i]);
      missing++;
    } else {
      fprintf(stderr, "OK    %s\n", EXPECTED[i]);
      offset = (size_t)(hit - captured) + strlen(EXPECTED[i]);
    }
  }

  if (!g_frame_ok) {
    fprintf(stderr, "MISS  rendered frame had no variation (blank screen?)\n");
    missing++;
  } else {
    fprintf(stderr, "OK    rendered a non-blank frame\n");
  }

  remove(CAPTURE_PATH);

  if (missing == 0) {
    fprintf(stderr, "\nPASS: all %zu dialogue steps present, frame rendered\n",
            LEN(EXPECTED));
    return 0;
  }
  fprintf(stderr, "\nFAIL: %d check(s) missing\n", missing);
  return 2;
}
