# Vania Volpe

A small collection of **Italian point-and-click adventure games for toddlers**,
written in **C99 + SDL2**. One engine, two adventures, three ways to play.

▶️ **Play in your browser:** https://potomak.github.io/VaniaVolpe/

## The adventures

Both are launched from a simple in-app hub:

- **Vania Volpe – Lo Scivolo** — Vania the fox fixes a slide at the playground:
  find the key, trade acorns with the squirrel, then slide! The spoken script is
  in [`src/adventures/vania_fox_the_slide/DIALOGS.md`](src/adventures/vania_fox_the_slide/DIALOGS.md).
- **Gina la Gallina in Piscina** — Gina the hen at the pool: sunscreen and
  goggles, a float the wind steals away, Carla the crow, and a grape-picking
  trade to get it back. Currently uses placeholder art.

Dialogue is spoken Italian with lip-synced mouths, plus **read-along
subtitles**: the line appears on screen with the currently spoken word
highlighted karaoke-style — an accessibility layer and a gentle learn-to-read
aid (add `?subtitles=0` to the URL to turn it off).

## Build & run

Needs the SDL2 / SDL2_image / SDL2_mixer / SDL2_ttf dev libraries (resolved via
`pkg-config`). Build with `make`:

| Command | Output | Notes |
| --- | --- | --- |
| `make` | `./vaniavolpe` | desktop SDL window |
| `make terminal` | `./vaniavolpe_terminal` | libcaca renderer, no display server needed (also needs `caca`) |
| `make web` | `build/web/index.{html,js,wasm,data}` | WebAssembly; run after `source /path/to/emsdk/emsdk_env.sh` |

`make web` is what CI builds and deploys to GitHub Pages on every push to `main`.

## Layout

- `src/` — the shared engine / app shell: game loop, scene framework, the generic
  `Actor`, image/sound/asset handling, and the adventure hub.
- `src/adventures/<name>/` — one self-contained adventure per directory (its
  scenes, actor spec, module, and `assets/`).
- `include/` — bundled SDL_image / SDL_mixer / SDL_ttf forwarding headers for
  the native build.

## More docs

- [`CLAUDE.md`](CLAUDE.md) — build/run, repo map and conventions (for contributors
  and AI agents).
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — the engine design in depth (including the
  terminal and headless-test backends).
- [`MOVEMENT.md`](MOVEMENT.md) — the walkability grid and A* pathfinding design.
- [`DEPTH_AND_CAMERA.md`](DEPTH_AND_CAMERA.md) — walk-behind props, depth
  variants, the scrolling camera and parallax planes.
- [`SPEECH.md`](SPEECH.md) — lip-sync cues and the read-along subtitles.
- [`LIVELINESS.md`](LIVELINESS.md) — idle fidgets, dragging the actor, and
  boiling (squiggly) hotspots.
- [`TOOLS.md`](TOOLS.md) — every internal dev tool, in one place.
- Queued work lives in [GitHub issues](https://github.com/potomak/VaniaVolpe/issues)
  (label `backlog`).
