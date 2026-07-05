//
//  lipsync.c
//  Parsing and playback of the .cues / .words sidecars (see SPEECH.md).
//  Parsers are strict on purpose (the .anim parser's lesson): a bad byte
//  rejects the whole file loudly instead of half-working.
//

#include <stdlib.h>
#include <string.h>

#include "lipsync.h"

// Rhubarb's letters mapped to frames; the extended shapes G (F/V) and H (L)
// are accepted defensively and collapsed onto near equivalents, so
// regenerating cues with extended shapes on needs no code change.
static bool shape_of(char letter, Uint8 *out) {
  switch (letter) {
  case 'X':
    *out = MOUTH_X;
    return true;
  case 'A':
    *out = MOUTH_A;
    return true;
  case 'B':
    *out = MOUTH_B;
    return true;
  case 'C':
    *out = MOUTH_C;
    return true;
  case 'D':
    *out = MOUTH_D;
    return true;
  case 'E':
    *out = MOUTH_E;
    return true;
  case 'F':
    *out = MOUTH_F;
    return true;
  case 'G':
    *out = MOUTH_B;
    return true;
  case 'H':
    *out = MOUTH_C;
    return true;
  default:
    return false;
  }
}

// Split `data` into lines (tolerating \r\n and a missing final newline) and
// call visit(line, length) for each non-empty one; false aborts.
typedef bool (*LineVisitor)(const char *line, size_t length, void *user);

static bool for_each_line(const char *data, size_t size, LineVisitor visit,
                          void *user) {
  size_t start = 0;
  for (size_t i = 0; i <= size; i++) {
    if (i == size || data[i] == '\n') {
      size_t end = i;
      if (end > start && data[end - 1] == '\r') {
        end--;
      }
      if (end > start) {
        if (!visit(data + start, end - start, user)) {
          return false;
        }
      }
      start = i + 1;
    }
  }
  return true;
}

// Parse a bounded non-negative integer; advances *p past it.
static bool parse_ms(const char **p, const char *end, Uint32 *out) {
  const char *q = *p;
  if (q >= end || *q < '0' || *q > '9') {
    return false;
  }
  Uint32 value = 0;
  while (q < end && *q >= '0' && *q <= '9') {
    if (value > 100000000) { // > ~27 hours: garbage
      return false;
    }
    value = value * 10 + (Uint32)(*q - '0');
    q++;
  }
  *p = q;
  *out = value;
  return true;
}

typedef struct cue_parse {
  MouthCues *cues;
  bool ok;
} CueParse;

static bool visit_cue(const char *line, size_t length, void *user) {
  CueParse *parse = user;
  MouthCues *cues = parse->cues;
  const char *p = line;
  const char *end = line + length;

  Uint32 ms;
  Uint8 shape;
  if (!parse_ms(&p, end, &ms) || p >= end || *p != ' ' ||
      !shape_of(*(p + 1), &shape) || p + 2 != end) {
    parse->ok = false;
    return false;
  }
  if (cues->length >= LIPSYNC_MAX_CUES ||
      (cues->length > 0 && ms < cues->cues[cues->length - 1].at_ms)) {
    parse->ok = false;
    return false;
  }
  cues->cues[cues->length].at_ms = ms;
  cues->cues[cues->length].shape = shape;
  cues->length++;
  return true;
}

bool lipsync_parse(const char *data, size_t size, MouthCues *out) {
  out->cues = malloc(sizeof(MouthCue) * LIPSYNC_MAX_CUES);
  out->length = 0;
  if (out->cues == NULL) {
    return false;
  }

  CueParse parse = {out, true};
  if (!for_each_line(data, size, visit_cue, &parse) || !parse.ok ||
      out->length == 0) {
    lipsync_free(out);
    return false;
  }
  return true;
}

typedef struct word_parse {
  WordTimings *words;
  bool ok;
} WordParse;

static bool visit_word(const char *line, size_t length, void *user) {
  WordParse *parse = user;
  WordTimings *words = parse->words;
  const char *p = line;
  const char *end = line + length;

  Uint32 start_ms;
  Uint32 end_ms;
  if (!parse_ms(&p, end, &start_ms) || p >= end || *p != ' ') {
    parse->ok = false;
    return false;
  }
  p++;
  if (!parse_ms(&p, end, &end_ms) || p >= end || *p != ' ') {
    parse->ok = false;
    return false;
  }
  p++;
  size_t word_length = (size_t)(end - p);
  if (word_length == 0 || start_ms > end_ms ||
      words->length >= LIPSYNC_MAX_WORDS ||
      (words->length > 0 &&
       start_ms < words->words[words->length - 1].start_ms)) {
    parse->ok = false;
    return false;
  }

  char *word = malloc(word_length + 1);
  if (word == NULL) {
    parse->ok = false;
    return false;
  }
  memcpy(word, p, word_length);
  word[word_length] = '\0';
  words->words[words->length].start_ms = start_ms;
  words->words[words->length].end_ms = end_ms;
  words->words[words->length].word = word;
  words->length++;
  return true;
}

bool lipsync_parse_words(const char *data, size_t size, WordTimings *out) {
  out->words = malloc(sizeof(WordTiming) * LIPSYNC_MAX_WORDS);
  out->length = 0;
  if (out->words == NULL) {
    return false;
  }

  WordParse parse = {out, true};
  if (!for_each_line(data, size, visit_word, &parse) || !parse.ok ||
      out->length == 0) {
    lipsync_free_words(out);
    return false;
  }
  return true;
}

// Read a sidecar through the asset layer; NULL when the file is absent
// (which is the normal case for SFX chunks and untranslated locales).
static char *load_sidecar(Asset asset, size_t *size) {
  char path[ASSET_PATH_MAX];
  if (!asset_try_resolve(asset, path, sizeof(path))) {
    return NULL;
  }
  return SDL_LoadFile(path, size);
}

bool lipsync_load(Asset asset, MouthCues *out) {
  out->cues = NULL;
  out->length = 0;
  size_t size = 0;
  char *data = load_sidecar(asset, &size);
  if (data == NULL) {
    return false;
  }
  bool ok = lipsync_parse(data, size, out);
  SDL_free(data);
  if (!ok) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Malformed cues sidecar: %s/%s",
                 asset.directory, asset.filename);
  }
  return ok;
}

bool lipsync_load_words(Asset asset, WordTimings *out) {
  out->words = NULL;
  out->length = 0;
  size_t size = 0;
  char *data = load_sidecar(asset, &size);
  if (data == NULL) {
    return false;
  }
  bool ok = lipsync_parse_words(data, size, out);
  SDL_free(data);
  if (!ok) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Malformed words sidecar: %s/%s",
                 asset.directory, asset.filename);
  }
  return ok;
}

void lipsync_free(MouthCues *cues) {
  free(cues->cues);
  cues->cues = NULL;
  cues->length = 0;
}

void lipsync_free_words(WordTimings *words) {
  for (int i = 0; i < words->length; i++) {
    free(words->words[i].word);
  }
  free(words->words);
  words->words = NULL;
  words->length = 0;
}

MouthShape lipsync_shape_at(const MouthCues *cues, Uint32 ms, int *cursor) {
  if (cues == NULL || cues->length == 0) {
    return MOUTH_X;
  }
  int i = *cursor;
  if (i >= cues->length || cues->cues[i].at_ms > ms) {
    i = 0; // time moved backwards (new line): rewind
  }
  while (i + 1 < cues->length && cues->cues[i + 1].at_ms <= ms) {
    i++;
  }
  *cursor = i;
  if (cues->cues[i].at_ms > ms) {
    return MOUTH_X; // before the first cue
  }
  return (MouthShape)cues->cues[i].shape;
}

int lipsync_word_at(const WordTimings *words, Uint32 ms, int *cursor) {
  if (words == NULL || words->length == 0) {
    return -1;
  }
  int i = *cursor;
  if (i >= words->length || words->words[i].start_ms > ms) {
    i = 0;
  }
  while (i + 1 < words->length && words->words[i + 1].start_ms <= ms) {
    i++;
  }
  *cursor = i;
  if (words->words[i].start_ms <= ms && ms < words->words[i].end_ms) {
    return i;
  }
  return -1;
}
