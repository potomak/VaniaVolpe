//
//  test_lipsync.c
//  Unit tests for the lip-sync sidecar parsers and lookups (SPEECH.md
//  Phase 2). The parsers are strict: any malformed input must reject the
//  whole file, so a bad sidecar degrades to the legacy talking loop instead
//  of half-working.
//

#include <stdio.h>
#include <string.h>

#include "lipsync.h"

#include "test_lipsync.h"

static int failures;

static void check(bool ok, const char *what) {
  if (ok) {
    fprintf(stderr, "OK    %s\n", what);
  } else {
    fprintf(stderr, "MISS  %s\n", what);
    failures++;
  }
}

static bool parse_cues(const char *text, MouthCues *out) {
  return lipsync_parse(text, strlen(text), out);
}

static bool parse_words(const char *text, WordTimings *out) {
  return lipsync_parse_words(text, strlen(text), out);
}

static void test_cue_parser(void) {
  MouthCues cues;

  check(parse_cues("0 X\n230 B\n340 D\n910 X\n", &cues) && cues.length == 4 &&
            cues.cues[1].at_ms == 230 && cues.cues[1].shape == MOUTH_B,
        "valid cues file parses");
  lipsync_free(&cues);

  check(parse_cues("0 X\r\n230 B\r\n340 D", &cues) && cues.length == 3,
        "CRLF and a missing final newline are tolerated");
  lipsync_free(&cues);

  check(parse_cues("0 G\n10 H\n", &cues) && cues.cues[0].shape == MOUTH_B &&
            cues.cues[1].shape == MOUTH_C,
        "extended shapes G/H collapse onto B/C");
  lipsync_free(&cues);

  check(!parse_cues("0 X\n230 B\n100 D\n", &cues) && cues.length == 0,
        "out-of-order times reject the file");
  check(!parse_cues("0 X\n230 Q\n", &cues), "an unknown letter rejects");
  check(!parse_cues("0 X\n230\n", &cues), "a missing shape rejects");
  check(!parse_cues("abc X\n", &cues), "a non-numeric time rejects");
  check(!parse_cues("0 X extra\n", &cues), "trailing junk rejects");
  check(!parse_cues("", &cues), "an empty file rejects");

  // Over the cue cap: LIPSYNC_MAX_CUES + 1 monotonically increasing lines.
  static char big[LIPSYNC_MAX_CUES * 16 + 64];
  size_t at = 0;
  for (int i = 0; i <= LIPSYNC_MAX_CUES; i++) {
    at += (size_t)snprintf(big + at, sizeof(big) - at, "%d A\n", i * 10);
  }
  check(!lipsync_parse(big, at, &cues), "more than the cue cap rejects");
}

static void test_shape_at(void) {
  MouthCues cues;
  parse_cues("100 A\n200 B\n300 X\n", &cues);
  int cursor = 0;

  check(lipsync_shape_at(&cues, 0, &cursor) == MOUTH_X,
        "before the first cue the mouth rests");
  check(lipsync_shape_at(&cues, 100, &cursor) == MOUTH_A &&
            lipsync_shape_at(&cues, 199, &cursor) == MOUTH_A &&
            lipsync_shape_at(&cues, 200, &cursor) == MOUTH_B &&
            lipsync_shape_at(&cues, 5000, &cursor) == MOUTH_X,
        "shapes hold from their cue until the next");
  check(lipsync_shape_at(&cues, 150, &cursor) == MOUTH_A,
        "the cursor rewinds when time moves backwards");
  check(lipsync_shape_at(NULL, 0, &cursor) == MOUTH_X &&
            lipsync_shape_at(&(MouthCues){NULL, 0}, 0, &cursor) == MOUTH_X,
        "no cues means a resting mouth");
  lipsync_free(&cues);
}

static void test_word_parser(void) {
  WordTimings words;

  check(parse_words("0 819 Ehhhh!\n819 1287 Che\n1287 2691 divertente!\n",
                    &words) &&
            words.length == 3 && words.words[1].start_ms == 819 &&
            strcmp(words.words[2].word, "divertente!") == 0,
        "valid words file parses");
  lipsync_free_words(&words);

  check(parse_words("0 100 ciào\n", &words) &&
            strcmp(words.words[0].word, "ci\xc3\xa0o") == 0,
        "UTF-8 words survive parsing");
  lipsync_free_words(&words);

  check(!parse_words("200 100 backwards\n", &words), "start after end rejects");
  check(!parse_words("0 100 a\n50 60\n", &words), "a missing word rejects");
  check(!parse_words("100 200 b\n0 50 a\n", &words),
        "out-of-order starts reject");
  check(!parse_words("", &words), "an empty words file rejects");
}

static void test_word_at(void) {
  WordTimings words;
  // A silence gap between 300 and 400.
  parse_words("100 300 primo\n400 600 secondo\n", &words);
  int cursor = 0;

  check(lipsync_word_at(&words, 50, &cursor) == -1 &&
            lipsync_word_at(&words, 100, &cursor) == 0 &&
            lipsync_word_at(&words, 350, &cursor) == -1 &&
            lipsync_word_at(&words, 400, &cursor) == 1 &&
            lipsync_word_at(&words, 700, &cursor) == -1,
        "word lookup tracks words and stays quiet in gaps");
  check(lipsync_word_at(&words, 150, &cursor) == 0,
        "the word cursor rewinds when time moves backwards");
  lipsync_free_words(&words);
}

int test_lipsync(void) {
  failures = 0;
  fprintf(stderr, "\n-- lipsync unit tests --\n");
  test_cue_parser();
  test_shape_at();
  test_word_parser();
  test_word_at();
  return failures;
}
