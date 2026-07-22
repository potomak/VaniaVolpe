//
//  hen.h
//  Gina the hen: a concrete Actor — an ActorSpec (in hen.c), no code, mirroring
//  fox.{c,h}. The framework owns its lifecycle from HEN_SPEC — make/load/free
//  (#141) and tick/draw (#147) — and scenes act on it through the generic
//  actor_* API. A new character is a new spec, not a new code file.
//

#ifndef hen_h
#define hen_h

#include "actor.h"

extern const ActorSpec HEN_SPEC;

typedef Actor Hen;

// A scene declares `.actor_spec = &HEN_SPEC` and uses the generic actor_* API;
// there are no hen_* wrappers.

#endif /* hen_h */
