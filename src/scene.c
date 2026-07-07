//
//  scene.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/22/25.
//

#include "scene.h"

int action_layer_order(const Prop *props, int props_length,
                       Actor *const *actors, int actors_length,
                       int out_order[ACTION_LAYER_MAX]) {
  int keys[ACTION_LAYER_MAX];
  int count = 0;
  for (int i = 0; i < props_length; i++) {
    if (!props[i].visible) {
      continue;
    }
    SDL_assert(count < ACTION_LAYER_MAX);
    if (count >= ACTION_LAYER_MAX) {
      break;
    }
    keys[count] = props[i].baseline;
    out_order[count++] = i;
  }
  for (int i = 0; i < actors_length; i++) {
    SDL_assert(count < ACTION_LAYER_MAX);
    if (count >= ACTION_LAYER_MAX) {
      break;
    }
    keys[count] = (int)actor_feet_y(actors[i]);
    out_order[count++] = props_length + i;
  }

  // Insertion sort, ascending. Stable, so equal keys keep insertion order:
  // props (inserted first) draw before actors, and props keep their table
  // order among themselves.
  for (int i = 1; i < count; i++) {
    int key = keys[i];
    int order = out_order[i];
    int j = i - 1;
    while (j >= 0 && keys[j] > key) {
      keys[j + 1] = keys[j];
      out_order[j + 1] = out_order[j];
      j--;
    }
    keys[j + 1] = key;
    out_order[j + 1] = order;
  }
  return count;
}

void render_action_layer(SDL_Renderer *renderer, Prop *props, int props_length,
                         Actor **actors, int actors_length) {
  int order[ACTION_LAYER_MAX];
  int count =
      action_layer_order(props, props_length, actors, actors_length, order);
  for (int i = 0; i < count; i++) {
    if (order[i] < props_length) {
      Prop *prop = &props[order[i]];
      if (prop->animation != NULL) {
        render_animation(renderer, prop->animation, prop->pos);
      } else {
        render_image(renderer, prop->image, prop->pos);
      }
    } else {
      actor_render(actors[order[i] - props_length], renderer);
    }
  }
}

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

// Load the optional dialogue sidecars next to a chunk's WAV (see SPEECH.md).
// Missing files are the normal case (SFX chunks have none).
static void load_chunk_sidecars(ChunkData *chunk) {
  chunk->text = NULL;
  chunk->cues = (MouthCues){NULL, 0};
  chunk->words = (WordTimings){NULL, 0};

  char filename[ASSET_PATH_MAX];
  if (asset_swap_extension(chunk->filename, ".txt", filename,
                           sizeof(filename))) {
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
  if (asset_swap_extension(chunk->filename, ".cues", filename,
                           sizeof(filename))) {
    lipsync_load((Asset){.filename = filename, .directory = chunk->directory},
                 &chunk->cues);
  }
  if (asset_swap_extension(chunk->filename, ".words", filename,
                           sizeof(filename))) {
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
