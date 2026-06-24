CC      = gcc
CFLAGS  = -std=c99 -Wall $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer) -I./include
LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer) -lm
TARGET  = vaniavolpe

CACA_CFLAGS = $(shell pkg-config --cflags caca)
CACA_LIBS   = $(shell pkg-config --libs   caca)
TARGET_TERMINAL = vaniavolpe_terminal

GAME_SRCS = \
	src/game.c \
	src/scene.c \
	src/actor.c \
	src/fox.c \
	src/vania_fox_the_slide.c \
	src/image.c \
	src/sound.c \
	src/asset.c \
	src/debug.c \
	src/intro.c \
	src/playground_entrance.c \
	src/playground.c \
	src/outro.c \
	src/example.c

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
EM_CFLAGS  = -std=c99 -Wall $(EM_PORTS) -I./src/emscripten/compat
EM_PRELOAD = --preload-file intro --preload-file fox --preload-file playground \
             --preload-file playground_entrance --preload-file outro \
             --preload-file example --preload-file music
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
