#!/usr/bin/env python3
"""Generate the placeholder answers for the leave-the-adventure question.

The back-to-hub button asks before it leaves (src/confirm.c). The question is
wordless, because the audience cannot read: one button means "yes, leave", the
other "no, stay". Each is a whole button — background and glyph in one image —
so the drawn art can change the shape, colour and framing without the engine
knowing.

These belong to the engine rather than to any adventure, so they live in the
repo-level assets/ tree beside the subtitle font, and are listed in
assets/index.json for the *Assets to author* page.

Usage:
  tools/gen_confirm_placeholders.py --out assets/ui

Requires Pillow.
"""

import argparse
import os

from PIL import Image, ImageDraw

SIZE = (120, 120)
RADIUS = 18
GLYPH = (255, 255, 255, 255)
YES_FILL = (63, 169, 85, 255)
NO_FILL = (209, 75, 63, 255)


def button(fill):
    image = Image.new("RGBA", SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([0, 0, SIZE[0] - 1, SIZE[1] - 1], RADIUS, fill=fill)
    return image, draw


def yes():
    """A tick: a short stroke down-right, a long one up-right."""
    image, draw = button(YES_FILL)
    draw.line([(28, 62), (52, 86)], fill=GLYPH, width=14)
    draw.line([(50, 86), (94, 36)], fill=GLYPH, width=14)
    # Round the join and the ends, which .line leaves square.
    for x, y in ((28, 62), (51, 86), (94, 36)):
        draw.ellipse([x - 7, y - 7, x + 7, y + 7], fill=GLYPH)
    return image


def no():
    """A cross: two strokes through the middle."""
    image, draw = button(NO_FILL)
    draw.line([(36, 36), (84, 84)], fill=GLYPH, width=14)
    draw.line([(84, 36), (36, 84)], fill=GLYPH, width=14)
    for x, y in ((36, 36), (84, 84), (84, 36), (36, 84)):
        draw.ellipse([x - 7, y - 7, x + 7, y + 7], fill=GLYPH)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True,
                        help="the engine UI asset dir (assets/ui)")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    for name, make in (("confirm_yes", yes), ("confirm_no", no)):
        path = os.path.join(args.out, name + ".png")
        make().save(path)
        print(f"  {path} ({SIZE[0]}x{SIZE[1]})")


if __name__ == "__main__":
    main()
