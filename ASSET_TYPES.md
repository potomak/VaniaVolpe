# Asset types

A refinement of the asset manifest (`ASSETS.md`): make the manifest's `type`
field name **what the framework does with an asset** directly —
`image | animation | sfx | music | speech | sound` — instead of spreading that
across `type`, a flag, and the directory name. The asset's **file type**
(`image`, `audio`, `text` — what the file physically is) stays a distinct
concept, but a *derived* one: a projection of `type`, not a second field to
author. Implemented in #152.

> Implementation note: the spec was drafted with five types; a sixth, **`sound`**,
> was added for non-bank, filename-referenced audio effects — the actor's move
> sound, referenced by the `ActorSpec` rather than triggered by code. (UI button
> clicks, by contrast, are code-triggered and went in the `sfx` bank so a scene
> plays them with `play_<name>()`.) `sound` shares the generator's plain-chunk
> handling with `music`.

## The problem

The manifest declares three `type`s — `image`, `animation`, `audio` — but the
framework treats assets as **six** distinct types, and the extra three are
inferred from *other* signals:

| Framework type | How it's identified today | Generated output |
|---|---|---|
| image | `type: image` | images table (`_IMAGE_*`, `load_scene_images`) |
| animation | `type: animation` | animations table (`_ANIM_*_SPEC`, make/load) |
| sfx | `type: audio` **+ `sfx: true`** | adventure SFX bank, `play_<name>()` |
| speech | `type: audio` **+ `dir` ends in `dialog`** | per-scene chunk, `say_<name>()`, `optional_audio`, subtitle `.txt` |
| music | `type: audio`, not sfx, not dialogue | `_ASSET_INIT` → scene `.music` stream |
| voice | `type: voice` | none — the C generator skips it |

Three problems fall out of this:

- **`speech` is directory magic.** `gen_asset_decls.py` decides an audio file is
  a spoken line with `dialogue = rel_dir.endswith("dialog")`. Renaming the folder
  silently changes what the asset *is* (a `say_()` helper + a subtitle vs. a plain
  chunk).
- **`music` is undistinguished.** The generator emits an `_ASSET_INIT` for every
  non-dialogue chunk; "music" only means "the one a scene assigns to `.music`."
  A future ambient loop would be indistinguishable from music.
- **`voice` is vestigial.** It's in the schema and the generator skips it, but no
  manifest uses it (the `say_()` migration moved dialogue to `type: audio` in
  `dialog/`). Its meaning — "a spoken line still to author" — is already covered
  by `task: true`.

So the type is spread across `type` + `sfx` + `dir`, and one signal (the
directory name) is implicit and fragile.

## The model

`type` names what the framework does with the asset — one field, no flag, no
directory inference:

```
type ∈ { image, animation, sfx, music, speech, sound }
```

Old → new:

| Today | Proposed |
|---|---|
| `type: image` | `type: image` (unchanged) |
| `type: animation` | `type: animation` (unchanged) |
| `type: audio`, `sfx: true` | `type: sfx` (drop the flag) |
| `type: audio` in a `dialog/` dir | `type: speech` |
| `type: audio` used as `.music` | `type: music` |
| `type: audio`, non-bank, referenced by filename (the actor's move sound) | `type: sound` |
| `type: voice` | `type: speech` + `task: true` |

The `sfx` boolean and the `voice` type both go away. The type no longer depends
on the directory name.

## Type vs. file type

`type` is what the framework does with the asset; the **file type** is what the
file physically is — `image` (a PNG), `audio` (a WAV), or `text`. They are
different axes: `sfx`, `music`, `speech` and `sound` are all `audio` files;
`image` and `animation` are both `image` files. And a single `type` can span several files of
different file types — an `animation` is a sprite-sheet PNG **and** an `.anim`
text table; a `speech` is a WAV **and** its subtitle / lip-sync text sidecars.

Because `type` **determines** the file type(s), file type is *not authored* — it
is derived from `type` by a fixed table the tooling already encodes implicitly:

| `type` | file type(s) | files |
|---|---|---|
| image | image | `<name>.png` |
| animation | image + text | `<name>.png`, `<name>.anim` |
| sfx | audio | `<name>.wav` |
| music | audio | `<name>.wav` |
| speech | audio + text | `<name>.wav`, `<name>.txt` (+ `.cues` / `.words` lip-sync) |
| sound | audio | `<name>.wav` |

Authoring both fields would be redundant — and would let a manifest contradict
itself (`{type: music, file_type: image}`) — and it breaks on the multi-file rows
above: which single file type would an `animation` entry carry? So `type` is the
source of truth and file type is a projection. Tools that need file facts
(extensions, the pipeline's stitch-vs-move, the web preload globs) read this
table; there is no second field to keep in sync.

## What each consumer changes

`type` is read by four tools; each switches on the wider enum instead of the
`type`/`sfx`/`dir` combination, and derives file facts from the table above
rather than from an ad-hoc extension map:

- **`tools/gen_asset_decls.py`** — the C declarations. `sfx` entries → the
  adventure bank (`play_<name>()`); `speech` → per-scene chunk with
  `optional_audio` + the `say_<name>()` helper + subtitle text; `music` → the
  `_ASSET_INIT` a scene wires to `.music`. The `dir.endswith("dialog")` test, the
  `sfx` flag, and the hard-coded `EXT` map are removed in favour of the
  `type` → file-type table.
- **`tools/gen_asset_tasks.py`** — the *Assets to author* page groups and labels
  by type; teach it the new values.
- **`tools/consolidate_assets.py`** — folds uploads; the file-type table drives
  it directly (an `image + text` type like `animation` stitches frames into a
  sheet and writes the `.anim`; a single-file type just moves the file).
- **`src/emscripten/cost_estimate.html`** — counts assets by type.

## Migration

Behavior-preserving, and it was: rewriting each manifest entry's `type` per the
table above and regenerating left the emitted C **byte-identical** (the drift
guard confirms it), so no scene code moved. Only the manifest's spelling and the
generator's branching changed. The generator derives file extensions from the
`type → file type` table, and `gen_asset_tasks.py` / `consolidate_assets.py`
branch on `type` (speech = record a line; sfx/music/sound = a finished WAV).

## Open questions

- **Directories.** The type no longer *needs* the `dialog`/`music` dir names. Do
  we keep them as an organizing convention (nice for the asset catalog and the
  locale layout), or drop the requirement entirely? Subtitle `.txt` sidecars and
  the locale rules currently live beside the `dialog/` WAVs, so at minimum the
  layout stays; the *inference* is what this change removes.
- **`sound` vs `sfx`.** Resolved by adding `sound` for filename-referenced
  effects (the actor move sound, UI clicks), distinct from the `play_<name>()`
  bank (`sfx`). The generator handles `sound` exactly like `music` (a plain
  chunk with `_FILE`/`_ASSET`), so the split is semantic; if the two ever need
  to diverge, they already have separate type names.
- **`_inbox` scaffolding.** The type rename changes task ids
  (`audio-…`/`voice-…` → `speech-…`/`sfx-…`), so the premade `_inbox/<id>/`
  drop-box folders are now stale (they were already stale from the earlier
  voice→audio rename). Regenerating them (`gen_asset_tasks.py` with no `--out`)
  and pruning the orphans is a separate housekeeping step, not done here.
