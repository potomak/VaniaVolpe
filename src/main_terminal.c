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
#include "terminal.h"
#include "vania_fox_the_slide.h"

static Uint32 last_frame_time = 0;
static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;

static bool init_window(void) {
  // Must be set before SDL_Init so the video subsystem uses the offscreen
  // driver instead of trying to connect to a display server.
  SDL_setenv("SDL_VIDEODRIVER", "offscreen", 1);

  // Use the dummy audio driver so SDL_mixer can open a device and load/query
  // WAV files even when no real audio hardware is available. Sounds will be
  // silently discarded.
  SDL_setenv("SDL_AUDIODRIVER", "dummy", 1);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s",
                 SDL_GetError());
    return false;
  }

  window =
      SDL_CreateWindow("Vania Volpe", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
  if (!window) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow failed: %s",
                 SDL_GetError());
    return false;
  }

  // Software renderer — no GPU or display required.
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  if (!renderer) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateRenderer failed: %s",
                 SDL_GetError());
    return false;
  }

  SDL_RenderSetLogicalSize(renderer, WINDOW_WIDTH, WINDOW_HEIGHT);
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);

  // PNG loading
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

  if (!terminal_init(renderer)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to initialize terminal backend");
    return false;
  }

  return true;
}

static void process_input(void) {
  // Translate terminal events into SDL events first so they are available
  // to SDL_PollEvent in the same iteration.
  terminal_poll_events();

  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    game_process_input(&event);

    switch (event.type) {
    case SDL_QUIT:
      game.is_running = false;
      break;
    case SDL_KEYDOWN:
      if (event.key.keysym.sym == SDLK_ESCAPE) {
        game.is_running = false;
      }
      break;
    }
  }
}

static void update(void) {
  // Read the tick count once and use it for both the delta and the next frame's
  // baseline, so the two can't drift.
  Uint32 now = SDL_GetTicks();
  float delta_time = (now - last_frame_time) / 1000.0F;
  last_frame_time = now;
  game_update(delta_time);
}

static void render(void) {
  SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
  SDL_RenderClear(renderer);

  game_render(renderer);

  // Read back pixels and push to the terminal via libcaca instead of
  // presenting to a real window.
  terminal_present(renderer);
}

static void destroy_window(void) {
  terminal_deinit();
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
}

static void destroy_image(void) { IMG_Quit(); }

static void destroy_sound(void) {
  Mix_CloseAudio(); // matches Mix_OpenAudio
  Mix_Quit();
}

int main(int argc, char *argv[]) {
  game.is_running = init_window();
  if (!game.is_running) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to initialize terminal window!");
    return 1;
  }

  // Choose the language before any media is loaded.
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

  // Register the hub first (start screen + back-to-hub target), then content.
  // Both arrays stay static because hub_register/register_adventures retain the
  // pointers for the whole run.
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
    return 1;
  }

  adventure_switch_to(&hub);

  last_frame_time = SDL_GetTicks();
  while (game.is_running) {
    process_input();
    update();
    render();
  }

  game_deinit();

  // Tear down in reverse-init order: audio, then image, then terminal backend
  // and renderer/window (destroy_window calls SDL_Quit last).
  subtitle_deinit();
  destroy_sound();
  destroy_image();
  destroy_window();

  return 0;
}
