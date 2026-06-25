//
//  asset.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/26/25.
//

#include <SDL2/SDL.h>

#include "asset.h"

// Base directory prepended to every (non-iOS) asset path, e.g. an adventure's
// assets directory. NULL means "no prefix" (paths relative to the CWD).
static const char *asset_root = NULL;

void asset_set_root(const char *root) { asset_root = root; }

const char *asset_path(Asset asset) {
#if defined(__IPHONEOS__) || defined(__TVOS__)
  // On iOS, return the asset name directly (no directories)
  return asset.filename;
#else
  // Else build the asset path as [root/]directory/filename
  static char resolved_path[1024];
  if (asset_root != NULL) {
    snprintf(resolved_path, sizeof(resolved_path), "%s/%s/%s", asset_root,
             asset.directory, asset.filename);
  } else {
    snprintf(resolved_path, sizeof(resolved_path), "%s/%s", asset.directory,
             asset.filename);
  }
  return resolved_path;
#endif
}
