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

Dialogue is **audio-only** — there is no on-screen text, so the games suit
pre-readers.

## Build & run

Needs the SDL2 / SDL2_image / SDL2_mixer dev libraries (resolved via
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
- `include/` — bundled SDL_image / SDL_mixer forwarding headers for the native
  build.

## More docs

- [`CLAUDE.md`](CLAUDE.md) — build/run, repo map and conventions (for contributors
  and AI agents).
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — the engine design in depth (including the
  terminal and headless-test backends).
- [`MOVEMENT.md`](MOVEMENT.md) — focused notes on the movement limitation.
- Queued work lives in [GitHub issues](https://github.com/potomak/VaniaVolpe/issues)
  (label `backlog`).
