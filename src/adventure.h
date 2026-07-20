//
//  adventure.h
//  An adventure is a self-contained, ordered set of scenes with an entry point.
//  The engine runs whichever adventure is current; adventures are registered by
//  content modules (see vania_fox_the_slide.c), keeping the engine
//  content-agnostic.
//

#ifndef adventure_h
#define adventure_h

#include "scene.h"

typedef struct adventure {
  const char *id;
  const char *title;
  const char *assets_root; // base dir for this adventure's assets (non-iOS)
  Scene *scenes;           // table of scenes; indices are adventure-local
  int scenes_length;
  int entry_scene; // index of the scene to start on

  // Optional: called when the adventure is entered (e.g. selected from the
  // hub), before its entry scene's on_scene_active. Use it to reset cross-scene
  // state so the adventure is replayable. May be NULL.
  void (*on_enter)(void);

  // The adventure's shared sound-effect bank (SCENES.md milestone 4): every SFX
  // any of its scenes triggers, loaded once here rather than per scene. A scene
  // plays one by its generated play_<name>() helper, which calls sfx_play with
  // the sound's index into this table (the manifest's `sfx: true` entries, via
  // <PREFIX>_SFX_INIT). The framework loads it in the media pass and frees it
  // on teardown. NULL/0 for an adventure with no sound effects (e.g. the hub).
  ChunkData *sfx;
  int sfx_length;
} Adventure;

// Lifecycle for a whole adventure: each delegates to every one of its scenes,
// so the game can drive an adventure without knowing how a scene loads or frees
// its media. The game iterates its registered adventures and calls these.

// Initialize every scene in the adventure.
void adventure_init(const Adventure *adventure);

// Load every scene's media (the adventure's own assets root is selected first).
// Returns false as soon as any scene fails to load.
bool adventure_load_media(const Adventure *adventure, SDL_Renderer *renderer);

// Deinitialize every scene and free its loaded media.
void adventure_deinit(const Adventure *adventure);

#endif /* adventure_h */
