//
//  the_slide.h
//  The "Lo Scivolo" adventure module: its scene indices and registration.
//

#ifndef the_slide_h
#define the_slide_h

#include "adventure.h"

// Scene indices for this adventure (used by its scenes for transitions).
enum the_slide_scene {
  INTRO,
  PLAYGROUND_ENTRANCE,
  PLAYGROUND,
  OUTRO,
  EXAMPLE,
};

extern Adventure the_slide;

// Populate the adventure's scene table. Call once before set_current_adventure.
void the_slide_register(void);

#endif /* the_slide_h */
