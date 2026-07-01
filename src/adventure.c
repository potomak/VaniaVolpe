//
//  adventure.c
//  Adventure-level lifecycle: prepare/teardown for a whole adventure by
//  delegating to each of its scenes. Keeping the per-scene loop here (instead
//  of in game.c) means the game layer drives adventures without depending on
//  how a scene loads or frees its images and sound chunks.
//

#include "adventure.h"

#include "asset.h"
#include "scene.h"

void adventure_init(const Adventure *adventure) {
  for (int i = 0; i < adventure->scenes_length; i++) {
    adventure->scenes[i].init();
  }
}

bool adventure_load_media(const Adventure *adventure, SDL_Renderer *renderer) {
  // Resolve this adventure's assets from its own directory.
  asset_set_root(adventure->assets_root);

  for (int i = 0; i < adventure->scenes_length; i++) {
    Scene scene = adventure->scenes[i];
    if (!scene.load_media(renderer)) {
      return false;
    }
    if (!load_scene_images(scene, renderer)) {
      return false;
    }
    if (!load_scene_chunks(scene)) {
      return false;
    }
  }
  return true;
}

void adventure_deinit(const Adventure *adventure) {
  for (int i = 0; i < adventure->scenes_length; i++) {
    Scene scene = adventure->scenes[i];
    scene.deinit();
    free_scene_images(scene);
    free_scene_chunks(scene);
    free_scene_animations(scene);
  }
}
