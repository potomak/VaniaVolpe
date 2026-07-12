# Asset upload drop-box

One premade folder per outstanding asset, named by its task **id** (the same id
shown on the *Assets to author* page —
[`asset_tasks.html`](https://potomak.github.io/VaniaVolpe/asset_tasks.html), or
Tools). Drop the raw files into the matching folder; the folder name is the
whole identity, so filenames don't matter (for animations they only set frame
order, e.g. `01.png`, `02.png`…). Each page card's **Upload here** link lands
straight in the right folder.

- **animation** → the frame PNGs
- **voice** → the recorded WAV
- **art** → the finished PNG

A maintainer then runs `tools/consolidate_assets.py`, which stitches/moves the
files into their real asset paths and archives the raw inputs under
`_sources/<id>/` (the done-record and re-stitch source), then removes the
folder here. `tools/gen_asset_tasks.py` refreshes these folders when the task
list changes. Full flow: `TOOLS.md` → *Asset pipeline*. Nothing under `_inbox/`
or `_sources/` is shipped in the game or the web build.
