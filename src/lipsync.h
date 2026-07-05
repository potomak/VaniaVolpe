//
//  lipsync.h
//  Mouth-shape cues and word timings for spoken lines (see SPEECH.md). Both
//  are tiny text sidecars generated offline by tools/gen_lipsync.py and
//  committed next to each dialogue WAV; the engine only parses and plays
//  them. A missing sidecar simply means "no data" — the talking animation
//  falls back to its classic loop, and the read-along highlight stays off.
//

#ifndef lipsync_h
#define lipsync_h

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "asset.h"

// Values double as frame indices into a cue-driven talking sprite sheet:
// index 0 = rest/closed, then Rhubarb's six basic shapes A-F in order.
typedef enum mouth_shape {
  MOUTH_X,
  MOUTH_A,
  MOUTH_B,
  MOUTH_C,
  MOUTH_D,
  MOUTH_E,
  MOUTH_F,
  MOUTH_SHAPE_COUNT, // 7
} MouthShape;

#define LIPSYNC_MAX_CUES 512
#define LIPSYNC_MAX_WORDS 64

typedef struct mouth_cue {
  Uint32 at_ms; // the shape holds from here until the next cue
  Uint8 shape;  // a MouthShape
} MouthCue;

typedef struct mouth_cues {
  MouthCue *cues;
  int length; // 0 = no cues (legacy talking loop)
} MouthCues;

typedef struct word_timing {
  Uint32 start_ms;
  Uint32 end_ms;
  char *word;
} WordTiming;

typedef struct word_timings {
  WordTiming *words;
  int length; // 0 = no timings (no read-along highlight)
} WordTimings;

// Strict parsers (see SPEECH.md for the formats): any malformed input logs an
// error, frees what was read, and returns false with an empty result — the
// callers treat that exactly like a missing sidecar.
bool lipsync_parse(const char *data, size_t size, MouthCues *out);
bool lipsync_parse_words(const char *data, size_t size, WordTimings *out);

// Load a sidecar resolved through the asset layer. An absent file is normal
// (returns false silently with an empty result); a malformed one logs.
bool lipsync_load(Asset asset, MouthCues *out);
bool lipsync_load_words(Asset asset, WordTimings *out);

void lipsync_free(MouthCues *cues);
void lipsync_free_words(WordTimings *words);

// Shape active at `ms` (MOUTH_X before the first cue or with no cues).
// *cursor caches the last cue index (initialise to 0); it rewinds itself if
// ms moves backwards, so lookups are O(1) amortised.
MouthShape lipsync_shape_at(const MouthCues *cues, Uint32 ms, int *cursor);

// Index of the word active at `ms`, or -1 (before/after/in a silence gap).
// *cursor caches like lipsync_shape_at.
int lipsync_word_at(const WordTimings *words, Uint32 ms, int *cursor);

#endif /* lipsync_h */
