#include "play_vania.h"

#include "constants.h" // LEN
#include "game.h"
#include "harness.h"
#include "script.h"
#include "vania_fox_the_slide.h" // the scene enum
#include "vania_script.h" // generated from test/scripts/vania.json: VANIA_STEPS, VANIA_EXPECT

int play_vania(void) {
  int failures = script_run(VANIA_STEPS, LEN(VANIA_STEPS));
  failures += harness_check_lines_in_order(VANIA_EXPECT, LEN(VANIA_EXPECT));
  // The script ends on the tap that opens the end card, so that is where the
  // game should be. It used to land back at the hub: the tap's release was
  // delivered to the freshly-active outro, which navigates away (#182).
  failures += harness_check(game.current_scene == OUTRO,
                            "the last tap leaves the end card on screen");
  return failures;
}
