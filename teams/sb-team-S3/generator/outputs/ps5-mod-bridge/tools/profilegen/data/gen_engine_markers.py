#!/usr/bin/env python3
"""Regenerate engine_markers.json from patternsleuth's engine_version.rs (MIT licensed).

This is a developer tool, not part of a profile run: the marker/gate/version tables are engine
facts (UTF-16 strings that exist in a UE build only when a given preprocessor condition or engine
version window is active), transcribed mechanically so nothing is typed by hand.

    python3 gen_engine_markers.py PATH/TO/patternsleuth/src/resolvers/unreal/engine_version.rs > engine_markers.json
"""
import json
import re
import sys

GATES = ("", "NotShipping", "DevOnly", "TestOrShipping", "ShippingOnly", "WithStats")


def rust_string(lit):
    body = lit[1:-1]
    out, i = [], 0
    esc = {"n": "\n", "r": "\r", "t": "\t", "\\": "\\", '"': '"', "'": "'", "0": "\0"}
    while i < len(body):
        if body[i] == "\\":
            out.append(esc[body[i + 1]])
            i += 2
        else:
            out.append(body[i])
            i += 1
    return "".join(out)


def main(path):
    src = open(path, encoding="utf-8").read()
    vtab = re.search(r"const VERSIONS:\s*&\[EngineVersion\]\s*=\s*&\[(.*?)\];", src, re.S).group(1)
    versions = [[int(a), int(b)] for a, b in re.findall(r"v\((\d+),\s*(\d+)\)", vtab)]
    index = {tuple(v): i for i, v in enumerate(versions)}
    mtab = re.search(r"const MARKERS:\s*&\[Marker\]\s*=\s*&\[(.*?)\n\];", src, re.S).group(1)
    markers = []
    pat = re.compile(
        r'Marker\s*\{\s*string:\s*("(?:[^"\\]|\\.)*")\s*,'
        r'\s*gates:\s*&\[((?:[A-Za-z_][A-Za-z0-9_]*)?)\]\s*,'
        r'\s*first:\s*(None|Some\(\s*v\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*\))\s*,'
        r'\s*last:\s*(None|Some\(\s*v\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*\))\s*,?\s*\}')
    for m in pat.finditer(mtab):
        gate = m.group(2) or ""
        assert gate in GATES, gate
        first = None if m.group(3) == "None" else index[(int(m.group(4)), int(m.group(5)))]
        last = None if m.group(6) == "None" else index[(int(m.group(7)), int(m.group(8)))]
        markers.append({"s": rust_string(m.group(1)), "gate": gate, "first": first, "last": last})
    assert len(markers) == len(re.findall(r"Marker\s*\{", mtab)), "parser missed a marker row"
    assert len({m["s"] for m in markers}) == len(markers), "duplicate marker"
    json.dump({"source": "patternsleuth src/resolvers/unreal/engine_version.rs (MIT)",
               "gates": list(GATES), "versions": versions, "markers": markers},
              sys.stdout, indent=0, separators=(",", ":"), ensure_ascii=True)
    sys.stdout.write("\n")


if __name__ == "__main__":
    main(sys.argv[1])
