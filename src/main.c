#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "asset.h"
#include "constants.h"
#include "depth_demo.h"
#include "game.h"
#include "gina_hen_at_the_pool.h"
#include "hub.h"
#include "image.h"
#include "locale.h"
#include "subtitle.h"
#include "vania_fox_the_slide.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// Global variables
Uint32 last_frame_time = 0;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

// Function to initialize our SDL window
bool init_window(void) {
  // Initialize only the subsystems the game needs. SDL_INIT_EVERYTHING also
  // pulls in haptic/joystick/sensor, which Emscripten's SDL2 port does not
  // implement, causing SDL_Init to fail in the browser.
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
                 SDL_GetError());
    return false;
  }

  // Set texture filtering to linear
  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Linear texture filtering not enabled");
  }

  window = SDL_CreateWindow("Vania Volpe - Lo Scivolo", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT,
                            SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
  if (!window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s",
                 SDL_GetError());
    return false;
  }

  // Prefer an accelerated, vsynced renderer; fall back to software so a machine
  // with no usable GPU (or a headless/software context) renders instead of
  // showing a black screen.
  renderer = SDL_CreateRenderer(
      window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer) {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Accelerated renderer unavailable (%s); trying software",
                SDL_GetError());
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  }
  if (!renderer) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s",
                 SDL_GetError());
    return false;
  }

  // Scale
  SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);

  // Initialize renderer color
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

  // Initialize PNG loading
  int imgFlags = IMG_INIT_PNG;
  if (!(IMG_Init(imgFlags) & imgFlags)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IMG_Init failed: %s",
                 IMG_GetError());
    return false;
  }

  // Initialize SDL_mixer
  if (Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT,
                    MIX_DEFAULT_CHANNELS, AUDIO_CHUNK_SIZE) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Mix_OpenAudio failed: %s",
                 Mix_GetError());
    return false;
  }
  // Reserve channel 0 for dialogue so SFX can't steal or cut it off.
  Mix_AllocateChannels(MIXER_CHANNEL_COUNT);
  Mix_ReserveChannels(1);

  // Lower the music volume
  Mix_VolumeMusic(MUSIC_VOLUME);

  return true;
}

// Function to poll SDL events and process keyboard input
void process_input(void) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    game_process_input(&event);

    switch (event.type) {
    case SDL_QUIT:
      game.is_running = false;
      break;
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
      // Exit the game
      case SDLK_ESCAPE:
        game.is_running = false;
        break;
      }
    }
  }
}

// Update function with a fixed time step
void update(void) {
  // Read the tick count once and use it for both the delta and the next frame's
  // baseline, so the two can't drift.
  Uint32 now = SDL_GetTicks();
  float delta_time = (now - last_frame_time) / 1000.0F;
  last_frame_time = now;

  game_update(delta_time);
}

// Render function to draw game objects in the SDL window
void render(void) {
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderClear(renderer);

  game_render(renderer);

  // Update screen
  SDL_RenderPresent(renderer);
}

// Function to destroy SDL window and renderer
void destroy_window(void) {
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

void destroy_image(void) { IMG_Quit(); }

void destroy_sound(void) {
  Mix_CloseAudio(); // matches Mix_OpenAudio
  Mix_Quit();
}

// One iteration of the game loop, shared by the native and web entry points.
static void main_loop(void) {
  process_input();
  update();
  render();
#ifdef __EMSCRIPTEN__
  if (!game.is_running) {
    emscripten_cancel_main_loop();
  }
#endif
}

// Main function
int SDL_main(int argc, char *argv[]) {
  game.is_running = init_window();
  if (!game.is_running) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize window!");
  }

  // Choose the language before any media is loaded (asset paths resolve through
  // the active locale).
  asset_set_locale(detect_locale(argc, argv));

  // Dialogue text overlay (SPEECH.md): reads --subtitles=/$VANIA_SUBTITLES and
  // loads the bundled font. A failure logs and disables subtitles, no more.
  subtitle_init(argc, argv, renderer);

  // Build each adventure's scene table before initializing scenes.
  vania_fox_the_slide_register();
  gina_hen_at_the_pool_register();
  depth_demo_register();

  // The content adventures, in menu order. This is the single source of truth:
  // the hub's list and the engine's full registry below both derive from it, so
  // adding an adventure is a one-line edit here.
  static const Adventure *content[] = {&vania_fox_the_slide,
                                       &gina_hen_at_the_pool, &depth_demo};
  hub_register(content, LEN(content));

  // Register the hub first (it is the start screen and the back-to-hub target),
  // then the content adventures. All of them are initialized and loaded up
  // front. Both arrays stay static because hub_register/register_adventures
  // retain the pointers for the whole run.
  static const Adventure *all[1 + LEN(content)];
  all[0] = &hub;
  for (int i = 0; i < (int)LEN(content); i++) {
    all[i + 1] = content[i];
  }
  register_adventures(&hub, all, LEN(all));

  game_init();

  if (game.is_running) {
    game.is_running = game_load_media(renderer);
  }
  if (!game.is_running) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load media!");
  }

  adventure_switch_to(&hub);

  // Baseline the frame clock right before the loop so the first delta is ~0
  // instead of "time since SDL init" (which would teleport time-stepped
  // actors).
  last_frame_time = SDL_GetTicks();

#ifdef __EMSCRIPTEN__
  // The browser owns the event loop; drive the game via requestAnimationFrame
  // (fps = 0) and never return (simulate_infinite_loop = 1).
  emscripten_set_main_loop(main_loop, 0, 1);
#else
  while (game.is_running) {
    main_loop();
  }

  game_deinit();

  // Tear down in reverse-init order: audio, then image, then renderer/window
  // (destroy_window calls SDL_Quit last).
  subtitle_deinit();
  destroy_sound();
  destroy_image();
  destroy_window();
#endif

  return 0;
}

/* Include the SDL main definition header */
#include <SDL2/SDL_main.h>

#if defined(__IPHONEOS__) || defined(__TVOS__)

#ifndef SDL_MAIN_HANDLED
#ifdef main
#undef main
#endif

int main(int argc, char *argv[]) {
  return SDL_UIKitRunApp(argc, argv, SDL_main);
}

#endif /* !SDL_MAIN_HANDLED */

#else

int main(int argc, char *argv[]) { return SDL_main(argc, argv); }

#endif /* __IPHONEOS__ || __TVOS__ */
