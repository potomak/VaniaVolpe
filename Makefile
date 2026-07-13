CC      = gcc
# Adventure module directories (each has its own sources, headers and assets).
VFTS_DIR = src/adventures/vania_fox_the_slide
GINA_DIR = src/adventures/gina_hen_at_the_pool
DEMO_DIR = src/adventures/depth_demo
CFLAGS  = -std=c99 -Wall $(shell pkg-config --cflags sdl2 SDL2_image SDL2_mixer SDL2_ttf) \
          -I./include -I./src -I$(VFTS_DIR) -I$(GINA_DIR) -I$(DEMO_DIR)
LDFLAGS = $(shell pkg-config --libs sdl2 SDL2_image SDL2_mixer SDL2_ttf) -lm
TARGET  = vaniavolpe

CACA_CFLAGS = $(shell pkg-config --cflags caca)
CACA_LIBS   = $(shell pkg-config --libs   caca)
TARGET_TERMINAL = vaniavolpe_terminal

# Shared engine / common sources live under src/; adventure-specific sources
# (scenes, the fox actor, the adventure module) live under $(VFTS_DIR).
GAME_SRCS = \
	src/game.c \
	src/adventure.c \
	src/hub.c \
	src/scene.c \
	src/walk.c \
	src/camera.c \
	src/actor.c \
	src/image.c \
	src/sound.c \
	src/lipsync.c \
	src/subtitle.c \
	src/asset.c \
	src/locale.c \
	src/debug.c \
	$(VFTS_DIR)/fox.c \
	$(VFTS_DIR)/vania_fox_the_slide.c \
	$(VFTS_DIR)/intro.c \
	$(VFTS_DIR)/playground_entrance.c \
	$(VFTS_DIR)/playground.c \
	$(VFTS_DIR)/outro.c \
	$(GINA_DIR)/hen.c \
	$(GINA_DIR)/gina_state.c \
	$(GINA_DIR)/gina_hen_at_the_pool.c \
	$(GINA_DIR)/pool.c \
	$(GINA_DIR)/tree.c \
	$(GINA_DIR)/vine.c \
	$(GINA_DIR)/sunscreen_minigame.c \
	$(GINA_DIR)/grapes_minigame.c \
	$(DEMO_DIR)/depth_demo.c \
	$(DEMO_DIR)/field.c

SRCS = src/main.c $(GAME_SRCS)
OBJS = $(SRCS:.c=.o)

TERMINAL_SRCS = src/main_terminal.c src/terminal.c $(GAME_SRCS)
TERMINAL_OBJS = $(patsubst %.c,%.terminal.o,$(TERMINAL_SRCS))

TARGET_TEST = vaniavolpe_test
# Headless smoke test: the game plus a scripted-playthrough harness (test/). No
# libcaca — it renders offscreen and reads pixels back instead of drawing to a
# terminal. The .test.o suffix keeps its objects separate from the other builds.
TEST_SRCS = test/main_test.c test/harness.c test/script.c test/play_gina.c \
            test/test_walk.c test/test_lipsync.c test/test_scene.c \
            test/test_camera.c \
            $(GAME_SRCS)
TEST_OBJS = $(patsubst %.c,%.test.o,$(TEST_SRCS))

# Playthrough scripts are the single source of truth shared with the browser
# test (test/web); the native test consumes a generated header of each one.
GEN_DIR = build/gen
GINA_SCRIPT_H = $(GEN_DIR)/gina_script.h

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

# ── headless test target (scripted playthrough, no display server) ────────────

test: $(TARGET_TEST)

$(TARGET_TEST): $(TEST_OBJS)
	$(CC) $(TEST_OBJS) $(LDFLAGS) -o $@

# Generate the C step table from the shared JSON script.
$(GINA_SCRIPT_H): test/scripts/gina.json tools/gen_playtest.py
	mkdir -p $(GEN_DIR)
	python3 tools/gen_playtest.py --name gina --in $< --out $@

# The Gina play-test #includes the generated header.
test/play_gina.test.o: $(GINA_SCRIPT_H)

%.test.o: %.c
	$(CC) $(CFLAGS) -Itest -I$(GEN_DIR) -c $< -o $@

# Build and run the smoke test (offscreen video + dummy audio are set by the
# binary itself). Exits non-zero if the playthrough regresses.
run-test: $(TARGET_TEST)
	./$(TARGET_TEST)

# ── emscripten / web target (WebAssembly, runs in the browser) ───────────────

EMCC       = emcc
WEB_DIR    = build/web
WEB_TARGET = $(WEB_DIR)/index.html
# Per-build id stamped into the page to cache-bust the fixed-name sub-resources
# (index.js/.wasm/.data) on each deploy. The commit SHA changes whenever the
# build does; fall back to a timestamp outside a git checkout.
WEB_CACHE_BUST := $(shell git rev-parse --short HEAD 2>/dev/null || date +%s)
EM_PORTS   = -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png"]' \
             -sUSE_SDL_MIXER=2 -sUSE_SDL_TTF=2
EM_CFLAGS  = -std=c99 -Wall $(EM_PORTS) -I./src/emscripten/compat \
             -I./src -I$(VFTS_DIR) -I$(GINA_DIR) -I$(DEMO_DIR)
# Preload each adventure's shared (common) layer plus every locale. Per-locale
# web bundles (download only the chosen language) are a future optimisation.
EM_PRELOAD = --preload-file $(VFTS_DIR)/assets/common \
             --preload-file $(VFTS_DIR)/assets/it_IT \
             --preload-file $(VFTS_DIR)/assets/en_US \
             --preload-file $(GINA_DIR)/assets/common \
             --preload-file $(GINA_DIR)/assets/it_IT \
             --preload-file $(GINA_DIR)/assets/en_US \
             --preload-file $(DEMO_DIR)/assets/common \
             --preload-file assets/fonts
EM_SHELL   = src/emscripten/shell.html
# -sGROWABLE_ARRAYBUFFERS=0: Emscripten >= 6 defaults this to auto-detect, and
# when the browser has WebAssembly.Memory.toResizableBuffer the heap becomes a
# resizable ArrayBuffer — WebKit/Safari then rejects heap-backed typed-array
# views in WebGL/WebAudio calls ("TypeError: Resizable ArrayBuffer is not
# allowed"), a black screen at boot. Chromium accepts them, so CI's browser
# test can't catch it. -sMIN_SAFARI_VERSION pins the compatibility floor
# (Safari/iOS 14.1) explicitly instead of drifting with emcc defaults, which
# had silently raised it to Safari 15 (ES2020 output).
EM_COMPAT  = -sGROWABLE_ARRAYBUFFERS=0 -sMIN_SAFARI_VERSION=140100
EM_LDFLAGS = $(EM_PORTS) -sALLOW_MEMORY_GROWTH=1 $(EM_COMPAT) -lm \
             $(EM_PRELOAD) --shell-file $(EM_SHELL)
# Every file packed into index.data; listing them makes the bundle rebuild when
# art/audio changes (the sources alone wouldn't trigger it).
WEB_ASSETS = $(shell find $(VFTS_DIR)/assets $(GINA_DIR)/assets -type f)

web: $(WEB_TARGET)

$(WEB_TARGET): $(SRCS) $(EM_SHELL) $(WEB_ASSETS) \
               src/emscripten/catalog.html tools/gen_asset_catalog.py \
               src/emscripten/asset_tasks.html tools/gen_asset_tasks.py \
               src/emscripten/cost_estimate.html \
               $(wildcard src/adventures/*/assets/tasks.json)
	mkdir -p $(WEB_DIR)
	$(EMCC) $(EM_CFLAGS) $(SRCS) $(EM_LDFLAGS) -o $(WEB_TARGET)
	# Stamp the per-build id: replace the shell's __CACHE_BUST__ placeholder (used
	# by Module.locateFile for the .wasm/.data fetches) and version the index.js
	# script tag. GitHub Pages serves these under stable names, so without this a
	# redeploy can pair a cached old file with a fresh sibling -> failed load /
	# black screen. Stamping keeps every fetch from one deploy consistent.
	sed -i 's|__CACHE_BUST__|$(WEB_CACHE_BUST)|g; s|src="index.js"|src="index.js?v=$(WEB_CACHE_BUST)"|g' $(WEB_TARGET)
	# Asset catalog reference page (build/web/catalog.html): copy the raw assets
	# into the published site (the game bundles them into the opaque index.data,
	# which the catalog can't read) and emit catalog.json describing them.
	python3 tools/gen_asset_catalog.py --out $(WEB_DIR)
	cp src/emscripten/catalog.html $(WEB_DIR)/catalog.html
	# Asset to-do page (build/web/asset_tasks.html): emit the task data with live
	# status; the page renders it. Writes only JSON, never the source tree.
	python3 tools/gen_asset_tasks.py --out $(WEB_DIR)
	cp src/emscripten/asset_tasks.html $(WEB_DIR)/asset_tasks.html
	# Dev-tools index + browser walk-mask editor + art cost estimate (see TOOLS.md).
	cp src/emscripten/tools.html $(WEB_DIR)/tools.html
	cp src/emscripten/walk_editor.html $(WEB_DIR)/walk_editor.html
	cp src/emscripten/cost_estimate.html $(WEB_DIR)/cost_estimate.html

# ── formatting (clang-format, LLVM style; see .clang-format) ──────────────────

CLANG_FORMAT ?= clang-format
# All first-party C sources/headers; the bundled SDL shims under
# src/emscripten/compat/ and include/ are intentionally left alone.
FORMAT_SRCS = $(shell find src test \( -name '*.c' -o -name '*.h' \) \
                -not -path 'src/emscripten/compat/*')

# Rewrite sources in place to match .clang-format.
format:
	$(CLANG_FORMAT) -i $(FORMAT_SRCS)

# Fail if anything is not formatted (what CI mirrors); changes nothing.
format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_SRCS)

# ── housekeeping ──────────────────────────────────────────────────────────────

clean:
	rm -f $(OBJS) $(TERMINAL_OBJS) $(TEST_OBJS) \
	      $(TARGET) $(TARGET_TERMINAL) $(TARGET_TEST)
	rm -rf build

.PHONY: all terminal test run-test web clean format format-check
