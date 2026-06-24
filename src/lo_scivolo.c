//
//  lo_scivolo.c
//  Registers the "Lo Scivolo" adventure by assembling its scenes into a table.
//

#include "lo_scivolo.h"

#include "example.h"
#include "intro.h"
#include "outro.h"
#include "playground.h"
#include "playground_entrance.h"

#define LO_SCIVOLO_SCENES_LENGTH 5

static Scene scenes[LO_SCIVOLO_SCENES_LENGTH];

Adventure lo_scivolo = {
    .id = "lo-scivolo",
    .title = "Vania Volpe - Lo Scivolo",
    .scenes = scenes,
    .scenes_length = LO_SCIVOLO_SCENES_LENGTH,
    .entry_scene = INTRO,
};

void lo_scivolo_register(void) {
  scenes[INTRO] = intro_scene;
  scenes[PLAYGROUND_ENTRANCE] = playground_entrance_scene;
  scenes[PLAYGROUND] = playground_scene;
  scenes[OUTRO] = outro_scene;
  scenes[EXAMPLE] = example_scene;
}
