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
    scene.chunks[i].chunk = Mix_LoadWAV(scene.chunks[i].path);
    if (scene.chunks[i].chunk == NULL) {
      fprintf(stderr, "Failed to load %s! SDL_mixer Error: %s\n",
              scene.chunks[i].path, Mix_GetError());
      return false;
    }
  }

  return true;
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
