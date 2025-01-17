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

AnimationData *make_animation_data(int frames, AnimationPlaybackStyle style) {
  AnimationData *animation = malloc(sizeof(AnimationData));
  SDL_Rect *sprite_clips = (SDL_Rect *)malloc(sizeof(SDL_Rect) * frames);
  animation->current_frame = 0;
  animation->frames = frames;
  animation->is_playing = false;
  animation->style = style;
  animation->loop_count = 0;
  animation->sprite_clips = sprite_clips;
  animation->image = (ImageData){NULL, 0, 0};
  // Default multiplier used to slow down animations
  animation->speed_multiplier = 1. / 4;
  animation->flip = SDL_FLIP_NONE;
  return animation;
}

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

void free_animation(AnimationData *animation) {
  free_image_texture(&animation->image);
  free(animation->sprite_clips);
  free(animation);
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

void render_animation(SDL_Renderer *renderer, AnimationData *animation,
                      SDL_Point point) {
  // Get current frame clip
  int clip_index = animation->current_frame * animation->speed_multiplier;
  SDL_Rect *clip = &animation->sprite_clips[clip_index];

  // Set rendering space and render to screen
  SDL_Rect render_quad = {point.x, point.y, clip->w, clip->h};

  // Render to screen
  SDL_RenderCopyEx(renderer, animation->image.texture, clip, &render_quad, 0,
                   NULL, animation->flip);

  if (animation->is_playing) {
    // Go to next frame
    ++animation->current_frame;
  }

  // Cycle animation
  if (animation->current_frame * animation->speed_multiplier >=
      animation->frames) {
    animation->current_frame = 0;
    animation->loop_count++;

    if (animation->style == ONE_SHOT) {
      animation->is_playing = false;
    }
  }
}

void render_image(SDL_Renderer *renderer, ImageData *image, SDL_Point point) {
  // Set rendering space and render to screen
  SDL_Rect render_quad = {point.x, point.y, image->width, image->height};

  // Render to screen
  SDL_RenderCopy(renderer, image->texture, NULL, &render_quad);
}
