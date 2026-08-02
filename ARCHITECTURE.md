# VaniaVolpe – Architecture Document

## Overview

**VaniaVolpe** is the engine behind a small collection of Italian point-and-click
adventures for a pre-reader. Each adventure is a short chain of scenes with one
puzzle apiece, solved by tapping things; the audience cannot read, so everything
is said out loud and shown in pictures.

- **Language:** C99
- **Core libraries:** SDL2, SDL2_image (PNG), SDL2_mixer (WAV), SDL2_ttf (subtitles)
- **Build system:** the `Makefile` — desktop, terminal (libcaca), headless test,
  and WebAssembly. The Xcode project for iOS/tvOS is not in step with the current
  layout (#31).
- **Ships as:** a desktop window, an ASCII terminal, a web build on GitHub Pages,
  and an Android APK.

Deliberately absent from this document: file counts, asset counts and line
totals. They went stale here three times over; the tree is the source of truth
for how big things are, and `assets/index.json` for what art exists.

### The design documents

This file describes the shape of the engine. Each subsystem with a real design
behind it has its own document, and those are the ones to read before changing
that subsystem:

| document | subject |
|---|---|
| [`MOVEMENT.md`](MOVEMENT.md) | walkability grid + A* pathfinding |
| [`SCALING.md`](SCALING.md) | continuous actor scaling by depth |
| [`DEPTH_AND_CAMERA.md`](DEPTH_AND_CAMERA.md) | props, scrolling camera, parallax planes |
| [`SPEECH.md`](SPEECH.md) | lip-sync cues + read-along subtitles |
| [`LIVELINESS.md`](LIVELINESS.md) | idle fidgets, actor drag & drop, boiling hotspots |
| [`ASSETS.md`](ASSETS.md) | the per-adventure asset manifest and what is generated from it |
| [`SCENES.md`](SCENES.md) | the declarative scene/dialogue layer |
| [`TOOLS.md`](TOOLS.md) | every dev tool: debug overlay, generators, harnesses |

---

## Directory Layout

```
VaniaVolpe/
├── src/                       # Common engine / app shell
│   ├── main.c                 # Desktop entry point and game loop
│   ├── main_terminal.c        # Terminal (libcaca) entry point
│   ├── terminal.{c,h}         # libcaca rendering backend
│   ├── game.{c,h}             # Central controller, adventure registry, input
│   ├── adventure.{c,h}        # Adventure descriptor + lifecycle
│   ├── hub.{c,h}              # The adventure-selection screen
│   ├── confirm.{c,h}          # The leave-the-adventure question
│   ├── scene.{c,h}            # Scene framework: scenes, hotspots, props, planes
│   ├── actor.{c,h}            # Generic character engine (an ActorSpec drives it)
│   ├── walk.{c,h}             # Walkability grid + A* (MOVEMENT.md)
│   ├── scaling.{c,h}          # Depth ramps (SCALING.md)
│   ├── camera.{c,h}           # Following camera for scenes wider than the window
│   ├── tween.{c,h}            # Scene-object position/scale tweens
│   ├── image.{c,h}            # Sprite/animation engine + render offset
│   ├── sound.{c,h}            # Audio utilities
│   ├── lipsync.{c,h}          # Mouth-cue & word-timing sidecars (SPEECH.md)
│   ├── subtitle.{c,h}         # Dialogue overlay + read-along highlight
│   ├── asset.{c,h}            # Locale-aware asset path resolution
│   ├── locale.{c,h}           # Locale selection
│   ├── clock.{c,h}            # Simulation clock seam (real vs virtual time)
│   ├── constants.h            # Global constants (resolution, FPS, volumes)
│   ├── debug.{c,h}            # Debug overlay + walk-mask paint mode
│   ├── emscripten/            # Web compat-shim headers, shell.html, tool pages
│   └── adventures/            # One self-contained adventure per directory
│       ├── vania_fox_the_slide/    # "Lo Scivolo": the fox and the slide
│       ├── gina_hen_at_the_pool/   # "Gina la Gallina in Piscina": the hen
│       └── depth_demo/             # Developer reference: props, depth, parallax
│           └── <each>/assets/      # common/ + one dir per locale (it_IT, en_US)
│
├── assets/                    # Engine-owned art: the subtitle font, the
│   │                          # selection screen, the leave confirmation
│   └── index.json             # …and its manifest, like an adventure's
├── gen/                       # Committed generated asset declarations
├── test/                      # Harness, play-tests, unit tests, browser test
├── tools/                     # Generators and pipeline scripts (TOOLS.md)
├── android/                   # Android project (SDL2's android-project template)
└── include/                   # Bundled SDL_image / SDL_mixer / SDL_ttf headers
```

---

## Game Loop

```
main()
  init_window()             → SDL init, window (800×600), renderer, audio, font
  <adventure>_register()    → each adventure fills its scene table
  hub_register(content)     → the selection screen learns the content adventures
  register_adventures(...)  → the engine learns all of them, hub included
  game_init()               → init every scene of every adventure
  game_load_media()         → load every texture, animation and sound, up front
  adventure_switch_to(&hub) → start on the selection screen

  loop while running:
    game_process_input()    → normalize touch → modal → engine keys/buttons → scene
    game_update(dt)         → hotspot boils, scene animations, scene, camera, modal
    game_render()           → planes → sprites → scene → UI overlays → present

  game_deinit()             → free every adventure's resources
```

- **Resolution:** 800 × 600 logical pixels (resizable window, high-DPI aware);
  `SDL_RenderSetLogicalSize` maps input and output, so the game only ever deals
  in logical coordinates.
- **Frame cap:** V-Sync (`SDL_RENDERER_PRESENTVSYNC`); `FPS` / `FRAME_TARGET_TIME` in `constants.h`.
- **Web:** the loop is driven by `emscripten_set_main_loop` under
  `__EMSCRIPTEN__` — a blocking `while` would hang the browser.
- **Time** is read through `clock_now_ms()` (`clock.{c,h}`), not `SDL_GetTicks()`
  directly. In real mode the two are the same; the headless test switches to a
  virtual clock and steps it a fixed amount per frame, which is what makes the
  scripted playthroughs reproducible and instant.
- **Media loads up front**, all adventures at once, and a failure aborts startup
  rather than limping on with missing textures. Loading on demand is #49.

---

## Module Responsibilities

### `game.c` – Central Controller

Owns the top-level state and delegates to the active scene.

```c
typedef struct game {
  bool is_running;
  bool is_debugging;
  const Adventure *current_adventure;
  int current_scene;          // index into current_adventure->scenes
} Game;
```

Scene indices are **adventure-local**: each adventure's header declares its own
enum (`INTRO…OUTRO` for Vania, `GINA_INTRO…GINA_OUTRO` for Gina), and
`scene_instance(i)` resolves one against the current adventure.

Key functions: `register_adventures`, `adventure_switch_to`, `game_init`,
`game_load_media`, `set_active_scene` / `set_active_scene_at`, `game_update`,
`game_render`, `game_process_input`.

`set_active_scene` calls `on_scene_inactive` on the outgoing scene and `on_scene_active` on the incoming one, allowing each scene to reset its state and start/stop background music. `set_active_scene_at` additionally stands the incoming scene's actor at a given point, applied *after* `on_scene_active` so it overrides the scene's own default — a location reachable from several directions keeps one default start, and the transition says which way the player came in. Gina's three outdoor scenes use it: each exports a `GINA_<SCENE>_ENTRY_FROM_<ORIGIN>` point for each of its doors, and its neighbours pass that when they send her over — so where a door is stays a property of the scene that owns the art.

**Touch is converted to mouse events at the door.** SDL delivers a finger event *and* a mouse event synthesized from the same touch; `normalize_touch` (in `game_process_input`) drops the synthesized one and rewrites the finger event as the mouse event the rest of the engine speaks, so nothing downstream knows a touchscreen exists. Finger coordinates are normalized to the *window*, so they go through `SDL_RenderWindowToLogical` — scaling by `WINDOW_WIDTH` directly would land the tap in the wrong place whenever the window is letterboxed. The first finger down owns the interaction until it lifts: a toddler puts down more than one, and a second palm must not drag what the first is holding.

**Taps act on release, gestures on press.** Every tap dispatch — the hub's menu, the back-to-hub button, both title screens, every hotspot, walk-to-click — runs on `SDL_MOUSEBUTTONUP`. Only gestures use the press: the actor drag arms on `MOUSEBUTTONDOWN`, and the minigames' brush strokes run press-to-release.

Two reasons. It is the ordinary convention — a press that turns out to be a mistake can be slid away from and released harmlessly, which matters for the audience. And it makes a whole class of bug impossible: a release is the last event of a tap, so a scene reached by one is never handed the back half of the tap that opened it. Mixing the two models is what kept Vania's end card off screen — `playground.c` switched to it on press, and the matching release went to the card, which reads a release as "leave".

**The debug layer has two ways in, and a way to have none.** The **D** key, and a two-second press-and-hold in the top-left corner for devices with no keyboard — one finger, so it needs no exception to the ownership rule above, and it fires from `game_update` rather than the event loop, since holding still produces no events. Only the release that completes a hold is swallowed, so an ordinary tap in that corner still reaches the scene. Both are inside `#ifndef PROD`: a `PROD=1` build has no path that can set `is_debugging`, which is the only thing standing between a player and the overlay.

The back-to-hub button is the one piece of engine-level UI that acts on the game rather than on a scene, and it opens a confirmation instead of leaving outright (`src/confirm.{c,h}`): it sits in a corner a palm finds by accident, and leaving discards an adventure's progress. It is 64 px square — Apple's minimum tap target is 44 pt, and this is aimed at a two-year-old's finger — and drawn from `assets/ui/hub_button.png`. It is the one tappable thing in the game that does **not** boil: it sits over a scene whose own hotspots are boiling to say "tap me", and a wobble in the corner would pull the eye off them. The corner it occupies is reserved — a scene must not put a hotspot under it, since the button is tested first and would swallow the tap (Gina's destination tiles sit below it for that reason). The overlay is modal — it consumes every mouse event while up, so nothing walks or drags behind it — and wordless, since the audience cannot read. The panel and its two buttons are engine-owned art (`assets/ui`), listed in the repo-level `assets/index.json` and drawn like any other outstanding asset — shared by every adventure rather than duplicated into each one's tree. The engine knows only where each goes, so the drawn art decides how they look. The buttons are tappable and so they boil, the same cue the scenes' hotspots use (`LIVELINESS.md` Part 3); the panel behind them is not, and stays still. Anything that is not the tick counts as "no", including a miss outside the panel: the forgiving answer is the easy one to give.

**The selection screen is a home screen of cartridges** (`src/hub.c`). The hub is an ordinary `Adventure` with one scene, and each content adventure is a cartridge in a 3×2 grid: one shared cartridge drawing with a transparent hole, tinted with that adventure's `cartridge_color`, drawn over that adventure's `hub_icon`. Tapping one starts it. Splitting the drawing this way means an adventure contributes only the picture that identifies it — the shell, the background and the corner buttons belong to the screen and are engine art under `assets/hub`, loaded by path like the confirmation's. The icon is the exception: it carries the adventure's name, so it is localized and lives in that adventure's own tree, which is why `load_media` borrows each adventure's asset root to load it. The colour is applied by multiplying the shell's texture, so the cartridge art is drawn light — a dark body would swallow the tint. Three corner buttons (settings, about, quit) sit apart from the grid; the first two are deliberately inert, so the screen's shape settles before there is anything behind them. Six slots is what fits at a size a toddler can hit; a seventh adventure needs the screen to scroll (#191). The eventual look — an '80s kid's room, the cartridge inserted into a console, the TV zooming to fill the screen — is #104; this grid is what it starts from.

---

### `scene.h` – Scene Interface

A scene is a struct of callbacks and data tables — a vtable in C. Each scene file
exports one `Scene` (e.g. `intro_scene`, `pool_scene`), and an adventure's
register function collects them into its scene table.

```c
struct Scene {
    // Lifecycle. Every callback is optional except where a scene needs it:
    // leaving update/render NULL gets the framework's default (tick and draw
    // the actor), which is what most scenes want.
    void (*init)(void);
    bool (*load_media)(SDL_Renderer *);
    void (*process_input)(SDL_Event *);   // NULL = default hotspot dispatch
    void (*update)(float dt);
    void (*render)(SDL_Renderer *);
    void (*deinit)(void);
    void (*on_scene_active)(void);
    void (*on_scene_inactive)(void);

    Hotspot   *hotspots;      // rect + on_tap / poi + on_arrive, optional boil
    SDL_Point *pois;          // named waypoints
    Actor    **actor;         // the scene's actor, built from actor_spec
    const ActorSpec *actor_spec;
    SDL_FPoint       actor_start;

    const ScaleRamp *scale_ramp;   // depth → scale (SCALING.md)
    WalkGrid        *walk_grid;    // walkability + A* (MOVEMENT.md)
    const char      *walk_mask_dir;
    Camera          *camera;       // scenes larger than the window
    SceneSprite     *sprites;      // static drawn layer
    Prop            *props;        // depth-sorted with the actor
    Plane           *bg_planes, *fg_planes;   // parallax (DEPTH_AND_CAMERA.md)

    ImageData *images;             // scene-owned textures
    ChunkData *chunks;             // SFX + per-line dialogue
    Asset      music;
    AnimationData **animations;
    const SceneAnimSpec *anim_specs;  // declarative: framework makes & loads them
};
```

The trend across the framework is **declarative**: a scene states what it has
(`anim_specs`, `sprites`, `hotspots` with their boils) and the framework does
the making, loading, ticking, drawing and freeing. See `SCENES.md`.

---

### `actor.c` – Characters

There is one character engine. A character is an **`ActorSpec`** — a `const`
description of sprites, speed, sounds and which state is its idle — and
`actor.c` runs it. The fox (`fox.c`) and the hen (`hen.c`) are each just a spec
plus a couple of thin wrappers; neither owns any movement or dialogue code.

#### State machine

```
IDLE ──walk_actor_to()──> WALKING ──arrival──> IDLE ──> (on_arrive fires)
IDLE ──actor_talk()────> TALKING  ──line ends──> IDLE
IDLE ──idle timeout────> FIDGETING ──> IDLE          (LIVELINESS.md Part 1)
IDLE ──drag───────────> DRAGGED ──release──> FALLING ──> LANDING ──> IDLE
                                                      (LIVELINESS.md Part 2)
plus the spec's own resting states: SITTING, WAVING
```

#### Movement

`walk_actor_to` routes around blocked ground: the scene's `WalkGrid` gives A* a
path, which is smoothed and walked as a short list of waypoints. Facing is the
sprite's own `flip` — sheets are drawn facing **west**, and east is the mirror.
See `MOVEMENT.md`.

#### Scale and depth

An actor's size is read from the scene's `ScaleRamp` at its ground line, so it
shrinks with distance continuously rather than in bands. Two coordinate spaces
matter and are easy to confuse: `current_position` is the sprite's **centre**,
while `ground_y` and the ramp are **ground lines**. See `SCALING.md`.

#### Dialogue

`actor_talk(actor, chunk, text)` plays the line and drives the mouth from its
`.cues` sidecar, with the spoken word highlighted in the subtitle overlay from
its `.words` sidecar. A line with no recorded WAV still shows its text, so an
adventure is playable before the voice is recorded. See `SPEECH.md`.

#### Inventory

There is no engine-level inventory. What a character is carrying is the
adventure's own state (`gina_state.c`, or plain statics in Vania's scenes);
`actor_carried_at` only answers where a held item should be drawn, mirroring
the offset when the sprite is flipped.

---

### `image.c` – Sprite & Animation Engine

Sprite-sheet loading, frame extraction, playback, and the camera's render
offset.

```c
struct AnimationData {
    ImageData image;                // SDL_Texture* + dimensions
    SDL_Rect  *sprite_clips;        // one SDL_Rect per frame
    int        frames, current_frame;
    AnimationPlaybackStyle style;   // LOOP | ONE_SHOT (PING_PONG unimplemented)
    int        loop_count, max_loop_count;
    int        ms_per_frame;        // 0 = DEFAULT_MS_PER_FRAME (12 FPS)
    bool       is_playing;
    int        start_time;
    SDL_RendererFlip flip;
    void     (*on_end)(void);       // fires once when a ONE_SHOT completes
};
```

- **Timing** lives in `animation_update`, called once per frame by the owner —
  not in `render_animation`. Playback speed therefore does not depend on how
  often a frame is drawn, and an end callback cannot fire mid-render.
- **Frame data** is a `.anim` file: one `x,y,w,h` per line, one line per frame,
  parsed strictly — a malformed file is rejected loudly rather than half-loaded.
  (An older `.json` format is gone.)
- **Transparency** is the PNG's own alpha; there is no colour key.
- **Render offset**: `render_set_offset` shifts every draw by the camera's
  scroll, set by `game_render` around the scene's pass and reset for screen-space
  UI. Scenes keep drawing in scene coordinates and never learn a camera exists.

---

### `sound.c` – Audio Utilities

Thin wrapper providing `get_chunk_time_ms(Mix_Chunk*)`, the playback duration of
a WAV computed from its byte count, frequency and format — how the dialogue
system knows when a line ends.

---

### `asset.c` – Asset Path & Locale Resolution

Resolves an `Asset{directory, filename}` to a path, layering a **locale** over an
adventure's `assets_root`:

```c
bool asset_resolve(Asset a, char *buf, size_t n); // resolve into caller's buffer;
                                        // true if the locale layer was used
void asset_set_root(const char *root);  // per-adventure assets dir
void asset_set_locale(const char *l);   // "it_IT" (default/source), "en_US", …
```

`asset_resolve` writes into a caller-owned buffer (`ASSET_PATH_MAX` bytes), so two
resolved paths can be live at once without clobbering each other — every call site
declares a local buffer, resolves, then loads. `asset_set_root`/`asset_set_locale`
store the pointer, not a copy, so the string must outlive the asset system.

Localized assets (voice WAVs, text-bearing images like the intro/outro and the
labelled buttons) live under `assets/<locale>/`; shared assets (sprites, music,
SFX, drawn backgrounds) under `assets/common/`. The lookup tries the active
locale, then `common` — strict, with no cross-language fallback, so each locale
must provide all of its assets. On iOS the bundle is flat today (localization
there is a follow-up). `detect_locale()` (`locale.c`) picks the locale from a
`--locale=` argument, `$VANIA_LOCALE`, or `$LANG`; the web build forwards the
browser language (or `?lang=`) via `Module.arguments`.

---

### `debug.c` – Debug Overlay

Reached with the **D** key or by holding the top-left corner for two seconds
(see the input section above); both are compiled out by `PROD=1`. It draws cyan
outlines over the scene's hotspots, magenta dots at its POIs, and translucent
red over blocked cells of its walk grid.

It also carries two authoring tools: a **rect picker** (drag anywhere to print
an `SDL_Rect` to the log — how hotspot and walkable rectangles are authored
against the live scene) and **walk-mask paint mode** (**W**, then drag to paint
walkability and **S** to save a `.walk` file into the source tree). `TOOLS.md`
documents both.

---

## Adventures & Scenes

Three adventures are registered. The engine knows nothing about any of them
beyond the `Adventure` struct: an id, a title, an assets root, an ordered scene
table, an entry scene, and how it appears on the selection screen.

### Vania Volpe — Lo Scivolo (`vania_fox_the_slide/`)

The fox, four scenes, real recorded Italian voice.

```
INTRO ──Play──> PLAYGROUND_ENTRANCE ──gate opens──> PLAYGROUND ──3 slides──> OUTRO
  │                                                                            │
  └──Exit──> (quit)                                          (tap) ──> the hub ─┘
```

- **Playground entrance** — dig up the key with the shovel, open the gate.
- **Playground** — shake acorns from the tree, trade them to the squirrel for
  the missing peg, fix the slide, then slide three times. The slide trajectory
  follows a sigmoid.

### Gina la Gallina in Piscina (`gina_hen_at_the_pool/`)

The hen, seven scenes, three of them a connected outdoor ring she walks between
by tapping a tile of where she is going. Voice not yet recorded, so its dialogue
plays as subtitles.

```
GINA_INTRO ──Play──> GINA_POOL
GINA_POOL ⇄ GINA_TREE ⇄ GINA_VINE ⇄ GINA_POOL   a ring, by destination tiles
GINA_POOL ──sunscreen──> GINA_SUNSCREEN_MINIGAME ──> GINA_POOL
GINA_VINE ──grapes────> GINA_GRAPES_MINIGAME ────> GINA_VINE
GINA_POOL ──dive──────> GINA_OUTRO ──(tap)──> the hub
```

Sun cream before going out, goggles, then the float — which the wind takes into
the tree, so Carla the crow trades help for a basket of grapes. Cross-scene
puzzle state lives in `gina_state.c` and is reset by the adventure's `on_enter`,
so it is replayable.

### Depth demo (`depth_demo/`)

One scene, developer-facing: the reference implementation for props, depth
scaling, the camera and parallax planes.

---

## Input System

```
SDL_PollEvent()
  │
  ├─ normalize_touch      finger events → mouse events; drop the ones SDL
  │                       synthesized from the same touch; first finger wins
  ├─ confirm_process_input   a modal question owns the pointer while it is up
  ├─ engine keys          D (debug), ESC (quit) — D compiled out by PROD=1
  ├─ the corner hold      press-and-hold top-left → debug (also PROD-gated)
  ├─ the back-to-hub button  screen space, tested before any camera conversion
  ├─ camera conversion    scrolling scenes: mouse coords → scene coords, in place
  ├─ debug_process_input  may swallow (walk-paint mode)
  └─ scene.process_input  or the framework's default hotspot dispatch
```

Two rules hold throughout, and both are load-bearing enough to be worth
restating: **taps act on release, gestures act on press**; and everything
downstream of `normalize_touch` deals only in mouse events, in logical
coordinates. A scene never learns whether it was touched or clicked, nor
whether a camera is scrolling it.

---

## Audio Architecture

| Category | Notes |
|---|---|
| Background music | a scene may name one (`Scene.music`); looped at `MUSIC_VOLUME`. Vania's scenes do; Gina's have none yet |
| Movement SFX | the actor spec's `move_sound_filename`, looped while walking |
| Dialogue | one WAV per line; its duration and `.cues` drive the mouth |
| Item / event SFX | the adventure's shared bank, played by generated `play_<name>()` |

All WAV: SDL2_mixer's `Mix_Chunk` for everything but music streams. Channel 0 is reserved for dialogue (`DIALOG_CHANNEL`) so an effect cannot cut off a voice line mid-sentence. Dialogue is
localized (one WAV per locale); music and SFX are shared. A line whose WAV has
not been recorded yet still plays as text, so an adventure is playable before
the voice exists.

On iOS Safari the audio context must be resumed inside a user gesture; that is
handled in `emscripten/shell.html` and should not be removed.

---

## Memory Management

| Resource | Allocated | Freed |
|---|---|---|
| Scene textures / chunks / animations | the scene's `load_media` (or the framework, from `anim_specs`) | its `deinit` |
| Adventure-shared SFX and images | `adventure_load_media` | `adventure_deinit` |
| Actors | the framework, from the scene's `actor_spec` | with the scene |
| Engine UI (subtitle font, hub art, confirmation, back-to-hub button) | once at startup | `game_deinit` |
| `AnimationData` / `sprite_clips` | `make_animation_data` | `free_animation` |

Everything is loaded up front and freed at exit; there is no unload-on-leave,
which is why total asset size, not per-scene size, is the budget that matters
(#49 would change this).

---

## Terminal & Headless Backends

Both the terminal renderer and the automated test reuse one trick: before `SDL_Init`, set `SDL_VIDEODRIVER=offscreen` and create a **software** renderer, so the game runs and draws into an ordinary RGBA buffer with no display server, GPU, or sound card (audio uses the `dummy` driver). The scene / actor / game code is identical across every backend.

### Terminal backend (`make terminal`)

`main_terminal.c` + `terminal.{c,h}` render the game as coloured ASCII art with [libcaca](http://caca.zoy.org/wiki/libcaca):

- **Output:** each frame, `SDL_RenderReadPixels` copies the 800×600 frame into an RGBA buffer that `caca_dither_bitmap` dithers onto the libcaca canvas. `caca_set_display_time` caps the terminal refresh at ~10 fps while game logic runs at full speed.
- **Input:** libcaca mouse/keyboard events are translated into `SDL_MOUSEBUTTONDOWN` / `SDL_MOUSEMOTION` / `SDL_KEYDOWN` events and pushed onto SDL's queue, with character-cell coordinates scaled back into 800×600 game space — so `game_process_input` is unchanged.
- **Running:** `./vaniavolpe_terminal`; `ESC` / `q` (or Ctrl+C / Ctrl+D) quits, `d` toggles the debug overlay. Inside tmux, add `set -g mouse on` to `~/.tmux.conf`, or tmux eats the mouse events before the game sees them.

### Headless test target (`make test`)

`test/` builds `vaniavolpe_test`: the same offscreen game, no libcaca. It runs a **scripted** playthrough through a reusable harness (`test/harness.{c,h}`) and asserts the adventure ran correctly:

- **Assertions on behaviour, not pixels.** Dialogue and messages go through `SDL_Log`; the harness installs an `SDL_LogSetOutputFunction` sink, captures that stream, and checks the expected lines appear in order. A `SDL_RenderReadPixels` "frame isn't a single flat colour" check guards against a blank-screen / missing-texture regression. The binary exits non-zero on any miss.
- **Shared script.** The playthrough — click coordinates, waits, the sunscreen "brush" gesture, and the expected dialogue — lives in `test/scripts/<name>.json`, the single source of truth. `tools/gen_playtest.py` turns it into a generated C header (`build/gen/<name>_script.h`) that the matching `test/play_<name>.c` drives via `script_run` (`test/script.{c,h}`). Both shipped adventures have one; the browser runner takes any of them as its argument, and CI runs every `test/scripts/*.json`.
- **Unit tests** run alongside the playthroughs in the same binary: walk grid and pathfinding, lip-sync parsing, scene geometry, camera, tweens, the leave confirmation, and input (touch, the debug gesture, key auto-repeat).
- **Virtual time.** The harness puts `clock.{c,h}` into virtual mode and steps it a fixed amount per frame, so a scripted `wait_ms` costs nothing and the run is reproducible regardless of machine speed. Only the browser playthrough waits in real time — which is why a script's `wait_ms` budget is what that CI job costs, and not this one.
- **CI:** `.github/workflows/test.yml` builds and runs it on every push / PR, gating merges the way the web build does.

### Browser test (`test/web`)

`test/web/run_playtest.js` reads the **same** `test/scripts/<name>.json` and drives the deployed-shape web build with Puppeteer, so the native and browser playthroughs stay in lockstep. It saves a screenshot at each `screenshot` step and asserts the expected dialogue in the browser console (Emscripten routes `SDL_Log` there). `.github/workflows/web-test.yml` builds the web target, serves it, runs the script, and uploads the screenshots as an artifact.

---

## Known Limitations

Live constraints, not a backlog — queued work lives in GitHub issues (label
`backlog`).

- **`PING_PONG` playback is declared but not implemented** (`image.h`); an
  animation asking for it plays as `LOOP`.
- **Everything loads at startup**, so the whole asset set is resident for the
  whole run and the web build ships one `index.data` (#49).
- **The selection screen holds six adventures.** A seventh needs it to scroll
  (#191).
- **iOS bundles assets flat**, so the locale layering `asset_resolve` does
  everywhere else does not apply there yet; the Xcode project is also out of
  step with the current source layout (#31).
- **The subtitle overlay is the only text in the game.** There is no font
  fallback: a locale needing glyphs the bundled font lacks would render blanks.
