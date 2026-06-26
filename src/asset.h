//
//  asset.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/26/25.
//

#ifndef asset_h
#define asset_h

typedef struct asset {
  const char *filename;
  const char *directory;
} Asset;

// Resolve an asset to a path. Localized assets live under <root>/<locale>/ and
// shared assets under <root>/common/; asset_path returns the localized one when
// it exists, else the common one (see asset_set_locale).
const char *asset_path(Asset asset);

// Set the base directory prepended to non-iOS asset paths (NULL to disable).
void asset_set_root(const char *root);

// Set the active locale (e.g. "it_IT", "en_US"). Defaults to "it_IT". Lookups
// try <root>/<locale>/<dir>/<file> first, then <root>/common/<dir>/<file>;
// there is no cross-language fallback, so each locale must provide all of its
// assets.
void asset_set_locale(const char *locale);

// The active locale.
const char *asset_get_locale(void);

#endif /* asset_h */
