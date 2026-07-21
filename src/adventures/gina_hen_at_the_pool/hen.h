//
//  hen.h
//  Gina the hen: a concrete Actor (a spec in hen.c plus thin wrappers over the
//  generic actor_* API), mirroring fox.{c,h}. A new character is a new spec,
//  not a new code file.
//

#ifndef hen_h
#define hen_h

#include "actor.h"

extern const ActorSpec HEN_SPEC;

typedef Actor Hen;

// No make/load/free wrappers: the framework owns the actor's lifecycle from
// HEN_SPEC (#141). A scene declares `.actor_spec = &HEN_SPEC`.

void hen_update(Hen *hen, float delta_time);

void hen_render(Hen *hen, SDL_Renderer *renderer);

void hen_walk_to(Hen *hen, SDL_FPoint position, void (*on_end)(void));

void hen_talk(Hen *hen, const ChunkData *dialog);

#endif /* hen_h */
