#!/usr/bin/env python3
"""Generate a placeholder "boil" sheet from a still object PNG (LIVELINESS.md
Part 3 — boiling hotspots).

A boil is the hand-drawn trick for "this object is alive": the same drawing
traced a few times and cycled slowly, so its lines wobble while the object
stays put. This turns one flat PNG into an N-frame sheet where each frame is
the source nudged by a gentle sine wiggle (a per-row horizontal wave + a
per-column vertical wave, ~1px, a different phase per frame), packed as the
usual vertical strip with a matching .anim.

Each frame keeps the source's exact width and height, so a scene swaps
`render_image(obj, AT)` for `render_animation(boil, AT)` with no change to the
object's position — only its name and the animations table.

These are *placeholders to develop and test the boil against*: real traced
frames replace them file-for-file (same name, same .anim shape). File the art
as a `needs-art` issue (see #95).

Usage:
  tools/gen_boil_sheet.py [--frames 3] [--amp 1.0] [--wavelength 14] \
      src/adventures/gina_hen_at_the_pool/assets/common/pool/sunscreen.png ...

Each argument is a still object PNG; <name>_boil.png / <name>_boil.anim land
next to it. The scene plays the sheet at BOIL_MS_PER_FRAME (constants.h).

Requires Pillow (pip install Pillow).
"""

import argparse
import math
import os

from PIL import Image


# One boil frame: shift each row horizontally and each column vertically by a
# small sine wave, so the drawing's lines wobble without the shape drifting.
# The frame stays the source's size; sub-pixel edges that fall off are ~1px
# and imperceptible on these hand-drawn blobs.
def boil_frame(src, phase, amp, wavelength):
    w, h = src.size
    horizontal = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for y in range(h):
        dx = round(amp * math.sin(2 * math.pi * y / wavelength + phase))
        horizontal.paste(src.crop((0, y, w, y + 1)), (dx, y))
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    for x in range(w):
        dy = round(amp * math.cos(2 * math.pi * x / wavelength + phase * 1.3))
        out.paste(horizontal.crop((x, 0, x + 1, h)), (x, dy))
    return out


def boil_sheet(png_path, frames, amp, wavelength):
    src = Image.open(png_path).convert("RGBA")
    w, h = src.size

    # Phases spread evenly around the circle so the cycle returns smoothly.
    made = [
        boil_frame(src, 2 * math.pi * i / frames, amp, wavelength)
        for i in range(frames)
    ]

    sheet = Image.new("RGBA", (w, h * frames), (0, 0, 0, 0))
    rects = []
    for i, frame in enumerate(made):
        sheet.paste(frame, (0, i * h))
        rects.append((0, i * h, w, h))

    base = os.path.splitext(png_path)[0] + "_boil"
    sheet.save(base + ".png")
    with open(base + ".anim", "w") as f:
        for rect in rects:
            f.write(",".join(str(n) for n in rect) + "\n")
    print(f"{base}.png ({w}x{h * frames}) + {base}.anim ({frames} frames)")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("pngs", nargs="+", help="still object PNGs")
    parser.add_argument("--frames", type=int, default=3,
                        help="frames in the boil cycle (default 3)")
    parser.add_argument("--amp", type=float, default=1.0,
                        help="wiggle amplitude in px (default 1.0)")
    parser.add_argument("--wavelength", type=float, default=14.0,
                        help="wiggle wavelength in px (default 14)")
    args = parser.parse_args()
    if args.frames < 2:
        parser.error("--frames must be at least 2")
    for png in args.pngs:
        boil_sheet(png, args.frames, args.amp, args.wavelength)


if __name__ == "__main__":
    main()
