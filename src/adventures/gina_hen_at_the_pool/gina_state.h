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

typedef struct gina_state {
  bool has_sunscreen;     // applied in the sunscreen minigame; gates moving
  bool has_goggles;       // needed before the pool will let her dive
  FloatState float_state; // the floaty puzzle
  bool has_basket;        // given by Carla; needed to collect grapes
  bool has_grapes;        // collected in the grapes minigame; fed to Carla

  // Repeated-tap dialogue counter for the float stuck up the tree.
  int examine_float_count;
} GinaState;

extern GinaState gina_state;

// Reset all progress (called on the adventure's entry scene activation).
void gina_state_reset(void);

// Speak a line: play the (silent placeholder) chunk through the actor and also
// log the text to stdout, so the audio-only flow is testable without art.
void gina_say(Hen *gina, const char *line, Mix_Chunk *voice);

#endif /* gina_state_h */
