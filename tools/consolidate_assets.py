#!/usr/bin/env python3
"""Consolidate uploaded raw assets into the game's real asset files.

Companion to `tools/gen_asset_tasks.py`. Artists drop raw files into a task's
drop-box `<assets_root>/_inbox/<id>/` (the directory name is the task id, so
the upload is identified no matter what the files are called). This tool, for
every drop-box that has files:

1. moves the raw inputs to `<assets_root>/_sources/<id>/` (the archive /
   re-stitch source / done-record);
2. produces the real asset the game loads, from those sources:
   - animation — stitches the frames (ordered by filename) into
     `<dir>/<name>.png` + a matching `<name>.anim`;
   - voice     — copies the WAV to `<locale>/<dir>/<name>.wav`;
   - art       — copies the PNG to `<dir>/<name>.png`;
3. clears the drop-box.

It reads the same `assets/assets.json` manifests, so a fresh session can run it
with no other context — "consolidate the new assets I uploaded" — and it is
idempotent: an empty drop-box is skipped. Re-run `gen_asset_tasks.py`
afterwards to tick the boxes.

Usage:
  tools/consolidate_assets.py               # every adventure
  tools/consolidate_assets.py --dry-run     # report only, touch nothing
  tools/consolidate_assets.py path/to/assets.json ...

Requires Pillow (for stitching animation frames).
"""

import argparse
import glob
import os
import shutil
import sys

import gen_asset_tasks as tasks  # sibling helpers: ids, paths, uploaded_files

from PIL import Image


def stitch(frame_paths, out_png, out_anim):
    frames = [Image.open(p).convert("RGBA") for p in frame_paths]
    width = max(f.width for f in frames)
    height = sum(f.height for f in frames)
    sheet = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    rects, y = [], 0
    for f in frames:
        sheet.paste(f, (0, y))
        rects.append((0, y, f.width, f.height))
        y += f.height
    sheet.save(out_png)
    with open(out_anim, "w") as fh:
        for r in rects:
            fh.write(",".join(str(n) for n in r) + "\n")


def consolidate_task(root, manifest, task, dry_run):
    inbox = tasks.inbox_rel(manifest, task)
    raw = tasks.uploaded_files(root, inbox)
    if not raw:
        return None

    tid = tasks.task_id(task)
    sources = tasks.sources_rel(manifest, task)
    src_abs = os.path.join(root, sources)
    inbox_abs = os.path.join(root, inbox)

    # 1) archive raw inputs into _sources/<id>/ (canonical source).
    moved = []
    for name in raw:
        moved.append(name)
        if not dry_run:
            os.makedirs(src_abs, exist_ok=True)
            shutil.move(os.path.join(inbox_abs, name),
                        os.path.join(src_abs, name))

    # 2) produce the real asset from the archived sources.
    targets = tasks.target_rel(manifest, task)
    notes = []
    if task["type"] == "animation":
        want = task.get("frames")
        if want and len(moved) != want:
            notes.append(f"warning: {len(moved)} frames, manifest expects {want}")
        out_png, out_anim = (os.path.join(root, t) for t in targets)
        if not dry_run:
            os.makedirs(os.path.dirname(out_png), exist_ok=True)
            stitch([os.path.join(src_abs, n) for n in sorted(moved)],
                   out_png, out_anim)
    else:
        if len(moved) > 1:
            notes.append(f"warning: {len(moved)} files, using {sorted(moved)[0]}")
        out = os.path.join(root, targets[0])
        if not dry_run:
            os.makedirs(os.path.dirname(out), exist_ok=True)
            shutil.copy(os.path.join(src_abs, sorted(moved)[0]), out)

    # 3) clear the drop-box (raw files are gone; drop the scaffold too).
    if not dry_run and os.path.isdir(inbox_abs):
        for name in os.listdir(inbox_abs):
            os.remove(os.path.join(inbox_abs, name))
        os.rmdir(inbox_abs)

    return {"id": tid, "type": task["type"], "count": len(moved),
            "targets": targets, "notes": notes}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("manifests", nargs="*")
    parser.add_argument("--dry-run", action="store_true",
                        help="report what would happen; change nothing")
    args = parser.parse_args()

    root = tasks.repo_root()
    paths = args.manifests or sorted(glob.glob(
        os.path.join(root, "src/adventures/*/assets/assets.json")))
    if not paths:
        parser.error("no assets.json manifests found")

    done = 0
    for manifest in tasks.load_manifests(root, paths):
        for task in tasks.authoring_tasks(manifest):
            result = consolidate_task(root, manifest, task, args.dry_run)
            if result is None:
                continue
            done += 1
            verb = "would consolidate" if args.dry_run else "consolidated"
            print(f"{verb} {result['id']}: {result['count']} file(s) -> "
                  f"{result['targets'][0]}")
            for note in result["notes"]:
                print(f"  {note}")

    if done == 0:
        print("No uploads found in any _inbox/<id>/ drop-box.")
    else:
        tail = "" if args.dry_run else " — re-run gen_asset_tasks.py to refresh"
        print(f"{done} task(s) {'pending' if args.dry_run else 'consolidated'}{tail}")


if __name__ == "__main__":
    main()
