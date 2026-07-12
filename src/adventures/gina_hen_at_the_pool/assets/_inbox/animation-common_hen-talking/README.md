# Upload: talking

- Task id: `animation-common_hen-talking`
- Type: animation
- Gina's beak-moving talk loop. For read-along lip-sync this can grow to a 7-frame mouth-shape sheet (X A B C D E F, see SPEECH.md); 3 frames stay fine as a classic loop.
- 3 frames · 120x120 each

**Drop here:** the 3 frame PNGs (120x120), in order — filenames only decide frame order, so name them e.g. 01.png, 02.png…

Then run `tools/consolidate_assets.py` — it moves the files to `src/adventures/gina_hen_at_the_pool/assets/common/hen/talking.png` and archives the raw inputs under `_sources/animation-common_hen-talking/`.
