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

static void (*on_animation_playback_end)(void);

AnimationData *make_animation_data(int frames, AnimationPlaybackStyle style) {
  AnimationData *animation = malloc(sizeof(AnimationData));
  SDL_Rect *sprite_clips = (SDL_Rect *)malloc(sizeof(SDL_Rect) * frames);
  animation->start_time = 0;
  animation->frames = frames;
  animation->is_playing = false;
  animation->style = style;
  animation->loop_count = 0;
  animation->max_loop_count = 0;
  animation->sprite_clips = sprite_clips;
  animation->image = (ImageData){NULL, 0, 0};
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
bool load_image(SDL_Renderer *renderer, ImageData *image) {
  // Free texture if it exists
  free_image_texture(image);

  // Load image at specified path
  const char *image_path = asset_path((Asset){
      .filename = image->filename,
      .directory = image->directory,
  });
  SDL_Surface *loaded_surface = IMG_Load(image_path);
  if (loaded_surface == NULL) {
    fprintf(stderr, "Unable to load image %s! SDL_image Error: %s\n",
            image_path, IMG_GetError());
    return false;
  }

  // Color key image
  SDL_SetColorKey(loaded_surface, SDL_TRUE,
                  SDL_MapRGB(loaded_surface->format, 0, 0xFF, 0xFF));

  // Create texture from surface pixels
  image->texture = SDL_CreateTextureFromSurface(renderer, loaded_surface);
  if (image->texture == NULL) {
    fprintf(stderr, "Unable to create texture from %s! SDL Error: %s\n",
            image_path, SDL_GetError());
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
static bool load_animation_data(AnimationData *animation, const char *path) {
  size_t size;
  char *data = SDL_LoadFile(path, &size);
  if (data == NULL) {
    fprintf(stderr, "Failed to load data!\n");
    return false;
  }

  // Support coordinates up to 99999
  char num[6], c;
  int rect[4], row = 0, num_i = 0, rect_i = 0;
  for (int i = 0; i < size; i++) {
    c = data[i];
    if (c == ',' || c == '\n') {
      num[num_i] = '\0';
      rect[rect_i++] = atoi(num);
      num_i = 0;
      if (c == '\n') {
        animation->sprite_clips[row++] = (SDL_Rect){
            .x = rect[0],
            .y = rect[1],
            .w = rect[2],
            .h = rect[3],
        };
        rect_i = 0;
      }
    } else {
      num[num_i++] = c;
    }
  }

  SDL_free(data);
  return true;
}

bool load_animation(SDL_Renderer *renderer, AnimationData *animation,
                    Asset sprite_asset, Asset data_asset) {
  animation->image.filename = sprite_asset.filename;
  animation->image.directory = sprite_asset.directory;
  if (!load_image(renderer, &animation->image)) {
    fprintf(stderr, "Failed to load walking animation texture!\n");
    return false;
  }

  if (!load_animation_data(animation, asset_path(data_asset))) {
    return false;
  }

  return true;
}

void play_animation(AnimationData *animation, void (*on_end)(void)) {
  if (animation->is_playing) {
    return;
  }

  animation->loop_count = 0;
  animation->is_playing = true;
  animation->start_time = SDL_GetTicks();
  on_animation_playback_end = on_end;
}

void stop_animation(AnimationData *animation) {
  if (!animation->is_playing) {
    return;
  }

  animation->is_playing = false;
  animation->start_time = 0;
  if (on_animation_playback_end != NULL) {
    on_animation_playback_end();
  }
}

void render_animation(SDL_Renderer *renderer, AnimationData *animation,
                      SDL_Point point) {
  // Get current frame clip
  int delta = 0;
  int ms_per_frame = 83; // at 12 FPS: 1 / 12 * 1000
  int clip_index = 0;
  if (animation->is_playing) {
    delta = SDL_GetTicks() - animation->start_time;
    clip_index = (delta / ms_per_frame) % animation->frames;
  }
  SDL_Rect *clip = &animation->sprite_clips[clip_index];

  // Set rendering space and render to screen
  SDL_Rect render_quad = {point.x, point.y, clip->w, clip->h};

  // Render to screen
  SDL_RenderCopyEx(renderer, animation->image.texture, clip, &render_quad, 0,
                   NULL, animation->flip);

  // Cycle animation
  if (animation->is_playing) {
    animation->loop_count = delta / ms_per_frame / animation->frames;

    if (animation->style == ONE_SHOT &&
        animation->loop_count > animation->max_loop_count) {
      stop_animation(animation);
    }
  }
}

void render_image(SDL_Renderer *renderer, const ImageData *image,
                  SDL_Point point) {
  // Set rendering space and render to screen
  SDL_Rect render_quad = {point.x, point.y, image->width, image->height};

  // Render to screen
  SDL_RenderCopy(renderer, image->texture, NULL, &render_quad);
}
