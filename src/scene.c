//
//  scene.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/22/25.
//

#include "scene.h"

int depth_variant_for(const DepthBand *bands, int bands_length, float feet_y) {
  int variant = bands_length > 0 ? bands[0].variant : 0;
  for (int i = 0; i < bands_length; i++) {
    if (feet_y >= (float)bands[i].y_top) {
      variant = bands[i].variant;
    }
  }
  return variant;
}

bool hotspots_handle_click(const Hotspot *hotspots, int hotspots_length,
                           Actor *actor, const WalkGrid *grid, SDL_Point p) {
  for (int i = 0; i < hotspots_length; i++) {
    const Hotspot *hotspot = &hotspots[i];
    if (hotspot->enabled != NULL && !hotspot->enabled()) {
      continue;
    }
    if (!SDL_PointInRect(&p, &hotspot->rect)) {
      continue;
    }
    // A hotspot acts on the tap, walks-then-acts, or both (see Hotspot). At
    // least one callback must be set — a row that does nothing would silently
    // swallow the click.
    SDL_assert(hotspot->on_tap != NULL || hotspot->on_arrive != NULL);
    if (hotspot->on_tap != NULL) {
      hotspot->on_tap();
    }
    if (hotspot->on_arrive != NULL) {
      walk_actor_to(actor, grid, (SDL_FPoint){hotspot->poi.x, hotspot->poi.y},
                    true, hotspot->on_arrive);
    }
    return true;
  }
  return false;
}

void scene_default_process_input(const Scene *scene, SDL_Event *event) {
  // The default is for walking scenes, which always declare their actor.
  SDL_assert(scene->actor != NULL);
  Actor *actor = *scene->actor;

  // Drag & drop (LIVELINESS.md Part 2): a press-drag on the actor picks it up;
  // plain taps fall through to the hotspots, so one the actor stands on still
  // works.
  if (walk_actor_drag_event(actor, scene->walk_grid, event)) {
    return;
  }
  if (event->type != SDL_MOUSEBUTTONDOWN) {
    return;
  }
  // Hit-test the click's own coordinates (#64): a cached motion position can be
  // stale — e.g. a repeated tap with no motion in between while the camera
  // moved. The hotspot table says what each region does; anything else walks
  // the actor toward the click (routed around blocked ground).
  SDL_Point p = {event->button.x, event->button.y};
  if (hotspots_handle_click(scene->hotspots, scene->hotspots_length, actor,
                            scene->walk_grid, p)) {
    return;
  }
  walk_actor_to(actor, scene->walk_grid, (SDL_FPoint){p.x, p.y}, false, NULL);
}

void scene_default_update(const Scene *scene, float delta_time) {
  SDL_assert(scene->actor != NULL);
  actor_update(*scene->actor, delta_time);
}

void scene_default_render(const Scene *scene, SDL_Renderer *renderer) {
  SDL_assert(scene->actor != NULL);
  // The actor draws depth-sorted among the scene's props (#149), or alone when
  // there are none — render_action_layer handles props_length 0 (its props loop
  // runs zero times, never touching a NULL props pointer). scene->actor is the
  // address of the scene's single actor pointer, so it doubles as a one-element
  // actor array.
  render_action_layer(renderer, scene->props, scene->props_length, scene->actor,
                      1);
}

void sync_hotspot_active_anims(const Scene *scene) {
  for (int i = 0; i < scene->hotspots_length; i++) {
    AnimationData *anim = scene->hotspots[i].active_anim;
    if (anim == NULL) {
      continue;
    }
    // Handle each distinct animation once: two hotspots may share one object's
    // boil (e.g. the sunscreen bottle's before/after entries), and only the OR
    // of their enabled states is meaningful.
    bool already_handled = false;
    for (int j = 0; j < i; j++) {
      if (scene->hotspots[j].active_anim == anim) {
        already_handled = true;
        break;
      }
    }
    if (already_handled) {
      continue;
    }
    // Play if any hotspot carrying this animation is enabled.
    bool any_enabled = false;
    for (int j = 0; j < scene->hotspots_length; j++) {
      const Hotspot *h = &scene->hotspots[j];
      if (h->active_anim == anim && (h->enabled == NULL || h->enabled())) {
        any_enabled = true;
        break;
      }
    }
    if (any_enabled) {
      play_animation(anim, NULL); // no-op if already playing
    } else if (anim->is_playing) {
      // Freeze in place. stop_animation would reset the frame and fire the end
      // callback; there is none here, but hold the current frame and skip the
      // callback path explicitly rather than relying on that.
      anim->is_playing = false;
    }
  }
}

void render_hotspot_anims(SDL_Renderer *renderer, const Scene *scene) {
  for (int i = 0; i < scene->hotspots_length; i++) {
    const Hotspot *hotspot = &scene->hotspots[i];
    if (hotspot->active_anim == NULL) {
      continue;
    }
    // Draw each distinct animation once (shared boils use the first carrier's
    // anim_at/anim_visible), mirroring the sync dedup above.
    bool already_drawn = false;
    for (int j = 0; j < i; j++) {
      if (scene->hotspots[j].active_anim == hotspot->active_anim) {
        already_drawn = true;
        break;
      }
    }
    if (already_drawn) {
      continue;
    }
    if (hotspot->anim_visible != NULL && !hotspot->anim_visible()) {
      continue;
    }
    render_animation(renderer, hotspot->active_anim, hotspot->anim_at);
  }
}

int action_layer_order(const Prop *props, int props_length,
                       Actor *const *actors, int actors_length,
                       int *out_order) {
  if (props_length + actors_length <= 0) {
    return 0;
  }
  int keys[props_length + actors_length];
  int count = 0;
  for (int i = 0; i < props_length; i++) {
    if (props[i].visible) {
      keys[count] = props[i].baseline;
      out_order[count++] = i;
    }
  }
  for (int i = 0; i < actors_length; i++) {
    keys[count] = (int)actor_feet_y(actors[i]);
    out_order[count++] = props_length + i;
  }

  // Insertion sort, ascending. Stable, so equal keys keep insertion order:
  // props (inserted first) draw before actors, and props keep their table
  // order among themselves.
  for (int i = 1; i < count; i++) {
    int key = keys[i];
    int order = out_order[i];
    int j = i - 1;
    while (j >= 0 && keys[j] > key) {
      keys[j + 1] = keys[j];
      out_order[j + 1] = out_order[j];
      j--;
    }
    keys[j + 1] = key;
    out_order[j + 1] = order;
  }
  return count;
}

void render_action_layer(SDL_Renderer *renderer, Prop *props, int props_length,
                         Actor **actors, int actors_length) {
  if (props_length + actors_length <= 0) {
    return;
  }
  int order[props_length + actors_length];
  int count =
      action_layer_order(props, props_length, actors, actors_length, order);
  for (int i = 0; i < count; i++) {
    if (order[i] < props_length) {
      Prop *prop = &props[order[i]];
      if (prop->animation != NULL) {
        render_animation(renderer, prop->animation, prop->pos);
      } else {
        render_image(renderer, prop->image, prop->pos);
      }
    } else {
      actor_render(actors[order[i] - props_length], renderer);
    }
  }
}

void render_scene_sprites(SDL_Renderer *renderer, const SceneSprite *sprites,
                          int sprites_length) {
  for (int i = 0; i < sprites_length; i++) {
    const SceneSprite *sprite = &sprites[i];
    if (sprite->visible != NULL && !sprite->visible()) {
      continue;
    }
    if (sprite->animation != NULL) {
      render_animation(renderer, sprite->animation, sprite->at);
    } else {
      render_image(renderer, sprite->image, sprite->at);
    }
  }
}

bool load_scene_images(Scene *scene, SDL_Renderer *renderer) {
  for (int i = 0; i < scene->images_length; i++) {
    if (!load_image(renderer, &scene->images[i])) {
      // Unwind: free the images that did load before this failure.
      for (int j = 0; j < i; j++) {
        free_image_texture(&scene->images[j]);
      }
      return false;
    }
  }

  return true;
}

SDL_Point plane_screen_pos(const Plane *plane, SDL_FPoint camera_pos) {
  return (SDL_Point){plane->origin.x - (int)(camera_pos.x * plane->parallax),
                     plane->origin.y - (int)(camera_pos.y * plane->parallax)};
}

bool plane_covers_view(const Plane *plane, SDL_Point scene_size) {
  float need_w = WINDOW_WIDTH + plane->parallax * (scene_size.x - WINDOW_WIDTH);
  float need_h =
      WINDOW_HEIGHT + plane->parallax * (scene_size.y - WINDOW_HEIGHT);
  return plane->image.width >= (int)need_w &&
         plane->image.height >= (int)need_h;
}

// Load one bank of planes; on any image failure, unwind this bank and return
// false. `check_coverage` warns when a plane doesn't span the whole view —
// meaningful only for background layers, which must be opaque backdrops;
// foreground planes are decorative overlays (a fence, a bush row) with
// intentional transparent gaps, so the rule doesn't apply to them.
static bool load_plane_bank(Plane *planes, int length, SDL_Point scene_size,
                            bool check_coverage, SDL_Renderer *renderer) {
  for (int i = 0; i < length; i++) {
    if (planes[i].parallax < 0.0F) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Plane %s has a negative parallax %.2f",
                   planes[i].image.filename, planes[i].parallax);
      for (int j = 0; j < i; j++) {
        free_image_texture(&planes[j].image);
      }
      return false;
    }
    if (!load_image(renderer, &planes[i].image)) {
      for (int j = 0; j < i; j++) {
        free_image_texture(&planes[j].image);
      }
      return false;
    }
    // A background plane that doesn't cover the view exposes the clear colour
    // at the edges — a load-time diagnostic, not a hard failure.
    if (check_coverage && !plane_covers_view(&planes[i], scene_size)) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Background plane %s (%dx%d, parallax %.2f) does not cover "
                   "the view over a %dx%d scene",
                   planes[i].image.filename, planes[i].image.width,
                   planes[i].image.height, planes[i].parallax, scene_size.x,
                   scene_size.y);
    }
  }
  return true;
}

bool load_scene_planes(Scene *scene, SDL_Renderer *renderer) {
  SDL_Point scene_size = scene->camera != NULL
                             ? scene->camera->scene_size
                             : (SDL_Point){WINDOW_WIDTH, WINDOW_HEIGHT};
  if (!load_plane_bank(scene->bg_planes, scene->bg_planes_length, scene_size,
                       true, renderer)) {
    return false;
  }
  if (!load_plane_bank(scene->fg_planes, scene->fg_planes_length, scene_size,
                       false, renderer)) {
    // Unwind the bg bank that already loaded.
    for (int i = 0; i < scene->bg_planes_length; i++) {
      free_image_texture(&scene->bg_planes[i].image);
    }
    return false;
  }
  return true;
}

void render_scene_planes(SDL_Renderer *renderer, const Plane *planes,
                         int planes_length, const Camera *camera) {
  SDL_FPoint camera_pos = camera != NULL ? camera->pos : (SDL_FPoint){0, 0};
  for (int i = 0; i < planes_length; i++) {
    render_image(renderer, &planes[i].image,
                 plane_screen_pos(&planes[i], camera_pos));
  }
}

void free_scene_planes(Scene *scene) {
  for (int i = 0; i < scene->bg_planes_length; i++) {
    free_image_texture(&scene->bg_planes[i].image);
  }
  for (int i = 0; i < scene->fg_planes_length; i++) {
    free_image_texture(&scene->fg_planes[i].image);
  }
}

// Load the optional dialogue sidecars next to a chunk's WAV (see SPEECH.md).
// Missing files are the normal case (SFX chunks have none).
static void load_chunk_sidecars(ChunkData *chunk) {
  chunk->text = NULL;
  chunk->cues = (MouthCues){NULL, 0};
  chunk->words = (WordTimings){NULL, 0};

  char filename[ASSET_PATH_MAX];
  if (asset_swap_extension(chunk->filename, ".txt", filename,
                           sizeof(filename))) {
    char path[ASSET_PATH_MAX];
    if (asset_try_resolve(
            (Asset){.filename = filename, .directory = chunk->directory}, path,
            sizeof(path))) {
      size_t size = 0;
      char *data = SDL_LoadFile(path, &size);
      if (data != NULL) {
        // Trim to the first line, without the newline.
        size_t end = 0;
        while (end < size && data[end] != '\n' && data[end] != '\r') {
          end++;
        }
        data[end] = '\0';
        chunk->text = SDL_strdup(data);
        SDL_free(data);
      }
    }
  }
  if (asset_swap_extension(chunk->filename, ".cues", filename,
                           sizeof(filename))) {
    lipsync_load((Asset){.filename = filename, .directory = chunk->directory},
                 &chunk->cues);
  }
  if (asset_swap_extension(chunk->filename, ".words", filename,
                           sizeof(filename))) {
    lipsync_load_words(
        (Asset){.filename = filename, .directory = chunk->directory},
        &chunk->words);
  }
}

// Free one chunk's WAV and dialogue sidecars, leaving it safe to free again.
static void free_chunk_data(ChunkData *chunk) {
  if (chunk->chunk != NULL) {
    Mix_FreeChunk(chunk->chunk);
    chunk->chunk = NULL;
  }
  SDL_free(chunk->text);
  chunk->text = NULL;
  lipsync_free(&chunk->cues);
  lipsync_free_words(&chunk->words);
}

bool load_chunk_table(ChunkData *chunks, int length) {
  for (int i = 0; i < length; i++) {
    char path[ASSET_PATH_MAX];
    Asset asset = {
        .filename = chunks[i].filename,
        .directory = chunks[i].directory,
    };
    chunks[i].chunk = NULL;
    // An optional-audio line (a not-yet-recorded dialogue chunk) that has no
    // WAV loads as text-only; every other chunk must resolve and load.
    bool present = true;
    if (chunks[i].optional_audio) {
      present = asset_try_resolve(asset, path, sizeof(path));
    } else {
      asset_resolve(asset, path, sizeof(path));
    }
    if (present) {
      chunks[i].chunk = Mix_LoadWAV(path);
      if (chunks[i].chunk == NULL && !chunks[i].optional_audio) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load %s: %s",
                     path, Mix_GetError());
        // Unwind: free the chunks (and sidecars) that did load before this
        // failure.
        for (int j = 0; j < i; j++) {
          free_chunk_data(&chunks[j]);
        }
        return false;
      }
    }
    // Sidecars (text/cues/words) key off the filename, so a text-only line
    // still gets its subtitle even with no WAV.
    load_chunk_sidecars(&chunks[i]);
  }

  return true;
}

void free_chunk_table(ChunkData *chunks, int length) {
  for (int i = 0; i < length; i++) {
    free_chunk_data(&chunks[i]);
  }
}

bool load_scene_chunks(Scene *scene) {
  return load_chunk_table(scene->chunks, scene->chunks_length);
}

void make_scene_animations(Scene *scene) {
  for (int i = 0; i < scene->anim_specs_length; i++) {
    const SceneAnimSpec *spec = &scene->anim_specs[i];
    AnimationData *anim = make_animation_data(spec->frames, spec->style);
    // Declarative playback config (#150): apply only when set, so a spec that
    // leaves them 0 keeps make_animation_data's defaults.
    if (spec->ms_per_frame > 0) {
      anim->ms_per_frame = spec->ms_per_frame;
    }
    anim->max_loop_count = spec->max_loop_count;
    scene->animations[i] = anim;
  }
}

bool load_scene_animations(Scene *scene, SDL_Renderer *renderer) {
  for (int i = 0; i < scene->anim_specs_length; i++) {
    if (!load_animation(renderer, scene->animations[i],
                        scene->anim_specs[i].sprite,
                        scene->anim_specs[i].data)) {
      return false;
    }
  }
  return true;
}

void update_scene_animations(Scene scene, int now_ms) {
  for (int i = 0; i < scene.animations_length; i++) {
    if (scene.animations[i] != NULL) {
      animation_update(scene.animations[i], now_ms);
    }
  }
}

bool load_scene_music(Scene *scene) {
  scene->music_stream = NULL;
  if (scene->music.filename == NULL) {
    return true;
  }
  char path[ASSET_PATH_MAX];
  asset_resolve(scene->music, path, sizeof(path));
  scene->music_stream = Mix_LoadMUS(path);
  if (scene->music_stream == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load music %s: %s",
                 path, Mix_GetError());
    return false;
  }
  return true;
}

void scene_start_music(const Scene *scene) {
  if (scene->music_stream != NULL) {
    Mix_PlayMusic(scene->music_stream, -1);
  }
}

void scene_stop_music(const Scene *scene) {
  // One music channel plays at a time, so halting on exit is enough; the gate
  // keeps a music-less scene from silencing whatever (nothing) is playing.
  if (scene->music_stream != NULL) {
    Mix_HaltMusic();
  }
}

void free_scene_music(Scene *scene) {
  if (scene->music_stream != NULL) {
    Mix_FreeMusic(scene->music_stream);
    scene->music_stream = NULL;
  }
}

void scene_stop_channel(int channel) {
  if (channel >= 0) {
    Mix_HaltChannel(channel);
  }
}

void free_scene_images(Scene *scene) {
  for (int i = 0; i < scene->images_length; i++) {
    free_image_texture(&scene->images[i]);
  }
}

void free_scene_chunks(Scene *scene) {
  free_chunk_table(scene->chunks, scene->chunks_length);
}

void free_scene_animations(Scene *scene) {
  for (int i = 0; i < scene->animations_length; i++) {
    if (scene->animations[i] != NULL) {
      free_animation(scene->animations[i]);
    }
  }
}
