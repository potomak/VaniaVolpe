//
//  gina_state.c
//

#include <stdio.h>

#include "gina_state.h"

GinaState gina_state;

void gina_state_reset(void) {
  gina_state.has_sunscreen = false;
  gina_state.has_goggles = false;
  gina_state.float_state = FLOAT_AT_POOL;
  gina_state.has_basket = false;
  gina_state.has_grapes = false;
  gina_state.examine_float_count = 0;
}

void gina_say(Hen *gina, const char *line, Mix_Chunk *voice) {
  fprintf(stdout, "Gina: %s\n", line);
  hen_talk(gina, voice);
}
