//
//  scene.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/22/25.
//

#include "scene.h"

bool load_scene_images(Scene *scene, SDL_Renderer *renderer) {
  for (int i = 0; i < scene->images_length; i++) {
    if (!load_image(renderer, &scene->images[i])) {
      // Unwind: free the images that did load before this failure.
      for (int j = 0; j < i; j++) {
        free_image_texture(&scene->images[j]);
      }
      return false;
    }
  }

  return true;
}

// Derive a sidecar filename from a chunk's "<name>.wav" (e.g. ".cues" for
// suffix); false if the chunk filename has no .wav suffix or doesn't fit.
static bool sidecar_filename(const char *wav_filename, const char *suffix,
                             char *out, size_t out_size) {
  size_t length = SDL_strlen(wav_filename);
  if (length < 4 || SDL_strcmp(wav_filename + length - 4, ".wav") != 0) {
    return false;
  }
  size_t base = length - 4;
  if (base + SDL_strlen(suffix) + 1 > out_size) {
    return false;
  }
  SDL_memcpy(out, wav_filename, base);
  out[base] = '\0';
  SDL_strlcat(out, suffix, out_size);
  return true;
}

// Load the optional dialogue sidecars next to a chunk's WAV (see SPEECH.md).
// Missing files are the normal case (SFX chunks have none).
static void load_chunk_sidecars(ChunkData *chunk) {
  chunk->text = NULL;
  chunk->cues = (MouthCues){NULL, 0};
  chunk->words = (WordTimings){NULL, 0};

  char filename[ASSET_PATH_MAX];
  if (sidecar_filename(chunk->filename, ".txt", filename, sizeof(filename))) {
    char path[ASSET_PATH_MAX];
    if (asset_try_resolve(
            (Asset){.filename = filename, .directory = chunk->directory}, path,
            sizeof(path))) {
      size_t size = 0;
      char *data = SDL_LoadFile(path, &size);
      if (data != NULL) {
        // Trim to the first line, without the newline.
        size_t end = 0;
        while (end < size && data[end] != '\n' && data[end] != '\r') {
          end++;
        }
        data[end] = '\0';
        chunk->text = SDL_strdup(data);
        SDL_free(data);
      }
    }
  }
  if (sidecar_filename(chunk->filename, ".cues", filename, sizeof(filename))) {
    lipsync_load((Asset){.filename = filename, .directory = chunk->directory},
                 &chunk->cues);
  }
  if (sidecar_filename(chunk->filename, ".words", filename, sizeof(filename))) {
    lipsync_load_words(
        (Asset){.filename = filename, .directory = chunk->directory},
        &chunk->words);
  }
}

// Free one chunk's WAV and dialogue sidecars, leaving it safe to free again.
static void free_chunk_data(ChunkData *chunk) {
  if (chunk->chunk != NULL) {
    Mix_FreeChunk(chunk->chunk);
    chunk->chunk = NULL;
  }
  SDL_free(chunk->text);
  chunk->text = NULL;
  lipsync_free(&chunk->cues);
  lipsync_free_words(&chunk->words);
}

bool load_scene_chunks(Scene *scene) {
  for (int i = 0; i < scene->chunks_length; i++) {
    char path[ASSET_PATH_MAX];
    asset_resolve(
        (Asset){
            .filename = scene->chunks[i].filename,
            .directory = scene->chunks[i].directory,
        },
        path, sizeof(path));
    scene->chunks[i].chunk = Mix_LoadWAV(path);
    if (scene->chunks[i].chunk == NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load %s: %s", path,
                   Mix_GetError());
      // Unwind: free the chunks (and sidecars) that did load before this
      // failure.
      for (int j = 0; j < i; j++) {
        free_chunk_data(&scene->chunks[j]);
      }
      return false;
    }
    load_chunk_sidecars(&scene->chunks[i]);
  }

  return true;
}

void update_scene_animations(Scene scene, int now_ms) {
  for (int i = 0; i < scene.animations_length; i++) {
    if (scene.animations[i] != NULL) {
      animation_update(scene.animations[i], now_ms);
    }
  }
}

void free_scene_images(Scene *scene) {
  for (int i = 0; i < scene->images_length; i++) {
    free_image_texture(&scene->images[i]);
  }
}

void free_scene_chunks(Scene *scene) {
  for (int i = 0; i < scene->chunks_length; i++) {
    free_chunk_data(&scene->chunks[i]);
  }
}

void free_scene_animations(Scene *scene) {
  for (int i = 0; i < scene->animations_length; i++) {
    if (scene->animations[i] != NULL) {
      free_animation(scene->animations[i]);
    }
  }
}
