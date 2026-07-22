//
//  fox.h
//  The fox is just a concrete Actor: an ActorSpec (in fox.c), no code. The
//  framework owns its lifecycle from FOX_SPEC — make/load/free (#141) and
//  tick/draw (#147) — and scenes act on it through the generic actor_* API. A
//  new character (e.g. a chicken) is a new spec, not a new code file.
//

#ifndef fox_h
#define fox_h

#include "actor.h"

extern const ActorSpec FOX_SPEC;

typedef Actor Fox;

// A scene declares `.actor_spec = &FOX_SPEC` and uses the generic actor_* API;
// there are no fox_* wrappers.

#endif /* fox_h */
