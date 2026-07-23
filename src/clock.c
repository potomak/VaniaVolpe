//
//  clock.c
//  The simulation clock seam (see clock.h / #155).
//

#include "clock.h"

static bool virtual_mode = false;
static Uint32 virtual_now = 0;

Uint32 clock_now_ms(void) {
  return virtual_mode ? virtual_now : SDL_GetTicks();
}

void clock_set_virtual(bool on) {
  virtual_mode = on;
  virtual_now = 0;
}

void clock_advance(Uint32 ms) {
  if (virtual_mode) {
    virtual_now += ms;
  }
}

bool clock_is_virtual(void) { return virtual_mode; }
