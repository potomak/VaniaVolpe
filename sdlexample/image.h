//
//  image.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#ifndef image_h
#define image_h

#import "asset.h"

typedef struct image_data {
  SDL_Texture *texture;
  const char *filename;
  const char *directory;
  int width;
  int height;
} ImageData;

typedef enum animation_playback_style {
  LOOP,
  PING_PONG, // TODO: Not supported yet
  ONE_SHOT,
} AnimationPlaybackStyle;

typedef struct animation_data {
  int frames;
  int start_time;
  bool is_playing;
  AnimationPlaybackStyle style;
  int loop_count;
  int max_loop_count;
  SDL_Rect *sprite_clips;
  ImageData image;
  SDL_RendererFlip flip;
} AnimationData;

AnimationData *make_animation_data(int frames, AnimationPlaybackStyle style);

void free_image_texture(ImageData *image);

void free_animation(AnimationData *animation);

bool load_image(SDL_Renderer *renderer, ImageData *image);

bool load_animation(SDL_Renderer *renderer, AnimationData *animation,
                    Asset sprite_asset, Asset data_asset);

void play_animation(AnimationData *animation, void (*on_end)(void));

void stop_animation(AnimationData *animation);

void render_animation(SDL_Renderer *renderer, AnimationData *animation,
                      SDL_Point point);

void render_image(SDL_Renderer *renderer, const ImageData *image,
                  SDL_Point point);

#endif /* image_h */
