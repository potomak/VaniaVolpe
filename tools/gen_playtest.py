#!/usr/bin/env python3
"""Generate a C header from a playtest script JSON.

The script JSON (test/scripts/<name>.json) is the single source of truth shared
by the native C test and the browser test. The browser runner reads the JSON
directly; C has no JSON parser, so this emits a header with the step table and
the expected-dialogue array for the C play-test to #include.

Usage: gen_playtest.py --name gina --in test/scripts/gina.json --out build/gen/gina_script.h
"""

import argparse
import json
import sys


def c_string(s):
    """Escape a Python string as a C string literal (without the quotes)."""
    return s.replace("\\", "\\\\").replace('"', '\\"')


def num(x):
    # Emit ints without a trailing .0, floats as-is.
    if isinstance(x, bool):
        raise ValueError("unexpected bool")
    if isinstance(x, int):
        return str(x)
    return repr(float(x))


def step_initializer(step):
    action = step["action"]
    fields = []
    if action == "click":
        fields.append(".action = SCRIPT_CLICK")
        fields.append(f".x = {num(step['x'])}")
        fields.append(f".y = {num(step['y'])}")
        fields.append(f".wait_ms = {num(step.get('wait_ms', 0))}")
    elif action == "brush":
        fields.append(".action = SCRIPT_BRUSH")
        for k in ("x0", "x1", "y0", "y1"):
            fields.append(f".{k} = {num(step[k])}")
        fields.append(f".rows = {num(step['rows'])}")
        fields.append(f".wait_ms = {num(step.get('wait_ms', 0))}")
    elif action == "wait":
        fields.append(".action = SCRIPT_WAIT")
        fields.append(f".wait_ms = {num(step.get('wait_ms', 0))}")
    elif action == "screenshot":
        fields.append(".action = SCRIPT_SCREENSHOT")
        fields.append(f'.name = "{c_string(step["name"])}"')
    else:
        raise ValueError(f"unknown action: {action}")
    return "{" + ", ".join(fields) + "}"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", required=True, help="identifier prefix, e.g. gina")
    ap.add_argument("--in", dest="infile", required=True)
    ap.add_argument("--out", dest="outfile", required=True)
    args = ap.parse_args()

    with open(args.infile, encoding="utf-8") as f:
        script = json.load(f)

    name = args.name
    upper = name.upper()
    guard = f"{name}_script_h"

    lines = [
        f"// Generated from {args.infile} by tools/gen_playtest.py. Do not edit.",
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        '#include "script.h"',
        "",
        f"static const ScriptStep {upper}_STEPS[] = {{",
    ]
    for step in script["steps"]:
        lines.append(f"    {step_initializer(step)},")
    lines.append("};")
    lines.append("")
    lines.append(f"static const char *const {upper}_EXPECT[] = {{")
    for line in script["expect"]:
        lines.append(f'    "{c_string(line)}",')
    lines.append("};")
    lines.append("")
    lines.append(f"#endif /* {guard} */")

    out = "\n".join(lines) + "\n"
    with open(args.outfile, "w", encoding="utf-8") as f:
        f.write(out)


if __name__ == "__main__":
    sys.exit(main())
