#!/usr/bin/env python3
"""Generate lip-sync sidecars for every dialogue WAV (see SPEECH.md).

For each src/adventures/*/assets/<locale>/**/dialog/*.wav this writes, next to
the WAV:

- <name>.cues  — mouth-shape cues from Rhubarb Lip Sync, one "<ms> <shape>"
  per line (shapes X A B C D E F; extended shapes are disabled). Played at
  runtime by src/lipsync.c to drive the talking animation's frames.
- <name>.words — per-word timing estimated from the speech spans in the cues
  and the sibling <name>.txt transcript, one "<start_ms> <end_ms> <word>" per
  line. Drives the read-along subtitle highlight. Skipped when there is no
  transcript. A forced aligner could replace the estimate later by emitting
  the same format.

Sidecars are committed to the repo, so builds and CI never run this script.
Regenerate only when a recording or transcript changes.

Requires the Rhubarb Lip Sync CLI (https://github.com/DanielSWolf/rhubarb-lip-sync,
tested with 1.13.0): either on PATH or pointed at by $RHUBARB. English locales
use the transcript-guided recognizer; everything else uses the language-
agnostic phonetic one.
"""

import argparse
import os
import subprocess
import sys
import tempfile
import wave

ADVENTURES_DIR = os.path.join(os.path.dirname(__file__), "..", "src", "adventures")
ENGLISH_LOCALES = ("en_US",)


def find_rhubarb():
    exe = os.environ.get("RHUBARB")
    if exe:
        return exe
    from shutil import which

    exe = which("rhubarb")
    if not exe:
        sys.exit(
            "rhubarb not found: install Rhubarb Lip Sync "
            "(https://github.com/DanielSWolf/rhubarb-lip-sync) and put it on "
            "PATH or set $RHUBARB"
        )
    return exe


def wav_duration_ms(path):
    with wave.open(path, "rb") as w:
        return round(w.getnframes() * 1000 / w.getframerate())


def run_rhubarb(exe, wav_path, txt_path, locale):
    """Return the cue list [(ms, shape), ...] for one WAV."""
    with tempfile.NamedTemporaryFile(suffix=".tsv", delete=False) as tmp:
        tsv_path = tmp.name
    try:
        cmd = [exe, "--extendedShapes", "", "-f", "tsv", "-o", tsv_path]
        if locale in ENGLISH_LOCALES and txt_path:
            cmd += ["-r", "pocketSphinx", "--dialogFile", txt_path]
        else:
            cmd += ["-r", "phonetic"]
        cmd.append(wav_path)
        subprocess.run(cmd, check=True, capture_output=True)
        cues = []
        with open(tsv_path) as f:
            for line in f:
                parts = line.split()
                if len(parts) != 2:
                    continue
                cues.append((round(float(parts[0]) * 1000), parts[1]))
        return cues
    finally:
        os.unlink(tsv_path)


def speech_spans(cues, duration_ms):
    """Maximal [start, end) runs of consecutive non-X cues. Rhubarb doesn't
    always terminate with an X cue, so an open run ends at the WAV's end."""
    spans = []
    start = None
    for i, (ms, shape) in enumerate(cues):
        if shape != "X" and start is None:
            start = ms
        if shape == "X" and start is not None:
            spans.append((start, ms))
            start = None
    if start is not None:
        spans.append((start, duration_ms))
    return spans


def estimate_words(text, cues, duration_ms):
    """Distribute the transcript's words over the speech spans, proportionally
    to word length (see SPEECH.md). Returns [(start_ms, end_ms, word), ...]."""
    words = text.split()
    if not words:
        return []
    spans = speech_spans(cues, duration_ms)
    if not spans:
        spans = [(0, duration_ms)]
    total_ms = sum(end - start for start, end in spans)
    weights = [len(w) + 1 for w in words]
    ms_per_weight = total_ms / sum(weights)

    timings = []
    span_i = 0
    pos = spans[0][0]  # cursor within the current span
    used = 0.0  # speech ms consumed so far, exact
    consumed = 0  # speech ms consumed up to `pos`, integer
    for w, weight in zip(words, weights):
        start = pos
        used += weight * ms_per_weight
        # Advance the cursor until this word's share of speech time is spent;
        # crossing a silence gap keeps consuming from the next span.
        while True:
            span_start, span_end = spans[span_i]
            available = span_end - pos
            needed = used - consumed
            if needed <= available or span_i == len(spans) - 1:
                pos = min(pos + max(0, round(needed)), span_end)
                consumed += max(0, round(needed))
                break
            consumed += available
            span_i += 1
            pos = spans[span_i][0]
        timings.append((start, pos, w))
    # Force the last word to close the final speech span.
    start, _, w = timings[-1]
    timings[-1] = (start, spans[-1][1], w)
    return timings


def newer(a, b):
    return os.path.getmtime(a) > os.path.getmtime(b)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--force", action="store_true", help="regenerate everything")
    args = parser.parse_args()

    exe = find_rhubarb()
    generated = skipped = 0
    for root, _dirs, files in sorted(os.walk(ADVENTURES_DIR)):
        if os.path.basename(root) != "dialog":
            continue
        locale = None
        parts = os.path.normpath(root).split(os.sep)
        if "assets" in parts:
            locale = parts[parts.index("assets") + 1]
        for name in sorted(files):
            if not name.endswith(".wav"):
                continue
            wav = os.path.join(root, name)
            base = wav[: -len(".wav")]
            cues_path = base + ".cues"
            words_path = base + ".words"
            txt_path = base + ".txt"
            if not os.path.exists(txt_path):
                txt_path = None

            fresh = (
                os.path.exists(cues_path)
                and not newer(wav, cues_path)
                and (txt_path is None or os.path.exists(words_path))
                and (txt_path is None or not newer(txt_path, words_path))
            )
            if fresh and not args.force:
                skipped += 1
                continue

            duration = wav_duration_ms(wav)
            cues = run_rhubarb(exe, wav, txt_path, locale)
            with open(cues_path, "w") as f:
                for ms, shape in cues:
                    f.write(f"{ms} {shape}\n")

            if txt_path:
                with open(txt_path) as f:
                    text = f.read().strip()
                with open(words_path, "w") as f:
                    for start, end, word in estimate_words(text, cues, duration):
                        f.write(f"{start} {end} {word}\n")
            rel = os.path.relpath(wav, os.path.join(ADVENTURES_DIR, ".."))
            print(f"lipsync: {rel} -> {len(cues)} cues" + (" + words" if txt_path else ""))
            generated += 1

    print(f"lipsync: {generated} generated, {skipped} up to date")


if __name__ == "__main__":
    main()
