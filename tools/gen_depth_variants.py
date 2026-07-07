#!/usr/bin/env python3
"""Generate placeholder depth-variant sprites (see DEPTH_AND_CAMERA.md).

Given a variant-0 sprite sheet and its .anim, this emits a downscaled copy of
both — a sheet whose frames are each frame of the original resized by
--scale (nearest-neighbour, so the hand-drawn look survives), repacked as the
usual vertical strip, plus a matching .anim. The output slots straight into an
ActorVariantSpec as a far/mid variant.

These are *placeholders to develop and test depth bands against*: real
far/mid art replaces them file-for-file (same names, same .anim shape). File
the art request as a `needs-art` issue when a scene adopts bands.

Frames are cropped by the source .anim rects, resized independently, and
repacked at (0, i * new_h), so the emitted rects are exact whatever rounding
does to the frame size.

Usage:
  tools/gen_depth_variants.py --scale 0.6 --suffix _far \
      src/adventures/vania_fox_the_slide/assets/common/fox/walking.png ...

Each argument is a sprite sheet PNG; its .anim is expected next to it. The
output <name><suffix>.png / <name><suffix>.anim land in the same directory.

Requires Pillow (pip install Pillow).
"""

import argparse
import os
import sys

from PIL import Image


def parse_anim(path):
    rects = []
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            x, y, w, h = (int(n) for n in line.split(","))
            rects.append((x, y, w, h))
    if not rects:
        sys.exit(f"{path}: no frames")
    return rects


def scale_sheet(sheet_path, scale, suffix):
    anim_path = os.path.splitext(sheet_path)[0] + ".anim"
    rects = parse_anim(anim_path)
    sheet = Image.open(sheet_path).convert("RGBA")

    frames = []
    for x, y, w, h in rects:
        frame = sheet.crop((x, y, x + w, y + h))
        size = (max(1, round(w * scale)), max(1, round(h * scale)))
        frames.append(frame.resize(size, Image.NEAREST))

    out_w = max(f.width for f in frames)
    out_h = sum(f.height for f in frames)
    out = Image.new("RGBA", (out_w, out_h), (0, 0, 0, 0))
    out_rects = []
    y = 0
    for frame in frames:
        out.paste(frame, (0, y))
        out_rects.append((0, y, frame.width, frame.height))
        y += frame.height

    base = os.path.splitext(sheet_path)[0] + suffix
    out.save(base + ".png")
    with open(base + ".anim", "w") as f:
        for rect in out_rects:
            f.write(",".join(str(n) for n in rect) + "\n")
    print(f"{base}.png ({out_w}x{out_h}) + {base}.anim ({len(out_rects)} frames)")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("sheets", nargs="+", help="sprite sheet PNGs")
    parser.add_argument("--scale", type=float, default=0.6,
                        help="frame scale factor (default 0.6)")
    parser.add_argument("--suffix", default="_far",
                        help="output name suffix (default _far)")
    args = parser.parse_args()
    if not 0 < args.scale < 1:
        sys.exit("--scale must be between 0 and 1")
    for sheet in args.sheets:
        scale_sheet(sheet, args.scale, args.suffix)


if __name__ == "__main__":
    main()
