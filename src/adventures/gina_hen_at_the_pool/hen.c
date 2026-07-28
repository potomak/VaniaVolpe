//
//  hen.c
//  Gina the hen: an ActorSpec plus thin wrappers over the actor_* API.
//

#include "hen.h"
#include "constants.h"

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

// One sprite set: the engine draws it at whatever size the scene's depth ramp
// gives her (SCALING.md).
const ActorSpec HEN_SPEC = {
    .id = "hen",
    .display_name = "Gina",
    .assets_dir = "hen",
    .velocity = 200,
    .move_sound_filename = GINA_HEN_CHUNK_WALKING_FILE,
    .move_sound_volume = 20,
    .idle_state = IDLE,
    .move_state = WALKING,
    .anims = HEN_ANIMS,
    .anims_length = LEN(HEN_ANIMS),
};

// The hen is just data: HEN_SPEC. The framework owns its lifecycle from the
// spec; scenes act on it through the generic actor_* API, and dialogue goes
// through the generated say_<name>() helpers (scene_say → actor_talk). A new
// character is a spec, not a code file.
