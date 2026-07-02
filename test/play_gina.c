#include "play_gina.h"

#include <stddef.h>
#include <stdio.h>

#include "constants.h"
#include "harness.h"

// The sunscreen minigame: hold the button and serpentine over the close-up so
// every cell gets painted (mirrors scratchpad/gina_smoke.js's brush()).
static void brush(void) {
  double x0 = 0.36;
  double x1 = 0.64;
  harness_mouse_down(x0, 0.31);
  for (int row = 0; row < 8; row++) {
    double y = 0.31 + row * (0.38 / 7);
    double a = row % 2 == 0 ? x0 : x1;
    double b = row % 2 == 0 ? x1 : x0;
    for (int s = 0; s <= 16; s++) {
      harness_mouse_move(a + (b - a) * s / 16, y);
    }
    harness_step_frame(); // let the paint register progressively
  }
  harness_mouse_up(x1, 0.31);
  harness_pump_for(1500);
}

// Expected dialogue substrings, in order (the same set gina_smoke.js checks).
static const char *const EXPECTED[] = {
    "Hub: starting adventure 'Gina la Gallina in Piscina'",
    "Devo mettere la crema",
    "Pronta! Ora posso uscire al sole",
    "Ho preso gli occhialini",
    "Mi serve il salvagente",
    "Oh no! Il vento",
    "Il salvagente e' sull'albero",
    "Non ci arrivo",
    "Prendi questo cestino",
    "Cestino pieno d'uva",
    "Ecco il tuo salvagente",
    "Che bello! Ancora",
};

int play_gina(void) {
  bool frame_ok = false;

  // Hub → Gina (the second button).
  harness_click(0.5, 0.497);
  harness_pump_for(1500);

  frame_ok = harness_frame_has_variation(); // pool scene should be a real image

  // Try to leave the shade before sunscreen → refused.
  harness_click(0.5, 0.5);
  harness_pump_for(1200);
  // Tap the sunscreen bottle → walk → sunscreen minigame.
  harness_click(0.175, 0.883);
  harness_pump_for(2500);
  brush();

  // Collect goggles.
  harness_click(0.45, 0.808);
  harness_pump_for(2800);
  // Tap pool → needs the float.
  harness_click(0.5, 0.217);
  harness_pump_for(3000);
  // Tap the float → wind blows it into the tree.
  harness_click(0.756, 0.833);
  harness_pump_for(3000);
  // Tap pool again → float is now in the tree.
  harness_click(0.5, 0.217);
  harness_pump_for(3000);

  // Go to the tree (right edge).
  harness_click(0.981, 0.542);
  harness_pump_for(1200);
  // Examine the stuck float.
  harness_click(0.681, 0.167);
  harness_pump_for(3000);
  // Talk to Carla → she gives the basket.
  harness_click(0.494, 0.308);
  harness_pump_for(3000);

  // Go to the vine (right edge).
  harness_click(0.981, 0.542);
  harness_pump_for(1200);
  // Tap grapes → grapes minigame.
  harness_click(0.5, 0.4);
  harness_pump_for(2800);
  // Pick every grape.
  static const double grapes[6][2] = {{0.45, 0.283},   {0.525, 0.3},
                                      {0.6, 0.283},    {0.475, 0.383},
                                      {0.5625, 0.383}, {0.519, 0.483}};
  for (int i = 0; i < 6; i++) {
    harness_click(grapes[i][0], grapes[i][1]);
    harness_pump_for(400);
  }
  harness_pump_for(1000);

  // Back to the tree (left edge) and give Carla the grapes.
  harness_click(0.019, 0.542);
  harness_pump_for(1200);
  harness_click(0.494, 0.308);
  harness_pump_for(3000);

  // Back to the pool (left edge) and dive.
  harness_click(0.019, 0.542);
  harness_pump_for(1200);
  harness_click(0.5, 0.217);
  harness_pump_for(3500);

  int failures = harness_check_lines_in_order(EXPECTED, LEN(EXPECTED));
  if (!frame_ok) {
    fprintf(stderr, "MISS  rendered frame had no variation (blank screen?)\n");
    failures++;
  } else {
    fprintf(stderr, "OK    rendered a non-blank frame\n");
  }
  return failures;
}
