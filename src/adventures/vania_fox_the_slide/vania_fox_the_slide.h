//
//  vania_fox_the_slide.h
//  The Vania Fox - The Slide adventure module: its scene indices and
//  registration.
//

#ifndef vania_fox_the_slide_h
#define vania_fox_the_slide_h

#include "adventure.h"

// Scene indices for this adventure (used by its scenes for transitions).
enum vania_fox_the_slide_scene {
  INTRO,
  PLAYGROUND_ENTRANCE,
  PLAYGROUND,
  OUTRO,
  EXAMPLE,
};

extern Adventure vania_fox_the_slide;

// Populate the adventure's scene table. Call once before set_current_adventure.
void vania_fox_the_slide_register(void);

#endif /* vania_fox_the_slide_h */
