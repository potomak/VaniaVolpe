//
//  hen.c
//  Gina the hen: an ActorSpec plus thin wrappers over the actor_* API.
//

#include "hen.h"

static const ActorAnimSpec HEN_ANIMS[] = {
    {IDLE, "idle.png", "idle.anim", 2, LOOP},
    {WALKING, "walking.png", "walking.anim", 4, LOOP},
    {TALKING, "talking.png", "talking.anim", 3, LOOP},
};

const ActorSpec HEN_SPEC = {
    .id = "hen",
    .assets_dir = "hen",
    .velocity = 200,
    .move_sound_filename = "walking.wav",
    .move_sound_volume = 20,
    .idle_state = IDLE,
    .move_state = WALKING,
    .anims = HEN_ANIMS,
    .anims_length = 3,
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

void hen_talk(Hen *hen, Mix_Chunk *dialog) { actor_talk(hen, dialog); }
