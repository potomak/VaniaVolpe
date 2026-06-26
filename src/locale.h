//
//  locale.h
//  Pick the active UI/content locale at startup.
//

#ifndef locale_h
#define locale_h

// Choose the active locale, in order of precedence:
//   1. a `--locale=xx_XX` command-line argument (also how the web build passes
//      the browser language, via Module.arguments),
//   2. $VANIA_LOCALE,
//   3. $LC_ALL / $LANG (e.g. "en_US.UTF-8"),
//   4. the default ("it_IT").
// The result is always one of the supported locales; anything unrecognized maps
// to the default. Matching is by full code ("en_US") or language prefix ("en").
const char *detect_locale(int argc, char **argv);

#endif /* locale_h */
