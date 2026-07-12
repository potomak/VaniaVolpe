#!/usr/bin/env python3
"""Generate the asset to-do list for every adventure (LIVELINESS.md aside).

The game ships with placeholder art and silent voice lines; this turns a
per-adventure manifest of the *real* assets still to author into a grouped
to-do list — every animation, voice line and still image — where each task
carries what to make (frame count, sprite size, the Italian line to record)
and a GitHub **upload link** that lands in that task's drop-box directory.

Drop-box convention (per adventure, under <assets_root>/)
--------------------------------------------------------
Every task has a stable **id** (`<type>-<dir>-<name>`). Uploads for a task go
into `_inbox/<id>/` — the directory name identifies the task no matter what
the uploaded files are called. `tools/consolidate_assets.py` then stitches /
moves them to the real asset path and archives the raw inputs under
`_sources/<id>/`, which is the done-record. So status is:

- done ✅      — `_sources/<id>/` holds files (consolidated);
- uploaded 🟨  — `_inbox/<id>/` holds raw files (awaiting consolidation);
- to do ⬜     — neither.

This tool (default mode) writes `ASSETS_TODO.md` and scaffolds an
`_inbox/<id>/` drop directory (with a README describing the task) for every
outstanding task, pruning the ones that are done. `--web DIR` instead writes
`DIR/asset_tasks.json` for the browser tools page and touches nothing in the
source tree.

Usage:
  tools/gen_asset_tasks.py                    # ASSETS_TODO.md + drop-boxes
  tools/gen_asset_tasks.py --web build/web    # asset_tasks.json only
  tools/gen_asset_tasks.py --branch main

Stdlib only.
"""

import argparse
import glob
import json
import os
from urllib.parse import quote

REPO_OWNER = "potomak"
REPO_NAME = "VaniaVolpe"

# Files the tool itself drops in a `_inbox/<id>/`; never counted as uploads.
SCAFFOLD_FILES = {"README.md", ".gitkeep"}


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


def tree_url(branch, rel_path):
    return (f"https://github.com/{REPO_OWNER}/{REPO_NAME}/tree/"
            f"{branch}/{quote(rel_path)}")


# What the artist drops into the task's _inbox dir.
def upload_hint(task):
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
        "upload_hint": upload_hint(task),
    }


def manifest_view(root, manifest, branch):
    groups = []
    for task in manifest["tasks"]:
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


# ── Markdown (committed ASSETS_TODO.md) ──────────────────────────────────────

BOX = {"done": "x", "uploaded": "~", "todo": " "}
BADGE = {"done": "✅", "uploaded": "🟨", "todo": "⬜"}


def render_task_md(task):
    lines = [f"- [{BOX[task['status']]}] {BADGE[task['status']]} "
             f"**{task['name']}** — {task['description']}"]
    if task["meta"]:
        lines.append(f"  - {task['meta']}")
    if task["line"]:
        lines.append(f'  - Line: *"{task["line"]}"*')
    lines.append(f"  - Status: {task['status_label']}")
    if task["status"] != "done":
        lines.append(f"  - [Upload here]({task['upload_url']}) — "
                     f"{task['upload_hint']}")
    return "\n".join(lines)


def render_manifest_md(view):
    out = [f"## {view['title']}", "",
           f"**{view['done']} / {view['total']}** assets authored.", ""]
    for group in view["groups"]:
        out.append(f"### {group['name']} ({group['done']}/{group['total']})")
        out.append("")
        for task in group["tasks"]:
            out.append(render_task_md(task))
        out.append("")
    return "\n".join(out)


# ── drop-box scaffolding ─────────────────────────────────────────────────────

def readme_text(view):
    lines = [f"# Upload: {view['name']}", "",
             f"- Task id: `{view['id']}`",
             f"- Type: {view['type']}",
             f"- {view['description']}"]
    if view["meta"]:
        lines.append(f"- {view['meta']}")
    if view["line"]:
        lines.append(f'- Line to record: "{view["line"]}"')
    lines += ["",
              f"**Drop here:** {view['upload_hint']}", "",
              f"Then run `tools/consolidate_assets.py` — it moves the files to "
              f"`{view['target']}` and archives the raw inputs under "
              f"`_sources/{view['id']}/`.", ""]
    return "\n".join(lines)


# Keep <assets>/_inbox holding exactly the outstanding tasks: a self-describing
# drop directory per to-do/uploaded task, and none for the done ones.
def scaffold_inbox(root, manifest, views):
    by_id = {v["id"]: v for v in views}
    for v in views:
        rel = f"{manifest['assets_root']}/_inbox/{v['id']}"
        d = os.path.join(root, rel)
        if v["status"] == "done":
            # Consolidated: drop the (now empty) scaffold if only our files remain.
            if os.path.isdir(d) and not uploaded_files(root, rel):
                for name in SCAFFOLD_FILES:
                    p = os.path.join(d, name)
                    if os.path.isfile(p):
                        os.remove(p)
                if not os.listdir(d):
                    os.rmdir(d)
            continue
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, "README.md"), "w") as f:
            f.write(readme_text(v))
    # Tidy an empty _inbox root.
    inbox_root = os.path.join(root, manifest["assets_root"], "_inbox")
    if os.path.isdir(inbox_root) and not os.listdir(inbox_root):
        os.rmdir(inbox_root)


def load_manifests(root, paths):
    manifests = []
    for path in paths:
        with open(path) as f:
            manifests.append(json.load(f))
    return manifests


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("manifests", nargs="*",
                        help="tasks.json files (default: every adventure's)")
    parser.add_argument("--output", default="ASSETS_TODO.md",
                        help="Markdown path (default ASSETS_TODO.md)")
    parser.add_argument("--web", metavar="DIR",
                        help="write DIR/asset_tasks.json instead; no scaffolding")
    parser.add_argument("--branch", default="main",
                        help="branch the upload links target (default main)")
    args = parser.parse_args()

    root = repo_root()
    paths = args.manifests or sorted(glob.glob(
        os.path.join(root, "src/adventures/*/assets/tasks.json")))
    if not paths:
        parser.error("no tasks.json manifests found")
    manifests = load_manifests(root, paths)

    if args.web:
        views = [manifest_view(root, m, args.branch) for m in manifests]
        os.makedirs(args.web, exist_ok=True)
        out = os.path.join(args.web, "asset_tasks.json")
        with open(out, "w") as f:
            json.dump({"branch": args.branch, "adventures": views}, f, indent=1)
        print(f"asset_tasks.json: {len(manifests)} adventure(s) -> {out}")
        return

    header = [
        "# Assets to author",
        "",
        "_Generated by `tools/gen_asset_tasks.py` — do not edit by hand; "
        "re-run it to refresh the boxes._",
        "",
        "The game runs on placeholder art and silent voice lines. Each task is "
        "a real asset still to make: click **Upload here** to drop the raw "
        "files into that task's `_inbox/<id>/` directory, then run "
        "`tools/consolidate_assets.py` to stitch/move them into place and tick "
        "the box. ✅ done · 🟨 uploaded, awaiting consolidation · ⬜ to do.",
        "",
    ]
    sections = [render_manifest_md(manifest_view(root, m, args.branch))
                for m in manifests]
    text = "\n".join(header) + "\n" + "\n\n".join(sections).rstrip() + "\n"
    with open(os.path.join(root, args.output), "w") as f:
        f.write(text)

    for manifest in manifests:
        views = [task_view(root, manifest, args.branch, t)
                 for t in manifest["tasks"]]
        scaffold_inbox(root, manifest, views)
    print(f"{args.output} + drop-boxes: {len(manifests)} adventure(s)")


if __name__ == "__main__":
    main()
