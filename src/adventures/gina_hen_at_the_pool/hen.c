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

Hen *make_hen(SDL_FPoint initial_position) {
  return make_actor(&HEN_SPEC, initial_position);
}

bool hen_load_media(Hen *hen, SDL_Renderer *renderer) {
  return actor_load_media(hen, renderer);
}

void hen_update(Hen *hen, float delta_time) { actor_update(hen, delta_time); }

void hen_render(Hen *hen, SDL_Renderer *renderer) {
  actor_render(hen, renderer);
}

void hen_free(Hen *hen) { actor_free(hen); }

void hen_walk_to(Hen *hen, SDL_FPoint position, void (*on_end)(void)) {
  actor_walk_to(hen, position, on_end);
}

void hen_talk(Hen *hen, const ChunkData *dialog) {
  actor_talk(hen, dialog, NULL);
}
