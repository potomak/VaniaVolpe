//
//  scene.c
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/22/25.
//

#include "scene.h"

// Clamp a value to the inclusive range [lo, hi].
static int clampi(int v, int lo, int hi) {
  if (v < lo) {
    return lo;
  }
  if (v > hi) {
    return hi;
  }
  return v;
}

SDL_FPoint nearest_walkable_point(SDL_Point click, const SDL_Rect *walkables,
                                  int walkables_length, SDL_Rect non_walkable) {
  // Find the closest point to the click that sits inside one of the walkable
  // rects, remembering which rect won so we can keep the result inside it.
  SDL_Point best = {click.x, click.y};
  SDL_Rect best_rect = {0, 0, 0, 0};
  long best_dist2 = -1;
  for (int i = 0; i < walkables_length; i++) {
    SDL_Rect r = walkables[i];
    SDL_Point p = {
        clampi(click.x, r.x, r.x + r.w - 1),
        clampi(click.y, r.y, r.y + r.h - 1),
    };
    long dx = p.x - click.x;
    long dy = p.y - click.y;
    long dist2 = dx * dx + dy * dy;
    if (best_dist2 < 0 || dist2 < best_dist2) {
      best_dist2 = dist2;
      best = p;
      best_rect = r;
    }
  }

  // If the closest point lands inside the non-walkable rect, push it just past
  // the nearest edge, then keep it inside the winning walkable rect.
  if (SDL_PointInRect(&best, &non_walkable)) {
    int left = best.x - non_walkable.x;                      // exit left
    int right = (non_walkable.x + non_walkable.w) - best.x;  // exit right
    int top = best.y - non_walkable.y;                       // exit up
    int bottom = (non_walkable.y + non_walkable.h) - best.y; // exit down
    int min = left;
    if (right < min) {
      min = right;
    }
    if (top < min) {
      min = top;
    }
    if (bottom < min) {
      min = bottom;
    }
    if (min == left) {
      best.x = non_walkable.x - 1;
    } else if (min == right) {
      best.x = non_walkable.x + non_walkable.w;
    } else if (min == top) {
      best.y = non_walkable.y - 1;
    } else {
      best.y = non_walkable.y + non_walkable.h;
    }
    best.x = clampi(best.x, best_rect.x, best_rect.x + best_rect.w - 1);
    best.y = clampi(best.y, best_rect.y, best_rect.y + best_rect.h - 1);
  }

  return (SDL_FPoint){best.x, best.y};
}

bool load_scene_images(Scene scene, SDL_Renderer *renderer) {
  for (int i = 0; i < scene.images_length; i++) {
    if (!load_image(renderer, &scene.images[i])) {
      return false;
    }
  }

  return true;
}

bool load_scene_chunks(Scene scene) {
  for (int i = 0; i < scene.chunks_length; i++) {
    const char *path = asset_path((Asset){
        .filename = scene.chunks[i].filename,
        .directory = scene.chunks[i].directory,
    });
    scene.chunks[i].chunk = Mix_LoadWAV(path);
    if (scene.chunks[i].chunk == NULL) {
      fprintf(stderr, "Failed to load %s! SDL_mixer Error: %s\n", path,
              Mix_GetError());
      return false;
    }
  }

  return true;
}

void free_scene_images(Scene scene) {
  for (int i = 0; i < scene.images_length; i++) {
    free_image_texture(&scene.images[i]);
  }
}

void free_scene_chunks(Scene scene) {
  for (int i = 0; i < scene.chunks_length; i++) {
    Mix_FreeChunk(scene.chunks[i].chunk);
  }
}
