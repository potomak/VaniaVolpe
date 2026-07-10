//
//  subtitle.c
//  Dialogue text overlay with the read-along word highlight (see subtitle.h
//  and SPEECH.md Part 3). Layout happens once per line in subtitle_show: the
//  text is split on spaces, each word is rendered twice (white, and near-black
//  for the highlighted state) into small cached textures, wrapped greedily and
//  centred bottom-anchored. Per frame, subtitle_render just picks the active
//  word from the line's timings and blits.
//

#include <SDL2/SDL.h>
#include <SDL2_ttf/SDL_ttf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "lipsync.h"

#include "subtitle.h"

#define SUBTITLE_FONT_PATH "assets/fonts/AtkinsonHyperlegible-Regular.ttf"
#define SUBTITLE_FONT_SIZE 28
// Wrap width and margins, in screen px.
#define SUBTITLE_MAX_WIDTH 700
#define SUBTITLE_BOTTOM_MARGIN 24
#define SUBTITLE_PADDING 12
#define SUBTITLE_HIGHLIGHT_PADDING 4
// The line lingers briefly after the voice ends, like real subtitles.
#define SUBTITLE_GRACE_MS 300

static const SDL_Color WHITE = {0xFF, 0xFF, 0xFF, 0xFF};
static const SDL_Color NEAR_BLACK = {0x20, 0x20, 0x20, 0xFF};

typedef struct sub_word {
  SDL_Texture *white; // the word as normally drawn
  SDL_Texture *dark;  // the word as drawn over the highlight
  SDL_Rect box;       // screen-space position and size
} SubWord;

static bool initialized = false;
static bool enabled = true;
static TTF_Font *font = NULL;
static SDL_Renderer *sub_renderer = NULL;

// The active line.
static SubWord line_words[LIPSYNC_MAX_WORDS];
static int line_words_length = 0;
static SDL_Rect block; // backing box around the whole line
// Word timings borrowed from the line's ChunkData (chunks outlive the line);
// NULL = no highlight for this line.
static const WordTimings *line_timings = NULL;
static int word_cursor = 0;
static Uint32 shown_at = 0;
static Uint32 lingers_until = 0;
static bool active = false;

bool subtitles_enabled(void) { return enabled; }

// --subtitles=0 / $VANIA_SUBTITLES=0 disable; anything else keeps the
// default (on). Mirrors detect_locale's plumbing.
static void detect_setting(int argc, char **argv) {
  const char *value = NULL;
  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "--subtitles=", 12) == 0) {
      value = argv[i] + 12;
    }
  }
  if (value == NULL) {
    value = getenv("VANIA_SUBTITLES");
  }
  if (value != NULL && strcmp(value, "0") == 0) {
    enabled = false;
  }
}

bool subtitle_init(int argc, char **argv, SDL_Renderer *renderer) {
  detect_setting(argc, argv);

  if (TTF_Init() != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_Init failed: %s",
                 TTF_GetError());
    enabled = false;
    return false;
  }
  font = TTF_OpenFont(SUBTITLE_FONT_PATH, SUBTITLE_FONT_SIZE);
  if (font == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load %s: %s",
                 SUBTITLE_FONT_PATH, TTF_GetError());
    enabled = false;
    return false;
  }
  sub_renderer = renderer;
  initialized = true;
  return true;
}

static void free_line(void) {
  for (int i = 0; i < line_words_length; i++) {
    if (line_words[i].white != NULL) {
      SDL_DestroyTexture(line_words[i].white);
    }
    if (line_words[i].dark != NULL) {
      SDL_DestroyTexture(line_words[i].dark);
    }
    line_words[i] = (SubWord){NULL, NULL, {0, 0, 0, 0}};
  }
  line_words_length = 0;
  line_timings = NULL;
  active = false;
}

void subtitle_clear(void) { free_line(); }

// Render one word in `color` into a texture; false on failure.
static bool render_word(const char *word, SDL_Color color, SDL_Texture **out,
                        int *w, int *h) {
  SDL_Surface *surface = TTF_RenderUTF8_Blended(font, word, color);
  if (surface == NULL) {
    return false;
  }
  *out = SDL_CreateTextureFromSurface(sub_renderer, surface);
  *w = surface->w;
  *h = surface->h;
  SDL_FreeSurface(surface);
  return *out != NULL;
}

void subtitle_show(const char *text, const WordTimings *words,
                   Uint32 duration_ms, bool force) {
  if (!initialized || text == NULL || (!enabled && !force)) {
    return;
  }
  free_line();

  int space_w = 0;
  TTF_SizeUTF8(font, " ", &space_w, NULL);
  int line_skip = TTF_FontLineSkip(font);

  // Split on spaces and render each word twice (normal + highlighted),
  // assigning wrap rows as we go. box.x is the offset within its row for
  // now; rows are centred below once their widths are known.
  int rows[LIPSYNC_MAX_WORDS]; // row index per word
  int row_width[LIPSYNC_MAX_WORDS] = {0};
  int row = 0;
  int x = 0;
  const char *at = text;
  while (*at != '\0' && line_words_length < LIPSYNC_MAX_WORDS) {
    while (*at == ' ') {
      at++;
    }
    if (*at == '\0') {
      break;
    }
    const char *end = at;
    while (*end != '\0' && *end != ' ') {
      end++;
    }
    char word[128];
    size_t length = (size_t)(end - at);
    if (length >= sizeof(word)) {
      length = sizeof(word) - 1;
    }
    memcpy(word, at, length);
    word[length] = '\0';
    at = end;

    SubWord *sub = &line_words[line_words_length];
    int w = 0;
    int h = 0;
    if (!render_word(word, WHITE, &sub->white, &w, &h) ||
        !render_word(word, NEAR_BLACK, &sub->dark, &w, &h)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Subtitle: failed to render '%s': %s", word, TTF_GetError());
      free_line();
      return;
    }
    if (x > 0 && x + w > SUBTITLE_MAX_WIDTH) {
      row++;
      x = 0;
    }
    sub->box = (SDL_Rect){x, row, w, h}; // (offset in row, row) for now
    rows[line_words_length] = row;
    x += w + space_w;
    row_width[row] = sub->box.x + w;
    line_words_length++;
  }
  if (line_words_length == 0) {
    return;
  }

  // Centre each row; stack the block bottom-anchored.
  int row_count = row + 1;
  int block_h = row_count * line_skip;
  int block_top = WINDOW_HEIGHT - SUBTITLE_BOTTOM_MARGIN - block_h;
  int block_w = 0;
  for (int r = 0; r < row_count; r++) {
    if (row_width[r] > block_w) {
      block_w = row_width[r];
    }
  }
  for (int i = 0; i < line_words_length; i++) {
    int r = rows[i];
    line_words[i].box.x += (WINDOW_WIDTH - row_width[r]) / 2;
    line_words[i].box.y = block_top + r * line_skip;
  }
  block =
      (SDL_Rect){(WINDOW_WIDTH - block_w) / 2 - SUBTITLE_PADDING,
                 block_top - SUBTITLE_PADDING, block_w + 2 * SUBTITLE_PADDING,
                 block_h + 2 * SUBTITLE_PADDING};

  // The highlight pairs timing entries with the split words by index; a
  // count mismatch (stale sidecar) disables just the highlight.
  line_timings = NULL;
  if (words != NULL && words->length > 0) {
    if (words->length == line_words_length) {
      line_timings = words;
    } else {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                  "Subtitle: %d timed words for %d displayed; highlight off",
                  words->length, line_words_length);
    }
  }

  word_cursor = 0;
  shown_at = SDL_GetTicks();
  lingers_until = shown_at + duration_ms + SUBTITLE_GRACE_MS;
  active = true;
}

void subtitle_render(SDL_Renderer *renderer) {
  if (!active) {
    return;
  }
  Uint32 now = SDL_GetTicks();
  if (now >= lingers_until) {
    active = false; // textures are freed by the next show/clear/deinit
    return;
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xA0);
  SDL_RenderFillRect(renderer, &block);

  // The word being spoken right now, per the line's timings.
  int highlighted = -1;
  if (line_timings != NULL) {
    highlighted = lipsync_word_at(line_timings, now - shown_at, &word_cursor);
  }
  if (highlighted >= 0 && highlighted < line_words_length) {
    SDL_Rect box = line_words[highlighted].box;
    box.x -= SUBTITLE_HIGHLIGHT_PADDING;
    box.y -= SUBTITLE_HIGHLIGHT_PADDING;
    box.w += 2 * SUBTITLE_HIGHLIGHT_PADDING;
    box.h += 2 * SUBTITLE_HIGHLIGHT_PADDING;
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xCC, 0x33, 0xE6);
    SDL_RenderFillRect(renderer, &box);
  }
  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

  for (int i = 0; i < line_words_length; i++) {
    SDL_Texture *texture =
        i == highlighted ? line_words[i].dark : line_words[i].white;
    SDL_RenderCopy(renderer, texture, NULL, &line_words[i].box);
  }
}

void subtitle_deinit(void) {
  free_line();
  if (font != NULL) {
    TTF_CloseFont(font);
    font = NULL;
  }
  if (initialized) {
    TTF_Quit();
    initialized = false;
  }
  sub_renderer = NULL;
}
