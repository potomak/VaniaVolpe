//
//  asset.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/26/25.
//

#ifndef asset_h
#define asset_h

#include <stdbool.h>
#include <stddef.h>

// Buffer size for a resolved asset path. Roomy enough for an adventure root
// plus the <locale>/<dir>/<file> layers.
#define ASSET_PATH_MAX 1024

typedef struct asset {
  const char *filename;
  const char *directory;
} Asset;

// Resolve an asset into the caller's buffer. Localized assets live under
// <root>/<locale>/ and shared assets under <root>/common/; the localized layer
// wins when present, else the common one (see asset_set_locale). Returns true
// if the localized layer was used, false if it fell back to common. The caller
// owns the storage, so two resolved paths can be live at once without
// clobbering each other. Use a buffer of ASSET_PATH_MAX bytes.
bool asset_resolve(Asset asset, char *buf, size_t n);

// Like asset_resolve, but for optional files (e.g. dialogue sidecars): true
// if the file exists in the locale or common layer (resolved path in buf),
// false — silently, no error log — when absent in both.
bool asset_try_resolve(Asset asset, char *buf, size_t n);

// Set the base directory prepended to non-iOS asset paths (NULL to disable).
// The pointer is stored, not copied, so the string must outlive the asset
// system (true today: adventure roots are string literals).
void asset_set_root(const char *root);

// The active asset root (as set by asset_set_root; may be NULL).
const char *asset_get_root(void);

// Build the shared-layer path "<root>/common/<dir>/<file>" without checking
// existence — for writers (e.g. the walk-mask save) that create the file.
// False if the path doesn't fit.
bool asset_common_path(Asset asset, char *buf, size_t n);

// Set the active locale (e.g. "it_IT", "en_US"). Defaults to "it_IT". Lookups
// try <root>/<locale>/<dir>/<file> first, then <root>/common/<dir>/<file>;
// there is no cross-language fallback, so each locale must provide all of its
// assets. The pointer is stored, not copied, so the string must outlive the
// asset system (true today: detect_locale() returns statics / string literals).
void asset_set_locale(const char *locale);

// The active locale.
const char *asset_get_locale(void);

// Derive a sibling filename by swapping the extension (the part after the
// last '.'): "line.wav" + ".cues" -> "line.cues". False if filename has no
// extension or the result doesn't fit. Used for dialogue sidecars, which sit
// next to their WAV with the same base name.
bool asset_swap_extension(const char *filename, const char *extension,
                          char *out, size_t out_size);

#endif /* asset_h */
