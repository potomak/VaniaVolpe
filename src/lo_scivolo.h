//
//  lo_scivolo.h
//  The "Lo Scivolo" adventure module: its scene indices and registration.
//

#ifndef lo_scivolo_h
#define lo_scivolo_h

#include "adventure.h"

// Scene indices for this adventure (used by its scenes for transitions).
enum lo_scivolo_scene {
  INTRO,
  PLAYGROUND_ENTRANCE,
  PLAYGROUND,
  OUTRO,
  EXAMPLE,
};

extern Adventure lo_scivolo;

// Populate the adventure's scene table. Call once before set_current_adventure.
void lo_scivolo_register(void);

#endif /* lo_scivolo_h */
