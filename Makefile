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

# ── housekeeping ──────────────────────────────────────────────────────────────

clean:
	rm -f $(OBJS) $(TERMINAL_OBJS) $(TARGET) $(TARGET_TERMINAL)

.PHONY: all terminal clean
