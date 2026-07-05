//
//  image.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/15/25.
//

#ifndef image_h
#define image_h

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "asset.h"

// Cyan is the transparent color key for sprite sheets (see load_image).
#define COLOR_KEY_R 0x00
#define COLOR_KEY_G 0xFF
#define COLOR_KEY_B 0xFF

// Default animation speed, 12 FPS. Per-animation overrides live in
// AnimationData.ms_per_frame / ActorAnimSpec.ms_per_frame.
#define DEFAULT_MS_PER_FRAME 83

typedef struct image_data {
  SDL_Texture *texture;
  // Borrowed, not owned: these point at string literals in an ActorSpec / scene
  // table that outlive the ImageData. Don't free them.
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
  // Frame index to draw; advanced by animation_update, read by
  // render_animation.
  int current_frame;
  // Milliseconds per frame (defaults to DEFAULT_MS_PER_FRAME).
  int ms_per_frame;
  SDL_Rect *sprite_clips;
  ImageData image;
  SDL_RendererFlip flip;
  // Fired once when the animation stops (a ONE_SHOT reaching its loop count,
  // or an explicit stop_animation call); per-instance so starting one
  // animation can't clobber another's pending callback. Cleared before
  // firing. NULL for none.
  void (*on_end)(void);
} AnimationData;

AnimationData *make_animation_data(int frames, AnimationPlaybackStyle style);

void free_image_texture(ImageData *image);

void free_animation(AnimationData *animation);

bool load_image(SDL_Renderer *renderer, ImageData *image);

bool load_animation(SDL_Renderer *renderer, AnimationData *animation,
                    Asset sprite_asset, Asset data_asset);

void play_animation(AnimationData *animation, void (*on_end)(void));

void stop_animation(AnimationData *animation);

// Advance an animation's current frame from elapsed time, and (for ONE_SHOT)
// stop it when complete. Call once per frame from the owner's update step —
// keeping timing out of render_animation so playback speed doesn't depend on
// how often the frame is drawn, and so end callbacks don't fire mid-render.
void animation_update(AnimationData *animation, int now_ms);

void render_animation(SDL_Renderer *renderer, AnimationData *animation,
                      SDL_Point point);

void render_image(SDL_Renderer *renderer, const ImageData *image,
                  SDL_Point point);

#endif /* image_h */
