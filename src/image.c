//
//  image.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#include <SDL2/SDL.h>
#include <SDL2_image/SDL_image.h>
#include <stdbool.h>

#include "clock.h"
#include "image.h"

AnimationData *make_animation_data(int frames, AnimationPlaybackStyle style) {
  AnimationData *animation = malloc(sizeof(AnimationData));
  if (animation == NULL) {
    return NULL;
  }
  SDL_Rect *sprite_clips = (SDL_Rect *)malloc(sizeof(SDL_Rect) * frames);
  if (sprite_clips == NULL) {
    free(animation);
    return NULL;
  }
  animation->start_time = 0;
  animation->frames = frames;
  animation->is_playing = false;
  animation->style = style;
  animation->loop_count = 0;
  animation->max_loop_count = 0;
  animation->current_frame = 0;
  animation->ms_per_frame = DEFAULT_MS_PER_FRAME;
  animation->sprite_clips = sprite_clips;
  animation->image = (ImageData){NULL, 0, 0};
  animation->flip = SDL_FLIP_NONE;
  animation->on_end = NULL;
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

// Dilate sprite colour into fully transparent texels ("alpha bleed").
//
// PNG exporters store transparent pixels as (0,0,0,0). SDL blends with straight
// (non-premultiplied) alpha and filtering averages RGB and alpha independently,
// so a sample straddling the sprite edge mixes in that black and darkens it —
// invisible at 1:1, a halo the moment anything is drawn scaled (SCALING.md).
// Copying the neighbouring colour outward fixes the interpolated colour and
// leaves alpha untouched, so nothing about the unscaled look changes.
//
// Bilinear sampling reads a 2x2 neighbourhood, so one ring is enough; a second
// pass is nearly free at load time and covers heavier minification.
#define ALPHA_BLEED_PASSES 2

static void bleed_alpha_edges(SDL_Surface *surface) {
  const int w = surface->w;
  const int h = surface->h;
  const int count = w * h;
  // SDL_PIXELFORMAT_RGBA32 is byte-order R,G,B,A on every endianness, so the
  // channels can be indexed directly.
  Uint8 *pixels = (Uint8 *)surface->pixels;
  const int pitch = surface->pitch;

  Uint8 *has_colour = SDL_calloc((size_t)count, 1);
  if (has_colour == NULL) {
    return; // bleeding is an enhancement; a failed alloc just skips it
  }
  int transparent = 0;
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      bool opaque = pixels[y * pitch + x * 4 + 3] != 0;
      has_colour[y * w + x] = opaque;
      transparent += !opaque;
    }
  }
  // Backgrounds and other fully opaque art need no work.
  if (transparent == 0) {
    SDL_free(has_colour);
    return;
  }

  Uint8 *next = SDL_malloc((size_t)count);
  if (next == NULL) {
    SDL_free(has_colour);
    return;
  }
  for (int pass = 0; pass < ALPHA_BLEED_PASSES; pass++) {
    SDL_memcpy(next, has_colour, (size_t)count);
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        if (has_colour[y * w + x]) {
          continue;
        }
        // Average the neighbours that already carry colour, so a texel in a
        // concave corner doesn't inherit one arbitrary side.
        int r = 0;
        int g = 0;
        int b = 0;
        int n = 0;
        for (int dy = -1; dy <= 1; dy++) {
          for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx < 0 || nx >= w || ny < 0 || ny >= h) {
              continue;
            }
            if (!has_colour[ny * w + nx]) {
              continue;
            }
            Uint8 *p = &pixels[ny * pitch + nx * 4];
            r += p[0];
            g += p[1];
            b += p[2];
            n++;
          }
        }
        if (n == 0) {
          continue;
        }
        Uint8 *p = &pixels[y * pitch + x * 4];
        p[0] = (Uint8)(r / n);
        p[1] = (Uint8)(g / n);
        p[2] = (Uint8)(b / n);
        // p[3] (alpha) deliberately stays 0 — only the colour is dilated.
        next[y * w + x] = 1;
      }
    }
    SDL_memcpy(has_colour, next, (size_t)count);
  }
  SDL_free(next);
  SDL_free(has_colour);
}

// Load image from file and create texture in image
bool load_image(SDL_Renderer *renderer, ImageData *image) {
  // Free texture if it exists
  free_image_texture(image);

  // Load image at specified path
  char image_path[ASSET_PATH_MAX];
  asset_resolve(
      (Asset){
          .filename = image->filename,
          .directory = image->directory,
      },
      image_path, sizeof(image_path));
  SDL_Surface *loaded_surface = IMG_Load(image_path);
  if (loaded_surface == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to load image %s: %s",
                 image_path, IMG_GetError());
    return false;
  }

  // Work in a known channel order so the bleed pass can index bytes directly.
  // Conversion failing is not fatal: the unconverted surface still makes a
  // correct texture, it just doesn't get bled.
  SDL_Surface *rgba =
      SDL_ConvertSurfaceFormat(loaded_surface, SDL_PIXELFORMAT_RGBA32, 0);
  if (rgba != NULL) {
    bleed_alpha_edges(rgba);
    SDL_FreeSurface(loaded_surface);
    loaded_surface = rgba;
  } else {
    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                "Could not convert %s for alpha bleeding: %s", image_path,
                SDL_GetError());
  }

  // Create texture from surface pixels
  image->texture = SDL_CreateTextureFromSurface(renderer, loaded_surface);
  if (image->texture == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Unable to create texture from %s: %s", image_path,
                 SDL_GetError());
    return false;
  }
  // Sprites carry a real alpha channel; blending is what makes them composite.
  SDL_SetTextureBlendMode(image->texture, SDL_BLENDMODE_BLEND);
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
// * One sprite clip per row: "x,y,w,h"
// * Rows are delimited by '\n' (a trailing '\r' is tolerated)
// * Exactly `animation->frames` rows, no more, no less
//
// Strict on purpose: a malformed .anim (stray field, too many digits, wrong
// row/field count, garbage byte) rejects the whole file loudly instead of
// silently overflowing `num`/`rect`/`sprite_clips` or half-working.
static bool load_animation_data(AnimationData *animation, const char *path) {
  size_t size;
  char *data = SDL_LoadFile(path, &size);
  if (data == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to load animation data %s", path);
    return false;
  }

  // Support coordinates up to 99999 (5 digits + NUL).
  char num[6];
  int rect[4];
  int row = 0;
  int num_i = 0;
  int rect_i = 0;
  bool ok = true;

  for (size_t i = 0; i < size && ok; i++) {
    char c = data[i];
    if (c == '\r') {
      continue;
    }
    if (c == ',' || c == '\n') {
      if (num_i == 0 || rect_i >= 4) {
        ok = false;
        break;
      }
      num[num_i] = '\0';
      rect[rect_i++] = atoi(num);
      num_i = 0;
      if (c == '\n') {
        if (rect_i != 4 || row >= animation->frames) {
          ok = false;
          break;
        }
        animation->sprite_clips[row++] = (SDL_Rect){
            .x = rect[0],
            .y = rect[1],
            .w = rect[2],
            .h = rect[3],
        };
        rect_i = 0;
      }
    } else if (c >= '0' && c <= '9') {
      if (num_i >= (int)sizeof(num) - 1) {
        ok = false;
        break;
      }
      num[num_i++] = c;
    } else {
      ok = false;
      break;
    }
  }

  // Flush a final row with no trailing newline.
  if (ok && num_i > 0) {
    if (rect_i >= 4) {
      ok = false;
    } else {
      num[num_i] = '\0';
      rect[rect_i++] = atoi(num);
      if (rect_i != 4 || row >= animation->frames) {
        ok = false;
      } else {
        animation->sprite_clips[row++] = (SDL_Rect){
            .x = rect[0],
            .y = rect[1],
            .w = rect[2],
            .h = rect[3],
        };
      }
    }
  }

  SDL_free(data);

  if (!ok || row != animation->frames) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Malformed animation data %s",
                 path);
    return false;
  }
  return true;
}

bool load_animation(SDL_Renderer *renderer, AnimationData *animation,
                    Asset sprite_asset, Asset data_asset) {
  animation->image.filename = sprite_asset.filename;
  animation->image.directory = sprite_asset.directory;
  if (!load_image(renderer, &animation->image)) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                 "Failed to load animation texture %s", sprite_asset.filename);
    return false;
  }

  char data_path[ASSET_PATH_MAX];
  asset_resolve(data_asset, data_path, sizeof(data_path));
  if (!load_animation_data(animation, data_path)) {
    return false;
  }

  return true;
}

void play_animation(AnimationData *animation, void (*on_end)(void)) {
  if (animation->is_playing) {
    return;
  }

  animation->loop_count = 0;
  animation->current_frame = 0;
  animation->is_playing = true;
  animation->start_time = clock_now_ms();
  animation->on_end = on_end;
}

void stop_animation(AnimationData *animation) {
  if (!animation->is_playing) {
    return;
  }

  animation->is_playing = false;
  animation->start_time = 0;
  animation->current_frame = 0;
  void (*on_end)(void) = animation->on_end;
  animation->on_end = NULL;
  if (on_end != NULL) {
    on_end();
  }
}

void animation_update(AnimationData *animation, int now_ms) {
  if (!animation->is_playing) {
    return;
  }

  int ms_per_frame = animation->ms_per_frame > 0 ? animation->ms_per_frame
                                                 : DEFAULT_MS_PER_FRAME;
  int delta = now_ms - animation->start_time;
  animation->current_frame = (delta / ms_per_frame) % animation->frames;
  animation->loop_count = delta / ms_per_frame / animation->frames;

  // A ONE_SHOT stops (and fires its end callback) once it has looped its
  // allotted number of times. stop_animation resets current_frame to 0.
  if (animation->style == ONE_SHOT &&
      animation->loop_count > animation->max_loop_count) {
    stop_animation(animation);
  }
}

// Camera scroll offset (see image.h). One integer offset shared by every
// draw in a frame, so a scrolling scene can't jitter between images.
static SDL_Point render_offset = {0, 0};

void render_set_offset(SDL_Point offset) { render_offset = offset; }

SDL_Point render_get_offset(void) { return render_offset; }

void render_animation(SDL_Renderer *renderer, AnimationData *animation,
                      SDL_Point point) {
  // Failed/!loaded animations have no texture; skip them instead of asking SDL
  // to render NULL.
  if (animation->image.texture == NULL) {
    return;
  }

  // Pure draw: blit the frame chosen by animation_update. No timing here, so
  // playback speed is independent of how often the scene is rendered.
  SDL_Rect *clip = &animation->sprite_clips[animation->current_frame];
  SDL_Rect render_quad = {point.x + render_offset.x, point.y + render_offset.y,
                          clip->w, clip->h};
  SDL_RenderCopyEx(renderer, animation->image.texture, clip, &render_quad, 0,
                   NULL, animation->flip);
}

void render_image(SDL_Renderer *renderer, const ImageData *image,
                  SDL_Point point) {
  SDL_Rect render_quad = {point.x + render_offset.x, point.y + render_offset.y,
                          image->width, image->height};
  SDL_RenderCopy(renderer, image->texture, NULL, &render_quad);
}

// Destination rect for a draw of natural size w x h at `point`, scaled around
// its centre (shared by the two *_scaled variants).
static SDL_Rect scaled_quad(SDL_Point point, int w, int h, float scale) {
  int sw = (int)(w * scale);
  int sh = (int)(h * scale);
  return (SDL_Rect){point.x + render_offset.x + (w - sw) / 2,
                    point.y + render_offset.y + (h - sh) / 2, sw, sh};
}

SDL_Rect scaled_quad_about(SDL_Point point, int w, int h, float scale,
                           SDL_Point anchor) {
  float x = (float)anchor.x + (float)(point.x - anchor.x) * scale;
  float y = (float)anchor.y + (float)(point.y - anchor.y) * scale;
  return (SDL_Rect){(int)x + render_offset.x, (int)y + render_offset.y,
                    (int)((float)w * scale), (int)((float)h * scale)};
}

void render_animation_scaled_about(SDL_Renderer *renderer,
                                   AnimationData *animation, SDL_Point point,
                                   float scale, SDL_Point anchor) {
  if (animation->image.texture == NULL) {
    return;
  }
  SDL_Rect *clip = &animation->sprite_clips[animation->current_frame];
  SDL_Rect render_quad =
      scaled_quad_about(point, clip->w, clip->h, scale, anchor);
  SDL_RenderCopyEx(renderer, animation->image.texture, clip, &render_quad, 0,
                   NULL, animation->flip);
}

void render_animation_scaled(SDL_Renderer *renderer, AnimationData *animation,
                             SDL_Point point, float scale) {
  if (animation->image.texture == NULL) {
    return;
  }
  SDL_Rect *clip = &animation->sprite_clips[animation->current_frame];
  SDL_Rect render_quad = scaled_quad(point, clip->w, clip->h, scale);
  SDL_RenderCopyEx(renderer, animation->image.texture, clip, &render_quad, 0,
                   NULL, animation->flip);
}

void render_image_scaled(SDL_Renderer *renderer, const ImageData *image,
                         SDL_Point point, float scale) {
  SDL_Rect render_quad = scaled_quad(point, image->width, image->height, scale);
  SDL_RenderCopy(renderer, image->texture, NULL, &render_quad);
}
