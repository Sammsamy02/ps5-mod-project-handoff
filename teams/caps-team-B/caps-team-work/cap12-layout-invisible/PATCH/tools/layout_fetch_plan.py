#!/usr/bin/env python3
"""Read-only fetch plan for the texture-layout question (Mac-only tool, generic).

Builds a plan in the shape outputs/2026-10-08-l25-prep/harvest/ftp_range_fetch.py
accepts ({"chunks": [{ucas, save_as, priority, physical_ranges[{offset,length}]}]}).
Only the game's OWN base TOC is read; nothing names a game, mod or asset.

Selection (TOC metadata only, no payload bytes):
  priority 1  every texture-named package (UE naming convention "T_*.uasset") that has
              NO separate bulk file and an export chunk below 60 KiB.  A texture in the
              64 KiB swizzle layout needs at least one 65,536-byte block, so these cannot
              be in it: their headers show which layout (ExtData) the game ships for
              textures that are not 64 KiB-tiled, and whether any ships with no layout
              block at all (the state a PC mod texture is in).
  priority 2  up to --pairs texture packages whose bulk file has the size of an
              8-byte-block 2048x2048 chain in that layout (export + bulk chunk), so the
              8-byte-element swizzle can be checked on game pixels the way the 16-byte
              one was (the format is read from the header after the fetch; a 16-byte
              2048x1024 texture has the same size and is reported, not assumed).
Prints how many rows it planned; exits non-zero when that count is zero.
"""
import argparse
import json
import os
import sys

sys.dont_write_bytecode = True


def chain_bytes_64k_s(w_el, h_el, eb, blk_w, blk_h):
    """Non-tail mip bytes of a 2D chain in a 64 KiB swizzle (public addrlib rules)."""
    tail_w, tail_h = blk_w // 2, blk_h
    total = 0
    while True:
        if w_el <= tail_w and h_el <= tail_h:
            return total
        pw = -(-w_el // blk_w) * blk_w
        ph = -(-h_el // blk_h) * blk_h
        total += pw * ph * eb
        w_el, h_el = max(1, w_el // 2), max(1, h_el // 2)


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--utoc", required=True, help="pristine base .utoc on the Mac")
    ap.add_argument("--ucas-name", required=True, help="remote .ucas file name (same container)")
    ap.add_argument("--formats-lib", required=True, help="dir holding ue_formats.py (read-only import)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--pairs", type=int, default=3)
    ap.add_argument("--max-small", type=int, default=200)
    a = ap.parse_args(argv)
    sys.path.insert(0, a.formats_lib)
    import ue_formats as uf  # noqa: E402

    t = uf.parse_toc(open(a.utoc, "rb").read())
    low = {p.lower(): r for r, p in t.files.items()}
    stem = os.path.splitext(a.ucas_name)[0]
    rows, checked = [], 0
    dxt_2k = chain_bytes_64k_s(512, 512, 8, 128, 64)  # 8-byte elements: 128x64 element tile

    def add(row, prio, why):
        rng = []
        for o, n in t.physical_ranges(row):  # merge back-to-back blocks: one FTP read each
            if rng and rng[-1]["offset"] + rng[-1]["length"] == o:
                rng[-1]["length"] += n
            else:
                rng.append({"offset": o, "length": n})
        assert sum(x["length"] for x in rng) == t.lengths[row]
        cid = t.chunk_ids[row]
        rows.append({"ucas": a.ucas_name, "priority": prio, "why": why,
                     "save_as": "%s/%s.bin" % (stem, cid.hex()),
                     "toc_row": row, "chunk_id": cid.hex(), "length": t.lengths[row],
                     "io_hash20": t.hashes[row].hex(), "physical_ranges": rng})

    pairs = 0
    for p in sorted(low):
        name = p.rsplit("/", 1)[-1]
        if not (p.endswith(".uasset") and name.startswith("t_")):
            continue
        checked += 1
        r = low[p]
        b = low.get(p[:-7] + ".ubulk")
        if b is None and t.lengths[r] < 61440 and sum(1 for x in rows if x["priority"] == 1) < a.max_small:
            add(r, 1, "texture without bulk file, export < 60 KiB: layout header")
        elif b is not None and t.lengths[b] == dxt_2k and pairs < a.pairs:
            add(r, 2, "export of 8-byte-block-size candidate")
            add(b, 2, "bulk of 8-byte-block-size candidate (%d bytes)" % dxt_2k)
            pairs += 1
    plan = {"tool": "cap12-layout-fetch-plan-1", "source_utoc": os.path.abspath(a.utoc),
            "read_only": True, "texture_packages_checked": checked, "chunks": rows,
            "bytes_total": sum(x["length"] for x in rows)}
    with open(a.out, "w") as f:
        json.dump(plan, f, indent=1)
    print("layout_fetch_plan: %d texture packages checked, %d chunks planned (%d bytes): "
          "%d small-texture headers, %d export+bulk pairs"
          % (checked, len(rows), plan["bytes_total"],
             sum(1 for x in rows if x["priority"] == 1), pairs))
    return 0 if rows and checked else 1


if __name__ == "__main__":
    sys.exit(main())
