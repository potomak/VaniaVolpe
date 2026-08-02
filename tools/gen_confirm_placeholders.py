#!/usr/bin/env python3
"""Generate the placeholder art for leaving an adventure.

Two pieces: the corner button that asks, and the question it raises.

The button (assets/ui/hub_button.png) is the one tappable thing in the game
that does *not* boil. It sits over a scene whose own hotspots are boiling to
say "tap me", and a wobble in the corner would pull the eye away from them —
leaving is not what we want to advertise.

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
# The corner button, sized to HUB_BUTTON_SIZE in src/game.c.
HUB_BUTTON_SIZE = (64, 64)
HUB_BUTTON_FILL = (51, 51, 51, 216)
HUB_BUTTON_INK = (244, 241, 232, 255)
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


def hub_button():
    """The corner button: a house, for the place the adventures are chosen
    from. Deliberately quiet — dark and semi-transparent, so it reads as chrome
    over whatever scene is behind it rather than as part of the scene."""
    w, h = HUB_BUTTON_SIZE
    image = Image.new("RGBA", HUB_BUTTON_SIZE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([0, 0, w - 1, h - 1], 14, fill=HUB_BUTTON_FILL)
    # Roof, then the body under it.
    draw.polygon([(12, 33), (32, 15), (52, 33)], fill=HUB_BUTTON_INK)
    draw.rectangle([19, 33, 45, 49], fill=HUB_BUTTON_INK)
    draw.rectangle([28, 39, 36, 49], fill=HUB_BUTTON_FILL)
    return image


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True,
                        help="the engine UI asset dir (assets/ui)")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    for name, make, size in (("hub_button", hub_button, HUB_BUTTON_SIZE),
                             ("confirm_panel", panel, PANEL_SIZE)):
        path = os.path.join(args.out, name + ".png")
        make().save(path)
        print(f"  {path} ({size[0]}x{size[1]})")
    for name, make in (("confirm_yes", yes), ("confirm_no", no)):
        write_boil_sheet(make(), os.path.join(args.out, name + "_boil"))


if __name__ == "__main__":
    main()
