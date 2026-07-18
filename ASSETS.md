# Declarative adventure assets

How an adventure's assets are declared once, in a single manifest, and consumed
by everything that needs them: the game, the art pipeline, and the cost
estimator. This is the design behind issue #105; the pool scene is the
reference migration, the remaining scenes are tracked in #111.

## The problem

Before this, every scene hard-coded its asset tables inline — filenames,
directories, frame counts, play styles — and the art pipeline kept a *second*
list of the same assets in a pipeline manifest (what still needs authoring,
at what size, with which frame counts). Two lists, one truth: a frame count
changed in a `.anim` file could silently disagree with the scene that plays
it, and the estimator counted from a file the game never read.

## One manifest per adventure

`src/adventures/<adv>/assets/index.json` is the adventure's **asset
manifest** — the single source of truth read by:

- `tools/gen_asset_tasks.py` — the *Assets to author* page + `_inbox/`
  drop-boxes (authoring entries only);
- `tools/consolidate_assets.py` — folds uploads into real asset files;
- `src/emscripten/cost_estimate.html` — asset-count defaults;
- `tools/gen_asset_decls.py` — **generates the C declarations the game
  compiles against** (this document's subject).

### Schema

Top-level:

```json
{
  "adventure": "gina_hen_at_the_pool",
  "title": "Gina la Gallina in Piscina",
  "prefix": "GINA",
  "assets_root": "src/adventures/gina_hen_at_the_pool/assets",
  "reference_locale": "it_IT",
  "assets": [ ... ]
}
```

`prefix` names the generated macros (`GINA_...`); it defaults to the
upper-cased adventure id.

Each entry in `assets`:

| field | meaning |
|---|---|
| `group` | display grouping on the *Assets to author* page (consecutive entries with the same group render together) |
| `type` | `animation`, `image` (matching the C `ImageData`), `audio`, or `voice` (a spoken line) |
| `dir` | where the asset lives (see *directory rules* below) |
| `name` | base filename, no extension |
| `frames` | animations: frame count — **validated against the committed `.anim`** |
| `style` | animations: `loop` (default) or `one_shot` |
| `ms_per_frame` | animations: frame duration override; `0`/absent = engine default |
| `size` | authoring hint (`WxH`) shown to the artist |
| `text` | voice: the line to record |
| `localized` | the file lives under a locale dir, not `common/` |
| `task` | `true` = **still to author**: gets a drop-box, appears on the page and in the estimate (the game meanwhile shows a placeholder). Default `false`: a finished/runtime-managed asset |
| `runtime` | `false` = **authoring-only**: an artist makes it, but the game shows a derived asset instead (e.g. the pool stills, which render as boils) |
| `description` | one sentence of context, shown to the artist |

By default an entry is a finished runtime asset. `task: true` marks the ones
still to author; `runtime: false` marks the ones the game doesn't load
directly.

### Directory rules

Entries still to author (`task: true`) use **layered** dirs (`common/pool` —
the layer the finished file is committed to); the rest use **resolver** dirs
(`pool` — what `asset_resolve()` takes, since the layer is its job to pick).
The generator maps both to the same runtime dir by stripping the `common/`
prefix, so the emitted macros always hold resolver dirs.

`voice` entries are authoring-only for now (scenes still play a shared
per-scene `voice.wav` placeholder, declared as a runtime-only `audio` entry);
they'll gain a runtime shape when per-line WAVs land.

## The generated header

C99 can't read JSON without either a parser dependency or hand-rolled parsing,
and a toddler game has no business parsing manifests at boot. So — exactly
like the playthrough scripts (`tools/gen_playtest.py`) — the JSON is consumed
at **build time**: `make` runs `tools/gen_asset_decls.py`, which writes
`build/gen/<adv>_assets.h`, and migrated scenes `#include` it. Zero runtime
parsing, zero new dependencies, and the compiler enforces agreement.

Per runtime dir the header defines (pool shown; names are
`<PREFIX>_<DIR>_...`):

```c
GINA_POOL_IMAGE_BACKGROUND            // index into the images table
GINA_POOL_IMAGES_COUNT
GINA_POOL_IMAGES_INIT                 // {{NULL, "background.png", "pool", 0, 0}, ...}

GINA_POOL_CHUNK_WIND                  // index into the chunks table
GINA_POOL_CHUNKS_COUNT
GINA_POOL_CHUNKS_INIT

GINA_POOL_ANIM_SUNSCREEN_BOIL         // index into the animations table
GINA_POOL_ANIM_SUNSCREEN_BOIL_FRAMES
GINA_POOL_ANIM_SUNSCREEN_BOIL_STYLE   // LOOP / ONE_SHOT
GINA_POOL_ANIM_SUNSCREEN_BOIL_MS_PER_FRAME
GINA_POOL_ANIM_SUNSCREEN_BOIL_SPRITE_ASSET  // ((Asset){...}) compound literal
GINA_POOL_ANIM_SUNSCREEN_BOIL_DATA_ASSET
GINA_POOL_ANIMS_COUNT
```

Beyond the whole-table macros, each entry also gets standalone forms for the
places a table macro can't reach:

- `..._IMAGE_X_INIT` / `..._CHUNK_X_INIT` — one initializer row, for scene
  tables that mix directories (e.g. the playground's SFX + dialog lines in
  one chunks array);
- `..._CHUNK_X_FILE` / `..._ANIM_X_SPRITE_FILE` / `..._ANIM_X_DATA_FILE` —
  bare filename strings, for `ActorSpec`/`ActorAnimSpec` fields (resolved
  against the actor's `assets_dir`);
- `..._CHUNK_X_ASSET` — an `(Asset)` compound literal, for direct
  `asset_resolve` calls (music streams).

A scene then declares its tables from the manifest instead of repeating it:

```c
#include "gina_assets.h"

static ImageData images[GINA_POOL_IMAGES_COUNT] = GINA_POOL_IMAGES_INIT;
static const ImageData *background = &images[GINA_POOL_IMAGE_BACKGROUND];
...
sunscreen_boil = animations[GINA_POOL_ANIM_SUNSCREEN_BOIL] =
    make_animation_data(GINA_POOL_ANIM_SUNSCREEN_BOIL_FRAMES,
                        GINA_POOL_ANIM_SUNSCREEN_BOIL_STYLE);
...
load_animation(renderer, sunscreen_boil,
               GINA_POOL_ANIM_SUNSCREEN_BOIL_SPRITE_ASSET,
               GINA_POOL_ANIM_SUNSCREEN_BOIL_DATA_ASSET);
```

The `_ASSET` macros expand to `(Asset){...}` compound literals, so call
sites pass them like values.

### Build-time validation

The generator fails the build (native, test and web — all three depend on the
header) when the manifest and the tree disagree:

- an animation's `frames` must match the row count of its committed `.anim`
  (checked in `common/` and the reference locale);
- a runtime-only entry's files must exist on disk — the game *will* load them.

So editing a boil sheet without updating the manifest, or vice versa, is a
compile error instead of a runtime surprise.

## Migration status

Every scene and actor of both content adventures reads the generated headers
(#105 introduced the architecture with the pool scene; #111 migrated the
rest):

- **Gina** — all five scenes plus the hen `ActorSpec`. The manifest carries
  the authoring tasks and the runtime-only entries side by side.
- **Vania** — all four scenes plus the fox `ActorSpec`. Its art is finished,
  so nothing in `assets/index.json` is marked `task: true`: pure runtime
  declarations, nothing on the *Assets to author* page.
- **Depth demo** — deliberately left inline: it is the developer reference
  for scene mechanics, and its hand-written tables double as the "before"
  example of what the manifest replaces.
