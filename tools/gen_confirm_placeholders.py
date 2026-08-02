#!/usr/bin/env python3
"""Generate the placeholder art for the leave-the-adventure question.

The back-to-hub button asks before it leaves (src/confirm.c). The question is
wordless, because the audience cannot read: a panel with one button meaning
"yes, leave" and another "no, stay". Each piece is whole — the panel is its
own picture, each button is background and glyph in one image — so the drawn
art can change shape, colour and framing without the engine knowing more than
where each goes.

The two buttons are tappable, so each *is* a boil (LIVELINESS.md Part 3) —
written straight out as a 3-frame sheet, since the wobble is what says so. The
panel behind them is not tappable, and stays a still:

  tools/gen_confirm_placeholders.py --out assets/ui

These belong to the engine rather than to any adventure, so they live in the
repo-level assets/ tree beside the subtitle font, and are listed in
assets/index.json for the *Assets to author* page.

Requires Pillow.
"""

import argparse
import os

from gen_boil_sheet import write_boil_sheet

from PIL import Image, ImageDraw

SIZE = (120, 120)
RADIUS = 18
GLYPH = (255, 255, 255, 255)
YES_FILL = (63, 169, 85, 255)
NO_FILL = (209, 75, 63, 255)

# The panel the two answers sit on; must match PANEL in src/confirm.c.
PANEL_SIZE = (420, 220)
PANEL_RADIUS = 24
PANEL_FILL = (244, 241, 232, 255)
PANEL_EDGE = (51, 51, 51, 255)


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


def panel():
    """The card the question sits on, drawn as one picture behind the answers."""
    image = Image.new("RGBA", PANEL_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([0, 0, PANEL_SIZE[0] - 1, PANEL_SIZE[1] - 1],
                           PANEL_RADIUS, fill=PANEL_FILL, outline=PANEL_EDGE,
                           width=3)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True,
                        help="the engine UI asset dir (assets/ui)")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    path = os.path.join(args.out, "confirm_panel.png")
    panel().save(path)
    print(f"  {path} ({PANEL_SIZE[0]}x{PANEL_SIZE[1]})")
    for name, make in (("confirm_yes", yes), ("confirm_no", no)):
        write_boil_sheet(make(), os.path.join(args.out, name + "_boil"))


if __name__ == "__main__":
    main()
