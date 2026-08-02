#!/usr/bin/env python3
"""Check the Xcode project against the tree and the Makefile.

The Xcode target has no CI that compiles it — there is no macOS runner, and the
project drifted three refactors behind before anyone noticed (#31). This is the
cheap half of that job: it cannot tell you the project *builds*, but it can tell
you the project is internally consistent and lists the same sources the Makefile
does, which is what actually went wrong.

Checks:

1. every path a file reference names exists on disk;
2. every group child resolves to a real object — which is what catches a
   group pointing at, say, a build phase that shares its name;
3. every `fileRef` in a build phase resolves to a file reference;
4. every object id is unique;
5. the sources build phase matches the Makefile's GAME_SRCS + main.c exactly;
6. braces and parentheses balance.

Usage:
  tools/check_xcode_project.py        # exit non-zero on any problem

Stdlib only.
"""

import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = "tinyadventures.xcodeproj/project.pbxproj"
MAKEFILE = "Makefile"

ADVENTURE_DIRS = {
    "$(VFTS_DIR)": "src/adventures/vania_fox_the_slide",
    "$(GINA_DIR)": "src/adventures/gina_hen_at_the_pool",
    "$(DEMO_DIR)": "src/adventures/depth_demo",
}


def makefile_sources():
    """The sources the desktop target compiles: GAME_SRCS plus main.c. The
    terminal-only files are excluded there too, so the two lists should agree."""
    mk = open(os.path.join(REPO, MAKEFILE)).read()
    block = mk[mk.index("GAME_SRCS = "):mk.index("SRCS = src/main.c")]
    out = ["src/main.c"]
    for raw in re.findall(r"(?:src|\$\((?:VFTS|GINA|DEMO)_DIR\))/[a-z_/]+\.c",
                          block):
        for key, value in ADVENTURE_DIRS.items():
            raw = raw.replace(key, value)
        out.append(raw)
    return out


def parse(text):
    """File references and the group/phase membership, by object id. Not a real
    plist parser — the pbxproj format is line-oriented enough that the specific
    shapes below are unambiguous."""
    refs = {}
    for m in re.finditer(
            r"^\t\t(\w{24}) /\* (.*?) \*/ = \{isa = PBXFileReference;(.*?)\};$",
            text, re.M):
        oid, name, body = m.groups()
        path = re.search(r"\bpath = ([^;]+);", body)
        tree = re.search(r"\bsourceTree = ([^;]+);", body)
        refs[oid] = {
            "name": name,
            "path": (path.group(1).strip('"') if path else name),
            "tree": (tree.group(1).strip('"') if tree else "<group>"),
        }

    build_files = {}
    for m in re.finditer(
            r"^\t\t(\w{24}) /\* .*? \*/ = \{isa = PBXBuildFile; fileRef = (\w{24})",
            text, re.M):
        build_files[m.group(1)] = m.group(2)

    groups = {}
    for m in re.finditer(
            r"^\t\t(\w{24})(?: /\* .*? \*/)? = \{\n\t\t\tisa = PBXGroup;(.*?)^\t\t\};$",
            text, re.M | re.S):
        oid, body = m.groups()
        path = re.search(r"\n\t\t\tpath = ([^;]+);", body)
        tree = re.search(r"\n\t\t\tsourceTree = ([^;]+);", body)
        groups[oid] = {
            "children": re.findall(r"\t\t\t\t(\w{24})", body),
            "path": (path.group(1).strip('"') if path else None),
            "tree": (tree.group(1).strip('"') if tree else "<group>"),
        }
    return refs, build_files, groups


def group_path_of(oid, groups):
    """The on-disk prefix a group contributes, walking up to the root. A group
    rooted at SOURCE_ROOT ends the walk — its path is already repo-relative,
    and inheriting the parent's would double the prefix."""
    for gid, g in groups.items():
        if oid in g["children"]:
            if g["path"] and g["tree"] == "SOURCE_ROOT":
                return g["path"]
            parent = group_path_of(gid, groups)
            if g["path"]:
                return os.path.join(parent, g["path"]) if parent else g["path"]
            return parent
    return ""


def main():
    path = os.path.join(REPO, PROJECT)
    if not os.path.exists(path):
        print(f"missing {PROJECT}", file=sys.stderr)
        return 1
    text = open(path).read()
    problems = []

    if text.count("{") != text.count("}"):
        problems.append(f"unbalanced braces: {text.count('{')} vs {text.count('}')}")
    if text.count("(") != text.count(")"):
        problems.append(f"unbalanced parens: {text.count('(')} vs {text.count(')')}")

    refs, build_files, groups = parse(text)

    ids = re.findall(r"^\t\t(\w{24})(?: /\*.*?\*/)? = \{", text, re.M)
    duplicates = {i for i in ids if ids.count(i) > 1}
    if duplicates:
        problems.append(f"duplicate object ids: {sorted(duplicates)}")

    # 1) every referenced path exists (built products and frameworks excepted:
    # the .app does not exist until it is built, and the frameworks are
    # installed outside the tree).
    for oid, ref in refs.items():
        if ref["tree"] in ("BUILT_PRODUCTS_DIR", "SDKROOT"):
            continue
        if ref["path"].endswith(".framework"):
            continue
        rel = (ref["path"] if ref["tree"] == "SOURCE_ROOT"
               else os.path.join(group_path_of(oid, groups), ref["path"]))
        if not os.path.exists(os.path.join(REPO, rel)):
            problems.append(f"file reference points at nothing: {rel}")

    # 2) every group child resolves to a real object. This is what catches a
    # group pointing at, say, a build phase that happens to share its name.
    objects = set(re.findall(r"^\t\t(\w{24})(?: /\*.*?\*/)? = \{", text, re.M))
    for gid, g in groups.items():
        for child in g["children"]:
            if child not in objects:
                problems.append(f"group {gid} lists unknown child {child}")

    # 3) every build file resolves.
    for bid, fid in build_files.items():
        if fid not in refs:
            problems.append(f"build file {bid} references unknown file {fid}")

    # 4) the sources phase matches the Makefile.
    phase = re.search(
        r"isa = PBXSourcesBuildPhase;.*?files = \(\n(.*?)\n\t\t\t\);",
        text, re.S)
    listed = set()
    for bid in re.findall(r"\t\t\t\t(\w{24})", phase.group(1)):
        fid = build_files.get(bid)
        if fid and fid in refs:
            listed.add(os.path.join(group_path_of(fid, groups),
                                    refs[fid]["path"]))
    expected = set(makefile_sources())
    for missing in sorted(expected - listed):
        problems.append(f"source in the Makefile but not the Xcode target: {missing}")
    for extra in sorted(listed - expected):
        problems.append(f"source in the Xcode target but not the Makefile: {extra}")

    if problems:
        for p in problems:
            print(f"  {p}", file=sys.stderr)
        print(f"{PROJECT}: {len(problems)} problem(s)", file=sys.stderr)
        return 1
    print(f"{PROJECT}: {len(listed)} sources, {len(refs)} file references, "
          f"consistent with the Makefile")
    return 0


if __name__ == "__main__":
    sys.exit(main())
