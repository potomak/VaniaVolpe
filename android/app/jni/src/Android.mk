# The game as an Android shared library ("main": the name SDLActivity loads
# and whose SDL_main it calls). Sources are the same engine + adventure files
# the desktop build compiles (Makefile GAME_SRCS + src/main.c); the terminal
# renderer stays out. The <SDL2/...> include style resolves through the same
# compat shims the Emscripten build uses.
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main

# Repo root, four levels up from android/app/jni/src.
REPO := $(LOCAL_PATH)/../../../..

LOCAL_C_INCLUDES := \
  $(REPO)/src \
  $(REPO)/src/emscripten/compat \
  $(REPO)/src/adventures/vania_fox_the_slide \
  $(REPO)/src/adventures/gina_hen_at_the_pool \
  $(REPO)/src/adventures/depth_demo \
  $(REPO)/build/gen

LOCAL_SRC_FILES := \
  $(filter-out $(REPO)/src/main_terminal.c $(REPO)/src/terminal.c, \
    $(wildcard $(REPO)/src/*.c)) \
  $(wildcard $(REPO)/src/adventures/*/*.c)

LOCAL_CFLAGS := -std=c99 -Wall

LOCAL_SHARED_LIBRARIES := SDL2 SDL2_image SDL2_mixer SDL2_ttf

LOCAL_LDLIBS := -llog

include $(BUILD_SHARED_LIBRARY)
