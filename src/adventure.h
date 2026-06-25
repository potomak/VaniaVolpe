//
//  adventure.h
//  An adventure is a self-contained, ordered set of scenes with an entry point.
//  The engine runs whichever adventure is current; adventures are registered by
//  content modules (see vania_fox_the_slide.c), keeping the engine content-agnostic.
//

#ifndef adventure_h
#define adventure_h

#include "scene.h"

typedef struct adventure {
  const char *id;
  const char *title;
  const char *assets_root;  // base dir for this adventure's assets (non-iOS)
  Scene *scenes;     // table of scenes; indices are adventure-local
  int scenes_length;
  int entry_scene;   // index of the scene to start on

  // Optional: called when the adventure is entered (e.g. selected from the hub),
  // before its entry scene's on_scene_active. Use it to reset cross-scene state
  // so the adventure is replayable. May be NULL.
  void (*on_enter)(void);
} Adventure;

#endif /* adventure_h */
