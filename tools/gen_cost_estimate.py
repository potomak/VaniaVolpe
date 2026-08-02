#!/usr/bin/env python3
"""Fill the art cost estimate's asset counts from the adventure manifest.

`src/emscripten/cost_estimate.html` prices commissioning Gina's image assets.
Its three counts — backgrounds, props, animation frames — used to be literal
`value=` attributes kept in step by hand, and they drifted: the numbers walked
9 -> 17 -> 27 -> 43 -> 59 -> 86 across separate commits, each a manual edit
somebody had to remember, and the prose under the table disagreed with the
fields above it more than once.

So they are derived here instead, from the same `assets/index.json` that feeds
the game, the *Assets to author* page and the asset catalog. The counts cover
only entries marked `task: true` — what is still to author is what you would
be commissioning:

  backgrounds  images the size of the screen
  props        every other image
  frames       the frames of every animation

The page stays a single self-contained file (it opens straight off disk, no
fetch), so this rewrites the committed HTML in place rather than emitting a
sidecar — the same shape as the generated asset headers under gen/. Run
`make gen` after editing a manifest; CI fails if the tree changes.

Usage:
  tools/gen_cost_estimate.py [--check]

Stdlib only.
"""

import argparse
import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MANIFEST = "src/adventures/gina_hen_at_the_pool/assets/index.json"
PAGE = "src/emscripten/cost_estimate.html"
SCREEN = "800x600"

# The generator owns everything between each pair; the page owns the rest.
COUNTS_BEGIN = "      <!-- generated:counts -->\n"
COUNTS_END = "      <!-- /generated:counts -->\n"
BASIS_BEGIN = "      <!-- generated:basis -->\n"
BASIS_END = "      <!-- /generated:basis -->\n"


def counts(manifest):
    tasks = [a for a in manifest["assets"] if a.get("task", False)]
    backgrounds = [a for a in tasks
                   if a["type"] == "image" and a.get("size") == SCREEN]
    props = [a for a in tasks
             if a["type"] == "image" and a.get("size") != SCREEN]
    animations = [a for a in tasks if a["type"] == "animation"]

    # Frames broken down by the manifest's own grouping, so the label under the
    # field explains the number in it instead of contradicting it.
    by_group = {}
    order = []
    for a in animations:
        if a["group"] not in by_group:
            order.append(a["group"])
            by_group[a["group"]] = 0
        by_group[a["group"]] += a["frames"]

    return {
        "backgrounds": len(backgrounds),
        "props": len(props),
        "animations": len(animations),
        "frames": sum(a["frames"] for a in animations),
        "breakdown": [(g, by_group[g]) for g in order],
    }


def render_counts(c):
    breakdown = " + ".join(f"{g} {n}" for g, n in c["breakdown"])
    return f"""      <div class="row">
        <label>Backgrounds
          <span class="sub">full-screen scene backdrops</span></label>
        <input type="number" id="bg" value="{c['backgrounds']}" min="0" step="1">
      </div>
      <div class="row">
        <label>Props / objects
          <span class="sub">every other still image: carried items, scene
          objects, the cartridge icon</span></label>
        <input type="number" id="obj" value="{c['props']}" min="0" step="1">
      </div>
      <div class="row">
        <label>Animation frames
          <span class="sub">{breakdown} = {c['frames']}</span></label>
        <input type="number" id="frames" value="{c['frames']}" min="0" step="1">
      </div>
"""


def render_basis(c):
    return (f"""      <li>Counts come from Gina's manifest
      (<code>{MANIFEST}</code>),
      via <code>tools/gen_cost_estimate.py</code>:
      {c['backgrounds']} backgrounds, {c['props']} props,
      {c['animations']} animations / {c['frames']} frames — every entry still to
      author. Edit the fields to price a different scope.</li>\n""")


def splice(page, begin, end, body):
    i = page.index(begin) + len(begin)
    j = page.index(end)
    return page[:i] + body + page[j:]


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--check", action="store_true",
                        help="exit non-zero if the page is out of date")
    args = parser.parse_args()

    with open(os.path.join(REPO, MANIFEST)) as f:
        c = counts(json.load(f))

    path = os.path.join(REPO, PAGE)
    with open(path) as f:
        page = f.read()
    updated = splice(page, COUNTS_BEGIN, COUNTS_END, render_counts(c))
    updated = splice(updated, BASIS_BEGIN, BASIS_END, render_basis(c))

    if args.check:
        if updated != page:
            print(f"{PAGE} is stale — run 'make gen' and commit.",
                  file=sys.stderr)
            return 1
        print(f"{PAGE}: counts up to date")
        return 0

    with open(path, "w") as f:
        f.write(updated)
    print(f"{PAGE}: {c['backgrounds']} backgrounds, {c['props']} props, "
          f"{c['animations']} animations / {c['frames']} frames")
    return 0


if __name__ == "__main__":
    sys.exit(main())
