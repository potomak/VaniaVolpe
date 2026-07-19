//
//  tween.h
//  Scene-object tweens (#107): animate a position (and optionally a scale)
//  from A to B over a duration with simple easing, ticked from the scene's
//  update. Actors move through the walk system; this is for everything else —
//  a float blown into the tree, a grape falling, Gina's dive arc (#119).
//
//  Usage: keep a Tween in the scene, tween_start it on the trigger, tick it
//  in update (tween_update), and draw the object at tween_pos / tween_scale
//  while it is active. The optional fields (arc_height, from/to_scale) are
//  set on the struct right after tween_start, matching how animations take
//  max_loop_count.
//

#ifndef tween_h
#define tween_h

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum tween_ease {
  TWEEN_LINEAR,
  TWEEN_EASE_IN,  // accelerate (quadratic) — falls, drops
  TWEEN_EASE_OUT, // decelerate — landings, settles
  TWEEN_BOUNCE,   // decelerating bounces into the target — a dropped object
} TweenEase;

typedef struct tween {
  SDL_FPoint from, to;
  // Parabolic lift subtracted from y, peaking mid-flight: 0 = straight line,
  // >0 = hop (the dive arc). Applied on top of the eased path.
  float arc_height;
  // Scale interpolates alongside the position (render_image_scaled /
  // render_animation_scaled); both default to 1 in tween_start.
  float from_scale, to_scale;
  float elapsed_ms, duration_ms;
  TweenEase ease;
  bool active;
  void (*on_end)(void);
} Tween;

// Begin a tween. Resets elapsed time, activates, defaults arc_height to 0 and
// both scales to 1; set those fields right after if needed. on_end may be
// NULL; it fires exactly once, after the tween lands on its target, and may
// start the next tween of a chain (even on the same struct).
void tween_start(Tween *tween, SDL_FPoint from, SDL_FPoint to,
                 float duration_ms, TweenEase ease, void (*on_end)(void));

// Advance by delta_time seconds (the scenes' update clock). Fires on_end on
// completion. Returns true while the tween is still running.
bool tween_update(Tween *tween, float delta_time);

// Current position/scale. After completion they hold the target values, so a
// scene may keep drawing the landed object without special cases.
SDL_FPoint tween_pos(const Tween *tween);
float tween_scale(const Tween *tween);

#endif /* tween_h */
