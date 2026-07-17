#!/bin/sh
# Fetch the SDL family sources the Android build compiles from (ASSETS in the
# APK are handled by sync_assets.sh; see TOOLS.md -> Android build).
#
# Downloads the pinned release tarballs into app/jni/<lib>/ (each ships its
# own Android.mk that ndk-build picks up) and copies SDL's Java glue
# (org.libsdl.app.SDLActivity and friends) into the app source tree. Both
# destinations are git-ignored; run this once before the first build (make
# android does). Idempotent: an already-extracted lib is skipped.
set -eu

cd "$(dirname "$0")"

SDL2_VERSION=2.30.11
SDL2_IMAGE_VERSION=2.8.8
SDL2_MIXER_VERSION=2.8.1
SDL2_TTF_VERSION=2.22.0

JNI=app/jni
JAVA_GLUE=app/src/main/java/org/libsdl/app
mkdir -p "$JNI"

# fetch <dir-name> <tarball-name> <url>: download + extract app/jni/<dir-name>.
fetch() {
  dir="$JNI/$1"
  if [ -d "$dir" ]; then
    echo "$1: already present, skipping"
    return
  fi
  echo "$1: downloading $3"
  curl -fsSL -o "$JNI/$2" "$3"
  tar -xzf "$JNI/$2" -C "$JNI"
  mv "$JNI/${2%.tar.gz}" "$dir"
  rm "$JNI/$2"
}

fetch SDL2 "SDL2-$SDL2_VERSION.tar.gz" \
  "https://www.libsdl.org/release/SDL2-$SDL2_VERSION.tar.gz"
fetch SDL2_image "SDL2_image-$SDL2_IMAGE_VERSION.tar.gz" \
  "https://www.libsdl.org/projects/SDL_image/release/SDL2_image-$SDL2_IMAGE_VERSION.tar.gz"
fetch SDL2_mixer "SDL2_mixer-$SDL2_MIXER_VERSION.tar.gz" \
  "https://www.libsdl.org/projects/SDL_mixer/release/SDL2_mixer-$SDL2_MIXER_VERSION.tar.gz"
fetch SDL2_ttf "SDL2_ttf-$SDL2_TTF_VERSION.tar.gz" \
  "https://www.libsdl.org/projects/SDL_ttf/release/SDL2_ttf-$SDL2_TTF_VERSION.tar.gz"

# SDL's Java side: the SDLActivity classes every SDL2 Android app embeds.
if [ ! -d "$JAVA_GLUE" ]; then
  mkdir -p "$JAVA_GLUE"
  cp "$JNI"/SDL2/android-project/app/src/main/java/org/libsdl/app/*.java \
     "$JAVA_GLUE/"
  echo "SDL Java glue copied to $JAVA_GLUE"
fi

echo "Android dependencies ready."
