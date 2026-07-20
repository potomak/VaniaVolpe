# Declarative scenes (proposed)

A design for lighter scenes: a scene declares the assets it uses, where things
are, what's present, and the interactions — and the framework owns the
plumbing (making and loading animations, drawing the static layer, playing
sounds, the input default). The spec the implementation follows, the way
`LIVELINESS.md` and `ASSETS.md` were written before their code. Tracked in the
backlog (#129); **milestones 1–3 are shipped and milestone 4 is partly
shipped** — see *Migration plan* below.

## The problem

A scene today mixes two very different things: its **essence** (where objects
are, what's on screen, the interactions, the state changes) and a large amount
of **ceremony** that is the same in every scene. Grounding this in the current
`pool.c` / `tree.c` / `vine.c`, the ceremony is:

| Ceremony | Where | Example |
|---|---|---|
| Make animation objects | `init` | `celebration = animations[IDX] = make_animation_data(FRAMES, STYLE);` |
| Load animations | `load_media` | `load_animation(renderer, celebration, SPRITE_ASSET, DATA_ASSET) && …` |
| Pointer aliases | file scope | `static const ImageData *background = &images[IDX];` |
| Draw each thing | `render` | `render_image(renderer, background, POS);` … |
| Play a sound | handlers | `Mix_PlayChannel(-1, chime_sound->chunk, 0);` |
| Music lifecycle | load/active/deinit | `asset_resolve` → `Mix_LoadMUS` → `Mix_PlayMusic` → `Mix_FreeMusic` |
| Input plumbing | `process_input` | the identical drag + hit-test + walk switch |

None of that is what the scene *is*. All of it is derivable from a declaration
plus the asset manifest we already generate (`ASSETS.md`).

## The framework already owns half of this

The engine has been moving this way already. Per frame, `game.c` runs
`sync_hotspot_active_anims`, `update_scene_animations`, and both parallax
passes automatically; on load it runs `load_scene_images`, `load_scene_chunks`,
`load_scene_planes`. So **images, chunks and planes are already
declare-and-forget.** The gaps are: animations (the scene still *makes* and
*loads* them), the render list, sound/music playback, and the input default.

## The precedent to generalize

Three things in the tree are already the target shape — this is not a new
paradigm, it's finishing one that exists:

- **`Hotspot`** — a table of `{rect, enabled, poi, on_arrive, active_anim}`
  the framework dispatches. "Tapping the shovel digs" reads off the table.
- **`Plane`** — declared, loaded, drawn, freed entirely by the engine.
- **`ActorSpec`** — the strongest: the fox and hen are almost pure data
  (`ActorSpec` + an `ActorAnimSpec` list). The scene calls `make_hen` /
  `hen_update` / `hen_render` and never touches their four sheets' make/load/
  tick/free.

The through-line: **an `ActorSpec` for scene objects.** Everywhere a scene
hand-makes and hand-draws an animation, it does manually what the actor engine
does declaratively.

## The model

A scene becomes: **assets declared + placements + hotspots + behavior.** The
framework owns load / tick / render / free / dispatch. Concretely a scene
supplies data tables and a handful of behavior functions, and the engine
provides everything else.

### Assets and placements

A `SceneSprite` names a visual (an image or animation, by its manifest id) and
where it sits, with an optional visibility predicate — the same shape as
`Hotspot.enabled`:

```c
typedef struct scene_sprite {
  const char *visual;       // manifest id; image or animation
  SDL_Point at;             // top-left, scene coords
  bool (*visible)(void);    // NULL = always; else gated like a hotspot
} SceneSprite;
```

The framework makes and loads every declared animation (their frame count,
style and asset paths are already in the generated header), draws the sprite
list each frame in order, ticks the animations, and frees everything. The boil
case collapses here: a boil is one animation, placed at a spot, playing while
its object is tappable — today that is spread across the `animations` table, a
`Hotspot.active_anim` pointer, and a gated `render_animation` call. A hotspot
that carries its own `anim` + `at` unifies the three.

### Behavior with a compile-time-safe action API

Behavior functions (a hotspot's `on_arrive`, a predicate) act through
**generated, compile-checked helpers** rather than the low-level SDL_mixer /
render API. The manifest already emits compile-checked keys
(`GINA_POOL_CHUNK_WIND` is a `#define`), so the safety exists — the point is to
wrap the verbose call while keeping it.

The generator emits a thin `static inline` per sound/line, over a framework
primitive keyed by the generated index:

```c
static inline void play_chime(void) { scene_play(GINA_POOL_CHUNK_CHIME); }
```

so call sites read `play_chime()` — and `play_chmie()` is a compile error. This
matches the existing `fox_sit()` / `fox_wave()` named-action idiom. Because the
helper touches only a generated macro and a framework function (not the
scene's file-scope tables), it composes cleanly and sidesteps the "the chime
lives in another dir" problem #118 hit — the scene declares the chime among
its assets and the index is generated.

- **Sound effects:** `play_<name>()`.
- **Dialogue:** `say_<line>()` — a voice line's *text* already lives in the
  manifest (`"text"`), so the generated helper plays the voice chunk **and**
  passes the subtitle string. The asset, the subtitle and the call site
  collapse into one manifest entry.
- **Music:** a **declarative field** (`.music = "playground"`), no call at
  all — every scene already plays its music on enter and halts on exit, so the
  framework does the whole `asset_resolve`/`LoadMUS`/`PlayMusic`/`Free`
  lifecycle.

Why generated functions over an enum + one `scene_play(SFX_CHIME)`: both fail
at compile time, but the nullary named form reads better, autocompletes to the
scene's vocabulary, matches the actor helpers, and folds the subtitle text into
the dialogue case. The enum is the fallback if the generated-function surface
proves too large.

### Input default

The drag + hit-test + walk switch is byte-identical across the walking scenes.
It becomes the framework default; a scene supplies a `process_input` only for
something special (the pool's dive input-lock). Most scenes then declare no
input handler at all.

### Declaring a scene

A `DECLARE_SCENE` macro binds the tables and the optional behavior hooks,
replacing the hand-written `Scene` struct literal:

```c
DECLARE_SCENE(vine,
    .sprites = vine_sprites, .hotspots = vine_hotspots,
    .actor = &HEN_SPEC, .walk = VINE_AREA, .music = NULL);
```

## Before / after — vine.c

vine.c is ~176 lines today. Its essence is: one background, one grapes boil
tied to the grapes hotspot, one voice line, three hotspots. Declaratively
(illustrative, not final):

```c
static SceneSprite vine_sprites[] = {
    { .visual = "background", .at = {0, 0} },
};
static Hotspot vine_hotspots[] = {
    { .rect = GRAPES_HOTSPOT, .poi = GRAPES_POI, .on_arrive = pick_grapes,
      .anim = "grapes_boil", .at = GRAPES_AT },
    { .rect = TREE_NAV, .immediate = true, .on_arrive = go_to_tree },
    { .rect = POOL_NAV, .immediate = true, .on_arrive = go_to_pool },
};
DECLARE_SCENE(vine, .sprites = vine_sprites, .hotspots = vine_hotspots,
                    .actor = &HEN_SPEC, .walk = VINE_AREA);

static void pick_grapes(void) {
    if (gina_state.has_grapes) { say_already_grapes(); return; }
    if (gina_state.has_basket) { set_active_scene(GRAPES_MINIGAME); return; }
    say_nothing_to_pick();
}
```

The make/load/tick/free of the boil, the render loop, the mouse-switch
`process_input`, the `Scene` struct wiring and the pointer aliases are all
gone. What remains is where things are, what's present, and the one branch of
behavior — the four things a scene *is*.

## What stays imperative — the escape hatch

The danger is forcing the 20% that isn't declarative. These stay hand-written
and must not fight the framework:

- the dive arc and float tweens (a custom `update` + a dynamic draw)
- the sunscreen paint grid, the grape-fall states
- the `try_dive` state machine, the walk-grid rebuild on sunscreen
- Gina y-sorted with props via `render_action_layer`

So every layer keeps a clean opt-out: a scene may still supply
`update`/`render`/`process_input`, and they run **alongside** the declared
data, not instead of it — the framework draws the declared sprite list, then
calls the scene's `render` for the dynamic extras, exactly as it already draws
planes around the scene's render. Getting this composition right is the first
thing to prove.

## Build-time validation

Every id a scene references (`.visual`, `.anim`, `play_<name>`, `.music`) must
resolve to a manifest entry. Like the manifest generator's frame-count and
file-existence checks (`ASSETS.md`), an unknown id is a **build error**, never
a silent runtime miss — the generated helpers give this for free (an undefined
`play_typo` won't link), and the sprite/`.visual` ids are validated by the
generator.

## Relation to the manifest and the platform vision

This is the second half of the declarative-assets work (#105). That project
made assets *declared* and generates the asset *tables*; this generates the
*bindings* (handles + `play_<name>()` helpers) on top of them, the same
technique one layer up. It also feeds the Tiny Adventures vision (#104): the
lighter and more data-shaped a scene is, the closer an "adventure" gets to
being content rather than code — the precondition for cartridges / paid packs.

## Migration plan

Land it the way #105 did — incrementally, one layer and one reference scene at
a time, never a big-bang rewrite:

1. **Auto-make + auto-load declared animations.** ✅ **Shipped.** A scene lists
   `SceneAnimSpec` entries (`<PREFIX>_<DIR>_ANIM_<NAME>_SPEC`, generated) and
   drops its `make_animation_data`/`load_animation` loops; the framework makes
   them into the scene's `animations[]` before `init` (so a hotspot's
   `active_anim` can point at one) and loads them in the media pass
   (`make_scene_animations`/`load_scene_animations` in `scene.c`, wired in
   `adventure.c`). No behavior change — the playthrough is unchanged. `vine.c`
   is the reference; every animation scene of both adventures migrated
   (pool, tree, both minigames, intro, playground_entrance). Ticking and
   freeing were already the framework's job and are untouched. The named
   aliases (`grapes_boil = animations[IDX];`) stay until Layer 2 makes render
   declarative.
2. **The `SceneSprite` render list.** ✅ **Shipped.** A scene declares a
   `SceneSprite` table (`{image|animation, at, visible}`, built in its init like
   the hotspot table); the framework draws it (in order, skipping gated-off
   entries) before the scene's `render`, which shrinks to just the dynamic
   action layer — the actor, tweened objects, things that follow the actor, and
   overlays that sit on top (`render_scene_sprites` in `scene.c`, called from
   `game.c` inside the camera offset). Gated sprites use `visible` predicates
   (the boils reuse or mirror their hotspot gates). Every content scene of both
   adventures migrated; the depth demo stays inline. Verified visually (the
   playthrough only asserts dialogue, so a browser screenshot per adventure
   confirmed the sprites render).
3. **The boil collapse.** ✅ **Shipped.** A boil now lives in one place: on its
   hotspot. `Hotspot.active_anim` gained `anim_at` (draw position) and
   `anim_visible` (a draw gate, independent of `enabled` which gates the play),
   and `render_hotspot_anims` draws each distinct active_anim once in the sprite
   layer. So a scene no longer lists a boil as a separate sprite — the tappable
   thing and its squiggle are declared together. Migrated the boil scenes
   (pool, tree, vine); their sprite tables shrink to the backdrop (+ water).
4. **The action API.** 🚧 **Partly shipped.** Two halves:
   - **`.music` declarative field.** ✅ **Shipped.** A scene sets
     `.music = <PREFIX>_MUSIC_CHUNK_<NAME>_ASSET_INIT` (a generated, so
     build-time-validated, `Asset` initializer) and drops the whole
     `Mix_LoadMUS`/`PlayMusic`/`HaltMusic`/`FreeMusic` lifecycle: the framework
     loads it in the media pass, plays it on scene entry and halts it on exit
     (`scene_start_music`/`scene_stop_music`, wired into `game.c`'s
     `set_active_scene`/`adventure_switch_to`), and frees it on teardown. Both
     Vania scenes with music migrated (intro/outro share the theme, the two
     playground scenes share theirs); Gina has no music. Zero scenes keep a
     `Mix_Music *`.
   - **Sound effects: generated `play_<name>()` over a shared bank.**
     ✅ **Shipped.** Each adventure declares its sound effects with `"sfx": true`
     in the manifest; the generator collects them into an **adventure-wide SFX
     bank** (`<PREFIX>_SFX_INIT` + a `<PREFIX>_SFX_<NAME>` index per sound) and
     emits a `static inline play_<name>(void)` for each, over `sfx_play(index)`
     (`game.c`, playing the current adventure's bank). The bank lives on the
     `Adventure` (`.sfx`), loaded once in the media pass and freed on teardown —
     so the shared completion chime is loaded a single time, not per scene. A
     scene now **carries no sound effects at all**: its chunk table shrinks to
     just its dialogue voice lines (the two minigames drop the table entirely),
     and a handler simply calls `play_chime()` / `play_wind()`. The index macro
     keeps it compile-time-checked (`play_chmie()` won't link); every SFX call
     site in both adventures moved onto the helpers and **no scene includes
     `<SDL2_mixer/...>` any more.**
   - **Dialogue: `say_<line>()`** — still deferred. It needs the scene to carry
     its actor as data (the `.actor` field milestone 5 introduces) plus per-line
     voice WAVs and manifest text Gina's shared-placeholder model doesn't have
     yet. Until then dialogue keeps `gina_say` / `fox_talk` over the scene's own
     voice chunks. Tracked as a follow-up.
5. **The input default.**

Migrate both content adventures at each meaningful step to keep the
abstractions honest; leave the depth demo inline as the "before" counter-
example, as the manifest migration did.

## Risks

- **Complexity moves into the engine and the generator.** Worth it at 9 scenes
  and growing; keep each layer independently useful so the trade is visible.
- **Debuggability.** A data-driven bug (wrong predicate, wrong id) is harder to
  step through than an explicit call — build-time validation is the mitigation,
  so failures surface at compile time, not at runtime.
- **Escape-hatch discipline.** If the declarative path can't express something
  and the opt-out is awkward, scenes route around the framework and you get the
  worst of both. The composition model is the thing to get right first.
- **Churn.** This is a second pass over files just migrated for the manifest —
  hence the incremental plan, each step shippable on its own.
- **The "framework that's really one game" trap.** These shapes are drawn from
  Gina and Vania; migrating both keeps them honest.
