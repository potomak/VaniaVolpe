//
//  asset.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/26/25.
//

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
const char *asset_path(Asset asset) {
  // iOS bundles assets flat (by filename). Localizing the iOS build needs
  // folder-reference bundling so this can resolve <locale>/ then common/ like
  // the other platforms (tracked in BACKLOG). For now return the bare name.
  return asset.filename;
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

const char *asset_path(Asset asset) {
  static char resolved_path[1024];

  // The active locale's asset wins when present; otherwise fall back to the
  // shared "common" layer. Strict: no fallback to another language.
  build_path(resolved_path, sizeof(resolved_path), asset_locale, asset);
  if (access(resolved_path, F_OK) == 0) {
    return resolved_path;
  }
  build_path(resolved_path, sizeof(resolved_path), ASSET_COMMON, asset);
  return resolved_path;
}
#endif
