// Headless smoke test: brings up the game with no display server or audio
// hardware and runs the scripted adventure play-tests, asserting the expected
// dialogue appeared. The native analog of the browser scratchpad/gina_smoke.js.
//
// The harness (test/harness.c) owns SDL setup, the loop, scripted input and
// output capture; each play-test (test/play_gina.c, …) scripts one adventure.
//
// Exit code: 0 = every play-test passed, 2 = a check failed, 1 = setup failure.

#include <stdio.h>

#include "harness.h"
#include "play_gina.h"
#include "test_lipsync.h"
#include "test_scene.h"
#include "test_walk.h"

int main(void) {
  // Install the log-capture sink before anything logs; the run's own
  // OK/MISS/summary lines go to stderr and stay visible.
  if (!harness_capture_begin()) {
    return 1;
  }
  if (!harness_init()) {
    fprintf(stderr, "test: window/media init failed\n");
    return 1;
  }
  if (!harness_start_game()) {
    fprintf(stderr, "test: could not start the game\n");
    harness_shutdown();
    return 1;
  }

  int failures = play_gina();
  // Future play-tests (e.g. play_vania) are added here.

  // Pure unit tests (no window/assets; run after the playthrough so its
  // captured log stays contiguous).
  failures += test_walk();
  failures += test_lipsync();
  failures += test_scene();

  harness_shutdown();

  if (failures == 0) {
    fprintf(stderr, "\nPASS: all checks present\n");
    return 0;
  }
  fprintf(stderr, "\nFAIL: %d check(s) missing\n", failures);
  return 2;
}
