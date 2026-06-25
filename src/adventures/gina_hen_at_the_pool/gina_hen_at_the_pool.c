//
//  gina_hen_at_the_pool.c
//  Registers "Gina la Gallina in Piscina" by assembling its scenes into a table.
//

#include "gina_hen_at_the_pool.h"

#include "gina_state.h"
#include "grapes_minigame.h"
#include "pool.h"
#include "sunscreen_minigame.h"
#include "tree.h"
#include "vine.h"

#define GINA_HEN_AT_THE_POOL_SCENES_LENGTH 5

static Scene scenes[GINA_HEN_AT_THE_POOL_SCENES_LENGTH];

Adventure gina_hen_at_the_pool = {
    .id = "gina-hen-at-the-pool",
    .title = "Gina la Gallina in Piscina",
    .assets_root = "src/adventures/gina_hen_at_the_pool/assets",
    .scenes = scenes,
    .scenes_length = GINA_HEN_AT_THE_POOL_SCENES_LENGTH,
    .entry_scene = POOL,
    // Reset the cross-scene puzzle state every time the adventure is entered, so
    // it always starts fresh and is replayable.
    .on_enter = gina_state_reset,
};

void gina_hen_at_the_pool_register(void) {
  scenes[POOL] = pool_scene;
  scenes[TREE] = tree_scene;
  scenes[VINE] = vine_scene;
  scenes[SUNSCREEN_MINIGAME] = sunscreen_minigame_scene;
  scenes[GRAPES_MINIGAME] = grapes_minigame_scene;
}
