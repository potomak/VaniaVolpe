CC      = gcc
CFLAGS  = -std=c99 -Wall $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer) -I./include
LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer) -lm
TARGET  = vaniavolpe

CACA_CFLAGS = $(shell pkg-config --cflags caca)
CACA_LIBS   = $(shell pkg-config --libs   caca)
TARGET_TERMINAL = vaniavolpe_terminal

GAME_SRCS = \
	sdlexample/game.c \
	sdlexample/scene.c \
	sdlexample/fox.c \
	sdlexample/image.c \
	sdlexample/sound.c \
	sdlexample/asset.c \
	sdlexample/debug.c \
	sdlexample/intro.c \
	sdlexample/playground_entrance.c \
	sdlexample/playground.c \
	sdlexample/outro.c \
	sdlexample/example.c

SRCS = sdlexample/main.c $(GAME_SRCS)
OBJS = $(SRCS:.c=.o)

TERMINAL_SRCS = sdlexample/main_terminal.c sdlexample/terminal.c $(GAME_SRCS)
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
EM_CFLAGS  = -std=c99 -Wall $(EM_PORTS) -I./sdlexample/emscripten/compat
EM_PRELOAD = --preload-file intro --preload-file fox --preload-file playground \
             --preload-file playground_entrance --preload-file outro \
             --preload-file example --preload-file music
EM_SHELL   = sdlexample/emscripten/shell.html
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
