#!/usr/bin/env bash
#
# gen_en_us_placeholders.sh
# Scaffold a COMPLETE en_US locale from the it_IT (source) assets, so the strict
# resolver (locale -> common, no cross-language fallback) can fully load English.
#
# Localized images become same-sized English-text placeholders; sprite-sheet
# metadata (.anim/.json) is copied verbatim; every localized voice line becomes a
# short silent WAV. Real English art/audio is a later file swap.
#
# Requires ImageMagick (`convert`) and Python 3. Run from anywhere.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VFTS="$root/src/adventures/vania_fox_the_slide/assets"
GINA="$root/src/adventures/gina_hen_at_the_pool/assets"
PURPLE='#7a3cc0'
BLUE='#1565c0'

# silent <path> <seconds>  (48000 Hz, mono, matching the Vania dialogue WAVs)
silent() {
  python3 - "$1" "$2" <<'PY'
import sys, wave
path, seconds = sys.argv[1], float(sys.argv[2])
rate = 48000
with wave.open(path, "wb") as w:
    w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
    w.writeframes(b"\x00\x00" * int(rate * seconds))
PY
}

# button <out> <W> <H> <frames> <label>  (a vertical sprite sheet; label centered
# in each identical frame, on a transparent background)
button() {
  local out="$1" w="$2" fh=$(( $3 / $4 )) frames="$4" label="$5" ps="$6"
  convert -size "${w}x${fh}" xc:none -gravity center -pointsize "$ps" \
          -fill "$BLUE" -annotate 0 "$label" "PNG32:/tmp/_cell.png"
  local args=()
  for ((i = 0; i < frames; i++)); do args+=("/tmp/_cell.png"); done
  convert "${args[@]}" -append "PNG32:$out"
  rm -f /tmp/_cell.png
}

# ── Vania (vania_fox_the_slide) ──────────────────────────────────────────────
mkdir -p "$VFTS/en_US/intro" "$VFTS/en_US/outro" \
         "$VFTS/en_US/playground/dialog" "$VFTS/en_US/playground_entrance/dialog"

# Intro title screen (800x600), English title in the upper-left.
convert -size 800x600 xc:white -gravity NorthWest -pointsize 72 -fill "$PURPLE" \
        -annotate +40+70 'VANIA VOLPE' -annotate +40+160 'THE SLIDE' \
        "PNG32:$VFTS/en_US/intro/intro_background.png"

# Buttons: same geometry as it_IT (play 280x486 = 3x162, exit 265x243 = 3x81).
button "$VFTS/en_US/intro/play_button.png" 280 486 3 'PLAY!' 48
button "$VFTS/en_US/intro/exit_button.png" 265 243 3 'EXIT' 40
cp "$VFTS/it_IT/intro/play_button.anim" "$VFTS/en_US/intro/play_button.anim"
cp "$VFTS/it_IT/intro/exit_button.anim" "$VFTS/en_US/intro/exit_button.anim"
cp "$VFTS/it_IT/intro/exit_button.json" "$VFTS/en_US/intro/exit_button.json"

# Outro (800x600): "THE END" + English credits.
convert -size 800x600 xc:white -fill "$PURPLE" \
        -gravity North -pointsize 96 -annotate +0+90 'THE END' \
        -gravity SouthEast -pointsize 28 -fill "$BLUE" \
        -annotate +30+200 'Story: Papà' -annotate +30+160 'Art: Papà / Matilda' \
        -annotate +30+120 'Code: Papà' -annotate +30+80 'Music: Papà' \
        -annotate +30+40 'Sound: Papà' \
        "PNG32:$VFTS/en_US/outro/outro_background.png"

# Every localized voice line -> a short silent placeholder.
for f in "$VFTS"/it_IT/playground/dialog/*.wav; do
  silent "$VFTS/en_US/playground/dialog/$(basename "$f")" 1.5
done
for f in "$VFTS"/it_IT/playground_entrance/dialog/*.wav; do
  silent "$VFTS/en_US/playground_entrance/dialog/$(basename "$f")" 1.5
done

# ── Gina (gina_hen_at_the_pool): per-line dialogue lines are localized ───────
# Each spoken line is its own chunk (SCENES.md milestone 4); mirror every it_IT
# dialogue WAV with a short silent en_US placeholder (the it_IT .txt subtitle
# stays it_IT-only, like Vania's dialogue).
for f in "$GINA"/it_IT/pool/dialog/*.wav "$GINA"/it_IT/tree/dialog/*.wav \
         "$GINA"/it_IT/vine/dialog/*.wav; do
  rel="${f#"$GINA"/it_IT/}"
  mkdir -p "$GINA/en_US/$(dirname "$rel")"
  silent "$GINA/en_US/$rel" 0.6
done

echo "Generated en_US placeholders for Vania and Gina."
