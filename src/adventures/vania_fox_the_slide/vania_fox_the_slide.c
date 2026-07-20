//
//  vania_fox_the_slide.c
//  Registers the Vania Fox - The Slide adventure by assembling its scenes into
//  a table.
//

#include "vania_fox_the_slide.h"

#include "intro.h"
#include "outro.h"
#include "playground.h"
#include "playground_entrance.h"

// Asset declarations generated from the manifest (ASSETS.md): the shared
// sound-effect bank (SCENES.md milestone 4).
#include "vania_assets.h"

#define VANIA_FOX_THE_SLIDE_SCENES_LENGTH 4

static Scene scenes[VANIA_FOX_THE_SLIDE_SCENES_LENGTH];

// The adventure's shared SFX, played by scenes via play_<name>() (excavator,
// shovel, key_reveal, acorns_falling, peg_falling). Loaded once for the whole
// adventure.
static ChunkData sfx[VANIA_SFX_COUNT] = VANIA_SFX_INIT;

Adventure vania_fox_the_slide = {
    .id = "vania-fox-the-slide",
    .title = "Vania Volpe - Lo Scivolo",
    .assets_root = "src/adventures/vania_fox_the_slide/assets",
    .scenes = scenes,
    .scenes_length = VANIA_FOX_THE_SLIDE_SCENES_LENGTH,
    .entry_scene = INTRO,
    .sfx = sfx,
    .sfx_length = VANIA_SFX_COUNT,
};

void vania_fox_the_slide_register(void) {
  scenes[INTRO] = intro_scene;
  scenes[PLAYGROUND_ENTRANCE] = playground_entrance_scene;
  scenes[PLAYGROUND] = playground_scene;
  scenes[OUTRO] = outro_scene;
}
