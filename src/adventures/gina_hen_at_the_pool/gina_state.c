//
//  gina_state.c
//

#include <SDL2/SDL.h>

#include "gina_state.h"

GinaState gina_state;

void gina_state_reset(void) {
  gina_state.has_sunscreen = false;
  gina_state.has_goggles = false;
  gina_state.float_state = FLOAT_AT_POOL;
  gina_state.has_basket = false;
  gina_state.has_grapes = false;
  gina_state.announce_sunscreen = false;
  gina_state.announce_grapes = false;
  gina_state.examine_float_look = FLOAT_LOOK_CANT_REACH;
}
