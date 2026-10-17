#!/usr/bin/env python3
"""Keep the confirmed title -> Nexus game table in web/app.js equal to examples/title-map.json.

The UI bundles the table (the native host embeds web/app.js unchanged, so no route or asset is needed).
    sync_title_map.py            check: exit 1 when the block in web/app.js differs from the JSON file
    sync_title_map.py --write    rewrite the block from the JSON file
Only the file's own content is read; nothing is fetched and no device is contacted.
"""
import argparse
import json
from pathlib import Path
import re
import sys

PROJECT = Path(__file__).resolve().parents[1]
MAP_PATH = PROJECT / "examples" / "title-map.json"
APP_PATH = PROJECT / "web" / "app.js"
TITLE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$")
DOMAIN = re.compile(r"^[a-z0-9][a-z0-9_-]{0,79}$")
BEGIN = "// BEGIN title-map"
END = "// END title-map"
BLOCK = re.compile(r"(?P<head>^[ \t]*" + re.escape(BEGIN) + r"[^\n]*\n)(?P<body>.*?)(?P<tail>^[ \t]*" + re.escape(END) + r"[^\n]*$)",
                   re.DOTALL | re.MULTILINE)
CONSTANT = re.compile(r"^\s*const titleMap = Object\.freeze\((\{[^\n]*\})\);\s*$")


def load_map(path=MAP_PATH):
    """The validated table: a flat object of title id -> Nexus domain, nothing else."""
    value = json.loads(Path(path).read_text(encoding="utf-8"))
    if type(value) is not dict:
        raise ValueError("the title map must be one JSON object")
    for key, domain in value.items():
        if not TITLE_ID.fullmatch(key):
            raise ValueError(f"not a title id: {key!r}")
        if type(domain) is not str or not DOMAIN.fullmatch(domain):
            raise ValueError(f"not a Nexus domain: {domain!r}")
    return value


def render(table):
    return "const titleMap = Object.freeze(" + json.dumps(table, sort_keys=True, ensure_ascii=True) + ");\n"


def bundled(source):
    """The table inside web/app.js's marked block, parsed back from its one constant line."""
    found = BLOCK.search(source)
    if not found:
        raise ValueError("web/app.js has no title-map block")
    lines = [line for line in found.group("body").splitlines() if line.strip() and not line.strip().startswith("//")]
    if len(lines) != 1 or not CONSTANT.match(lines[0]):
        raise ValueError("the title-map block must hold exactly one 'const titleMap = Object.freeze({...});' line")
    return json.loads(CONSTANT.match(lines[0]).group(1))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--write", action="store_true", help="rewrite the block in web/app.js from examples/title-map.json")
    parser.add_argument("--map", type=Path, default=MAP_PATH)
    parser.add_argument("--app", type=Path, default=APP_PATH)
    args = parser.parse_args(argv)
    table = load_map(args.map)
    source = args.app.read_text(encoding="utf-8")
    if args.write:
        found = BLOCK.search(source)
        if not found:
            print("web/app.js has no title-map block", file=sys.stderr)
            return 2
        indent = re.match(r"[ \t]*", found.group("head")).group(0)
        updated = source[:found.start("body")] + indent + render(table) + source[found.start("tail"):]
        if updated != source:
            args.app.write_text(updated, encoding="utf-8")
            print("rewrote the title-map block")
        else:
            print("title-map block already matches")
        return 0
    try:
        current = bundled(source)
    except ValueError as error:
        print(str(error), file=sys.stderr)
        return 1
    if current != table:
        print("web/app.js title-map differs from examples/title-map.json; run tools/sync_title_map.py --write", file=sys.stderr)
        return 1
    print("title-map matches")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
