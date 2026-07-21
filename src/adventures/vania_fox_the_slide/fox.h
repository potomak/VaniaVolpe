//
//  fox.h
//  The fox is now just a concrete Actor: a spec (in fox.c) plus thin,
//  named wrappers over the generic actor_* API. A new character (e.g. a
//  chicken) is a new spec, not a new code file.
//

#ifndef fox_h
#define fox_h

#include "actor.h"

extern const ActorSpec FOX_SPEC;

typedef Actor Fox;

// No make/load/free wrappers: the framework owns the actor's lifecycle from
// FOX_SPEC (#141). A scene declares `.actor_spec = &FOX_SPEC`.

void fox_update(Fox *fox, float delta_time);

void fox_render(Fox *fox, SDL_Renderer *renderer);

void fox_walk_to(Fox *fox, SDL_FPoint position, void (*on_end)(void));

void fox_sit(Fox *fox);

void fox_wave(Fox *fox);

#endif /* fox_h */
