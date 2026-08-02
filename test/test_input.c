//
//  test_input.c
//  Engine-level input (src/game.c): touch, and the keys the game reads.
//
//  Touch (normalize_touch). The audience taps with fingers, and
//  on a device SDL delivers both a finger event and a mouse event synthesized
//  from it — so this covers the three things that path has to get right: a tap
//  acts once, the synthesized duplicate is dropped, and a second finger does
//  not interfere with the one already down.
//
//  Driven through the selection screen because that is where the game starts
//  and a tap there logs which adventure it opened, so a tap that lands is
//  distinguishable from one that does not. Every "must not act" check is
//  paired with a "must act" one on the same fingers: on its own, a negative
//  would pass just as well if touch did nothing at all. Leaves the game back
//  on the selection screen for the play-tests that follow.
//

#include <stdio.h>

#include "harness.h"

#include "game.h"

#include "test_input.h"

// Cartridge centres on the selection screen, and the corner button, as
// fractions of the window (hub.c: 220x200 cartridges, 35px gaps, top at 110;
// the button is 64px inset 12 from the top-right corner).
#define VANIA_X 0.18125
#define GINA_X 0.5
#define CARTRIDGE_Y 0.35
#define HUB_BUTTON_X 0.945
#define HUB_BUTTON_Y 0.0733
// The tick on the leave-the-adventure question (confirm.c).
#define CONFIRM_YES_X 0.3575
#define CONFIRM_YES_Y 0.5

#define OPENED_VANIA "Hub: starting adventure 'Vania Volpe - Lo Scivolo'"
#define OPENED_GINA "Hub: starting adventure 'Gina la Gallina in Piscina'"

// Leave whatever adventure is open, by touch: the corner button raises the
// question, the tick answers it.
static void leave_by_touch(void) {
  harness_tap(HUB_BUTTON_X, HUB_BUTTON_Y);
  harness_pump_for(200);
  harness_tap(CONFIRM_YES_X, CONFIRM_YES_Y);
  harness_pump_for(400);
}

int test_input(void) {
  fprintf(stderr, "\n-- input --\n");
  int failures = 0;

  // A tap opens the cartridge under it, the same as a click would. This is
  // also the coordinate conversion under test: finger positions are normalized
  // to the window, and landing on the right cartridge means they were mapped
  // through the logical viewport.
  harness_log_reset();
  harness_tap(VANIA_X, CARTRIDGE_Y);
  harness_pump_for(600);
  failures += harness_check(harness_log_contains(OPENED_VANIA),
                            "a tap opens the cartridge under it");

  // Leaving is by touch too: a 64px corner button, then the tick. Proven by
  // what follows — a cartridge can only be opened from the selection screen,
  // so opening one means both taps landed.
  leave_by_touch();
  harness_log_reset();
  harness_tap(GINA_X, CARTRIDGE_Y);
  harness_pump_for(600);
  failures += harness_check(harness_log_contains(OPENED_GINA),
                            "the corner button and the tick both take a tap");

  // The mouse event SDL synthesizes from a real tap must be ignored, or every
  // tap on a device counts twice.
  leave_by_touch();
  harness_log_reset();
  harness_synthesized_click(VANIA_X, CARTRIDGE_Y);
  harness_pump_for(400);
  failures += harness_check(!harness_log_contains(OPENED_VANIA),
                            "a mouse event synthesized from touch is ignored");

  // Two fingers at once: the first one down owns the interaction. The second's
  // release must not act at its own position...
  harness_log_reset();
  harness_finger_down(1, VANIA_X, CARTRIDGE_Y);
  harness_finger_down(2, GINA_X, CARTRIDGE_Y);
  harness_finger_up(2, GINA_X, CARTRIDGE_Y);
  harness_pump_for(400);
  failures +=
      harness_check(!harness_log_contains(OPENED_GINA),
                    "a second finger's release does not act while the first "
                    "is down");

  // ...and the first one's still must, once it lifts. Together these say the
  // stray finger was ignored rather than the whole gesture dropped.
  harness_finger_up(1, VANIA_X, CARTRIDGE_Y);
  harness_pump_for(600);
  failures += harness_check(harness_log_contains(OPENED_VANIA),
                            "the first finger's release still acts");

  // Hand the play-tests the selection screen they expect to start from.
  leave_by_touch();
  harness_log_reset();

  bool before = game.is_debugging;
#ifdef PROD
  // A distributed build has no way into the debug layer: neither the gesture
  // nor the key is compiled in, so neither can reach it.
  harness_mouse_down(0.02, 0.02);
  harness_pump_for(3000);
  harness_mouse_up(0.02, 0.02);
  harness_key_down(SDLK_d, false);
  harness_pump_for(50);
  failures += harness_check(game.is_debugging == before,
                            "PROD build: nothing opens the debug overlay");
#else
  // The way in on a device with no keyboard: press and hold the top-left
  // corner. A press alone, held briefly, must not be enough.
  harness_mouse_down(0.02, 0.02);
  harness_pump_for(500);
  failures += harness_check(game.is_debugging == before,
                            "a short press in the corner does not toggle");
  harness_pump_for(2000);
  failures += harness_check(game.is_debugging != before,
                            "holding the corner toggles the debug overlay");
  harness_mouse_up(0.02, 0.02);
  harness_pump_for(50);

  // Off again, so the rest of the run is unaffected.
  harness_mouse_down(0.02, 0.02);
  harness_pump_for(2500);
  harness_mouse_up(0.02, 0.02);
  harness_pump_for(50);
  failures += harness_check(game.is_debugging == before,
                            "holding it again toggles back");

  // Wandering out of the corner is not a hold, however long the finger is down.
  harness_mouse_down(0.02, 0.02);
  harness_pump_for(300);
  harness_mouse_move(0.5, 0.5);
  harness_pump_for(2500);
  harness_mouse_up(0.5, 0.5);
  harness_pump_for(50);
  failures += harness_check(game.is_debugging == before,
                            "a press that leaves the corner is not a hold");

  // Holding D must toggle the debug overlay once, not once per auto-repeat.
  bool was_debugging = game.is_debugging;
  harness_key_down(SDLK_d, false);
  harness_pump_for(50);
  bool toggled = game.is_debugging != was_debugging;
  for (int i = 0; i < 5; i++) {
    harness_key_down(SDLK_d, true);
  }
  harness_pump_for(50);
  failures += harness_check(toggled && game.is_debugging != was_debugging,
                            "auto-repeat does not strobe the debug overlay");
  // Leave it as it was found.
  harness_key_down(SDLK_d, false);
  harness_pump_for(50);
#endif /* PROD */

  return failures;
}
