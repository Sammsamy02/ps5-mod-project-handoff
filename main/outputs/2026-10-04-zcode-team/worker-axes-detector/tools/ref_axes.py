#!/usr/bin/env python3
"""Independent reference implementation of the axes classifier.

Deliberately written from the upstream Rust source (engine_version.rs) with
a DIFFERENT scanning strategy than the C probe unit: raw bytes.find over
each PT_LOAD's file-backed slice (no chunking, no hashing). If both
implementations agree on the retained Townfall image, the C result is not
an artifact of the C scan mechanics.

Prints one JSON line: {"build_configuration":..,"stats":..,...}
"""
import json
import re
import struct
import sys

RS = os_path = None


def load_markers(rs_path):
    src = open(rs_path, encoding="utf-8").read()
    vtab = re.search(r"const VERSIONS:\s*&\[EngineVersion\]\s*=\s*&\[(.*?)\];",
                     src, re.S).group(1)
    versions = [tuple(map(int, v)) for v in
                re.findall(r"v\((\d+),\s*(\d+)\)", vtab)]
    vindex = {v: i for i, v in enumerate(versions)}
    mtab = re.search(r"const MARKERS:\s*&\[Marker\]\s*=\s*&\[(.*?)\n\];",
                     src, re.S).group(1)
    markers = []
    for m in re.finditer(
            r'Marker\s*\{\s*string:\s*("(?:[^"\\]|\\.)*")\s*,'
            r'\s*gates:\s*&\[((?:[A-Za-z_][A-Za-z0-9_]*)?)\]\s*,'
            r'\s*first:\s*(None|Some\(\s*v\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*\))\s*,'
            r'\s*last:\s*(None|Some\(\s*v\(\s*(\d+)\s*,\s*(\d+)\s*\)\s*\))\s*,?'
            r'\s*\}', mtab):
        s = json.loads(m.group(1))  # Rust simple escapes == JSON here
        gate = m.group(2) or ""
        first = None if m.group(3) == "None" else (int(m.group(4)), int(m.group(5)))
        last = None if m.group(6) == "None" else (int(m.group(7)), int(m.group(8)))
        markers.append((s, gate,
                        vindex[first] if first else 0xFF,
                        vindex[last] if last else 0xFF))
    return versions, markers


def covers(marker, version_idx, cfg, stats_on):
    s, gate, first, last = marker
    if gate == "NotShipping" and cfg == 2: return False
    if gate == "DevOnly" and cfg != 0: return False
    if gate == "TestOrShipping" and cfg == 0: return False
    if gate == "ShippingOnly" and cfg != 2: return False
    if gate == "WithStats" and not stats_on: return False
    if first == 0xFF and last == 0xFF: return True
    if version_idx < 0: return False
    if first != 0xFF and version_idx < first: return False
    if last != 0xFF and version_idx > last: return False
    return True


def main():
    rs_path, elf_path = sys.argv[1], sys.argv[2]
    versions, markers = load_markers(rs_path)

    data = open(elf_path, "rb").read()
    phoff = struct.unpack_from("<Q", data, 32)[0]
    phentsize = struct.unpack_from("<H", data, 54)[0]
    phnum = struct.unpack_from("<H", data, 56)[0]
    loads = []
    extent = 0
    for i in range(phnum):
        p = data[phoff + i * phentsize: phoff + (i + 1) * phentsize]
        ptype, = struct.unpack_from("<I", p, 0)
        if ptype != 1:
            continue
        off, va, _pa, filesz, memsz, _al = struct.unpack_from("<QQQQQQ", p, 8)
        loads.append((off, va, filesz, memsz))
        extent = max(extent, va + memsz)

    # presence: substring search per marker over each LOAD's file slice
    needles = [m[0].encode("utf-16-le") for m in markers]
    present = [False] * len(markers)
    for off, va, filesz, _memsz in loads:
        seg = data[off: off + filesz]
        for i, nd in enumerate(needles):
            if not present[i] and seg.find(nd) != -1:
                present[i] = True
    present_count = sum(present)

    # cost model over candidates (None + versions) x cfg x stats
    def candidate(c):
        return c // 6 - 1, (c // 2) % 3, c % 2

    ncand = (len(versions) + 1) * 6
    costs = []
    for c in range(ncand):
        vi, cfg, on = candidate(c)
        cost = 0
        for mi, marker in enumerate(markers):
            covered = covers(marker, vi, cfg, on)
            if not covered and present[mi]:
                cost += 5
            elif covered and not present[mi]:
                cost += 1
        costs.append(cost)

    def resolve(axis):
        best = min(range(ncand), key=lambda c: (costs[c], c))
        vi, cfg, on = candidate(best)
        val = (vi, cfg, on)[axis]
        runner = min((costs[c] for c in range(ncand)
                      if (candidate(c))[axis] != val))
        if runner - costs[best] < 4:
            return None, costs[best], runner
        return val, costs[best], runner

    cfg_v, cfg_best, cfg_runner = resolve(1)
    stats_v, s_best, s_runner = resolve(2)
    ver_v, v_best, v_runner = resolve(0)

    out = {
        "build_configuration": 0 if cfg_v is None else
            {"0": 2, "1": 3, "2": 1}[str(cfg_v)],  # dev/test/shipping -> 2/3/1
        "stats": 0 if stats_v is None else (2 if stats_v == 1 else 1),
        "version": None if ver_v in (None, -1) else
            "%d.%d" % versions[ver_v],
        "present_count": present_count,
        "extent_bytes": extent,
        "costs": {"config": [cfg_best, cfg_runner],
                  "stats": [s_best, s_runner],
                  "version": [v_best, v_runner]},
    }
    print(json.dumps(out))


if __name__ == "__main__":
    main()
