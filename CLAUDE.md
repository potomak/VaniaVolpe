# CLAUDE.md

Guidance for AI agents (and humans) working in this repo.

## What this is
**Vania Volpe – Lo Scivolo**: a small Italian point-and-click adventure game in
**C99 + SDL2** (SDL2, SDL2_image for PNG, SDL2_mixer for WAV, SDL2_ttf for the
subtitle text). The same code
builds three ways: a desktop SDL window, a terminal (libcaca) renderer, and a
**WebAssembly** build (Emscripten) deployed to GitHub Pages.

Live web build: https://potomak.github.io/VaniaVolpe/

## Build & run
All source is in `src/`. Build via `make`:

- `make` — desktop SDL window → `./vaniavolpe`. Needs the SDL2 / SDL2_image /
  SDL2_mixer / SDL2_ttf dev libraries (resolved via `pkg-config`).
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

The click choreography, waits and expected dialogue live in
`test/scripts/<name>.json` — one source of truth shared by the native test (via
a generated header) and the browser test. `test/web/run_playtest.js` reads the
same JSON and drives the web build with Puppeteer, saving screenshots.

CI on every push/PR: `.github/workflows/test.yml` runs the native headless
playthrough and `web-test.yml` runs the browser playthrough (both gating);
`lint.yml` runs clang-format/clang-tidy (advisory).
`.github/workflows/deploy-pages.yml` compiles the web target on pushes to `main`
and deploys it to GitHub Pages. See ARCHITECTURE.md → *Terminal & Headless
Backends* for how the harnesses work.

## Repo map
- `src/` — **common engine / app shell**: `main.c` (desktop entry + game loop),
  `main_terminal.c` + `terminal.c` (libcaca entry), `game.c` + `adventure.h`
  (adventure registry), `scene.{c,h}` (scene framework: scenes, props, depth
  bands, parallax planes), `actor.{c,h}` (generic character engine + depth
  variants), `walk.{c,h}` (walkability grid + A*), `camera.{c,h}` (scrolling
  scenes), `image.c` (sprite/animation engine + render offset), `sound.c`,
  `lipsync.c` (mouth-cue & word-timing sidecars), `subtitle.c` (dialogue
  overlay + read-along highlight), `asset.c` (locale-aware path resolution) +
  `locale.c` (locale selection), `debug.c`.
  - `emscripten/compat/` — header shims so `<SDL2/...>` includes resolve under
    Emscripten; `emscripten/shell.html` — custom web page (canvas + iOS audio
    unlock).
- `src/adventures/<adventure>/` — **one self-contained adventure per directory**.
  E.g. `src/adventures/vania_fox_the_slide/`: the adventure module
  (`vania_fox_the_slide.{c,h}`), its scenes (`intro.c`, `playground_entrance.c`,
  `playground.c`, `outro.c`), its actor (`fox.{c,h}` — the fox spec),
  and its `assets/` subtree, split into `common/` (shared) and one dir per locale
  (`it_IT/`, `en_US/`); each holds the scene subdirs (`intro/ playground/ …`).
  `src/adventures/depth_demo/` is a one-scene **developer demo**: the reference
  implementation for props, depth bands, camera and parallax (third hub entry).
- `assets/fonts/` — the engine-level subtitle font (Atkinson Hyperlegible, OFL).
- `include/` — bundled SDL_image / SDL_mixer / SDL_ttf forwarding headers
  (native build).
- `test/` — headless test harness (`harness.{c,h}`), the script interpreter
  (`script.{c,h}`), the play-tests (`play_gina.c`, …) and `main_test.c` entry
  point (`make test`); `scripts/` holds the shared JSON playthroughs and `web/`
  the Puppeteer browser test that consumes them.
- Docs: `ARCHITECTURE.md` (deep design, incl. the terminal & headless-test
  backends), `MOVEMENT.md` (walkability grid + A* pathfinding; shipped),
  `DEPTH_AND_CAMERA.md` (props, depth variants, camera, parallax; shipped),
  `SPEECH.md` (lip-sync cues + read-along subtitles; code phases shipped),
  `LIVELINESS.md` (idle fidgets — spec; actor drag & drop + boiling hotspots — shipped),
  `TOOLS.md` (index of every dev tool: debug overlay & walk-mask paint mode,
  browser tools, generator scripts, test harnesses). The queued
  work lives in **GitHub issues** (label `backlog`), not a file.

## How it works (quick)
Scene-based: `Game` runs the current `Adventure` (an ordered scene table); each
scene implements the `Scene` callback struct (`init` / `load_media` /
`process_input` / `update` / `render` / …) in `scene.h`. The playable character is
a generic `Actor` (the fox is an `ActorSpec`): a small state machine (IDLE /
WALKING / TALKING / …); clicks are routed around blocked ground via
`walk_actor_to` (grid + A*, see `MOVEMENT.md`). Animations are sprite sheets + a
CSV `.anim` file (`x,y,w,h` per frame, 12 FPS). Dialogue is spoken audio via
`actor_talk`, with cue-driven mouth frames and an on-screen **subtitle overlay**
whose currently spoken word is highlighted (see `SPEECH.md`). Scenes may be
larger than the window with a following camera and parallax planes (see
`DEPTH_AND_CAMERA.md`). Each adventure declares an `assets_root` so
`asset_resolve()` resolves its assets. See `ARCHITECTURE.md` for the full design.

## Gotchas
- **Assets are localized.** Each adventure's `assets/` splits into `common/`
  (shared) and one dir per locale (`it_IT`, `en_US`, …). `asset_resolve()` resolves
  `<assets_root>/<locale>/<dir>/<file>`, then `<assets_root>/common/<dir>/<file>`
  — strict, no cross-language fallback, so **every locale must be complete**.
  Voice/dialog WAVs and text-bearing images (intro/outro, labelled buttons) go
  under the locale; sprites, music, SFX and plain backgrounds go under `common/`.
  The active locale comes from `--locale=` / `$VANIA_LOCALE` / `$LANG` (web: the
  browser language, or `?lang=`); default `it_IT`. Dialogue **subtitles** (with
  the read-along word highlight, see `SPEECH.md`) are on by default;
  `--subtitles=0` / `$VANIA_SUBTITLES=0` / web `?subtitles=0` disable them. `tools/gen_en_us_placeholders.sh`
  scaffolds a complete `en_US` from `it_IT`.
- **Placeholder art & voice have a pipeline.** Each adventure lists the real
  assets it still needs in `assets/tasks.json`; artists upload raw files into
  per-task drop-boxes `<assets_root>/_inbox/<id>/`. To ingest uploads ("check
  and consolidate the assets I uploaded"), run `tools/consolidate_assets.py`
  (stitches frames → sprite sheet, moves WAVs/PNGs into place, archives sources
  under `_sources/<id>/`); the *Assets to author* page refreshes on the next
  `make web`. The `_inbox` / `_sources` dirs are never shipped. Full flow:
  `TOOLS.md` → *Asset pipeline*.
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
