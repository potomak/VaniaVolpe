//
//  scaling.c
//  See SCALING.md.
//

#include <stddef.h>

#include "scaling.h"

float scale_ramp_at(const ScaleRamp *ramp, float y) {
  if (ramp == NULL) {
    return 1.0F;
  }
  int span = ramp->y_near - ramp->y_far;
  if (span == 0) {
    return ramp->scale_near; // degenerate ramp: the scene is one depth
  }
  float t = (y - (float)ramp->y_far) / (float)span;
  if (t < 0.0F) {
    t = 0.0F;
  }
  if (t > 1.0F) {
    t = 1.0F;
  }
  return ramp->scale_far + t * (ramp->scale_near - ramp->scale_far);
}
