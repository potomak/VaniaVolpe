//
//  fox.c
//  The fox character: an ActorSpec plus thin wrappers over the actor_* API.
//

#include "fox.h"

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

// One sprite set so far: the near variant. Far/mid depth variants (see
// DEPTH_AND_CAMERA.md) slot in here as more entries.
static const ActorVariantSpec FOX_VARIANTS[] = {
    {.anims = FOX_ANIMS, .anims_length = 4, .speed_scale = 1.0F},
};

const ActorSpec FOX_SPEC = {
    .id = "fox",
    .display_name = "Vania",
    .assets_dir = "fox",
    .velocity = 200,
    .move_sound_filename = VANIA_FOX_CHUNK_WALKING_FILE,
    .move_sound_volume = 20,
    .idle_state = SITTING,
    .move_state = WALKING,
    .variants = FOX_VARIANTS,
    .variants_length = 1,
    .talk_shape_frames = MOUTH_SHAPE_COUNT,
};

Fox *make_fox(SDL_FPoint initial_position) {
  return make_actor(&FOX_SPEC, initial_position);
}

bool fox_load_media(Fox *fox, SDL_Renderer *renderer) {
  return actor_load_media(fox, renderer);
}

void fox_update(Fox *fox, float delta_time) { actor_update(fox, delta_time); }

void fox_render(Fox *fox, SDL_Renderer *renderer) {
  actor_render(fox, renderer);
}

void fox_free(Fox *fox) { actor_free(fox); }

void fox_walk_to(Fox *fox, SDL_FPoint position, void (*on_end)(void)) {
  actor_walk_to(fox, position, on_end);
}

void fox_talk(Fox *fox, const ChunkData *dialog) {
  actor_talk(fox, dialog, NULL);
}

void fox_sit(Fox *fox) { actor_play_state(fox, SITTING); }

void fox_wave(Fox *fox) { actor_play_state(fox, WAVING); }
