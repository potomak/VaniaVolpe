#!/usr/bin/env python3
"""Generate the placeholder art for the adventure-selection screen.

The selection screen is a home screen of **cartridges** (see hub.c): one shared
cartridge drawing, tinted per adventure, with a hole in the middle showing that
adventure's icon. This emits everything that screen draws:

- the screen background;
- the cartridge, as a boil sheet — it is what you tap, and the wobble is what
  says so. Drawn light and neutral: the per-adventure colour is applied by
  multiplying the texture, so a dark body would swallow the tint. The hole is
  fully transparent, and the icon is drawn behind it;
- the gear / question-mark / exit buttons in the corner, also boils;
- each adventure's icon, once per locale, since it carries the adventure's
  name.

  tools/gen_hub_placeholders.py            # everything
  tools/gen_hub_placeholders.py --chrome   # just the screen's own art
  tools/gen_hub_placeholders.py --icons    # just the per-adventure icons

Sizes here are the contract with hub.c: CARTRIDGE and ICON must match the
constants there, or the icon will not sit in the hole.

Requires Pillow.
"""

import argparse
import os

from gen_boil_sheet import write_boil_sheet

from PIL import Image, ImageDraw

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SCREEN = (800, 600)
CARTRIDGE = (220, 200)
ICON = (180, 120)
# Where the hole sits inside the cartridge — the icon's top-left, cartridge-
# relative. Mirrors ICON_OFFSET in hub.c.
HOLE_AT = (20, 22)
BUTTON = (56, 56)

CHROME_DIR = "assets/hub"

# Kept light so the per-adventure colour multiplies visibly.
BODY = (222, 222, 226, 255)
BODY_EDGE = (150, 150, 158, 255)
GRIP = (198, 198, 204, 255)

BACKDROP_TOP = (46, 58, 84, 255)
BACKDROP_BOTTOM = (26, 32, 48, 255)

BUTTON_FILL = (238, 238, 242, 255)
BUTTON_INK = (60, 64, 76, 255)

# One placeholder icon per adventure: a flat plate in the adventure's own
# colour. The shipped ones carry the adventure's name, so they are drawn once
# per locale; the depth demo's never will be, so it sits in common/.
LOCALES = ["it_IT", "en_US"]
ADVENTURES = [
    ("src/adventures/vania_fox_the_slide/assets", (226, 138, 60, 255), LOCALES),
    ("src/adventures/gina_hen_at_the_pool/assets", (78, 168, 216, 255), LOCALES),
    ("src/adventures/depth_demo/assets", (154, 154, 164, 255), ["common"]),
]


def background():
    """A plain vertical wash — a stand-in for whatever room this becomes."""
    image = Image.new("RGBA", SCREEN, BACKDROP_TOP)
    draw = ImageDraw.Draw(image)
    for y in range(SCREEN[1]):
        t = y / (SCREEN[1] - 1)
        draw.line(
            [(0, y), (SCREEN[0], y)],
            fill=tuple(round(a + (b - a) * t)
                       for a, b in zip(BACKDROP_TOP, BACKDROP_BOTTOM)))
    return image


def cartridge():
    """The shell: a rounded body, a grip along the bottom, a hole in the middle."""
    w, h = CARTRIDGE
    image = Image.new("RGBA", CARTRIDGE, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([0, 0, w - 1, h - 1], 14, fill=BODY,
                           outline=BODY_EDGE, width=3)
    # The ridged grip a cartridge is pushed by, along the bottom edge.
    for i in range(6):
        x = 24 + i * 30
        draw.rounded_rectangle([x, h - 42, x + 20, h - 16], 4, fill=GRIP)
    # The hole: punched out, so whatever is drawn behind shows through.
    hx, hy = HOLE_AT
    draw.rectangle([hx, hy, hx + ICON[0] - 1, hy + ICON[1] - 1], fill=(0, 0, 0, 0))
    return image


def button(glyph):
    """A round corner button carrying one wordless glyph."""
    w, h = BUTTON
    image = Image.new("RGBA", BUTTON, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse([0, 0, w - 1, h - 1], fill=BUTTON_FILL, outline=BUTTON_INK,
                 width=2)
    glyph(draw, w, h)
    return image


def gear_glyph(draw, w, h):
    cx, cy = w / 2, h / 2
    draw.ellipse([cx - 15, cy - 15, cx + 15, cy + 15], outline=BUTTON_INK,
                 width=6)
    for dx, dy in ((0, -19), (0, 19), (-19, 0), (19, 0)):
        draw.rectangle([cx + dx - 4, cy + dy - 4, cx + dx + 4, cy + dy + 4],
                       fill=BUTTON_INK)
    draw.ellipse([cx - 5, cy - 5, cx + 5, cy + 5], fill=BUTTON_FILL)


def help_glyph(draw, w, h):
    cx, cy = w / 2, h / 2
    draw.arc([cx - 12, cy - 20, cx + 12, cy + 2], start=170, end=20,
             fill=BUTTON_INK, width=6)
    draw.line([(cx + 1, cy - 1), (cx + 1, cy + 8)], fill=BUTTON_INK, width=6)
    draw.ellipse([cx - 3, cy + 13, cx + 4, cy + 20], fill=BUTTON_INK)


def exit_glyph(draw, w, h):
    cx, cy = w / 2, h / 2
    draw.line([(cx - 12, cy - 12), (cx + 12, cy + 12)], fill=BUTTON_INK,
              width=6)
    draw.line([(cx + 12, cy - 12), (cx - 12, cy + 12)], fill=BUTTON_INK,
              width=6)


def icon(fill):
    """Stand-in for the drawn icon: a plate in a pale wash of the adventure's
    colour, so it reads as a picture in the hole rather than more cartridge —
    the shell is tinted with that same colour."""
    pale = tuple(round(c + (255 - c) * 0.72) for c in fill[:3]) + (255,)
    image = Image.new("RGBA", ICON, (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle([0, 0, ICON[0] - 1, ICON[1] - 1], 8, fill=pale,
                           outline=fill, width=4)
    return image


def write_chrome():
    out = os.path.join(REPO, CHROME_DIR)
    os.makedirs(out, exist_ok=True)
    path = os.path.join(out, "background.png")
    background().save(path)
    print(f"  {path} ({SCREEN[0]}x{SCREEN[1]})")
    write_boil_sheet(cartridge(), os.path.join(out, "cartridge_boil"))
    for name, glyph in (("gear", gear_glyph), ("help", help_glyph),
                        ("exit", exit_glyph)):
        write_boil_sheet(button(glyph), os.path.join(out, name + "_boil"))


def write_icons():
    for assets_root, fill, layers in ADVENTURES:
        for locale in layers:
            out = os.path.join(REPO, assets_root, locale, "hub")
            os.makedirs(out, exist_ok=True)
            path = os.path.join(out, "icon.png")
            icon(fill).save(path)
            print(f"  {path} ({ICON[0]}x{ICON[1]})")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--chrome", action="store_true",
                        help="only the selection screen's own art")
    parser.add_argument("--icons", action="store_true",
                        help="only the per-adventure cartridge icons")
    args = parser.parse_args()
    both = not args.chrome and not args.icons
    if both or args.chrome:
        write_chrome()
    if both or args.icons:
        write_icons()


if __name__ == "__main__":
    main()
