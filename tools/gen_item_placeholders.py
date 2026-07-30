#!/usr/bin/env python3
"""Generate placeholder art for the objects Gina picks up and carries.

Each item is **one drawing**, used everywhere it appears: lying in the scene
(where a boil sheet derived from it makes it squiggle, see gen_boil_sheet.py)
and riding on Gina once she has it (gina_worn.c draws it at an anchor on her
sprite). Authoring at object size rather than on her frame is what makes that
possible, and it keeps an item the same size in her wings as it was on the
ground.

Real drawn art replaces these file-for-file via the asset pipeline (ASSETS.md).

Usage:
  tools/gen_item_placeholders.py --out <assets>/common/items

Requires Pillow.
"""

import argparse
import os

from PIL import Image, ImageDraw

GOGGLE_GLASS = (90, 200, 235, 210)
GOGGLE_STRAP = (40, 60, 90, 255)
FLOAT_RING = (235, 90, 80, 235)
FLOAT_STRIPE = (250, 250, 250, 235)
BASKET_WEAVE = (176, 124, 66, 255)
BASKET_RIM = (128, 86, 44, 255)


def goggles(w, h):
    """Two lenses on a strap, seen head-on."""
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    mid = h // 2
    draw.line([(2, mid), (w - 3, mid)], fill=GOGGLE_STRAP, width=5)
    radius = h // 2 - 2
    for cx in (w // 2 - radius, w // 2 + radius):
        draw.ellipse([cx - radius, mid - radius, cx + radius, mid + radius],
                     fill=GOGGLE_GLASS, outline=GOGGLE_STRAP, width=2)
    return image


def pool_float(w, h):
    """A swim ring, seen slightly from above so it reads as a ring not a disc."""
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    outer = [0, 0, w - 1, h - 1]
    inner = [w * 0.25, h * 0.28, w * 0.75, h * 0.72]
    draw.ellipse(outer, fill=FLOAT_RING)
    # Two stripes, the classic swim-ring look.
    draw.pieslice(outer, 200, 250, fill=FLOAT_STRIPE)
    draw.pieslice(outer, 20, 70, fill=FLOAT_STRIPE)
    draw.ellipse(inner, fill=(0, 0, 0, 0))
    return image


def basket(w, h):
    """A little woven basket: tapered body, rim, and a handle to carry it by."""
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    rim, base = h * 0.4, h - 3
    # The handle first, so the rim covers where it enters the basket.
    draw.arc([w * 0.2, h * 0.08, w * 0.8, rim + 4], 180, 360,
             fill=BASKET_RIM, width=3)
    draw.polygon([(1, rim), (w - 2, rim), (w * 0.82, base), (w * 0.18, base)],
                 fill=BASKET_WEAVE, outline=BASKET_RIM)
    draw.line([(1, rim), (w - 2, rim)], fill=BASKET_RIM, width=4)
    # A few weave lines, fanning out with the taper.
    for i in (0.3, 0.5, 0.7):
        draw.line([(w * i, rim + 3), (w * (0.18 + i * 0.64), base - 1)],
                  fill=BASKET_RIM, width=1)
    return image


ITEMS = (
    ("goggles", goggles, (60, 30)),
    ("float", pool_float, (90, 60)),
    ("basket", basket, (50, 50)),
)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True,
                        help="the shared item dir (e.g. <assets>/common/items)")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    for name, make, (w, h) in ITEMS:
        path = os.path.join(args.out, name + ".png")
        make(w, h).save(path)
        print(f"  {path} ({w}x{h})")


if __name__ == "__main__":
    main()
