#!/usr/bin/env python3
#
# gen_asset_catalog.py
# Build the data for the GitHub Pages asset catalog (catalog.html).
#
# Walks every adventure's assets/ tree, copies the raw files into the published
# web dir (so the browser can fetch them individually -- the game itself bundles
# them into the opaque index.data, which the catalog can't read), and emits a
# catalog.json manifest describing them grouped by adventure -> layer (common +
# each locale) -> scene. Sprite sheets that have a sibling .anim are tagged as
# playable animations (with their frame rectangles); .wav as playable audio;
# other .png as static images.
#
# It also diffs each locale against the reference locale (it_IT) so the page can
# flag any localized asset that is present in the reference but missing here.
#
# Requires only the Python 3 standard library. Run from anywhere.

import argparse
import json
import os
import re
import shutil
import struct
import sys
from datetime import datetime, timezone

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
ADVENTURES_DIR = os.path.join(ROOT, "src", "adventures")
COMMON_LAYER = "common"
REFERENCE_LOCALE = "it_IT"

# A .png is a playable animation when a sibling "<base>.anim" exists; image.c
# reads that CSV (one "x,y,w,h" frame rect per line) at 12 FPS.
ANIM_EXT = ".anim"
JSON_EXT = ".json"


def adventure_title(adv_dir, adv_name):
    """Read the adventure module's `.title = "..."`, else prettify the dir name."""
    c_file = os.path.join(adv_dir, adv_name + ".c")
    try:
        with open(c_file, encoding="utf-8") as handle:
            match = re.search(r'\.title\s*=\s*"([^"]*)"', handle.read())
            if match:
                return match.group(1)
    except OSError:
        pass
    return adv_name.replace("_", " ").title()


def png_size(path):
    """Return (width, height) from a PNG's IHDR, or None if unreadable."""
    try:
        with open(path, "rb") as handle:
            header = handle.read(24)
        if header[:8] == b"\x89PNG\r\n\x1a\n" and header[12:16] == b"IHDR":
            return struct.unpack(">II", header[16:24])
    except OSError:
        pass
    return None


def parse_anim(path):
    """Parse an .anim CSV into a list of {x,y,w,h} frame rectangles."""
    frames = []
    try:
        with open(path, encoding="utf-8") as handle:
            for line in handle:
                line = line.strip()
                if not line:
                    continue
                parts = line.split(",")
                if len(parts) != 4:
                    continue
                x, y, w, h = (int(p) for p in parts)
                frames.append({"x": x, "y": y, "w": w, "h": h})
    except (OSError, ValueError):
        pass
    return frames


def list_layers(assets_dir):
    """Return (locales, layers) for an adventure: common first, then locales."""
    entries = sorted(
        name
        for name in os.listdir(assets_dir)
        # Skip the asset-pipeline drop-boxes (_inbox / _sources): they hold raw
        # uploads, not shippable layers, and must not be mistaken for locales.
        if os.path.isdir(os.path.join(assets_dir, name))
        and not name.startswith("_")
    )
    locales = [name for name in entries if name != COMMON_LAYER]
    layers = ([COMMON_LAYER] if COMMON_LAYER in entries else []) + locales
    return locales, layers


def scene_of(layer_dir, file_path):
    """Group key for an asset: its directory relative to the layer root."""
    rel = os.path.relpath(os.path.dirname(file_path), layer_dir)
    return "(root)" if rel == "." else rel.replace(os.sep, "/")


def collect_layer(adv_id, layer, layer_dir, url_base):
    """Build the scene->assets structure for one layer, plus its relpath set."""
    # Index every file so animations can absorb their .anim/.json siblings.
    files = []
    for dirpath, _dirs, filenames in os.walk(layer_dir):
        for name in sorted(filenames):
            files.append(os.path.join(dirpath, name))
    present = {os.path.relpath(p, layer_dir).replace(os.sep, "/") for p in files}

    def url_for(path):
        return url_base + "/" + os.path.relpath(path, layer_dir).replace(os.sep, "/")

    scenes = {}
    consumed = set()  # .anim/.json files folded into an animation entry

    # First pass: animations and images (so we know which .anim/.json are used).
    for path in files:
        if not path.lower().endswith(".png"):
            continue
        base = path[:-4]
        anim_path = base + ANIM_EXT
        json_path = base + JSON_EXT
        scene = scene_of(layer_dir, path)
        entry = {
            "name": os.path.basename(path),
            "path": url_for(path),
            "bytes": os.path.getsize(path),
        }
        size = png_size(path)
        if size:
            entry["width"], entry["height"] = size[0], size[1]
        if os.path.exists(anim_path):
            entry["type"] = "animation"
            entry["anim_path"] = url_for(anim_path)
            entry["frames"] = parse_anim(anim_path)
            entry["frame_count"] = len(entry["frames"])
            consumed.add(anim_path)
            if os.path.exists(json_path):
                entry["json_path"] = url_for(json_path)
                consumed.add(json_path)
        else:
            entry["type"] = "image"
        scenes.setdefault(scene, []).append(entry)

    # Second pass: audio, and any orphan metadata not tied to a sprite sheet.
    for path in files:
        lower = path.lower()
        if lower.endswith(".png") or path in consumed:
            continue
        scene = scene_of(layer_dir, path)
        if lower.endswith(".wav"):
            kind = "audio"
        elif lower.endswith(ANIM_EXT) or lower.endswith(JSON_EXT):
            kind = "metadata"
        else:
            kind = "other"
        scenes.setdefault(scene, []).append(
            {
                "type": kind,
                "name": os.path.basename(path),
                "path": url_for(path),
                "bytes": os.path.getsize(path),
            }
        )

    scene_list = [
        {"name": scene, "assets": sorted(scenes[scene], key=lambda a: a["name"])}
        for scene in sorted(scenes)
    ]
    return scene_list, present


def main():
    parser = argparse.ArgumentParser(description="Generate the asset catalog data.")
    parser.add_argument(
        "--out",
        default=os.path.join(ROOT, "build", "web"),
        help="published web dir (receives assets/ and catalog.json)",
    )
    args = parser.parse_args()
    out_dir = os.path.abspath(args.out)
    assets_out = os.path.join(out_dir, "assets")
    os.makedirs(assets_out, exist_ok=True)

    adventures = []
    for adv_name in sorted(os.listdir(ADVENTURES_DIR)):
        adv_dir = os.path.join(ADVENTURES_DIR, adv_name)
        assets_dir = os.path.join(adv_dir, "assets")
        if not os.path.isdir(assets_dir):
            continue

        locales, layers = list_layers(assets_dir)
        adv_id = adv_name
        layer_data = []
        present_by_layer = {}
        type_counts, layer_counts, total = {}, {}, 0

        for layer in layers:
            layer_src = os.path.join(assets_dir, layer)
            layer_dst = os.path.join(assets_out, adv_id, layer)
            # Refresh the copy so removed source files don't linger in the site.
            if os.path.exists(layer_dst):
                shutil.rmtree(layer_dst)
            shutil.copytree(layer_src, layer_dst)

            url_base = "assets/" + adv_id + "/" + layer
            scenes, present = collect_layer(adv_id, layer, layer_src, url_base)
            present_by_layer[layer] = present

            count = sum(len(s["assets"]) for s in scenes)
            total += count
            layer_counts[layer] = count
            for scene in scenes:
                for asset in scene["assets"]:
                    type_counts[asset["type"]] = type_counts.get(asset["type"], 0) + 1

            layer_data.append(
                {
                    "name": layer,
                    "is_locale": layer != COMMON_LAYER,
                    "is_reference": layer == REFERENCE_LOCALE,
                    "scenes": scenes,
                    "count": count,
                }
            )

        # Diff each locale against the reference locale.
        reference = present_by_layer.get(REFERENCE_LOCALE, set())
        for layer in layer_data:
            if not layer["is_locale"]:
                continue
            here = present_by_layer.get(layer["name"], set())
            if layer["name"] == REFERENCE_LOCALE:
                layer["missing"], layer["extra"] = [], []
            else:
                layer["missing"] = sorted(reference - here)
                layer["extra"] = sorted(here - reference)

        adventures.append(
            {
                "id": adv_id,
                "title": adventure_title(adv_dir, adv_name),
                "dir": os.path.relpath(adv_dir, ROOT),
                "locales": locales,
                "reference_locale": REFERENCE_LOCALE,
                "layers": layer_data,
                "counts": {
                    "total": total,
                    "by_type": type_counts,
                    "by_layer": layer_counts,
                },
            }
        )

    manifest = {
        "generated": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "adventures": adventures,
        "totals": {
            "adventures": len(adventures),
            "assets": sum(a["counts"]["total"] for a in adventures),
        },
    }

    out_json = os.path.join(out_dir, "catalog.json")
    with open(out_json, "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    missing_total = sum(
        len(layer.get("missing", []))
        for adv in adventures
        for layer in adv["layers"]
    )
    print(
        "catalog: {adv} adventures, {assets} assets -> {path}".format(
            adv=manifest["totals"]["adventures"],
            assets=manifest["totals"]["assets"],
            path=os.path.relpath(out_json, ROOT),
        )
    )
    if missing_total:
        print("WARNING: {n} localized asset(s) missing vs {ref}".format(
            n=missing_total, ref=REFERENCE_LOCALE), file=sys.stderr)


if __name__ == "__main__":
    main()
