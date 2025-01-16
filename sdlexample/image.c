//
//  image.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <stdbool.h>

#include "image.h"

// Free texture if it exists
void free_image_texture(ImageData *image) {
  if (image->texture == NULL) {
    return;
  }

  SDL_DestroyTexture(image->texture);
  image->texture = NULL;
  image->width = 0;
  image->height = 0;
}

bool load_from_file(const char *path, SDL_Renderer *renderer,
                    ImageData *image) {
  // Free texture if it exists
  free_image_texture(image);

  // Load image at specified path
  SDL_Surface *loaded_surface = IMG_Load(path);
  if (loaded_surface == NULL) {
    fprintf(stderr, "Unable to load image %s! SDL_image Error: %s\n", path,
            IMG_GetError());
    return false;
  }

  // Color key image
  SDL_SetColorKey(loaded_surface, SDL_TRUE,
                  SDL_MapRGB(loaded_surface->format, 0, 0xFF, 0xFF));

  // Create texture from surface pixels
  image->texture = SDL_CreateTextureFromSurface(renderer, loaded_surface);
  if (image->texture == NULL) {
    fprintf(stderr, "Unable to create texture from %s! SDL Error: %s\n", path,
            SDL_GetError());
    return false;
  }
  // Get image dimensions
  image->width = loaded_surface->w;
  image->height = loaded_surface->h;

  // Get rid of old loaded surface
  SDL_FreeSurface(loaded_surface);

  // Return success
  return true;
}
