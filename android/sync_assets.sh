#!/bin/sh
# Mirror the game's asset trees into the APK's assets/ dir, preserving their
# repo-relative paths — the same layout the web build preloads. On Android,
# SDL_RWFromFile resolves relative paths inside the APK assets, so
# asset_resolve()'s paths work unchanged. Only the shipped layers are copied
# (common/ + locales); _inbox/ and _sources/ never ship, same as the web.
# Re-run after changing assets (make android does); the destination is
# git-ignored and rebuilt from scratch every time.
set -eu

cd "$(dirname "$0")/.."

DEST=android/app/src/main/assets
rm -rf "$DEST"
mkdir -p "$DEST"

copy_tree() {
  if [ -d "$1" ]; then
    mkdir -p "$DEST/$(dirname "$1")"
    cp -R "$1" "$DEST/$1"
  fi
}

for adv in src/adventures/*/assets; do
  copy_tree "$adv/common"
  copy_tree "$adv/it_IT"
  copy_tree "$adv/en_US"
done
copy_tree assets/fonts

echo "APK assets synced to $DEST ($(find "$DEST" -type f | wc -l) files)."
