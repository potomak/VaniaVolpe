#!/usr/bin/env python3
"""Generate placeholder "worn item" overlays for an actor.

An item the actor wears once she has collected it (Gina's swimming goggles, her
pool float) is drawn as an overlay on top of her sprite. The overlay is authored
**on the actor's own frame** — same size as her animation frames — so it
registers with her by construction: no per-item offset to tune, and it scales
and anchors exactly like her sprite (see actor_render_carried).

The positions below were measured off the hen's idle frame: her eye sits at
(48, 41) and her body centre at (69, 68) on a 120x120 frame. Real drawn overlays
replace these file-for-file via the asset pipeline (ASSETS.md).

Usage:
  tools/gen_worn_placeholders.py --out <assets>/common/hen

Requires Pillow.
"""

import argparse
import os

from PIL import Image, ImageDraw

FRAME = (120, 120)

EYE = (48, 41)   # where her eye is on the frame
BODY = (69, 82)  # her middle, a little below the bbox centre so a ring sits right

GOGGLE_GLASS = (90, 200, 235, 210)
GOGGLE_STRAP = (40, 60, 90, 255)
FLOAT_RING = (235, 90, 80, 235)
FLOAT_STRIPE = (250, 250, 250, 235)


def goggles():
    """A strap across her head with two lenses over the eye."""
    image = Image.new("RGBA", FRAME, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    x, y = EYE
    # The strap runs back from the lenses over her head.
    draw.line([(x - 12, y - 2), (x + 30, y - 6)], fill=GOGGLE_STRAP, width=5)
    for cx in (x - 6, x + 9):
        draw.ellipse([cx - 8, y - 8, cx + 8, y + 8], fill=GOGGLE_GLASS,
                     outline=GOGGLE_STRAP, width=2)
    return image


def pool_float():
    """A ring around her middle, drawn as an ellipse seen slightly from above."""
    image = Image.new("RGBA", FRAME, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    x, y = BODY
    outer = [x - 40, y - 18, x + 40, y + 18]
    inner = [x - 22, y - 9, x + 22, y + 9]
    draw.ellipse(outer, fill=FLOAT_RING)
    # Two stripes, the classic swim-ring look.
    draw.pieslice(outer, 200, 250, fill=FLOAT_STRIPE)
    draw.pieslice(outer, 20, 70, fill=FLOAT_STRIPE)
    draw.ellipse(inner, fill=(0, 0, 0, 0))
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True,
                        help="the actor's asset dir (e.g. <assets>/common/hen)")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    for name, make in (("goggles_worn", goggles), ("float_worn", pool_float)):
        path = os.path.join(args.out, name + ".png")
        make().save(path)
        print(f"  {path} ({FRAME[0]}x{FRAME[1]})")


if __name__ == "__main__":
    main()
