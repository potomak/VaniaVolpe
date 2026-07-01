//
//  asset.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/26/25.
//

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#include "asset.h"

// Base directory prepended to every (non-iOS) asset path, e.g. an adventure's
// assets directory. NULL means "no prefix" (paths relative to the CWD).
static const char *asset_root = NULL;

// Active locale; localized assets are looked up under this subdirectory.
static const char *asset_locale = "it_IT";

// Subdirectory holding locale-independent (shared) assets.
#define ASSET_COMMON "common"

void asset_set_root(const char *root) { asset_root = root; }

void asset_set_locale(const char *locale) {
  if (locale != NULL) {
    asset_locale = locale;
  }
}

const char *asset_get_locale(void) { return asset_locale; }

#if defined(__IPHONEOS__) || defined(__TVOS__)
bool asset_resolve(Asset asset, char *buf, size_t n) {
  // iOS bundles assets flat (by filename). Localizing the iOS build needs
  // folder-reference bundling so this can resolve <locale>/ then common/ like
  // the other platforms (tracked in the backlog). For now use the bare name;
  // there is no locale layer, so report the common fallback.
  snprintf(buf, n, "%s", asset.filename);
  return false;
}
#else
// Build "[root/]<layer>/<directory>/<filename>" into buf.
static void build_path(char *buf, size_t n, const char *layer, Asset asset) {
  if (asset_root != NULL) {
    snprintf(buf, n, "%s/%s/%s/%s", asset_root, layer, asset.directory,
             asset.filename);
  } else {
    snprintf(buf, n, "%s/%s/%s", layer, asset.directory, asset.filename);
  }
}

bool asset_resolve(Asset asset, char *buf, size_t n) {
  // The active locale's asset wins when present; otherwise fall back to the
  // shared "common" layer. Strict: no fallback to another language.
  build_path(buf, n, asset_locale, asset);
  if (access(buf, F_OK) == 0) {
    return true;
  }
  build_path(buf, n, ASSET_COMMON, asset);
  return false;
}
#endif
