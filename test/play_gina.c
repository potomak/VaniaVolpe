#include "play_gina.h"

#include "constants.h" // LEN
#include "gina_script.h" // generated from test/scripts/gina.json: GINA_STEPS, GINA_EXPECT
#include "harness.h"
#include "script.h"

int play_gina(void) {
  int failures = script_run(GINA_STEPS, LEN(GINA_STEPS));
  failures += harness_check_lines_in_order(GINA_EXPECT, LEN(GINA_EXPECT));
  return failures;
}
