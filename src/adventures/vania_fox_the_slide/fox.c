//
//  fox.c
//  The fox character: an ActorSpec plus thin wrappers over the actor_* API.
//

#include "fox.h"
#include "constants.h"

// Filenames and frame counts come from the adventure manifest (ASSETS.md).
#include "vania_assets.h"

// The talking sheet is cue-driven: one frame per canonical mouth shape
// (X A B C D E F, see SPEECH.md), so its manifest frame count must stay the
// engine's shape count.
SDL_COMPILE_TIME_ASSERT(fox_talking_frames,
                        VANIA_FOX_ANIM_TALKING_FRAMES == MOUTH_SHAPE_COUNT);

static const ActorAnimSpec FOX_ANIMS[] = {
    {WALKING, VANIA_FOX_ANIM_WALKING_SPRITE_FILE,
     VANIA_FOX_ANIM_WALKING_DATA_FILE, VANIA_FOX_ANIM_WALKING_FRAMES,
     VANIA_FOX_ANIM_WALKING_STYLE},
    // The .anim maps the seven shapes onto the three drawn mouths
    // (closed / open / small round) until per-shape art exists.
    {TALKING, VANIA_FOX_ANIM_TALKING_SPRITE_FILE,
     VANIA_FOX_ANIM_TALKING_DATA_FILE, VANIA_FOX_ANIM_TALKING_FRAMES,
     VANIA_FOX_ANIM_TALKING_STYLE},
    {SITTING, VANIA_FOX_ANIM_SITTING_SPRITE_FILE,
     VANIA_FOX_ANIM_SITTING_DATA_FILE, VANIA_FOX_ANIM_SITTING_FRAMES,
     VANIA_FOX_ANIM_SITTING_STYLE},
    {WAVING, VANIA_FOX_ANIM_WAVING_SPRITE_FILE, VANIA_FOX_ANIM_WAVING_DATA_FILE,
     VANIA_FOX_ANIM_WAVING_FRAMES, VANIA_FOX_ANIM_WAVING_STYLE},
};

// One sprite set: the engine draws it at whatever size the scene's depth ramp
// gives her (SCALING.md).
const ActorSpec FOX_SPEC = {
    .id = "fox",
    .display_name = "Vania",
    .assets_dir = "fox",
    .velocity = 200,
    .move_sound_filename = VANIA_FOX_CHUNK_WALKING_FILE,
    .move_sound_volume = 20,
    .idle_state = SITTING,
    .move_state = WALKING,
    .anims = FOX_ANIMS,
    .anims_length = LEN(FOX_ANIMS),
    .talk_shape_frames = MOUTH_SHAPE_COUNT,
};

// The fox is just data: FOX_SPEC. The framework owns its lifecycle from the
// spec, and scenes act on it through the generic actor_* API (e.g.
// actor_play_state(fox, SITTING)). A new character is a spec, not a code file.
