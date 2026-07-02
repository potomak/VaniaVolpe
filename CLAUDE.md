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
- `make test` — headless scripted playthrough → `./vaniavolpe_test` (offscreen
  renderer + dummy audio, no display server). Asserts the adventure's dialogue
  and exits non-zero on a regression. See ARCHITECTURE.md → *Terminal & Headless
  Backends*.
- `make web` — WebAssembly → `build/web/index.{html,js,wasm,data}`. Needs the
  **Emscripten SDK** (`emcc` on `PATH`); run `make web` after
  `source /path/to/emsdk/emsdk_env.sh`. Also emits the **asset catalog** —
  `build/web/catalog.html` + `catalog.json` + a raw copy of every asset under
  `build/web/assets/` — a reference/QA page (deployed to
  `…/VaniaVolpe/catalog.html`) that lists assets per adventure/locale, plays
  audio and animations, and flags any locale missing a file the reference locale
  (`it_IT`) has. Built by `tools/gen_asset_catalog.py` + `src/emscripten/catalog.html`.
- `make clean` — removes objects, binaries, and `build/`.

`build/` is git-ignored; never commit generated web artifacts.

## Testing
`make test` runs a **headless scripted playthrough** (`test/`) that asserts the
adventure's dialogue in order — the gate for gameplay regressions. Before pushing:

- Build the relevant target (at minimum `make web`, which CI also runs).
- Run `make test` if you touched engine/gameplay code.
- Play-test the interaction you changed (desktop, or the deployed web build).

CI on every push/PR: `.github/workflows/test.yml` runs the headless playthrough
(gating) and `.github/workflows/lint.yml` runs clang-format/clang-tidy (advisory).
`.github/workflows/deploy-pages.yml` compiles the web target on pushes to `main`
and deploys it to GitHub Pages. See ARCHITECTURE.md → *Terminal & Headless
Backends* for how the test harness works.

## Repo map
- `src/` — **common engine / app shell**: `main.c` (desktop entry + game loop),
  `main_terminal.c` + `terminal.c` (libcaca entry), `game.c` + `adventure.h`
  (adventure registry), `scene.{c,h}` (scene framework), `actor.{c,h}` (generic
  character engine), `image.c` (sprite/animation engine), `sound.c`, `asset.c`
  (locale-aware path resolution) + `locale.c` (locale selection), `debug.c`.
  - `emscripten/compat/` — header shims so `<SDL2/...>` includes resolve under
    Emscripten; `emscripten/shell.html` — custom web page (canvas + iOS audio
    unlock).
- `src/adventures/<adventure>/` — **one self-contained adventure per directory**.
  E.g. `src/adventures/vania_fox_the_slide/`: the adventure module
  (`vania_fox_the_slide.{c,h}`), its scenes (`intro.c`, `playground_entrance.c`,
  `playground.c`, `outro.c`), its actor (`fox.{c,h}` — the fox spec),
  and its `assets/` subtree, split into `common/` (shared) and one dir per locale
  (`it_IT/`, `en_US/`); each holds the scene subdirs (`intro/ playground/ …`).
- `include/` — bundled SDL_image / SDL_mixer forwarding headers (native build).
- `test/` — headless test harness (`harness.{c,h}`), the scripted play-tests
  (`play_gina.c`, …), and the `main_test.c` entry point (`make test`).
- Docs: `ARCHITECTURE.md` (deep design, incl. the terminal & headless-test
  backends), `MOVEMENT.md` (movement limitation + future pathfinding). The queued
  work lives in **GitHub issues** (label `backlog`), not a file.

## How it works (quick)
Scene-based: `Game` runs the current `Adventure` (an ordered scene table); each
scene implements the `Scene` callback struct (`init` / `load_media` /
`process_input` / `update` / `render` / …) in `scene.h`. The playable character is
a generic `Actor` (the fox is an `ActorSpec`): a small state machine (IDLE /
WALKING / TALKING / …); clicks hit-test `hotspots` and walk to `pois` via
`actor_walk_to`. Animations are sprite sheets + a CSV `.anim` file (`x,y,w,h` per
frame, 12 FPS). Dialogue is **audio-only** (no on-screen text) via `actor_talk`.
Each adventure declares an `assets_root` so `asset_resolve()` resolves its assets.
See `ARCHITECTURE.md` for the full design.

## Gotchas
- **Assets are localized.** Each adventure's `assets/` splits into `common/`
  (shared) and one dir per locale (`it_IT`, `en_US`, …). `asset_resolve()` resolves
  `<assets_root>/<locale>/<dir>/<file>`, then `<assets_root>/common/<dir>/<file>`
  — strict, no cross-language fallback, so **every locale must be complete**.
  Voice/dialog WAVs and text-bearing images (intro/outro, labelled buttons) go
  under the locale; sprites, music, SFX and plain backgrounds go under `common/`.
  The active locale comes from `--locale=` / `$VANIA_LOCALE` / `$LANG` (web: the
  browser language, or `?lang=`); default `it_IT`. `tools/gen_en_us_placeholders.sh`
  scaffolds a complete `en_US` from `it_IT`.
- **Assets in the web build** must live under a `--preload-file` dir in the
  `Makefile` (now `assets/common` + each `assets/<locale>` per adventure); files
  elsewhere won't exist in the browser's virtual filesystem. An adventure's
  `assets_root` must match where those assets are preloaded.
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
  re-rebasing after each merge. The queued work lives in **GitHub issues** (label
  `backlog`, prioritised with `priority: high` / `priority: medium` / `priority:
  low`, and `needs-art` / `needs-audio` where assets must be authored separately).
- **Close the issue in the same PR.** A PR that addresses a `backlog` issue closes
  it with `Closes #N` in the PR description — or, if only part ships, comment on the
  issue with what's done and what's left and leave it open. New work surfaced while
  doing a task becomes a new `backlog` issue rather than a TODO left in the tree.
- Match the existing C style (C99, two-space indent, `lower_snake_case`).
