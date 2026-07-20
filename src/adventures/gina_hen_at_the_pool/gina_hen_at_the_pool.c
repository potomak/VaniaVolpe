//
//  gina_hen_at_the_pool.c
//  Registers "Gina la Gallina in Piscina" by assembling its scenes into a
//  table.
//

#include "gina_hen_at_the_pool.h"

#include "gina_state.h"
#include "grapes_minigame.h"
#include "pool.h"
#include "sunscreen_minigame.h"
#include "tree.h"
#include "vine.h"

// Asset declarations generated from the manifest (ASSETS.md): the shared
// sound-effect bank (SCENES.md milestone 4).
#include "gina_assets.h"

#define GINA_HEN_AT_THE_POOL_SCENES_LENGTH 5

static Scene scenes[GINA_HEN_AT_THE_POOL_SCENES_LENGTH];

// The adventure's shared SFX, played by scenes via play_<name>() (chime, wind,
// splash, caw, pop). Loaded once for the whole adventure.
static ChunkData sfx[GINA_SFX_COUNT] = GINA_SFX_INIT;

Adventure gina_hen_at_the_pool = {
    .id = "gina-hen-at-the-pool",
    .title = "Gina la Gallina in Piscina",
    .assets_root = "src/adventures/gina_hen_at_the_pool/assets",
    .scenes = scenes,
    .scenes_length = GINA_HEN_AT_THE_POOL_SCENES_LENGTH,
    .entry_scene = POOL,
    // Reset the cross-scene puzzle state every time the adventure is entered,
    // so it always starts fresh and is replayable.
    .on_enter = gina_state_reset,
    .sfx = sfx,
    .sfx_length = GINA_SFX_COUNT,
};

void gina_hen_at_the_pool_register(void) {
  scenes[POOL] = pool_scene;
  scenes[TREE] = tree_scene;
  scenes[VINE] = vine_scene;
  scenes[SUNSCREEN_MINIGAME] = sunscreen_minigame_scene;
  scenes[GRAPES_MINIGAME] = grapes_minigame_scene;
}
