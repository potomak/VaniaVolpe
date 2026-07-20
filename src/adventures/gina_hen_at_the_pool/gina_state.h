//
//  gina_state.h
//  Shared, cross-scene progress for "Gina la Gallina in Piscina".
//
//  Unlike Vania's scene-local state, Gina's puzzle spans the pool, the tree and
//  the vine, so the relevant flags live in one struct the scenes share. Reset
//  on the adventure's entry so it is replayable.
//

#ifndef gina_state_h
#define gina_state_h

#include <SDL2_mixer/SDL_mixer.h>
#include <stdbool.h>

#include "hen.h"

// Where the pool float currently is.
typedef enum float_state {
  FLOAT_AT_POOL,       // by the pool, ready to be played with
  FLOAT_STUCK_IN_TREE, // a gust of wind blew it into the tree
  FLOAT_RETRIEVED,     // Carla dropped it back to Gina
} FloatState;

// Which line Gina gives when she examines the float stuck in the tree: each
// look advances one stage, saturating at the last so every later tap repeats
// it (tree.c examine_float).
typedef enum float_look {
  FLOAT_LOOK_CANT_REACH, // first look: she can't reach it
  FLOAT_LOOK_NEED_HELP,  // second look: she needs help
  FLOAT_LOOK_ASK_CARLA,  // third and later: think of asking Carla
} FloatLook;

typedef struct gina_state {
  bool has_sunscreen;     // applied in the sunscreen minigame; gates moving
  bool has_goggles;       // needed before the pool will let her dive
  FloatState float_state; // the floaty puzzle
  bool has_basket;        // given by Carla; needed to collect grapes
  bool has_grapes;        // collected in the grapes minigame; fed to Carla

  // A minigame just finished (#116): the scene it returns to sees the flag,
  // clears it and has Gina explain what the completion means.
  bool announce_sunscreen; // pool: she can now go play in the sun
  bool announce_grapes;    // vine: bring the full basket to Carla

  // Which line the next look at the float stuck in the tree gives (see tree.c
  // examine_float); reset to FLOAT_LOOK_CANT_REACH on entry.
  FloatLook examine_float_look;
} GinaState;

extern GinaState gina_state;

// Reset all progress (called on the adventure's entry scene activation).
void gina_state_reset(void);

#endif /* gina_state_h */
