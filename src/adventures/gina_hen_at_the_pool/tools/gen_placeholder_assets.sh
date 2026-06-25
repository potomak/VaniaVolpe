#!/usr/bin/env bash
#
# gen_placeholder_assets.sh
# Regenerate the placeholder art and audio for "Gina la Gallina in Piscina".
#
# Every asset is a solid-colour PNG rectangle (one frame), a one-line .anim, or a
# short silent WAV, produced through the same image/sound pipeline the real art
# will use. Filling in real assets is therefore a file swap, no code change.
#
# Requires ImageMagick (`convert`) and Python 3 (for the silent WAV writer).
# Run from anywhere; paths are resolved relative to this script.
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"
assets="$root/assets"

# rect <path> <w> <h> <color>
#
# Force 32-bit RGBA (PNG32) output. A solid colour would otherwise be written as
# a 1-bit palette PNG, and the engine's cyan colour-key (SDL_MapRGB on the
# surface format) then snaps to the single palette entry and turns the whole
# texture transparent. Truecolor RGBA matches the real art and keys correctly.
rect() {
  convert -size "${2}x${3}" "xc:${4}" "PNG32:$1"
}

# anim <path> <w> <h>   (single-frame sprite sheet metadata: x,y,w,h)
anim() {
  printf '0,0,%s,%s\n' "$2" "$3" > "$1"
}

# silence <path> <seconds>   (22050 Hz, 16-bit, stereo silence)
silence() {
  python3 - "$1" "$2" <<'PY'
import struct, sys, wave
path, seconds = sys.argv[1], float(sys.argv[2])
rate, chans = 22050, 2
frames = int(rate * seconds)
with wave.open(path, "wb") as w:
    w.setnchannels(chans)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(b"\x00\x00" * chans * frames)
PY
}

mkdir -p "$assets"/{hen,pool,tree,vine,sunscreen,grapes}

# ── Gina the hen (the only actor): one solid frame per state, distinct colours ──
rect "$assets/hen/idle.png"    120 120 '#f4d03f'   # yellow hen
anim "$assets/hen/idle.anim"   120 120
rect "$assets/hen/walking.png" 120 120 '#e67e22'   # orange while walking
anim "$assets/hen/walking.anim" 120 120
rect "$assets/hen/talking.png" 120 120 '#e84393'   # pink while talking
anim "$assets/hen/talking.anim" 120 120
silence "$assets/hen/walking.wav" 0.4

# ── Pool scene ────────────────────────────────────────────────────────────────
rect "$assets/pool/background.png" 800 600 '#aee3f0'   # bright poolside
rect "$assets/pool/water.png"      460 180 '#2980b9'   # the pool water
rect "$assets/pool/sunscreen.png"   40  60 '#ecf0f1'   # lotion bottle
rect "$assets/pool/goggles.png"     60  30 '#1abc9c'   # goggles
rect "$assets/pool/float.png"       90  60 '#e74c3c'   # pool float
silence "$assets/pool/voice.wav" 0.6
silence "$assets/pool/wind.wav"  0.6
silence "$assets/pool/splash.wav" 0.7

# ── Tree scene ────────────────────────────────────────────────────────────────
rect "$assets/tree/background.png" 800 600 '#7dcea0'   # grassy area by a tree
rect "$assets/tree/float.png"       90  60 '#e74c3c'   # the float, stuck up high
rect "$assets/tree/carla.png"       70  70 '#2c3e50'   # Carla the crow
rect "$assets/tree/basket.png"      50  40 '#a0522d'   # basket Carla gives
silence "$assets/tree/voice.wav" 0.6
silence "$assets/tree/caw.wav"   0.6

# ── Vine scene ────────────────────────────────────────────────────────────────
rect "$assets/vine/background.png" 800 600 '#58a05a'   # the vine
rect "$assets/vine/grapes.png"     100 120 '#8e44ad'   # bunch of grapes
silence "$assets/vine/voice.wav" 0.6

# ── Sunscreen minigame ────────────────────────────────────────────────────────
rect "$assets/sunscreen/background.png" 800 600 '#f0e2c0'   # close-up backdrop
rect "$assets/sunscreen/gina.png"       240 240 '#f4d03f'   # Gina close-up
silence "$assets/sunscreen/voice.wav" 0.6

# ── Grapes minigame ───────────────────────────────────────────────────────────
rect "$assets/grapes/background.png" 800 600 '#58a05a'   # the vine, close-up
rect "$assets/grapes/grape.png"       40  40 '#8e44ad'   # one grape
silence "$assets/grapes/voice.wav" 0.6
silence "$assets/grapes/pop.wav"   0.4

echo "Generated placeholder assets under $assets"
