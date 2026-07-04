//
//  scene.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/22/25.
//

#include "scene.h"

bool load_scene_images(Scene scene, SDL_Renderer *renderer) {
  for (int i = 0; i < scene.images_length; i++) {
    if (!load_image(renderer, &scene.images[i])) {
      return false;
    }
  }

  return true;
}

bool load_scene_chunks(Scene scene) {
  for (int i = 0; i < scene.chunks_length; i++) {
    char path[ASSET_PATH_MAX];
    asset_resolve(
        (Asset){
            .filename = scene.chunks[i].filename,
            .directory = scene.chunks[i].directory,
        },
        path, sizeof(path));
    scene.chunks[i].chunk = Mix_LoadWAV(path);
    if (scene.chunks[i].chunk == NULL) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load %s: %s", path,
                   Mix_GetError());
      return false;
    }
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

void free_scene_images(Scene scene) {
  for (int i = 0; i < scene.images_length; i++) {
    free_image_texture(&scene.images[i]);
  }
}

void free_scene_chunks(Scene scene) {
  for (int i = 0; i < scene.chunks_length; i++) {
    Mix_FreeChunk(scene.chunks[i].chunk);
  }
}

void free_scene_animations(Scene scene) {
  for (int i = 0; i < scene.animations_length; i++) {
    if (scene.animations[i] != NULL) {
      free_animation(scene.animations[i]);
    }
  }
}
