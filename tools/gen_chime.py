#!/usr/bin/env python3
"""Generate the placeholder minigame completion chime (see issue #116).

A short ascending three-note arpeggio (C5 E5 G5) with a soft attack and an
exponential ring-out — a friendly "you did it!" sound to develop the reward
moment against until a real recording lands (it replaces this file
file-for-file via the asset pipeline).

Usage:
  tools/gen_chime.py --out <assets>/common/minigames/chime.wav

Stdlib only (wave + math).
"""

import argparse
import math
import struct
import wave

RATE = 22050
NOTES = [523.25, 659.25, 783.99]  # C5, E5, G5
NOTE_SPACING = 0.12  # seconds between note onsets
NOTE_LENGTH = 0.5  # each note rings this long
ATTACK = 0.012  # soft attack, no click
AMPLITUDE = 0.28  # per note; three overlap without clipping


def note_sample(freq, t):
    if t < 0 or t >= NOTE_LENGTH:
        return 0.0
    attack = min(t / ATTACK, 1.0)
    decay = math.exp(-5.0 * t / NOTE_LENGTH)
    return AMPLITUDE * attack * decay * math.sin(2 * math.pi * freq * t)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--out", required=True)
    args = parser.parse_args()

    length = NOTE_SPACING * (len(NOTES) - 1) + NOTE_LENGTH
    frames = bytearray()
    for i in range(int(length * RATE)):
        t = i / RATE
        sample = sum(
            note_sample(freq, t - n * NOTE_SPACING)
            for n, freq in enumerate(NOTES))
        frames += struct.pack("<h", int(max(-1.0, min(1.0, sample)) * 32767))

    with wave.open(args.out, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))
    print(f"{args.out}: {length:.2f}s chime")


if __name__ == "__main__":
    main()
