//
//  tween.c
//

#include "tween.h"

void tween_start(Tween *tween, SDL_FPoint from, SDL_FPoint to,
                 float duration_ms, TweenEase ease, void (*on_end)(void)) {
  tween->from = from;
  tween->to = to;
  tween->arc_height = 0;
  tween->from_scale = 1;
  tween->to_scale = 1;
  tween->elapsed_ms = 0;
  tween->duration_ms = duration_ms;
  tween->ease = ease;
  tween->active = true;
  tween->on_end = on_end;
}

// Map linear progress p (0..1) through the easing curve.
static float eased(TweenEase ease, float p) {
  switch (ease) {
  case TWEEN_EASE_IN:
    return p * p;
  case TWEEN_EASE_OUT:
    return 1 - (1 - p) * (1 - p);
  case TWEEN_BOUNCE: {
    // Standard ease-out-bounce: three decaying bounces into the target.
    const float n = 7.5625F, d = 2.75F;
    if (p < 1 / d) {
      return n * p * p;
    }
    if (p < 2 / d) {
      p -= 1.5F / d;
      return n * p * p + 0.75F;
    }
    if (p < 2.5F / d) {
      p -= 2.25F / d;
      return n * p * p + 0.9375F;
    }
    p -= 2.625F / d;
    return n * p * p + 0.984375F;
  }
  case TWEEN_LINEAR:
  default:
    return p;
  }
}

// Linear progress 0..1 (1 once finished).
static float progress(const Tween *tween) {
  if (!tween->active || tween->duration_ms <= 0) {
    return 1;
  }
  float p = tween->elapsed_ms / tween->duration_ms;
  return p > 1 ? 1 : p;
}

bool tween_update(Tween *tween, float delta_time) {
  if (!tween->active) {
    return false;
  }
  tween->elapsed_ms += delta_time * 1000.0F;
  if (tween->elapsed_ms >= tween->duration_ms) {
    // Land exactly on the target before the callback runs, so on_end sees
    // the finished state (and may chain a new tween on this struct).
    tween->elapsed_ms = tween->duration_ms;
    tween->active = false;
    if (tween->on_end != NULL) {
      tween->on_end();
    }
    return false;
  }
  return true;
}

SDL_FPoint tween_pos(const Tween *tween) {
  float e = eased(tween->ease, progress(tween));
  SDL_FPoint pos = {tween->from.x + (tween->to.x - tween->from.x) * e,
                    tween->from.y + (tween->to.y - tween->from.y) * e};
  // The hop: a parabola in linear time peaking mid-flight, so the arc shape
  // doesn't distort with the easing of the path itself.
  float p = progress(tween);
  pos.y -= tween->arc_height * 4 * p * (1 - p);
  return pos;
}

float tween_scale(const Tween *tween) {
  float e = eased(tween->ease, progress(tween));
  return tween->from_scale + (tween->to_scale - tween->from_scale) * e;
}
