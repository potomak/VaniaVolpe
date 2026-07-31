//
//  test_gina_nav.c
//  Gina's scene ring: the ground reaches the tiles she travels by, and the two
//  scenes either side of a tile agree about where she comes out.
//

#include <math.h>
#include <stdio.h>

#include "constants.h"
#include "gina_nav.h"
#include "scaling.h"
#include "walk.h"

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

int test_gina_nav(void) {
  fprintf(stderr, "\n-- Gina scene ring unit tests --\n");
  failures = 0;

  WalkGrid grid;
  walk_grid_build(&grid, &GINA_OUTDOOR_WALK_AREA,
                  (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT});

  // The tiles are only useful if she can get to them: both path ends must be
  // walkable, and reachable from the near ground rather than stranded islands.
  SDL_Point ends[2];
  gina_nav_pois(ends);
  for (int i = 0; i < 2; i++) {
    check(walk_grid_contains(&grid, ends[i]),
          i == 0 ? "the left path end is walkable"
                 : "the right path end is walkable");
  }

  SDL_FPoint near_ground = {400, 500};
  for (int i = 0; i < 2; i++) {
    SDL_FPoint path[ACTOR_MAX_WAYPOINTS];
    SDL_FPoint goal = {(float)ends[i].x, (float)ends[i].y};
    int count = walk_grid_find_path(&grid, near_ground, goal, path, LEN(path));
    bool arrived = count >= 1 &&
                   fabsf(path[count - 1].x - goal.x) < WALK_CELL_SIZE &&
                   fabsf(path[count - 1].y - goal.y) < WALK_CELL_SIZE;
    check(arrived, i == 0 ? "she can walk from the near ground to the left tile"
                          : "and to the right tile");
  }

  // Depth has to cover the whole of that ground, or she would arrive at the
  // horizon still full size — the thing the paths exist to show. The ramp is
  // in feet coordinates, half a 120px frame below the walk positions above.
  const float FEET = 60;
  float near_scale = scale_ramp_at(&GINA_OUTDOOR_RAMP, near_ground.y + FEET);
  float far_scale = scale_ramp_at(&GINA_OUTDOOR_RAMP, (float)ends[0].y + FEET);
  check(far_scale < near_scale,
        "she is drawn smaller at the tiles than on the near ground");
  check(far_scale <= 0.7F,
        "and small enough there to read as distance, not a rounding error");
  check(GINA_OUTDOOR_RAMP.scale_far >= 0.6F,
        "but not past the floor where her mouth shapes stop reading");

  return failures;
}
