# ndk-build entry point: builds SDL2 + satellites from the release sources
# fetched by fetch_deps.sh (git-ignored, each ships its own Android.mk) and
# the game itself (src/Android.mk).
#
# The game only decodes WAV audio and PNG images, so every optional decoder
# the satellites would otherwise compile is switched off here (these must be
# set before the sub-makefiles are included; they all use ?=).
SUPPORT_FLAC_DRFLAC := false
SUPPORT_OGG_STB := false
SUPPORT_MP3_MINIMP3 := false
SUPPORT_WAVPACK := false
SUPPORT_GME := false
SUPPORT_HARFBUZZ := false

include $(call all-subdir-makefiles)
