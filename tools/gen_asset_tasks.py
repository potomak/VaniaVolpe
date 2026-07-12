#!/usr/bin/env python3
"""Generate an asset to-do checklist for an adventure (ASSETS_TODO.md).

The game ships with placeholder art and silent voice lines; this turns a
per-adventure manifest of the *real* assets still to author into a grouped,
checkbox task list — every animation, voice line and still image — where each
task carries what to make (frame count, sprite size, the Italian line to
record) and a **GitHub upload link** that lands you in the right directory to
drop the raw files. Frames you upload are later stitched into a sprite sheet;
audio and stills go in as-is.

Status is read live from the repo, so re-running the tool after an upload
ticks the box:

- animation — done when its raw frame dir holds files (N of `frames`);
- voice     — done when the recorded WAV exists at its target path;
- art       — done when the raw image has been uploaded.

The manifest (JSON) lives next to the adventure's assets; see
`src/adventures/gina_hen_at_the_pool/assets/tasks.json`. Outputs are
committed, so builds and CI never run this.

Usage:
  tools/gen_asset_tasks.py                 # every adventure with a tasks.json
  tools/gen_asset_tasks.py path/to/tasks.json ...
  tools/gen_asset_tasks.py --output ASSETS_TODO.md --branch main

Stdlib only.
"""

import argparse
import glob
import json
import os
from urllib.parse import quote

REPO_OWNER = "potomak"
REPO_NAME = "VaniaVolpe"

IMAGE_EXTS = (".png", ".jpg", ".jpeg", ".gif", ".bmp")


def repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


# GitHub's web uploader only opens on a directory that already exists, so link
# to the nearest existing ancestor of the intended upload dir and spell out the
# remaining sub-path as the name to type. Returns (link_dir, typed_subpath).
def upload_target(root, intended_dir):
    parts = intended_dir.split("/")
    for cut in range(len(parts), 0, -1):
        candidate = "/".join(parts[:cut])
        if os.path.isdir(os.path.join(root, candidate)):
            return candidate, "/".join(parts[cut:])
    return parts[0], "/".join(parts[1:])


def upload_link(branch, link_dir):
    return (f"https://github.com/{REPO_OWNER}/{REPO_NAME}/upload/"
            f"{branch}/{quote(link_dir)}")


def tree_link(branch, path):
    return (f"https://github.com/{REPO_OWNER}/{REPO_NAME}/tree/"
            f"{branch}/{quote(path)}")


def count_images(root, rel_dir):
    d = os.path.join(root, rel_dir)
    if not os.path.isdir(d):
        return 0
    return sum(1 for f in os.listdir(d) if f.lower().endswith(IMAGE_EXTS))


# Resolve a task to its filesystem facts: the intended upload directory, a
# done flag, and a short status label. Everything is relative to the repo root.
def resolve(root, manifest, task):
    assets = manifest["assets_root"]
    locale = manifest.get("reference_locale", "it_IT")
    kind = task["type"]

    if kind == "animation":
        raw_dir = f"{assets}/{task['dir']}/raw/{task['name']}"
        have = count_images(root, raw_dir)
        want = task.get("frames", 0)
        done = have >= want > 0
        if have == 0:
            status = "no frames yet"
        elif done:
            status = f"{have} frames uploaded — ready to stitch"
        else:
            status = f"{have}/{want} frames uploaded"
        return raw_dir, done, status

    if kind == "voice":
        final = f"{assets}/{locale}/{task['dir']}/{task['name']}.wav"
        done = os.path.isfile(os.path.join(root, final))
        return (f"{assets}/{locale}/{task['dir']}", done,
                "recorded" if done else "not recorded")

    # art
    raw_dir = f"{assets}/{task['dir']}/raw"
    have = os.path.isfile(os.path.join(root, raw_dir, task["name"] + ".png"))
    return raw_dir, have, "uploaded" if have else "placeholder in game"


def render_task(root, manifest, branch, task):
    intended, done, status = resolve(root, manifest, task)
    link_dir, typed = upload_target(root, intended)
    box = "x" if done else " "

    lines = [f"- [{box}] **{task['name']}** — {task['description']}"]
    meta = []
    if task["type"] == "animation":
        meta.append(f"{task['frames']} frames")
        if task.get("size"):
            meta.append(f"{task['size']} each")
    elif task["type"] == "art" and task.get("size"):
        meta.append(task["size"])
    if meta:
        lines.append(f"  - {' · '.join(meta)}")
    if task["type"] == "voice":
        lines.append(f'  - Line: *"{task["text"]}"* → `{task["name"]}.wav`')
    lines.append(f"  - Status: {status}")

    upload = upload_link(branch, link_dir)
    if typed:
        lines.append(f"  - [Upload here]({upload}) — name the file(s) "
                     f"`{typed}/…`")
    else:
        lines.append(f"  - [Upload here]({upload})")
    return "\n".join(lines)


def render_manifest(root, manifest, branch):
    tasks = manifest["tasks"]
    done = sum(1 for t in tasks if resolve(root, manifest, t)[1])
    out = [f"## {manifest['title']}", "",
           f"**{done} / {len(tasks)}** assets authored.", ""]

    groups = []
    for task in tasks:
        if not groups or groups[-1][0] != task["group"]:
            groups.append((task["group"], []))
        groups[-1][1].append(task)

    for name, group_tasks in groups:
        gdone = sum(1 for t in group_tasks if resolve(root, manifest, t)[1])
        out.append(f"### {name} ({gdone}/{len(group_tasks)})")
        out.append("")
        for task in group_tasks:
            out.append(render_task(root, manifest, branch, task))
        out.append("")
    return "\n".join(out)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("manifests", nargs="*",
                        help="tasks.json files (default: every adventure's)")
    parser.add_argument("--output", default="ASSETS_TODO.md")
    parser.add_argument("--branch", default="main",
                        help="branch the upload links target (default main)")
    args = parser.parse_args()

    root = repo_root()
    manifest_paths = args.manifests or sorted(glob.glob(
        os.path.join(root, "src/adventures/*/assets/tasks.json")))
    if not manifest_paths:
        parser.error("no tasks.json manifests found")

    sections = []
    for path in manifest_paths:
        with open(path) as f:
            manifest = json.load(f)
        sections.append(render_manifest(root, manifest, args.branch))

    header = [
        "# Assets to author",
        "",
        "_Generated by `tools/gen_asset_tasks.py` — do not edit by hand; "
        "re-run it to refresh the checkboxes after uploading._",
        "",
        "The game runs on placeholder art and silent voice lines. Each task "
        "below is a real asset still to make: click **Upload here** to drop "
        "the raw files straight into the right GitHub directory (frames get "
        "stitched into a sprite sheet later; audio and stills go in as-is), "
        "then re-run the tool to tick the box.",
        "",
    ]
    text = "\n".join(header) + "\n" + "\n\n".join(sections).rstrip() + "\n"

    out_path = os.path.join(root, args.output)
    with open(out_path, "w") as f:
        f.write(text)
    print(f"{args.output}: {len(manifest_paths)} adventure(s)")


if __name__ == "__main__":
    main()
