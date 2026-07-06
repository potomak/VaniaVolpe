//
//  asset.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/26/25.
//

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>

#include "asset.h"

// Base directory prepended to every (non-iOS) asset path, e.g. an adventure's
// assets directory. NULL means "no prefix" (paths relative to the CWD).
static const char *asset_root = NULL;

// Active locale; localized assets are looked up under this subdirectory.
static const char *asset_locale = "it_IT";

// Subdirectory holding locale-independent (shared) assets.
#define ASSET_COMMON "common"

void asset_set_root(const char *root) { asset_root = root; }

const char *asset_get_root(void) { return asset_root; }

void asset_set_locale(const char *locale) {
  if (locale != NULL) {
    asset_locale = locale;
  }
}

const char *asset_get_locale(void) { return asset_locale; }

// True if a file exists at path. Uses SDL_RWFromFile (the same mechanism the
// loaders use) rather than POSIX access(): portable (no <unistd.h>, which
// Windows lacks) and correct on Emscripten's virtual filesystem.
static bool file_exists(const char *path) {
  SDL_RWops *rw = SDL_RWFromFile(path, "rb");
  if (rw == NULL) {
    return false;
  }
  SDL_RWclose(rw);
  return true;
}

bool asset_swap_extension(const char *filename, const char *extension,
                          char *out, size_t out_size) {
  const char *dot = SDL_strrchr(filename, '.');
  if (dot == NULL || dot == filename) {
    return false;
  }
  size_t base = (size_t)(dot - filename);
  if (base + SDL_strlen(extension) + 1 > out_size) {
    return false;
  }
  SDL_memcpy(out, filename, base);
  out[base] = '\0';
  SDL_strlcat(out, extension, out_size);
  return true;
}

#if defined(__IPHONEOS__) || defined(__TVOS__)
bool asset_resolve(Asset asset, char *buf, size_t n) {
  // iOS bundles assets flat (by filename). Localizing the iOS build needs
  // folder-reference bundling so this can resolve <locale>/ then common/ like
  // the other platforms (tracked in the backlog). For now use the bare name;
  // there is no locale layer, so report the common fallback.
  snprintf(buf, n, "%s", asset.filename);
  return false;
}

bool asset_try_resolve(Asset asset, char *buf, size_t n) {
  snprintf(buf, n, "%s", asset.filename);
  return file_exists(buf);
}

bool asset_common_path(Asset asset, char *buf, size_t n) {
  // iOS bundles assets flat; there is no common/ layer to address.
  snprintf(buf, n, "%s", asset.filename);
  return true;
}
#else
// Build "[root/]<layer>/<directory>/<filename>" into buf. Returns false (and
// logs) if the path did not fit — snprintf would otherwise truncate silently,
// and buffers are tight on some targets (Windows PATH_MAX is 260).
static bool build_path(char *buf, size_t n, const char *layer, Asset asset) {
  int written;
  if (asset_root != NULL) {
    written = snprintf(buf, n, "%s/%s/%s/%s", asset_root, layer,
                       asset.directory, asset.filename);
  } else {
    written =
        snprintf(buf, n, "%s/%s/%s", layer, asset.directory, asset.filename);
  }
  if (written < 0 || (size_t)written >= n) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Asset path too long: %s/%s",
                 asset.directory, asset.filename);
    return false;
  }
  return true;
}

bool asset_try_resolve(Asset asset, char *buf, size_t n) {
  if (build_path(buf, n, asset_locale, asset) && file_exists(buf)) {
    return true;
  }
  return build_path(buf, n, ASSET_COMMON, asset) && file_exists(buf);
}

bool asset_common_path(Asset asset, char *buf, size_t n) {
  return build_path(buf, n, ASSET_COMMON, asset);
}

bool asset_resolve(Asset asset, char *buf, size_t n) {
  // The active locale's asset wins when present; otherwise fall back to the
  // shared "common" layer. Strict: no fallback to another language.
  if (build_path(buf, n, asset_locale, asset) && file_exists(buf)) {
    return true;
  }
  build_path(buf, n, ASSET_COMMON, asset);
  if (!file_exists(buf)) {
    // Neither layer has the file. Surface it here (with the resolved path)
    // instead of leaving it as a cryptic downstream "Couldn't open …".
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Asset not found in locale '%s' or common: %s/%s",
                 asset_locale, asset.directory, asset.filename);
  }
  return false;
}
#endif
