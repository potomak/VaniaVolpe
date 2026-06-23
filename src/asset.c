//
//  asset.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/26/25.
//

#include <SDL2/SDL.h>

#include "asset.h"

const char *asset_path(Asset asset) {
#if defined(__IPHONEOS__) || defined(__TVOS__)
  // On iOS, return the asset name directly (no directories)
  return asset.filename;
#else
  // Else (macOS) build the asset path as directory + "/" + filename
  static char resolved_path[1024];
  snprintf(resolved_path, sizeof(resolved_path), "%s/%s", asset.directory,
           asset.filename);
  return resolved_path;
#endif
}
