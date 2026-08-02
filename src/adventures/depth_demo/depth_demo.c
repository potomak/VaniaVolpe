//
//  depth_demo.c
//  Registers the developer depth demo (see field.c for the actual demo).
//

#include "depth_demo.h"

#include "field.h"

#define DEPTH_DEMO_SCENES_LENGTH 1

static Scene scenes[DEPTH_DEMO_SCENES_LENGTH];

Adventure depth_demo = {
    .id = "depth-demo",
    .title = "Demo: Depth & Props",
    .assets_root = "src/adventures/depth_demo/assets",
    .scenes = scenes,
    .scenes_length = DEPTH_DEMO_SCENES_LENGTH,
    .entry_scene = FIELD,
    // A developer entry, so its cartridge stays a grey placeholder — no drawn
    // icon is ever commissioned for it.
    .hub_icon = {"icon.png", "hub"},
    .cartridge_color = {0x9A, 0x9A, 0xA4, 0xFF},
};

void depth_demo_register(void) { scenes[FIELD] = field_scene; }
