//
//  vania_fox_the_slide.c
//  Registers the Vania Fox - The Slide adventure by assembling its scenes into a
//  table.
//

#include "vania_fox_the_slide.h"

#include "example.h"
#include "intro.h"
#include "outro.h"
#include "playground.h"
#include "playground_entrance.h"

#define VANIA_FOX_THE_SLIDE_SCENES_LENGTH 5

static Scene scenes[VANIA_FOX_THE_SLIDE_SCENES_LENGTH];

Adventure vania_fox_the_slide = {
    .id = "vania-fox-the-slide",
    .title = "Vania Volpe - Lo Scivolo",
    .assets_root = "src/adventures/vania_fox_the_slide/assets",
    .scenes = scenes,
    .scenes_length = VANIA_FOX_THE_SLIDE_SCENES_LENGTH,
    .entry_scene = INTRO,
};

void vania_fox_the_slide_register(void) {
  scenes[INTRO] = intro_scene;
  scenes[PLAYGROUND_ENTRANCE] = playground_entrance_scene;
  scenes[PLAYGROUND] = playground_scene;
  scenes[OUTRO] = outro_scene;
  scenes[EXAMPLE] = example_scene;
}
