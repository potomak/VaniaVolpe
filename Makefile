CC      = gcc
# Adventure module directory (its own sources, headers and assets).
VFTS_DIR = src/adventures/vania_fox_the_slide
CFLAGS  = -std=c99 -Wall $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer) \
          -I./include -I./src -I$(VFTS_DIR)
LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer) -lm
TARGET  = vaniavolpe

CACA_CFLAGS = $(shell pkg-config --cflags caca)
CACA_LIBS   = $(shell pkg-config --libs   caca)
TARGET_TERMINAL = vaniavolpe_terminal

# Shared engine / common sources live under src/; adventure-specific sources
# (scenes, the fox actor, the adventure module) live under $(VFTS_DIR).
GAME_SRCS = \
	src/game.c \
	src/scene.c \
	src/actor.c \
	src/image.c \
	src/sound.c \
	src/asset.c \
	src/debug.c \
	$(VFTS_DIR)/fox.c \
	$(VFTS_DIR)/vania_fox_the_slide.c \
	$(VFTS_DIR)/intro.c \
	$(VFTS_DIR)/playground_entrance.c \
	$(VFTS_DIR)/playground.c \
	$(VFTS_DIR)/outro.c \
	$(VFTS_DIR)/example.c

SRCS = src/main.c $(GAME_SRCS)
OBJS = $(SRCS:.c=.o)

TERMINAL_SRCS = src/main_terminal.c src/terminal.c $(GAME_SRCS)
TERMINAL_OBJS = $(patsubst %.c,%.terminal.o,$(TERMINAL_SRCS))

# ── default target (SDL window) ───────────────────────────────────────────────

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── terminal target (libcaca, no display server needed) ───────────────────────

terminal: $(TARGET_TERMINAL)

$(TARGET_TERMINAL): $(TERMINAL_OBJS)
	$(CC) $(TERMINAL_OBJS) $(LDFLAGS) $(CACA_LIBS) -o $@

%.terminal.o: %.c
	$(CC) $(CFLAGS) $(CACA_CFLAGS) -c $< -o $@

# ── emscripten / web target (WebAssembly, runs in the browser) ───────────────

EMCC       = emcc
WEB_DIR    = build/web
WEB_TARGET = $(WEB_DIR)/index.html
EM_PORTS   = -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png"]' \
             -sUSE_SDL_MIXER=2
EM_CFLAGS  = -std=c99 -Wall $(EM_PORTS) -I./src/emscripten/compat \
             -I./src -I$(VFTS_DIR)
EM_PRELOAD = --preload-file $(VFTS_DIR)/assets/intro \
             --preload-file $(VFTS_DIR)/assets/fox \
             --preload-file $(VFTS_DIR)/assets/playground \
             --preload-file $(VFTS_DIR)/assets/playground_entrance \
             --preload-file $(VFTS_DIR)/assets/outro \
             --preload-file $(VFTS_DIR)/assets/example \
             --preload-file $(VFTS_DIR)/assets/music
EM_SHELL   = src/emscripten/shell.html
EM_LDFLAGS = $(EM_PORTS) -sALLOW_MEMORY_GROWTH=1 -lm $(EM_PRELOAD) \
             --shell-file $(EM_SHELL)

web: $(WEB_TARGET)

$(WEB_TARGET): $(SRCS) $(EM_SHELL)
	mkdir -p $(WEB_DIR)
	$(EMCC) $(EM_CFLAGS) $(SRCS) $(EM_LDFLAGS) -o $(WEB_TARGET)

# ── housekeeping ──────────────────────────────────────────────────────────────

clean:
	rm -f $(OBJS) $(TERMINAL_OBJS) $(TARGET) $(TARGET_TERMINAL)
	rm -rf build

.PHONY: all terminal web clean
