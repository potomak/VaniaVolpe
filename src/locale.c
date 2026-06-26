//
//  locale.c
//

#include <stdlib.h>
#include <string.h>

#include "locale.h"

#define DEFAULT_LOCALE "it_IT"

// Locales the game ships assets for. Keep "it_IT" (the source language) first.
static const char *const SUPPORTED[] = {"it_IT", "en_US"};

// Return the supported locale that `name` selects, by full code ("en_US") or by
// language prefix ("en", "en-GB" -> "en_US"), or NULL if none matches.
static const char *match_supported(const char *name) {
  if (name == NULL || name[0] == '\0') {
    return NULL;
  }
  for (size_t i = 0; i < sizeof(SUPPORTED) / sizeof(SUPPORTED[0]); i++) {
    if (strcmp(name, SUPPORTED[i]) == 0) {
      return SUPPORTED[i];
    }
    // Two-letter language prefix, with the rest of the tag (region) ignored.
    if (strncmp(name, SUPPORTED[i], 2) == 0 &&
        (name[2] == '\0' || name[2] == '_' || name[2] == '-')) {
      return SUPPORTED[i];
    }
  }
  return NULL;
}

const char *detect_locale(int argc, char **argv) {
  const char *match;

  for (int i = 1; i < argc; i++) {
    if (strncmp(argv[i], "--locale=", 9) == 0) {
      match = match_supported(argv[i] + 9);
      if (match != NULL) {
        return match;
      }
    }
  }

  match = match_supported(getenv("VANIA_LOCALE"));
  if (match != NULL) {
    return match;
  }
  match = match_supported(getenv("LC_ALL"));
  if (match != NULL) {
    return match;
  }
  match = match_supported(getenv("LANG"));
  if (match != NULL) {
    return match;
  }

  return DEFAULT_LOCALE;
}
