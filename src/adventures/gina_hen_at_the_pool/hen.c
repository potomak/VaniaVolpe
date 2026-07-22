//
//  hen.c
//  Gina the hen: an ActorSpec plus thin wrappers over the actor_* API.
//

#include "hen.h"

// Filenames and frame counts come from the adventure manifest (ASSETS.md);
// the list stays explicit because only these sheets exist so far — the
// manifest's other hen animations (fidgets, drag states) are still authoring
// tasks and slot in here as their art lands.
#include "gina_assets.h"

static const ActorAnimSpec HEN_ANIMS[] = {
    {IDLE, GINA_HEN_ANIM_IDLE_SPRITE_FILE, GINA_HEN_ANIM_IDLE_DATA_FILE,
     GINA_HEN_ANIM_IDLE_FRAMES, GINA_HEN_ANIM_IDLE_STYLE},
    {WALKING, GINA_HEN_ANIM_WALKING_SPRITE_FILE,
     GINA_HEN_ANIM_WALKING_DATA_FILE, GINA_HEN_ANIM_WALKING_FRAMES,
     GINA_HEN_ANIM_WALKING_STYLE},
    {TALKING, GINA_HEN_ANIM_TALKING_SPRITE_FILE,
     GINA_HEN_ANIM_TALKING_DATA_FILE, GINA_HEN_ANIM_TALKING_FRAMES,
     GINA_HEN_ANIM_TALKING_STYLE},
};

// One sprite set so far: the near variant. Far/mid depth variants (see
// DEPTH_AND_CAMERA.md) slot in here as more entries.
static const ActorVariantSpec HEN_VARIANTS[] = {
    {.anims = HEN_ANIMS, .anims_length = 3, .speed_scale = 1.0F},
};

const ActorSpec HEN_SPEC = {
    .id = "hen",
    .display_name = "Gina",
    .assets_dir = "hen",
    .velocity = 200,
    .move_sound_filename = GINA_HEN_CHUNK_WALKING_FILE,
    .move_sound_volume = 20,
    .idle_state = IDLE,
    .move_state = WALKING,
    .variants = HEN_VARIANTS,
    .variants_length = 1,
};

// No wrappers: the framework owns the hen's whole lifecycle from HEN_SPEC —
// make/load/free (#141) and, when a scene declares no update/render, tick/draw
// (#147). Scenes act on the actor through the generic actor_* API, and dialogue
// goes through the generated say_<name>() helpers (scene_say → actor_talk). A
// new character is now a spec, not a code file.
