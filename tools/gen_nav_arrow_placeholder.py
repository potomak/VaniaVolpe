#!/usr/bin/env python3
"""Generate the placeholder "way out" arrow for Gina's scene edges.

Her three outdoor scenes form a ring, and each leaves by walking off the left
or right edge. Nothing marked those edges, so there was no way to tell they
were there. One arrow drawn pointing **right** covers both: the left-hand exit
draws the same sheet mirrored (see gina_nav.c), the way an actor's sheet is
mirrored to turn her around.

Real drawn art replaces this file-for-file via the asset pipeline (ASSETS.md);
run `tools/gen_boil_sheet.py --amp 0.4 <out>/arrow.png` afterwards to refresh
the boil that makes it squiggle. The amplitude is deliberately below the
default: a hand-drawn object survives a big wobble, but a crisp geometric
arrow just looks broken.

Usage:
  tools/gen_nav_arrow_placeholder.py --out <assets>/common/nav

Requires Pillow.
"""

import argparse
import os

from PIL import Image, ImageDraw

SIZE = (48, 64)

FILL = (255, 236, 150, 230)
EDGE = (120, 88, 20, 255)


def arrow(w, h):
    """A fat chevron pointing right, readable at a glance from across a room."""
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    mid = h / 2
    # A blocky arrow: a shaft out of a broad head, so it stays legible small.
    draw.polygon(
        [(4, mid - h * 0.16), (w * 0.5, mid - h * 0.16), (w * 0.5, 6),
         (w - 4, mid), (w * 0.5, h - 6), (w * 0.5, mid + h * 0.16),
         (4, mid + h * 0.16)],
        fill=FILL, outline=EDGE)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True,
                        help="the nav asset dir (e.g. <assets>/common/nav)")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    path = os.path.join(args.out, "arrow.png")
    arrow(*SIZE).save(path)
    print(f"  {path} ({SIZE[0]}x{SIZE[1]})")


if __name__ == "__main__":
    main()
