//
//  scene.h
//  sdlexample
//
//  Created by Giovanni Cappellotto on 1/16/25.
//

#ifndef scene_h
#define scene_h

#include <SDL2/SDL.h>
#include <stdbool.h>

#include "actor.h"
#include "camera.h"
#include "image.h"
#include "sound.h"
#include "walk.h"

// A prop the actor can walk behind or in front of (DEPTH_AND_CAMERA.md
// Phase 1). Props live in scene-local tables and are drawn, y-sorted with the
// actors, by render_action_layer.
typedef struct prop {
  // Exactly one of image / animation is non-NULL. Both point into the scene's
  // existing images/animations tables, so loading/freeing is unchanged.
  ImageData *image;
  AnimationData *animation;
  SDL_Point pos; // top-left, scene coordinates
  // Scene y of the ground-contact line; the actor renders behind the prop
  // while actor_feet_y < baseline.
  int baseline;
  bool visible; // scenes toggle this (e.g. a picked-up item)
} Prop;

// A static scene sprite (SCENES.md milestone 2): an image or animation drawn
// at a fixed scene position, optionally gated by a predicate — the declarative
// form of a scene's background/boil/button draws. The framework draws the
// scene's sprite table (in order, skipping gated-off entries) before the
// scene's own render, which is left to draw only the dynamic action layer (the
// actor, tweened objects, things that follow the actor, overlays on top).
// Scenes build the table in their init like the hotspot table, so an animation
// sprite can reference an object the framework made (SCENES.md milestone 1).
typedef struct scene_sprite {
  // Exactly one of image / animation is non-NULL. image is const to match how
  // scenes alias their backgrounds (const ImageData *).
  const ImageData *image;
  AnimationData *animation;
  SDL_Point at;          // top-left, scene coordinates
  bool (*visible)(void); // NULL = always drawn; else gated like a hotspot
} SceneSprite;

// A background/foreground layer that scrolls at its own parallax factor
// (DEPTH_AND_CAMERA.md Phase 4). Planes are drawn by the engine, not by the
// scene's render, each at origin - camera.pos * parallax — so a distant plane
// (small parallax) barely moves and a foreground plane (parallax > 1) slides
// past faster than the action layer. The scene framework loads and frees a
// plane's image like a scene image.
typedef struct plane {
  ImageData image;
  // 0 = fixed (sky), 1 = scene-locked (the ground the actor walks on),
  // > 1 = nearer than the action layer (foreground; only meaningful for
  // fg_planes). Must be >= 0. Background planes are validated to cover the
  // whole view (load_scene_planes); foreground planes are decorative overlays
  // and may be partial strips.
  float parallax;
  // Scene-coord position of the image's top-left when the camera is at (0,0);
  // usually {0, 0}.
  SDL_Point origin;
} Plane;

// A clickable scene region and what it does — the single source of truth for
// scene interactions (drawn by the debug overlay, dispatched by
// hotspots_handle_click). Declared per scene as data, so a scene's table
// reads like its description: "tapping the shovel digs".
typedef struct hotspot {
  SDL_Rect rect;
  // Gates this row for click dispatch: hotspots_handle_click takes the first
  // enabled row that contains the tap. NULL = always enabled. "Enabled" means
  // the row handles a tap right now — an explanatory "not yet" line still
  // counts, so an always-tappable object (the gate: open it with the key,
  // else say she needs one) leaves this NULL. Set a predicate only to let the
  // tap fall through to the rows below / the default walk when the row should
  // do nothing (a not-yet-dug key). active_anim (below) plays while any row
  // carrying it is enabled, so this gates the boil too.
  bool (*enabled)(void);
  // Where the actor walks before acting (exact goal, so a POI on blocked
  // ground is legal — see walk_actor_to). Ignored when immediate.
  SDL_Point poi;
  // true: fire on_arrive on the click itself, with no walk (navigation
  // arrows, "already done" lines, buttons).
  bool immediate;
  void (*on_arrive)(void);
  // Optional animation played while the hotspot is enabled and frozen while it
  // is gated off (LIVELINESS.md Part 3) — Gina's objects use it for their
  // "boil", so what squiggles on screen is what a tap would hit right now.
  // Borrowed from the scene's animations table (the scene loads, ticks and
  // frees it); NULL = the object stays static. Hotspots sharing one active_anim
  // play it if ANY of them is enabled (e.g. an object with a before/after
  // pair).
  AnimationData *active_anim;
  // Where the framework draws active_anim (SCENES.md milestone 3): the boil is
  // declared once, on the hotspot — the engine both plays it (above) and draws
  // it here, so a scene no longer lists a separate sprite for it. Top-left,
  // scene coordinates. Only meaningful when active_anim is set.
  SDL_Point anim_at;
  // Gates the *draw* of active_anim, independent of enabled (which gates the
  // play): an object may be visible-but-frozen before it becomes tappable, or
  // vanish once consumed. NULL = drawn whenever active_anim is set. Shared
  // active_anims draw once, using the first carrier's anim_at/anim_visible.
  bool (*anim_visible)(void);
} Hotspot;

// A scene animation declared as data (SCENES.md, milestone 1): the framework
// makes it (before the scene's init, so init can wire it into a hotspot's
// active_anim) and loads it (in the media-load pass) from these fields, which
// come straight from the generated asset header — a scene lists
// `<PREFIX>_<DIR>_ANIM_<NAME>_SPEC` entries and drops its make/load loops. The
// made objects land in the scene's own animations[] storage, so ticking and
// freeing are unchanged.
typedef struct scene_anim_spec {
  int frames;
  AnimationPlaybackStyle style;
  Asset sprite; // sprite sheet PNG
  Asset data;   // .anim clip table
} SceneAnimSpec;

// The field order here is deliberate — each `_length` sits next to the array it
// counts so the struct reads like documentation — over packing tightly for
// size. `Scene` is instantiated only ~13 times (one per scene), so the padding
// this costs is irrelevant; silence clang-analyzer's advisory padding check for
// this struct only.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
typedef struct scene {
  void (*init)(void);
  bool (*load_media)(SDL_Renderer *renderer);
  void (*process_input)(SDL_Event *event);
  void (*update)(float delta_time);
  void (*render)(SDL_Renderer *renderer);
  void (*deinit)(void);

  // Called before the scene is set as the current scene
  void (*on_scene_active)(void);
  // Called before the scene is unset as the current scene
  void (*on_scene_inactive)(void);

  // Hotspots in the scene (see Hotspot above)
  Hotspot *hotspots;
  int hotspots_length;

  // Points of interest in the scene
  SDL_Point *pois;
  int pois_length;

  // The scene's actor storage: the address of the scene's own actor pointer
  // (`.actor = &fox`), so it survives the scene struct being copied into the
  // adventure table and tracks the object the framework makes for it. Used for
  // three things: the framework writes the made actor through it (see
  // actor_spec below); the default input handler (scene_default_process_input)
  // drags and walks it when a scene supplies no process_input; and the say_()
  // helpers speak through it. NULL for scenes with no actor (menus, minigames).
  Actor **actor;

  // The actor's spec and start position (#141). When actor_spec is set, the
  // framework owns the actor's whole lifecycle: it makes the actor
  // (make_actor(actor_spec, actor_start)) before the scene's init — so init,
  // the hotspot table and camera_init can reference it — loads its media in the
  // media pass, and frees it on teardown, storing the pointer through `.actor`.
  // A scene then declares its actor instead of writing the make_*/load/free
  // calls by hand. NULL (with `.actor` also NULL) for scenes with no actor.
  const ActorSpec *actor_spec;
  SDL_FPoint actor_start;

  // Walkability grid (see walk.h); NULL for scenes with no player movement.
  // Scenes fill it once in init — from a committed walkable.walk mask when
  // one exists, else from their WalkArea rects (walk_grid_init). The debug
  // overlay shades its non-walkable cells, and its paint mode (see TOOLS.md)
  // edits the grid live, which is why the pointer is mutable.
  WalkGrid *walk_grid;

  // Asset directory the scene's walkable.walk mask lives in (under common/),
  // e.g. "playground"; NULL disables mask loading and the paint-mode save.
  const char *walk_mask_dir;

  // Camera for scenes larger than the window (see camera.h); NULL for static
  // scenes. A scrolling scene owns a static Camera, calls camera_init in its
  // init, and points this at it — the engine does everything else (snap on
  // entry, update after the scene's update, input conversion, render offset).
  Camera *camera;

  // Static sprites (SceneSprite): the framework draws these each frame, in
  // order, before the scene's own render — so the scene's render draws only
  // the dynamic action layer. NULL/0 for scenes that draw everything
  // themselves. Built in the scene's init (like the hotspot table).
  SceneSprite *sprites;
  int sprites_length;

  // Images
  ImageData *images;
  int images_length;

  // Chunks (sound effects)
  ChunkData *chunks;
  int chunks_length;

  // Declarative background music (SCENES.md milestone 4): the manifest asset of
  // a music stream the framework plays on scene entry and halts on exit, so a
  // scene sets `.music` and drops the whole Mix_LoadMUS/PlayMusic/HaltMusic/
  // FreeMusic lifecycle. A zeroed asset ({0}, no filename) means the scene has
  // no music. music_stream is the framework-owned runtime handle (loaded in the
  // media pass, freed on teardown); scenes never touch it.
  Asset music;
  Mix_Music *music_stream;

  // Animations the scene draws itself (buttons, props, …). The framework ticks
  // them each frame and frees them on teardown, so scenes only declare them —
  // they don't call animation_update. (An actor ticks its own animations.)
  // This is the runtime storage; a scene either fills it in its init (legacy)
  // or, when anim_specs is set, lets the framework make and load it.
  AnimationData **animations;
  int animations_length;

  // Declarative animations (SceneAnimSpec, SCENES.md milestone 1): when set,
  // the framework makes these into animations[] before init and loads them in
  // the media pass, so the scene declares them instead of writing the
  // make/load loops. anim_specs_length entries fill animations[0..N); the
  // scene keeps animations/animations_length for ticking and freeing. NULL/0
  // for scenes that still make their own animations.
  const SceneAnimSpec *anim_specs;
  int anim_specs_length;

  // Parallax planes (see Plane), drawn by the engine in array order: bg_planes
  // behind the scene's render (the action layer), fg_planes in front of it.
  // The framework loads and frees their images. NULL/0 for scenes with no
  // planes — every static scene draws its own background in render() as before.
  Plane *bg_planes;
  int bg_planes_length;
  Plane *fg_planes;
  int fg_planes_length;
} Scene;

// One horizontal depth band of a scene's floor (DEPTH_AND_CAMERA.md
// Phase 2): while the actor's feet are at or below y_top (and above the next
// band's y_top), the scene shows this sprite variant. Bands are scene data —
// each scene declares its own table, sorted by ascending y_top with band 0
// starting at 0 — so different scenes map their floors onto an actor's
// variants however their art needs; scenes without bands never switch and
// stay on variant 0.
typedef struct depth_band {
  int y_top;   // scene y where this band starts
  int variant; // sprite set to show (index into the actor's variants)
} DepthBand;

// The variant for the band containing feet_y: the last band whose y_top is
// <= feet_y, or the first band's variant above them all. Scenes that opt in
// call this once per frame in update, before the actor's own update:
//   actor_set_variant(fox, depth_variant_for(BANDS, LEN(BANDS),
//                                            actor_feet_y(fox)));
int depth_variant_for(const DepthBand *bands, int bands_length, float feet_y);

// Back-to-front draw order of the action layer: visible props and actors
// sorted by ascending baseline / feet y. On ties props draw first, so an
// actor standing exactly on a prop's ground line renders in front of it.
// out_order must hold props_length + actors_length entries and receives
// drawable indices: 0..props_length-1 name props, props_length + i names
// actors[i]. Returns how many entries were written. Split from
// render_action_layer so tests can assert the ordering without a renderer.
int action_layer_order(const Prop *props, int props_length,
                       Actor *const *actors, int actors_length, int *out_order);

// Draw visible props and actors in y-order (see action_layer_order). Scenes
// call this once instead of hand-ordering "props, then the actor last";
// backgrounds and draws that never overlap an actor stay manual.
void render_action_layer(SDL_Renderer *renderer, Prop *props, int props_length,
                         Actor **actors, int actors_length);

// Dispatch a click at p (scene coordinates) against a hotspot table: the
// first enabled hotspot containing p wins — the actor walks to its poi and
// on_arrive fires on arrival (or immediately, for immediate hotspots).
// Returns false when no hotspot claimed the click, so the scene can fall
// through to its default (usually: walk to the click). Scenes with only
// immediate hotspots may pass actor/grid as NULL.
bool hotspots_handle_click(const Hotspot *hotspots, int hotspots_length,
                           Actor *actor, const WalkGrid *grid, SDL_Point p);

// The default scene input handler (SCENES.md milestone 5): the drag + hit-test
// + walk interaction every walking scene shared. The engine runs it for a scene
// that supplies no process_input of its own — a press-drag on the actor picks
// it up (walk_actor_drag_event), a plain tap is dispatched to the hotspot
// table, and anything else walks the actor toward the click. Uses the scene's
// `.actor` (which must be set), `.walk_grid` and hotspot table; a scene needing
// anything more (an input lock, a win check) still writes its own process_input
// instead.
void scene_default_process_input(const Scene *scene, SDL_Event *event);

// Sync every hotspot's active_anim to its enabled state (LIVELINESS.md
// Part 3): it plays while any hotspot carrying it is enabled and freezes
// (current frame held, no callback) otherwise. Called once per frame by the
// engine, before the scene's animations are ticked. Hotspots with no
// active_anim are skipped; one shared by several hotspots is handled once,
// ORing their states.
void sync_hotspot_active_anims(const Scene *scene);

// Draw every hotspot's active_anim at its anim_at (SCENES.md milestone 3), each
// distinct animation once, skipping any gated off by anim_visible. Called by
// the engine in the sprite layer (after render_scene_sprites, before the
// scene's render), so a scene declares its boils on the hotspots instead of
// listing them as separate sprites.
void render_hotspot_anims(SDL_Renderer *renderer, const Scene *scene);

// Draw a scene's static sprite table (SceneSprite, SCENES.md milestone 2): each
// visible entry in order, an image or an animation at its scene position.
// Called by the engine before the scene's render, inside the camera offset.
void render_scene_sprites(SDL_Renderer *renderer, const SceneSprite *sprites,
                          int sprites_length);

bool load_scene_images(Scene *scene, SDL_Renderer *renderer);

// Screen position of a plane given a camera position (or {0,0} for a static
// scene): origin - camera * parallax, cast to int. Split out so the parallax
// math can be tested without a renderer.
SDL_Point plane_screen_pos(const Plane *plane, SDL_FPoint camera_pos);

// Does the plane's image cover the whole window across the camera's travel?
// image dimension >= WINDOW + parallax * (scene - WINDOW), per axis. Used by
// load_scene_planes to warn about a plane that would expose the background.
bool plane_covers_view(const Plane *plane, SDL_Point scene_size);

// Load (and validate the coverage of) every bg/fg plane's image. Unwinds on
// failure like load_scene_images. camera-less scenes are treated as
// window-sized for the coverage check.
bool load_scene_planes(Scene *scene, SDL_Renderer *renderer);

// Draw a plane table in array order, each shifted by its own parallax offset
// (the render offset must be {0,0} — planes carry their own). Called by the
// engine, not by scenes.
void render_scene_planes(SDL_Renderer *renderer, const Plane *planes,
                         int planes_length, const Camera *camera);

void free_scene_planes(Scene *scene);

bool load_scene_chunks(Scene *scene);

// Load / free a bare ChunkData table (WAV + dialogue sidecars). The scene and
// the adventure-wide SFX bank (adventure.h) share this so both load the same
// way; load unwinds already-loaded chunks on the first failure.
bool load_chunk_table(ChunkData *chunks, int length);
void free_chunk_table(ChunkData *chunks, int length);

// Make the scene's declarative animations (anim_specs) into its animations[]
// storage. Called by the engine before the scene's init (so init can hand a
// made animation to a hotspot's active_anim); a no-op when anim_specs is
// unset. make_animation_data needs no renderer, so this runs at init time.
void make_scene_animations(Scene *scene);

// Load the textures for the scene's declarative animations (anim_specs).
// Called by the engine in the media-load pass, after make_scene_animations.
// A no-op when anim_specs is unset. False on the first failed load.
bool load_scene_animations(Scene *scene, SDL_Renderer *renderer);

// Advance the scene's declared animations (called once per frame by the
// engine).
void update_scene_animations(Scene scene, int now_ms);

// Load a scene's declarative background music (.music) into its music_stream,
// or a no-op returning true when the scene declared none. Called by the engine
// in the media-load pass.
bool load_scene_music(Scene *scene);

// Play / halt a scene's declarative music around scene activation (called by
// the engine in set_active_scene / adventure_switch_to). Each is a no-op when
// the scene declared no music, so both switch paths call them unconditionally.
void scene_start_music(const Scene *scene);
void scene_stop_music(const Scene *scene);

// Free a scene's music stream (called by the engine on teardown).
void free_scene_music(Scene *scene);

// Stop whatever is playing on a channel previously returned by a play_<name>()
// helper (e.g. to retrigger a looping SFX from the top). A negative channel is
// ignored, so an un-started handle is safe to pass.
void scene_stop_channel(int channel);

void free_scene_images(Scene *scene);

void free_scene_chunks(Scene *scene);

void free_scene_animations(Scene *scene);

#endif /* scene_h */
