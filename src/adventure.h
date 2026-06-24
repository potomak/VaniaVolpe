//
//  adventure.h
//  An adventure is a self-contained, ordered set of scenes with an entry point.
//  The engine runs whichever adventure is current; adventures are registered by
//  content modules (see lo_scivolo.c), keeping the engine content-agnostic.
//

#ifndef adventure_h
#define adventure_h

#include "scene.h"

typedef struct adventure {
  const char *id;
  const char *title;
  Scene *scenes;     // table of scenes; indices are adventure-local
  int scenes_length;
  int entry_scene;   // index of the scene to start on
} Adventure;

#endif /* adventure_h */
