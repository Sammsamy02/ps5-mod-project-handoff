#!/usr/bin/env python3
"""How does the GAME'S OWN cooked texture data compare to a PC mod's? (Mac-only)

Uses only TOC metadata (chunk lengths), no payload bytes:
  * bulk (.ubulk) size: exact linear unpadded mip chain (PC layout) or not;
  * export (.uasset) size: tiny (<4 KiB: all pixel data in bulk), mod-like
    (4-60 KiB) or ~64 KiB (a 64 KiB inline mip-tail block);
  * size-group fit to a square virtual-texture tile model (128 px tiles +
    4 px border, 16-byte blocks) with the residual reported, never assumed.
Generic: any game TOC + any mod TOC.
"""
import argparse
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iostore_lib as io  # noqa: E402
import texture_diag as td  # noqa: E402


def bucket(n):
    return "<4KiB" if n < 4096 else ("4-60KiB" if n < 61440 else ("60-70KiB" if n < 71680 else ">70KiB"))


def vt_square_fit(total, tile=128, border=4, block_bytes=16):
    best = None
    tb = ((tile + 2 * border) // 4) ** 2 * block_bytes
    for lg in range(7, 15):
        size, tiles, levels = 1 << lg, 0, 0
        while True:
            tiles += max(1, size // tile) ** 2
            levels += 1
            if size <= tile:
                break
            size //= 2
        res = total - tiles * tb
        if 0 <= res < 262144 and (best is None or res < best["residual"]):
            best = {"size": 1 << lg, "tiles": tiles, "levels": levels, "residual": res,
                    "residual_minus_4x_levels": res - 4 * levels}
    return best


def stats(utoc, sums):
    t = io.parse_toc(utoc)
    low = {p.lower(): i for i, p in t.paths.items()}
    rows = collections.Counter()
    groups = collections.Counter()
    n = 0
    for p, i in low.items():
        name = p.rsplit("/", 1)[-1]
        if not (p.endswith(".uasset") and name.startswith("t_")):
            continue
        n += 1
        bi = low.get(p[:-7] + ".ubulk")
        exp = t.offlens[i][1]
        if bi is None:
            rows[("no-ubulk", bucket(exp))] += 1
            continue
        b = t.offlens[bi][1]
        kind = "ubulk-linear" if b in sums else "ubulk-nonlinear"
        rows[(kind, bucket(exp))] += 1
        groups[b] += 1
    return n, rows, groups


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--base-utoc", required=True)
    ap.add_argument("--mod-utoc", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    sums = td.linear_chain_sum_set()
    nb, base_rows, base_groups = stats(a.base_utoc, sums)
    nm, mod_rows, _ = stats(a.mod_utoc, sums)
    top = [{"ubulk_bytes": b, "textures": c, "linear": b in sums, "vt_square_fit": vt_square_fit(b)}
           for b, c in base_groups.most_common(12)]
    res = {"base_utoc": a.base_utoc, "mod_utoc": a.mod_utoc, "base_textures": nb, "mod_textures": nm,
           "base_rows": {f"{k[0]}|{k[1]}": v for k, v in sorted(base_rows.items())},
           "mod_rows": {f"{k[0]}|{k[1]}": v for k, v in sorted(mod_rows.items())},
           "base_top_bulk_size_groups": top}
    with open(a.out, "w") as f:
        json.dump(res, f, indent=1)
    print(f"base_texture_layout_stats: {nb} base textures, {nm} mod textures checked")
    return 0 if nb and nm else 2


if __name__ == "__main__":
    sys.exit(main())
