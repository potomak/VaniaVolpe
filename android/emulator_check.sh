#!/usr/bin/env bash
#
# Install the debug APK on a running emulator, launch it, and check it is
# still alive a few seconds later.
#
# That sounds thin and is not. The app exits its loop the moment anything
# essential fails: main.c gives up if SDL, the window, the renderer, the image
# or audio subsystems fail, and again if game_load_media() cannot load every
# texture and sound of every adventure. SDL_main returning ends the activity,
# so the process dies. "Still running" therefore means SDL came up, the window
# and renderer exist, every asset in the APK was found and loaded, and the game
# loop is turning.
#
# It also catches the failure that prompted this: a manifest naming an activity
# class that does not exist launches nothing at all.
#
# Run by .github/workflows/android.yml inside reactivecircus/android-emulator-runner,
# which has already booted a device and put adb on PATH.

set -euo pipefail

APK=android/app/build/outputs/apk/debug/app-debug.apk
PKG=it.curlybrackets.tinyadventures
ACTIVITY=org.libsdl.app.SDLActivity
# Generous: a cold start on an emulator unpacks the asset bundle and loads
# every texture up front (there is no lazy loading yet — see #49).
SETTLE_SECONDS=20

# On failure: the tail inline, where you are already looking, and the whole log
# as a file the workflow uploads as an artifact.
LOGCAT=/tmp/logcat.txt

fail() {
  echo "FAIL: $*"
  adb logcat -d > "$LOGCAT" 2>/dev/null || true
  echo "--- logcat (last 200 lines; full log uploaded as an artifact) ---"
  tail -200 "$LOGCAT" || true
  exit 1
}

adb logcat -c
echo "installing $APK"
adb install -r "$APK"

echo "launching $PKG/$ACTIVITY"
adb shell am start -n "$PKG/$ACTIVITY"

echo "waiting ${SETTLE_SECONDS}s for it to settle"
sleep "$SETTLE_SECONDS"

# 1) Did it die? The most informative check, for the reasons above.
if [ -z "$(adb shell pidof "$PKG" | tr -d '\r')" ]; then
  fail "$PKG is not running ${SETTLE_SECONDS}s after launch"
fi

# 2) A Java-side crash can leave a zygote process behind, so check the log too.
if adb logcat -d | grep -q "FATAL EXCEPTION"; then
  fail "a fatal exception was logged"
fi

# 3) The game's own failure paths log through SDL_Log, which SDL routes to
# logcat. Catch them by name so the report says what broke rather than just
# "it died".
if adb logcat -d | grep -qE "Failed to load media!|Failed to initialize window!|Unable to load image|Malformed animation data"; then
  fail "the game logged an asset or startup error"
fi

echo "OK: $PKG survived ${SETTLE_SECONDS}s with no fatal error"
