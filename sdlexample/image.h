//
//  image.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#ifndef image_h
#define image_h

typedef struct image_data {
  SDL_Texture *texture;
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
  int current_frame;
  bool is_playing;
  AnimationPlaybackStyle style;
  int loop_count;
  SDL_Rect *sprite_clips;
  ImageData image;
} AnimationData;

AnimationData *make_animation_data(int frames, AnimationPlaybackStyle style);

void free_image_texture(ImageData *image);

void free_animation(AnimationData *animation);

bool load_from_file(const char *path, SDL_Renderer *renderer, ImageData *image);

void render_animation(SDL_Renderer *renderer, AnimationData *animation,
                      SDL_Point point);

void render_image(SDL_Renderer *renderer, ImageData *image, SDL_Point point);

#endif /* image_h */
