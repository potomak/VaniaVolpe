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

// Load (or free) a whole table of images — a scene's, or an adventure's shared
// bank. On the first failure the images that already loaded are freed, so the
// caller can fail the media pass without leaking textures.
bool load_image_table(SDL_Renderer *renderer, ImageData *images, int length);
void free_image_table(ImageData *images, int length);

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

// Scaled variants for tweened objects (see tween.h): draw at `scale` times
// the natural size, scaling around the image's centre so a shrinking object
// stays put instead of drifting toward its top-left corner.
void render_animation_scaled(SDL_Renderer *renderer, AnimationData *animation,
                             SDL_Point point, float scale);

void render_image_scaled(SDL_Renderer *renderer, const ImageData *image,
                         SDL_Point point, float scale);

// Destination rect for a natural-size w x h draw whose top-left is `point`,
// with the whole quad scaled about `anchor`: every corner maps to
// anchor + (corner - anchor) * scale. At scale 1 this is exactly the unscaled
// rect, so a scene that declares no depth ramp renders unchanged. Split out
// (like plane_screen_pos) so the geometry can be tested without a renderer.
SDL_Rect scaled_quad_about(SDL_Point point, int w, int h, float scale,
                           SDL_Point anchor);

// Draw an animation frame scaled about `anchor` — for an actor, her
// ground-contact point, so she keeps her feet planted as she shrinks with
// depth (see SCALING.md).
void render_animation_scaled_about(SDL_Renderer *renderer,
                                   AnimationData *animation, SDL_Point point,
                                   float scale, SDL_Point anchor);

// `flip` mirrors the art in place, for drawing something registered against a
// mirrored sprite (see actor_render_carried).
void render_image_scaled_about(SDL_Renderer *renderer, const ImageData *image,
                               SDL_Point point, float scale, SDL_Point anchor,
                               SDL_RendererFlip flip);

// Camera scroll offset, applied to every render_image / render_animation
// draw. Engine-only (game_render sets it around a scrolling scene's render
// pass and resets it for screen-space UI); scenes never call these — they
// keep drawing in scene coordinates. See DEPTH_AND_CAMERA.md.
void render_set_offset(SDL_Point offset);
SDL_Point render_get_offset(void);

#endif /* image_h */
