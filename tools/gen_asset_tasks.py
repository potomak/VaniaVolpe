#!/usr/bin/env python3
"""Generate the asset to-do list for every adventure (LIVELINESS.md aside).

The game ships with placeholder art and silent voice lines; this turns a
per-adventure manifest of the *real* assets still to author into the data
behind the browser *Assets to author* page — every animation, voice line and
still image, with what to make (frame count, sprite size, the Italian line to
record) and a GitHub **upload link**.

Drop-box convention (per adventure, under <assets_root>/)
--------------------------------------------------------
Every task has a stable **id** (`<type>-<dir>-<name>`). Uploads for a task go
into a folder `_inbox/<id>/` — the folder name identifies the task no matter
what the uploaded files are called (see `_inbox/README.md`).
`tools/consolidate_assets.py` then stitches / moves them to the real asset
path and archives the raw inputs under `_sources/<id>/`, the done-record. So
status is:

- done ✅      — `_sources/<id>/` holds files (consolidated);
- uploaded 🟨  — `_inbox/<id>/` holds raw files (awaiting consolidation);
- to do ⬜     — neither.

Two jobs, one per mode:
- `--out DIR` writes `DIR/asset_tasks.json` for the page (`make web` runs this);
- with no `--out`, it refreshes the premade `_inbox/<id>/` drop-box folders —
  an empty `.gitkeep` per outstanding task so the folder is in git and the
  Upload here link resolves, pruning folders whose task is done. Run this after
  editing a manifest and commit the result.

Usage:
  tools/gen_asset_tasks.py                     # refresh _inbox drop-boxes
  tools/gen_asset_tasks.py --out build/web     # asset_tasks.json for the page

Stdlib only.
"""

import argparse
import glob
import json
import os
import shutil
from urllib.parse import quote

REPO_OWNER = "potomak"
REPO_NAME = "VaniaVolpe"

# Files the tool itself drops in a `_inbox/<id>/`; never counted as uploads.
SCAFFOLD_FILES = {"README.md", ".gitkeep"}


# The manifest also declares runtime-only entries ("task": false — see
# ASSETS.md) for the game's generated declarations; they are not authoring
# work, so every view here skips them.
def authoring_tasks(manifest):
    return [t for t in manifest["tasks"] if t.get("task", True)]


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def task_id(task):
    return f"{task['type']}-{task['dir'].replace('/', '_')}-{task['name']}"


def inbox_rel(manifest, task):
    return f"{manifest['assets_root']}/_inbox/{task_id(task)}"


def sources_rel(manifest, task):
    return f"{manifest['assets_root']}/_sources/{task_id(task)}"


# Repo-relative final asset path(s) the game actually loads.
def target_rel(manifest, task):
    assets = manifest["assets_root"]
    locale = manifest.get("reference_locale", "it_IT")
    if task["type"] == "animation":
        base = f"{assets}/{task['dir']}/{task['name']}"
        return [base + ".png", base + ".anim"]
    if task["type"] == "voice":
        return [f"{assets}/{locale}/{task['dir']}/{task['name']}.wav"]
    return [f"{assets}/{task['dir']}/{task['name']}.png"]


def uploaded_files(root, rel_dir):
    d = os.path.join(root, rel_dir)
    if not os.path.isdir(d):
        return []
    return sorted(f for f in os.listdir(d)
                  if f not in SCAFFOLD_FILES and not f.startswith("."))


# (state, label): state is "done" | "uploaded" | "todo".
def status(root, manifest, task):
    if uploaded_files(root, sources_rel(manifest, task)):
        return "done", "consolidated"
    pending = uploaded_files(root, inbox_rel(manifest, task))
    if pending:
        if task["type"] == "animation":
            return "uploaded", f"{len(pending)} file(s) uploaded — run consolidate"
        return "uploaded", "uploaded — run consolidate"
    if task["type"] == "voice":
        return "todo", "not recorded"
    if task["type"] == "animation":
        return "todo", "no frames yet"
    return "todo", "placeholder in game"


def upload_url(branch, rel_dir):
    return (f"https://github.com/{REPO_OWNER}/{REPO_NAME}/upload/"
            f"{branch}/{quote(rel_dir)}")


# What the artist drops into the task's _inbox/<id>/ folder.
def drop_desc(task):
    if task["type"] == "animation":
        size = f" ({task['size']})" if task.get("size") else ""
        return (f"the {task['frames']} frame PNGs{size}, in order — filenames "
                f"only decide frame order, so name them e.g. 01.png, 02.png…")
    if task["type"] == "voice":
        return f"the recorded line as a WAV (any filename; saved as {task['name']}.wav)"
    size = f" ({task['size']})" if task.get("size") else ""
    return f"the finished PNG{size} (any filename)"


def meta_line(task):
    if task["type"] == "animation":
        bits = [f"{task['frames']} frames"]
        if task.get("size"):
            bits.append(f"{task['size']} each")
        return " · ".join(bits)
    if task["type"] == "art" and task.get("size"):
        return task["size"]
    return ""


# ── data model shared by the Markdown and JSON renderers ─────────────────────

def task_view(root, manifest, branch, task):
    state, label = status(root, manifest, task)
    # Each task has a premade drop-box folder `_inbox/<id>/` (kept in git by a
    # .gitkeep), so Upload here lands straight in it — one click, no typing.
    return {
        "id": task_id(task),
        "name": task["name"],
        "type": task["type"],
        "description": task["description"],
        "meta": meta_line(task),
        "line": task.get("text"),
        "target": target_rel(manifest, task)[0],
        "status": state,
        "status_label": label,
        "upload_url": upload_url(branch, inbox_rel(manifest, task)),
        "upload_hint": drop_desc(task),
    }


def manifest_view(root, manifest, branch):
    groups = []
    for task in authoring_tasks(manifest):
        view = task_view(root, manifest, branch, task)
        if not groups or groups[-1]["name"] != task["group"]:
            groups.append({"name": task["group"], "tasks": []})
        groups[-1]["tasks"].append(view)
    for group in groups:
        group["done"] = sum(1 for t in group["tasks"] if t["status"] == "done")
        group["total"] = len(group["tasks"])
    total = sum(g["total"] for g in groups)
    done = sum(g["done"] for g in groups)
    return {"title": manifest["title"], "done": done, "total": total,
            "groups": groups}


def load_manifests(root, paths):
    manifests = []
    for path in paths:
        with open(path) as f:
            manifests.append(json.load(f))
    return manifests


# Keep <assets>/_inbox holding a premade, empty drop-box folder (a .gitkeep, so
# git tracks it and the Upload here link resolves) for exactly the outstanding
# tasks — and none for the done ones.
def scaffold_inbox(root, manifest):
    for task in authoring_tasks(manifest):
        rel = inbox_rel(manifest, task)
        d = os.path.join(root, rel)
        done = uploaded_files(root, sources_rel(manifest, task))
        if done:
            # Consolidated: drop the now-stale empty folder (only .gitkeep left).
            if os.path.isdir(d) and not uploaded_files(root, rel):
                shutil.rmtree(d)
            continue
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, ".gitkeep"), "a").close()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("manifests", nargs="*",
                        help="tasks.json files (default: every adventure's)")
    parser.add_argument("--out", metavar="DIR",
                        help="write DIR/asset_tasks.json for the page; without "
                             "it, refresh the _inbox drop-box folders instead")
    parser.add_argument("--branch", default="main",
                        help="branch the upload links target (default main)")
    args = parser.parse_args()

    root = repo_root()
    paths = args.manifests or sorted(glob.glob(
        os.path.join(root, "src/adventures/*/assets/tasks.json")))
    if not paths:
        parser.error("no tasks.json manifests found")
    manifests = load_manifests(root, paths)

    if args.out:
        views = [manifest_view(root, m, args.branch) for m in manifests]
        os.makedirs(args.out, exist_ok=True)
        out = os.path.join(args.out, "asset_tasks.json")
        with open(out, "w") as f:
            json.dump({"branch": args.branch, "adventures": views}, f, indent=1)
        print(f"asset_tasks.json: {len(manifests)} adventure(s) -> {out}")
        return

    for manifest in manifests:
        scaffold_inbox(root, manifest)
    print(f"drop-box folders refreshed: {len(manifests)} adventure(s)")


if __name__ == "__main__":
    main()
