//
//  adventure.c
//  Adventure-level lifecycle: prepare/teardown for a whole adventure by
//  delegating to each of its scenes. Keeping the per-scene loop here (instead
//  of in game.c) means the game layer drives adventures without depending on
//  how a scene loads or frees its images and sound chunks.
//

#include "adventure.h"

#include "actor.h"
#include "asset.h"
#include "scene.h"

void adventure_init(const Adventure *adventure) {
  for (int i = 0; i < adventure->scenes_length; i++) {
    Scene *scene = &adventure->scenes[i];
    // Make the scene's declarative animations before its init, so init can hand
    // one to a hotspot's active_anim. A no-op for scenes that make their own.
    make_scene_animations(scene);
    // The framework owns the actor's lifecycle when the scene declares a spec:
    // make it before init so init (hotspots, camera_init) can reference it.
    // Scenes with no actor leave actor_spec NULL.
    if (scene->actor_spec != NULL) {
      // Each scene owns its own actor instance, so the scene's depth ramp is
      // simply that actor's ramp for its whole life.
      *scene->actor =
          make_actor(scene->actor_spec, scene->actor_start, scene->scale_ramp);
    }
    scene->init();
  }
}

bool adventure_load_media(const Adventure *adventure, SDL_Renderer *renderer) {
  // Resolve this adventure's assets from its own directory.
  asset_set_root(adventure->assets_root);

  // The shared banks: loaded once for the whole adventure, before its scenes.
  if (!load_chunk_table(adventure->sfx, adventure->sfx_length)) {
    return false;
  }
  if (!load_image_table(renderer, adventure->images,
                        adventure->images_length)) {
    return false;
  }

  for (int i = 0; i < adventure->scenes_length; i++) {
    Scene *scene = &adventure->scenes[i];
    // Load the actor's media when the framework owns it.
    if (scene->actor_spec != NULL &&
        !actor_load_media(*scene->actor, renderer)) {
      return false;
    }
    // load_media is optional: the framework loads the actor, so a scene whose
    // only media is its actor can leave it NULL.
    if (scene->load_media != NULL && !scene->load_media(renderer)) {
      return false;
    }
    if (!load_scene_images(scene, renderer)) {
      return false;
    }
    if (!load_scene_planes(scene, renderer)) {
      return false;
    }
    if (!load_scene_chunks(scene)) {
      return false;
    }
    if (!load_scene_animations(scene, renderer)) {
      return false;
    }
    if (!load_scene_music(scene)) {
      return false;
    }
  }
  return true;
}

void adventure_deinit(const Adventure *adventure) {
  for (int i = 0; i < adventure->scenes_length; i++) {
    Scene *scene = &adventure->scenes[i];
    // deinit is optional: the framework frees the actor, so a scene whose only
    // teardown is its actor can leave it NULL.
    if (scene->deinit != NULL) {
      scene->deinit();
    }
    if (scene->actor_spec != NULL) {
      actor_free(*scene->actor);
    }
    free_scene_images(scene);
    free_scene_planes(scene);
    free_scene_chunks(scene);
    free_scene_animations(scene);
    free_scene_music(scene);
  }
  free_chunk_table(adventure->sfx, adventure->sfx_length);
  free_image_table(adventure->images, adventure->images_length);
}
