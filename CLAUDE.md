# CLAUDE.md

Guidance for AI agents (and humans) working in this repo.

## What this is
**Vania Volpe – Lo Scivolo**: a small Italian point-and-click adventure game in
**C99 + SDL2** (SDL2, SDL2_image for PNG, SDL2_mixer for WAV). The same code
builds three ways: a desktop SDL window, a terminal (libcaca) renderer, and a
**WebAssembly** build (Emscripten) deployed to GitHub Pages.

Live web build: https://potomak.github.io/VaniaVolpe/

## Build & run
All source is in `src/`. Build via `make`:

- `make` — desktop SDL window → `./vaniavolpe`. Needs the SDL2 / SDL2_image /
  SDL2_mixer dev libraries (resolved via `pkg-config`).
- `make terminal` — libcaca renderer → `./vaniavolpe_terminal` (no display
  server needed). Also needs `caca`.
- `make web` — WebAssembly → `build/web/index.{html,js,wasm,data}`. Needs the
  **Emscripten SDK** (`emcc` on `PATH`); run `make web` after
  `source /path/to/emsdk/emsdk_env.sh`.
- `make clean` — removes objects, binaries, and `build/`.

`build/` is git-ignored; never commit generated web artifacts.

## Testing
There is **no automated test suite** — compiling is the gate. Before pushing:

- Build the relevant target (at minimum `make web`, which CI also runs).
- Play-test the interaction you changed (desktop, or the deployed web build).

The GitHub Actions workflow `.github/workflows/deploy-pages.yml` compiles the web
target on every push to `main` and deploys it to GitHub Pages.

## Repo map
- `src/` — **common engine / app shell**: `main.c` (desktop entry + game loop),
  `main_terminal.c` + `terminal.c` (libcaca entry), `game.c` + `adventure.h`
  (adventure registry), `scene.{c,h}` (scene framework), `actor.{c,h}` (generic
  character engine), `image.c` (sprite/animation engine), `sound.c`, `asset.c`
  (path resolution), `debug.c`.
  - `emscripten/compat/` — header shims so `<SDL2/...>` includes resolve under
    Emscripten; `emscripten/shell.html` — custom web page (canvas + iOS audio
    unlock).
- `src/adventures/<adventure>/` — **one self-contained adventure per directory**.
  E.g. `src/adventures/vania_fox_the_slide/`: the adventure module
  (`vania_fox_the_slide.{c,h}`), its scenes (`intro.c`, `playground_entrance.c`,
  `playground.c`, `outro.c`, `example.c`), its actor (`fox.{c,h}` — the fox spec),
  and its `assets/` subtree (PNG / WAV / `.anim`: `intro/ fox/ playground/
  playground_entrance/ outro/ example/ music/`).
- `include/` — bundled SDL_image / SDL_mixer forwarding headers (native build).
- Docs: `ARCHITECTURE.md` (deep design), `MOVEMENT.md` (movement limitation +
  future pathfinding), `BACKLOG.md` (future tasks), `TERMINAL_PLAN.md`.

## How it works (quick)
Scene-based: `Game` runs the current `Adventure` (an ordered scene table); each
scene implements the `Scene` callback struct (`init` / `load_media` /
`process_input` / `update` / `render` / …) in `scene.h`. The playable character is
a generic `Actor` (the fox is an `ActorSpec`): a small state machine (IDLE /
WALKING / TALKING / …); clicks hit-test `hotspots` and walk to `pois` via
`actor_walk_to`. Animations are sprite sheets + a CSV `.anim` file (`x,y,w,h` per
frame, 12 FPS). Dialogue is **audio-only** (no on-screen text) via `actor_talk`.
Each adventure declares an `assets_root` so `asset_path()` resolves its assets.
See `ARCHITECTURE.md` for the full design.

## Gotchas
- **Assets in the web build** must live under one of the `--preload-file` dirs in
  the `Makefile` (the adventure's `assets/` subdirs, e.g.
  `src/adventures/vania_fox_the_slide/assets/{intro,fox,playground,…}`); files
  elsewhere won't exist in the browser's virtual filesystem. An adventure's
  `assets_root` (set in its module) must match where those assets are preloaded.
- **SDL init must be `SDL_INIT_VIDEO | SDL_INIT_AUDIO`**, not
  `SDL_INIT_EVERYTHING` — Emscripten's SDL2 port has no haptic/joystick support
  and `SDL_Init` fails otherwise (black screen).
- The game loop is driven by `emscripten_set_main_loop` under `__EMSCRIPTEN__`
  (a blocking `while` loop would hang the browser); native uses a normal loop.
- Includes use the `<SDL2/...>`, `<SDL2_image/...>`, `<SDL2_mixer/...>` style; the
  web build relies on `src/emscripten/compat/` to map these to
  Emscripten's unprefixed headers.
- iOS Safari audio needs an in-gesture `AudioContext.resume()`; handled in
  `emscripten/shell.html`. Don't remove it.

## Conventions
- Work on a feature branch off `main`; never push to `main` directly. When a task
  is finished, **the agent opens the pull request** (the maintainer prefers not to
  open PRs manually). The agent never merges — the maintainer reviews and merges.
- Keep changes scoped (one task per PR). Prefer **independent PRs branched off
  `main`** over stacked branches, so they can be merged in any order without
  re-rebasing after each merge. See `BACKLOG.md` for the queued work.
- Match the existing C style (C99, two-space indent, `lower_snake_case`).
