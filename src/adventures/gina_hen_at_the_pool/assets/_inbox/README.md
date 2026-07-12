# Asset upload drop-box

Raw asset files waiting to be folded into the game go here, one folder per
task, named by the task **id** shown on the *Assets to author* page
([`asset_tasks.html`](https://potomak.github.io/VaniaVolpe/asset_tasks.html),
also under Tools). The folder name is the whole identity — filenames don't
matter (for animations they only set frame order).

- **animation** → `_inbox/<id>/` with the frame PNGs, e.g. `01.png`, `02.png`…
- **voice** → `_inbox/<id>/` with the recorded WAV
- **art** → `_inbox/<id>/` with the finished PNG

Each task's page card has an **Upload here** link and says which `<id>/` folder
and what to drop.

Then a maintainer runs `tools/consolidate_assets.py`, which stitches/moves the
files into their real asset paths and archives the raw inputs under
`_sources/<id>/` (the done-record and re-stitch source). Full flow:
`TOOLS.md` → *Asset pipeline*. Nothing under `_inbox/` or `_sources/` is
shipped in the game or the web build.
