#!/usr/bin/env python3
"""Generate C asset declarations from an adventure's asset manifest.

The manifest (assets/index.json — see ASSETS.md) is the single source of
truth for an adventure's assets: the art pipeline, the cost estimator and —
via this generator — the game itself all read the same file. This emits a
header of initializer and index macros so scenes declare their tables from
the manifest instead of repeating filenames and frame counts inline:

    static ImageData images[GINA_POOL_IMAGES_COUNT] = GINA_POOL_IMAGES_INIT;
    static const ImageData *background = &images[GINA_POOL_IMAGE_BACKGROUND];

Zero runtime parsing: like the playthrough scripts (tools/gen_playtest.py),
the JSON is consumed at build time. Run by make; output under build/gen/.

The generator also validates what it can see:
- an animation's `frames` must match its committed .anim row count;
- finished entries (no "task": true flag) must have their files on disk.

Entry dirs: entries still to author ("task": true) use layered dirs
("common/pool"); the rest use resolver dirs ("pool"). Both map to the same
runtime dir (the layer prefix is stripped), which is what the emitted macros
use. `voice` entries (per-line recordings still to author) are not yet
runtime assets and are skipped.

Usage:
  tools/gen_asset_decls.py --manifest <assets>/index.json --out build/gen/<adv>_assets.h
"""

import argparse
import json
import os
import re
import sys

EXT = {"image": ".png", "audio": ".wav"}
STYLES = {"loop": "LOOP", "one_shot": "ONE_SHOT"}


def die(msg):
    sys.exit(f"gen_asset_decls: {msg}")


def sym(text):
    return re.sub(r"[^A-Za-z0-9]", "_", text).upper()


def runtime_dir(entry):
    d = entry["dir"]
    if entry.get("task", False) and d.startswith("common/"):
        return d[len("common/") :]
    return d


def anim_rows(path):
    rows = 0
    with open(path, encoding="utf-8") as f:
        for line in f:
            if line.strip():
                rows += 1
    return rows


# Absolute path of the asset file if present in common/ or the reference
# locale, else None.
def find_file(root, manifest, rel_dir, filename):
    for layer in ("common", manifest.get("reference_locale", "it_IT")):
        path = os.path.join(root, manifest["assets_root"], layer, rel_dir,
                            filename)
        if os.path.isfile(path):
            return path
    return None


def validate(root, manifest, entry, rel_dir):
    name = entry["name"]
    if entry["type"] == "animation":
        anim = find_file(root, manifest, rel_dir, name + ".anim")
        if anim is not None:
            rows = anim_rows(anim)
            if rows != entry["frames"]:
                die(f"{rel_dir}/{name}.anim has {rows} frames, "
                    f"manifest says {entry['frames']}")
        elif not entry.get("task", False):
            die(f"runtime asset {rel_dir}/{name}.anim is missing")
        if (not entry.get("task", False)
                and find_file(root, manifest, rel_dir, name + ".png") is None):
            die(f"runtime asset {rel_dir}/{name}.png is missing")
    elif not entry.get("task", False):
        filename = name + EXT[entry["type"]]
        if find_file(root, manifest, rel_dir, filename) is None:
            die(f"runtime asset {rel_dir}/{filename} is missing")


def emit_group(out, prefix, rel_dir, entries):
    tag = f"{prefix}_{sym(rel_dir)}"
    images = [e for e in entries if e["type"] == "image"]
    chunks = [e for e in entries if e["type"] == "audio"]
    anims = [e for e in entries if e["type"] == "animation"]

    out.append(f"// ── {rel_dir} ─────────────────────────────────────────────")
    if images:
        # Whole-table INIT for scenes whose table is one dir, per-entry _INIT
        # rows for scenes that mix dirs in one table.
        for i, e in enumerate(images):
            m = f"{tag}_IMAGE_{sym(e['name'])}"
            out.append(f"#define {m} {i}")
            out.append(f'#define {m}_INIT '
                       f'{{NULL, "{e["name"]}.png", "{rel_dir}", 0, 0}}')
        out.append(f"#define {tag}_IMAGES_COUNT {len(images)}")
        rows = ", ".join(f"{tag}_IMAGE_{sym(e['name'])}_INIT" for e in images)
        out.append(f"#define {tag}_IMAGES_INIT {{{rows}}}")
    if chunks:
        # _FILE feeds ActorSpec filename fields (resolved against the actor's
        # assets_dir); _ASSET feeds asset_resolve (e.g. music streams).
        for i, e in enumerate(chunks):
            m = f"{tag}_CHUNK_{sym(e['name'])}"
            out.append(f"#define {m} {i}")
            out.append(f'#define {m}_INIT '
                       f'{{NULL, "{e["name"]}.wav", "{rel_dir}"}}')
            out.append(f'#define {m}_FILE "{e["name"]}.wav"')
            out.append(f'#define {m}_ASSET ((Asset){{.filename = '
                       f'"{e["name"]}.wav", .directory = "{rel_dir}"}})')
            # A plain-brace Asset initializer (not the compound literal above)
            # so it is a constant aggregate valid at file scope — feeds a
            # scene's declarative `.music` field (SCENES.md milestone 4).
            out.append(f'#define {m}_ASSET_INIT '
                       f'{{"{e["name"]}.wav", "{rel_dir}"}}')
        out.append(f"#define {tag}_CHUNKS_COUNT {len(chunks)}")
        rows = ", ".join(f"{tag}_CHUNK_{sym(e['name'])}_INIT" for e in chunks)
        out.append(f"#define {tag}_CHUNKS_INIT {{{rows}}}")
    if anims:
        # _SPRITE_FILE/_DATA_FILE feed ActorAnimSpec (filenames relative to
        # the actor's assets_dir); the _ASSET pair feeds load_animation.
        for i, e in enumerate(anims):
            a = f"{tag}_ANIM_{sym(e['name'])}"
            out.append(f"#define {a} {i}")
            out.append(f"#define {a}_FRAMES {e['frames']}")
            out.append(f"#define {a}_STYLE {STYLES[e.get('style', 'loop')]}")
            out.append(f"#define {a}_MS_PER_FRAME {e.get('ms_per_frame', 0)}")
            out.append(f'#define {a}_SPRITE_FILE "{e["name"]}.png"')
            out.append(f'#define {a}_DATA_FILE "{e["name"]}.anim"')
            out.append(f'#define {a}_SPRITE_ASSET '
                       f'((Asset){{.filename = "{e["name"]}.png", '
                       f'.directory = "{rel_dir}"}})')
            out.append(f'#define {a}_DATA_ASSET '
                       f'((Asset){{.filename = "{e["name"]}.anim", '
                       f'.directory = "{rel_dir}"}})')
            # A SceneAnimSpec initializer (frames, style, sprite, data) — a
            # scene lists these and lets the framework make/load the animation
            # (SCENES.md milestone 1). Plain brace inits (not the _ASSET
            # compound literals) so the whole thing is a constant aggregate
            # initializer valid at file scope.
            out.append(f'#define {a}_SPEC '
                       f'{{{e["frames"]}, {STYLES[e.get("style", "loop")]}, '
                       f'{{"{e["name"]}.png", "{rel_dir}"}}, '
                       f'{{"{e["name"]}.anim", "{rel_dir}"}}}}')
        out.append(f"#define {tag}_ANIMS_COUNT {len(anims)}")
    out.append("")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with open(args.manifest) as f:
        manifest = json.load(f)
    prefix = manifest.get("prefix", sym(manifest["adventure"]))

    # Group runtime-relevant entries by their runtime dir, preserving order.
    groups = {}
    for entry in manifest["assets"]:
        if entry["type"] == "voice":
            continue  # per-line recordings: not runtime assets yet
        if not entry.get("runtime", True):
            continue  # authoring-only: the game shows a derived asset (a boil)
        rel_dir = runtime_dir(entry)
        validate(root, manifest, entry, rel_dir)
        groups.setdefault(rel_dir, []).append(entry)

    guard = f"GEN_{prefix}_ASSETS_H"
    out = [
        "// Generated by tools/gen_asset_decls.py from "
        + os.path.relpath(args.manifest, root)
        + " — do not edit.",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
    ]
    for rel_dir in sorted(groups):
        emit_group(out, prefix, rel_dir, groups[rel_dir])
    out.append(f"#endif // {guard}")

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    with open(args.out, "w") as f:
        f.write("\n".join(out) + "\n")
    print(f"{args.out}: {sum(len(v) for v in groups.values())} assets in "
          f"{len(groups)} dirs")


if __name__ == "__main__":
    main()
