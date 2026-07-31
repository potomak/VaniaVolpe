#include "play_vania.h"

#include "constants.h" // LEN
#include "harness.h"
#include "script.h"
#include "vania_script.h" // generated from test/scripts/vania.json: VANIA_STEPS, VANIA_EXPECT

int play_vania(void) {
  int failures = script_run(VANIA_STEPS, LEN(VANIA_STEPS));
  failures += harness_check_lines_in_order(VANIA_EXPECT, LEN(VANIA_EXPECT));
  return failures;
}
