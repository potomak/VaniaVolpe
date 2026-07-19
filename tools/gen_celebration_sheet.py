#!/usr/bin/env python3
"""Generate a placeholder celebration (confetti burst) sprite sheet (#116).

Frames of confetti pieces radiating out from the centre and fading — the
reward animation a minigame plays over its focus area on completion. Packed
vertically with a matching .anim, one-shot style; real drawn frames replace
the files one-for-one via the asset pipeline.

Usage:
  tools/gen_celebration_sheet.py --palette sun --out <dir>/celebration
  (writes <dir>/celebration.png and <dir>/celebration.anim)

Needs Pillow.
"""

import argparse
import math
import random

from PIL import Image, ImageDraw

SIZE = 240
FRAMES = 8
PARTICLES = 26
PALETTES = {
    # Sunny yellows/oranges for the sunscreen close-up.
    "sun": [(255, 205, 66), (255, 158, 27), (255, 240, 180), (255, 111, 60)],
    # Grape purples and vine greens for the picking minigame.
    "grape": [(154, 78, 174), (106, 27, 154), (139, 195, 74), (233, 213, 255)],
    # Pool blues and foam whites for the poolside goggles reward.
    "water": [(66, 165, 245), (3, 155, 229), (129, 212, 250), (225, 245, 254)],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--palette", choices=sorted(PALETTES), required=True)
    parser.add_argument("--out", required=True,
                        help="output basename (no extension)")
    args = parser.parse_args()

    rng = random.Random(args.palette)  # deterministic per palette
    colors = PALETTES[args.palette]
    parts = []
    for _ in range(PARTICLES):
        angle = rng.uniform(0, 2 * math.pi)
        parts.append({
            "angle": angle,
            "speed": rng.uniform(45, 110),  # px/s of sheet space
            "size": rng.randint(5, 10),
            "color": rng.choice(colors),
            "spin": rng.uniform(-4, 4),
        })

    sheet = Image.new("RGBA", (SIZE, SIZE * FRAMES), (0, 0, 0, 0))
    cx, cy = SIZE / 2, SIZE / 2
    for f in range(FRAMES):
        t = f / 12.0  # engine plays sheets at 12 FPS
        frame = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
        draw = ImageDraw.Draw(frame)
        alpha = 255 if f < FRAMES - 3 else int(255 * (FRAMES - 1 - f) / 3)
        for p in parts:
            d = p["speed"] * t
            x = cx + math.cos(p["angle"]) * d
            y = cy + math.sin(p["angle"]) * d + 30 * t * t  # slight gravity
            s = p["size"]
            box = [x - s / 2, y - s / 2, x + s / 2, y + s / 2]
            if p["spin"] > 0:
                draw.ellipse(box, fill=p["color"] + (alpha,))
            else:
                draw.rectangle(box, fill=p["color"] + (alpha,))
        sheet.paste(frame, (0, f * SIZE))

    sheet.save(args.out + ".png")
    with open(args.out + ".anim", "w") as fh:
        for f in range(FRAMES):
            fh.write(f"0,{f * SIZE},{SIZE},{SIZE}\n")
    print(f"{args.out}.png: {FRAMES} frames ({args.palette})")


if __name__ == "__main__":
    main()
