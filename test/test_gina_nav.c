//
//  test_gina_nav.c
//  Where Gina stands when she walks into a scene. Pure state: the rule reads
//  the current scene and the one she left, so the tests set both directly
//  rather than driving a playthrough.
//

#include <stdio.h>

#include "game.h"
#include "gina_hen_at_the_pool.h"
#include "gina_nav.h"
#include "gina_state.h"

#include "test_gina_nav.h"

static int failures;

static void check(bool ok, const char *what) {
  if (ok) {
    fprintf(stderr, "OK    %s\n", what);
  } else {
    fprintf(stderr, "MISS  %s\n", what);
    failures++;
  }
}

// The two edges every walkable scene shares, mirroring gina_nav.c.
static const float LEFT_X = 40;
static const float RIGHT_X = 760;

static SDL_FPoint arrive(int scene, int from) {
  game.current_scene = scene;
  gina_state.came_from = from;
  return gina_nav_entry((SDL_FPoint){400, 480});
}

int test_gina_nav(void) {
  fprintf(stderr, "\n-- Gina scene ring unit tests --\n");
  failures = 0;

  const Adventure *saved_adventure = game.current_adventure;
  int saved_scene = game.current_scene;

  // Walking off an edge puts her at the exit that leads back, so the two
  // scenes agree about which side of the world they share. The pool's right
  // edge goes to the tree, so she arrives on the tree's left.
  check(arrive(GINA_TREE, GINA_POOL).x == LEFT_X,
        "leaving the pool eastward lands her on the tree's west edge");
  check(arrive(GINA_POOL, GINA_TREE).x == RIGHT_X,
        "and coming back the other way lands her on the pool's east edge");
  check(arrive(GINA_VINE, GINA_TREE).x == LEFT_X,
        "the tree's east exit lands her on the vine's west edge");
  check(arrive(GINA_POOL, GINA_VINE).x == LEFT_X,
        "the ring closes: the vine's east exit is the pool's west edge");

  // Every arrival is on the walkable strip, not merely on the right side.
  check(arrive(GINA_TREE, GINA_POOL).y == arrive(GINA_POOL, GINA_TREE).y,
        "both edges put her at the same depth");

  // Not a walk: returning from a minigame, or the very first entry, has no
  // side to arrive on and must leave her where the scene wants her.
  check(arrive(GINA_POOL, GINA_NO_SCENE).x == 400,
        "an arrival that was not a walk falls back to the scene's own start");
  check(arrive(GINA_POOL, GINA_SUNSCREEN_MINIGAME).x == 400,
        "and so does one from a scene that is not on the ring");

  // The origin is consumed, so the *next* arrival — the minigame handing
  // control back — is not still placed by the walk before it.
  game.current_scene = GINA_VINE;
  gina_state.came_from = GINA_TREE;
  gina_nav_entry((SDL_FPoint){400, 480});
  check(gina_nav_entry((SDL_FPoint){400, 480}).x == 400,
        "the entry side is one-shot: a second arrival falls back");

  // A scene with no edges at all never places her.
  check(arrive(GINA_OUTRO, GINA_POOL).x == 400,
        "a scene off the ring ignores where she came from");

  game.current_adventure = saved_adventure;
  game.current_scene = saved_scene;
  gina_state.came_from = GINA_NO_SCENE;
  return failures;
}
