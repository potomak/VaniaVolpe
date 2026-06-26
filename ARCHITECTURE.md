# VaniaVolpe – Architecture Document

## Overview

**VaniaVolpe** ("Vania the Fox") is a 2D point-and-click adventure game written in C, built on SDL2. The player controls a fox character through a linear sequence of puzzle scenes, each requiring item collection and NPC interaction. The game title is "Vania Volpe - Lo Scivolo" (Italian: "The Slide").

- **Language:** C (C99)
- **Core libraries:** SDL2, SDL2_image, SDL2_mixer
- **Build system:** Xcode project (iOS/tvOS primary; macOS/Linux secondary)
- **Source files:** ~26 `.c`/`.h` files, ~2,500 lines of code
- **Assets:** 19 PNG sprites, 24 WAV audio files, 10 animation definition files

---

## Directory Layout

```
VaniaVolpe/
├── src/                       # Common engine / app shell
│   ├── main.c                 # Desktop entry point and game loop
│   ├── main_terminal.c        # Terminal (libcaca) entry point
│   ├── terminal.{c,h}         # libcaca rendering backend
│   ├── game.{c,h}             # Central controller / adventure registry
│   ├── adventure.h            # Adventure descriptor
│   ├── scene.{c,h}            # Generic scene interface
│   ├── actor.{c,h}            # Generic character engine (movement, talk, …)
│   ├── image.{c,h}            # Sprite/animation engine
│   ├── sound.{c,h}            # Audio utilities
│   ├── asset.{c,h}            # Cross-platform asset path resolution
│   ├── constants.h            # Global constants (resolution, FPS, etc.)
│   ├── debug.{c,h}            # Debug overlay rendering
│   ├── emscripten/            # Web compat-shim headers + custom shell.html
│   └── adventures/            # One self-contained adventure per directory
│       └── vania_fox_the_slide/
│           ├── vania_fox_the_slide.{c,h}   # Adventure module (scene table)
│           ├── fox.{c,h}                    # Its actor (the fox ActorSpec)
│           ├── intro.{c,h}                  # Scene: title/main menu
│           ├── playground_entrance.{c,h}    # Scene: key & gate puzzle
│           ├── playground.{c,h}             # Scene: slide repair puzzle
│           ├── outro.{c,h}                  # Scene: end screen
│           ├── example.{c,h}                # Scene: dev sandbox (unused)
│           └── assets/                      # This adventure's static assets
│               ├── fox/ intro/ outro/ music/ example/
│               ├── playground/        (└── dialog/  voice lines)
│               └── playground_entrance/ (└── dialog/  voice lines)
│
├── include/                   # Bundled SDL_image / SDL_mixer headers (native)
└── sdlexample.xcodeproj/      # Xcode project
```


---

## Game Loop

The game runs a standard fixed-timestep-style loop driven by SDL events and V-Sync:

```
main()
  init_window()           → SDL init, window (800×600), renderer, audio
  game_init()             → Initialize all scene structs
  game_load_media()       → Load all textures, animations, audio for every scene
  set_active_scene(INTRO) → Trigger on_scene_active() callback

  loop while running:
    game_process_input()  → SDL_PollEvent → route to active scene
    game_update(dt)       → Update active scene with delta-time
    game_render()         → Clear → active scene render → SDL_RenderPresent

  game_deinit()           → Free all resources
```

- **Resolution:** 800 × 600 logical pixels (resizable window with high-DPI support)
- **Frame cap:** V-Sync (`SDL_RENDERER_PRESENTVSYNC`)
- **Target FPS:** 30 (defined in `constants.h`)

---

## Module Responsibilities

### `game.c` – Central Controller

Owns the top-level game state and delegates to the active scene.

```c
enum GameScene { INTRO, PLAYGROUND_ENTRANCE, PLAYGROUND, OUTRO, EXAMPLE }

struct Game {
    bool is_running;
    bool is_debugging;
    GameScene current_scene;
}
```

Key functions: `game_init`, `game_load_media`, `set_active_scene`, `game_update`, `game_render`, `game_process_input`.

`set_active_scene` calls `on_scene_inactive` on the outgoing scene and `on_scene_active` on the incoming one, allowing each scene to reset its state and start/stop background music.

---

### `scene.h` – Scene Interface

Every scene is a `Scene` struct – a bundle of function pointers and data arrays:

```c
struct Scene {
    void (*init)(void);
    bool (*load_media)(void);
    void (*process_input)(SDL_Event *);
    void (*update)(float dt);
    void (*render)(void);
    void (*deinit)(void);
    void (*on_scene_active)(void);
    void (*on_scene_inactive)(void);

    SDL_Rect  *hotspots;   // Clickable / walkable regions
    SDL_Point *pois;       // Named waypoints (fox walk targets)
    ImageData *images;     // Background textures
    ChunkData *chunks;     // Sound effect handles
}
```

This is effectively a vtable pattern in C. Each scene file exports one `Scene` instance (e.g., `intro_scene`, `playground_scene`).

---

### `fox.c` – Player Character

The fox is the only controllable character. It moves, talks, and carries items.

#### State machine

```
IDLE ──walk_to()──> WALKING ──arrival──> IDLE
                                       └──> (callback fires)
IDLE ──fox_talk()──> TALKING ──duration──> IDLE
IDLE ──fox_sit()──>  SITTING
IDLE ──fox_wave()──> WAVING
```

#### Movement

`fox_walk_to(target, callback)`:
1. Computes a normalized direction vector from current position to target.
2. Sets state to `WALKING`, starts the walking animation and looping walk sound.
3. Determines facing direction (`SDL_FLIP_HORIZONTAL` when walking east).
4. Each frame: `position += direction × FOX_VELOCITY × dt` (200 px/s).
5. When distance to target < 2 px, stops sound, sets state to `IDLE`, fires callback.

**Known limitation:** movement is a straight line with no collision or pathfinding, so the fox can walk *through* non-walkable areas. See [`MOVEMENT.md`](MOVEMENT.md) for details and future work.

#### Inventory

```c
bool has_key;     // Obtained in playground_entrance
bool has_peg;     // Obtained from squirrel trade in playground
bool has_acorns;  // Shaken from tree in playground
```

#### Dialogue

`fox_talk(chunk, callback)`:
- Calculates WAV duration via `get_chunk_time_ms()`.
- Plays audio and talking animation simultaneously.
- After the duration elapses (checked in `fox_update`), transitions back to `IDLE` and fires callback.

---

### `image.c` – Sprite & Animation Engine

Handles sprite sheet loading, frame extraction, and playback.

#### Animation data

```c
struct AnimationData {
    ImageData image;           // SDL_Texture* + dimensions
    SDL_Rect  *sprite_clips;   // One SDL_Rect per frame
    int        frames;
    AnimationPlaybackStyle style;  // LOOP | ONE_SHOT
    int        loop_count;
    int        max_loop_count;
    bool       is_playing;
    SDL_RendererFlip flip;
    int        start_time;     // SDL_GetTicks() at play start
}
```

#### Frame timing

Fixed 12 FPS (83 ms/frame):

```c
frame_index = (elapsed_ms / 83) % num_frames;
```

#### Animation definition files

Two formats are supported:

| Format | Example | Notes |
|--------|---------|-------|
| `.anim` | `0,0,144,117\n0,117,144,117\n…` | Primary format; one `x,y,w,h` per line |
| `.json` | `[{"name":"fox-1","x":0,"y":0,"width":144,"height":117},…]` | Legacy; parsed but not actively used |

Color key for transparency: cyan `(0, 255, 255)`.

---

### `sound.c` – Audio Utilities

Thin wrapper providing `get_chunk_time_ms(Mix_Chunk*)` to calculate the playback duration of a WAV chunk from its raw byte count, frequency, and format. Used by the dialogue system to know when to end the talking animation.

---

### `asset.c` – Asset Path & Locale Resolution

Resolves an `Asset{directory, filename}` to a path, layering a **locale** over an
adventure's `assets_root`:

```c
const char *asset_path(Asset asset);    // <root>/<locale>/<dir>/<file>, falling
                                        // back to <root>/common/<dir>/<file>
void asset_set_root(const char *root);  // per-adventure assets dir
void asset_set_locale(const char *l);   // "it_IT" (default/source), "en_US", …
```

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

Toggled with the `D` key. Renders:
- Cyan rectangles over all scene hotspots.
- Magenta dots at all POIs.
- A drag-select rectangle that prints its SDL_Rect to stdout – used to define new hotspots.

---

## Scenes

### Intro

Simple title screen. Fox sits while background music plays. Two hotspot buttons: **Play** (→ `PLAYGROUND_ENTRANCE`) and **Exit** (quit).

### Playground Entrance

First puzzle: obtain a key to open the gate.

```
Click excavator  → excavator animates (distraction)
Click shovel     → fox walks to shovel → plays dig animation
                   → key revealed underground
Click key        → fox picks up key (has_key = true)
Click gate       → if has_key: gate opens → transition to PLAYGROUND
                   else: examine dialog plays (3 lines of progressive dialog)
```

State reset in `on_scene_active`: `has_key_been_revealed = false`, `examine_gate_count = 0`.

### Playground

Main puzzle: repair the slide, then use it three times to win.

```
Click slide      → if has_peg: fox fixes slide (has_slide_been_fixed = true)
                   else: examine dialog
Click tree       → acorns fall (have_acorns_fallen = true)
Click acorns     → fox picks up acorns (has_acorns = true)
Click squirrel   → if has_acorns: squirrel trades peg for acorns
                                  (has_peg = true)
Click slide (x3) → fox slides down; slides_count++
                   when slides_count == 3 → transition to OUTRO
```

Slide trajectory uses a sigmoid curve:

```c
float sigmoid(float x) {
    return SLIDE_SIGMOID_HEIGHT / (1.0f + expf(-0.025f * (x - 130.0f)));
}
```

### Outro

Fox waves. Background music plays. Click anywhere → return to `INTRO`.

### Example (dev)

Sandbox scene used during development. Not connected to the main game flow.

---

## Scene Transition Graph

```
INTRO ──Play──> PLAYGROUND_ENTRANCE ──gate+key──> PLAYGROUND ──3 slides──> OUTRO
                                                                              │
INTRO <─────────────────────────────────────────────────────────────click────┘
INTRO ──Exit──> (quit)
```

---

## Input System

```
SDL_PollEvent()
  │
  ├─ SDL_QUIT / ESC       → game.is_running = false
  ├─ SDL_KEYDOWN 'D'      → toggle debug mode
  └─ current_scene.process_input(event)
       │
       └─ SDL_MOUSEBUTTONDOWN
            ├─ Iterate hotspots: SDL_PointInRect(mouse, hotspot)
            └─ Dispatch to scene-specific action handler
```

Mouse position is mapped to logical 800×600 space with `SDL_RenderSetLogicalSize`.

---

## Audio Architecture

| Category | Format | Notes |
|----------|--------|-------|
| Background music | WAV | Looped via `Mix_PlayMusic(-1)`, volume 30 |
| Walking SFX | WAV | Looped via `Mix_PlayChannel(-1, …, -1)`, volume 20 |
| Dialog / voice | WAV | One-shot; duration drives talking animation |
| Item SFX | WAV | One-shot on dedicated channels |

Music tracks: `music/intro.wav` (intro + outro), `music/playground.wav` (main game).

---

## Memory Management

| Resource | Allocated | Freed |
|----------|-----------|-------|
| SDL textures | `load_media()` | `deinit()` |
| AnimationData / sprite_clips | `make_animation_data()` | `free_animation()` |
| Mix_Chunk | `load_media()` | `deinit()` |
| Mix_Music | global, loaded once | `game_deinit()` |

Scene resources are allocated per-scene and freed when the scene is deinitialized. The fox is a global object whose resources persist for the lifetime of the game.

---

## Known Limitations & TODOs

- No back-navigation between scenes; revisiting a scene resets its puzzle state.
- Animation frame rate (12 FPS) is global and not configurable per animation.
- `PING_PONG` animation playback style is defined but not implemented.
- One sound cue in the playground ("fix slide" event) has a pending re-recording TODO.
- Error handling is minimal: most load failures log to stderr but do not abort.
- The `.json` animation format is fully parsed but its data is not used (superseded by `.anim`).
