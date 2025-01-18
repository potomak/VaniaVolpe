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
  animation->max_loop_count = 0;
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

// Free animation created with make_animation_data
void free_animation(AnimationData *animation) {
  free_image_texture(&animation->image);
  free(animation->sprite_clips);
  free(animation);
}

// Load image from file and create texture in image
bool load_image(SDL_Renderer *renderer, ImageData *image, const char *path) {
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

// Load animation sprite clips
//
// Data format:
// * One sprite clip per row
// * Rows are delimited by '\n'
// * Sprite clip components are delimited by ','
bool load_animation_data(AnimationData *animation, const char *path) {
  size_t size;
  char *data = SDL_LoadFile(path, &size);
  if (data == NULL) {
    fprintf(stderr, "Failed to load data!\n");
    return false;
  }

  // Support coordinates up to 99999
  char num[6];
  int rect[4], row = 0, num_i = 0, rect_i = 0;
  for (int i = 0; i < size; i++) {
    if (data[i] == ',') {
      num[num_i] = '\0';
      rect[rect_i++] = atoi(num);
      num_i = 0;
    } else if (data[i] == '\n') {
      num[num_i] = '\0';
      rect[rect_i] = atoi(num);
      animation->sprite_clips[row] =
          (SDL_Rect){.x = rect[0], .y = rect[1], .w = rect[2], .h = rect[3]};
      rect_i = 0;
      num_i = 0;
      row++;
    } else {
      num[num_i++] = data[i];
    }
  }

  SDL_free(data);

  return true;
}

bool load_animation(SDL_Renderer *renderer, AnimationData *animation,
                    const char *sprite_path, const char *data_path) {
  if (!load_image(renderer, &animation->image, sprite_path)) {
    fprintf(stderr, "Failed to load walking animation texture!\n");
    return false;
  }

  if (!load_animation_data(animation, data_path)) {
    return false;
  }

  return true;
}

void play_animation(AnimationData *animation) {
  animation->loop_count = 0;
  animation->is_playing = true;
}

void stop_animation(AnimationData *animation) {
  animation->is_playing = false;
  animation->current_frame = 0;
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

    if (animation->style == ONE_SHOT &&
        animation->loop_count > animation->max_loop_count) {
      stop_animation(animation);
    }
  }
}

void render_image(SDL_Renderer *renderer, ImageData *image, SDL_Point point) {
  // Set rendering space and render to screen
  SDL_Rect render_quad = {point.x, point.y, image->width, image->height};

  // Render to screen
  SDL_RenderCopy(renderer, image->texture, NULL, &render_quad);
}
