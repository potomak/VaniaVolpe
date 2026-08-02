#!/usr/bin/env python3
"""Generate the placeholder "somewhere to go" tiles for Gina's scenes.

Her three outdoor scenes form a ring. Each shows, up near the horizon, a tile
standing in for each place it connects to — an abstract view of somewhere else
in the world, tapped to walk there. One tile per **destination**, so its colour
means the same thing in every scene it appears in: blue is the pool, brown the
tree, green the vine.

These are flat squares only until the real art is drawn. A tile *is* its boil
(see LIVELINESS.md Part 3) — the wobble is what says "tappable" — so each is
written straight out as a 3-frame sheet, which the real traced frames then
replace file-for-file:

  tools/gen_nav_square_placeholders.py --out <assets>/common/nav

Requires Pillow.
"""

import argparse
import os

from gen_boil_sheet import write_boil_sheet

from PIL import Image, ImageDraw

SIZE = (90, 90)

# One colour per destination, not per scene, so a tile reads the same wherever
# it appears: the pool's water, the tree's trunk, the vine's leaves.
DESTINATIONS = {
    "to_pool": ((70, 150, 210, 255), (30, 90, 150, 255)),
    "to_tree": ((150, 105, 60, 255), (95, 62, 30, 255)),
    "to_vine": ((105, 165, 75, 255), (60, 105, 40, 255)),
}


def tile(w, h, fill, edge):
    """A flat square with a darker rim, so it reads as a thing and not a hole."""
    image = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rectangle([0, 0, w - 1, h - 1], fill=fill, outline=edge, width=3)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True,
                        help="the nav asset dir (e.g. <assets>/common/nav)")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    for name, (fill, edge) in DESTINATIONS.items():
        write_boil_sheet(tile(*SIZE, fill, edge),
                         os.path.join(args.out, name + "_boil"))


if __name__ == "__main__":
    main()
