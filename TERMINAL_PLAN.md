# Terminal Rendering Backend — Implementation Plan

## Goal

Add a `terminal` build target that runs the game fully inside a terminal
(over SSH, inside tmux, or on a raw TTY) with no display server required.
Rendering is handled by **libcaca** (pixel → coloured ASCII art).
Input (mouse clicks + keyboard) is read from the terminal via libcaca's own
event system and injected back into SDL's event queue, so **all game logic
and scene code is completely unchanged**.

---

## New files

```
src/terminal.h                 — public API for the terminal backend
src/terminal.c                 — libcaca rendering + input translation
src/main_terminal.c            — entry point for the terminal target
                                 (replaces main.c; uses offscreen SDL driver)
include/                        — existing Linux header shims (unchanged)
```

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  main_terminal.c  (entry point)                                 │
│                                                                 │
│  init:   SDL_setenv("SDL_VIDEODRIVER", "offscreen")             │
│          SDL_Init + software renderer                           │
│          terminal_init(renderer)    ← sets up libcaca           │
│                                                                 │
│  loop:   terminal_poll_events()     ← caca → SDL event queue    │
│          SDL_PollEvent / game_process_input()  (unchanged)      │
│          game_update(dt)            (unchanged)                 │
│          SDL clear + game_render(renderer)     (unchanged)      │
│          terminal_present(renderer) ← readpixels → caca draw   │
└─────────────────────────────────────────────────────────────────┘
         │                              │
         ▼                              ▼
   SDL event queue              libcaca display
   (SDL_PushEvent)              (stdout → terminal)
         │
         ▼
   game_process_input()  ← hotspot detection, scene logic
   (unchanged)
```

---

## terminal.h / terminal.c

### Public API

```c
bool terminal_init(SDL_Renderer *renderer);   // call once after SDL init
void terminal_present(SDL_Renderer *renderer); // call instead of SDL_RenderPresent
void terminal_poll_events(void);               // call before SDL_PollEvent each frame
void terminal_deinit(void);                    // call on exit
```

### terminal_init

- Create a libcaca canvas (`caca_create_canvas(0, 0)` — auto terminal size).
- Create a libcaca display (`caca_create_display(cv)`).
- Enable mouse reporting: `caca_set_mouse(dp, 1)`.
- Hide cursor: `caca_set_cursor(dp, 0)`.
- Set display title.
- Create the dither object for 32-bit RGBA pixels at WINDOW_WIDTH × WINDOW_HEIGHT.
  Pixel format masks for little-endian (ARM64 / x86-64):
  R=0x000000FF  G=0x0000FF00  B=0x00FF0000  A=0xFF000000
- Set display time to 100 ms (`caca_set_display_time`) → caps terminal refresh at ~10 fps,
  avoiding unnecessary CPU usage (game logic still runs faster).
- Allocate a pixel read-back buffer: `WINDOW_WIDTH × WINDOW_HEIGHT × 4` bytes.

### terminal_present

```
SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_RGBA32, pixels, pitch)
caca_dither_bitmap(cv, 0, 0, canvas_w, canvas_h, dither, pixels)
caca_refresh_display(dp)          // blocks up to display_time ms
```

### terminal_poll_events

Non-blocking poll (`caca_get_event` with timeout=0), drain all pending events:

| libcaca event               | SDL event injected             | notes                              |
|-----------------------------|--------------------------------|------------------------------------|
| CACA_EVENT_MOUSE_PRESS      | SDL_MOUSEBUTTONDOWN            | coords scaled to game space        |
| CACA_EVENT_MOUSE_RELEASE    | SDL_MOUSEBUTTONUP              | coords scaled to game space        |
| CACA_EVENT_MOUSE_MOTION     | SDL_MOUSEMOTION                | coords scaled to game space        |
| CACA_EVENT_KEY_PRESS (ESC)  | SDL_KEYDOWN SDLK_ESCAPE        |                                    |
| CACA_EVENT_KEY_PRESS ('d')  | SDL_KEYDOWN SDLK_d             | debug overlay toggle               |
| CACA_EVENT_RESIZE           | (internal) update canvas size  | remeasure canvas w/h for scaling   |
| CACA_EVENT_QUIT             | SDL_QUIT                       |                                    |

Coordinate scaling (character cells → game pixels):
```c
game_x = (caca_x * WINDOW_WIDTH)  / caca_get_canvas_width(cv)
game_y = (caca_y * WINDOW_HEIGHT) / caca_get_canvas_height(cv)
```

---

## main_terminal.c

Mirrors `main.c` with these differences:

| | main.c | main_terminal.c |
|---|---|---|
| SDL video driver | default | `SDL_VIDEODRIVER=offscreen` (set before SDL_Init) |
| SDL renderer | accelerated + vsync | software |
| Audio init failure | fatal | warning only (continue without audio) |
| `render()` | `SDL_RenderPresent` | `terminal_present(renderer)` |
| `process_input()` | SDL_PollEvent only | `terminal_poll_events()` first, then SDL_PollEvent |
| Extra init | — | `terminal_init(renderer)` |
| Extra cleanup | — | `terminal_deinit()` |

---

## Makefile changes

Add alongside the existing `all` target:

```makefile
CACA_CFLAGS = $(shell pkg-config --cflags caca)
CACA_LIBS   = $(shell pkg-config --libs   caca)

TERMINAL_SRCS = <all SRCS except main.c> + main_terminal.c + terminal.c
TERMINAL_OBJS = $(patsubst %.c, %.terminal.o, $(TERMINAL_SRCS))

%.terminal.o: %.c
    $(CC) $(CFLAGS) $(CACA_CFLAGS) -c $< -o $@

terminal: $(TERMINAL_OBJS)
    $(CC) $(TERMINAL_OBJS) $(LDFLAGS) $(CACA_LIBS) -o vaniavolpe_terminal
```

The `.terminal.o` suffix keeps terminal and normal build objects separate so
both targets can coexist without a `make clean` between them.

---

## tmux setup (one-time)

Add to `~/.tmux.conf`:
```
set -g mouse on
```
Then `tmux source ~/.tmux.conf` (or restart tmux).
Without this, tmux consumes mouse events for its own UI.

---

## Running

```bash
make terminal
./vaniavolpe_terminal
```

Press `ESC` or `q` to quit. Press `d` to toggle the debug hotspot overlay.
Click anywhere the fox can walk to move him; click hotspots to interact.

---

## Headless test target — implemented

Because the game runs with no display server in this mode, it can be driven in
CI. The `test` target (`make test` → `vaniavolpe_test`, `src/main_test.c`) builds
the game with an offscreen software renderer and the `dummy` audio driver, pushes
a scripted sequence of mouse events onto SDL's queue, and plays Gina's whole
adventure — the native analog of the browser `scratchpad/gina_smoke.js`.

Rather than hashing pixels (brittle across SDL versions / CPUs), it asserts on
**behaviour**: it captures the dialogue the game prints and checks the expected
lines appear in order, plus a cheap `SDL_RenderReadPixels` "frame isn't a single
flat colour" check to catch a blank-screen regression. It exits non-zero on any
miss, and runs on every push/PR via `.github/workflows/test.yml`.

A faster, fully deterministic variant (fixed-step clock instead of real
wall-clock time) and optional golden-pixel snapshots are noted in the tracking
issue as possible future refinements.
