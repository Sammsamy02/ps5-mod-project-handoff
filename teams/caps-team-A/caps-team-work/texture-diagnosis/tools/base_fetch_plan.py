#!/usr/bin/env python3
"""Exact read-only byte ranges to pull from the game's own .ucas, and a verifier.

plan:   given the game's own .utoc and package paths, print for each package's
        chunks (ExportBundleData / BulkData) the exact .ucas byte ranges, the
        logical length and the TOC IoHash (BLAKE3-20) the fetched bytes must
        reproduce. The integrator performs the read (FTP REST+RETR, read-only).
verify: given the same TOC and a fetched raw-range file, check length + IoHash,
        then parse it (Zen header, export classes, texture platform data).
Generic: works for any game TOC; nothing game-specific is built in.
"""
from __future__ import annotations

import argparse
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iostore_lib as io  # noqa: E402
import blake3_min  # noqa: E402

TYPES_WANTED = {1: "ExportBundleData", 2: "BulkData"}


def plan(utoc: str, ucas_remote_name: str, wanted_paths, include_bulk: bool):
    toc = io.parse_toc(utoc)
    low = {p.lower(): i for i, p in toc.paths.items()}
    rows, missing = [], []
    for want in wanted_paths:
        stem = want.lower().rsplit(".", 1)[0]
        hits = [stem + ".uasset"] + ([stem + ".ubulk"] if include_bulk else [])
        found = False
        for h in hits:
            i = low.get(h)
            if i is None:
                continue
            found = True
            rngs = io.chunk_physical_ranges(toc, i)
            rows.append({"path": toc.paths[i], "toc_index": i, "chunk_id": toc.chunk_hex(i),
                         "chunk_type": io.CHUNK_TYPES[toc.chunk_type(i)],
                         "logical_length": toc.offlens[i][1],
                         "ucas_file": ucas_remote_name,
                         "ucas_ranges": [{"offset": s, "length": e - s} for s, e in rngs],
                         "expected_iohash20": toc.metas[i][0].hex(),
                         "compression": sorted({toc.method_names[toc.blocks[b][3]] for b in
                                                range(toc.offlens[i][0] // toc.block_size,
                                                      (toc.offlens[i][0] + toc.offlens[i][1] - 1)
                                                      // toc.block_size + 1)})})
        if not found:
            missing.append(want)
    return {"utoc": utoc, "utoc_sha256": toc.sha256, "rows": rows, "missing": missing}


def verify(utoc: str, toc_index: int, fetched: str, script=None):
    toc = io.parse_toc(utoc)
    with open(fetched, "rb") as f:
        data = f.read()
    rngs = io.chunk_physical_ranges(toc, toc_index)
    # A fetched file may be either the exact logical chunk, or the raw ranges concatenated.
    if len(data) != toc.offlens[toc_index][1]:
        return {"result": "LENGTH_MISMATCH", "got": len(data), "want": toc.offlens[toc_index][1],
                "ranges": rngs}
    h = blake3_min.io_hash20(data)
    if h != toc.metas[toc_index][0]:
        return {"result": "IOHASH_MISMATCH", "got": h.hex(), "want": toc.metas[toc_index][0].hex()}
    out = {"result": "VERIFIED", "path": toc.paths.get(toc_index), "bytes": len(data)}
    if toc.chunk_type(toc_index) == 1:
        pkg = io.parse_zen(data)
        out["package"] = pkg.name
        out["names"] = pkg.names
        out["imported_packages"] = pkg.imported_package_names
        out["bulk_map"] = pkg.bulk_map
        exps = []
        for e in pkg.exports:
            rec = {"name": e["name"], "size": e["size"]}
            if script:
                rec["class"] = io.resolve_index(pkg, e["class"], script)
                if rec["class"].endswith("Texture2D"):
                    import texture_diag
                    rec["texture"] = texture_diag.parse_texture_platform_data(pkg, e)
                    rec["unversioned_property_indices"] = texture_diag.unversioned_property_indices(
                        pkg.export_bytes(e))[0]
            exps.append(rec)
        out["exports"] = exps
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("plan")
    p.add_argument("--utoc", required=True)
    p.add_argument("--ucas-name", required=True, help="file name of the paired .ucas on the console")
    p.add_argument("--path", action="append", required=True, help="package path as in the TOC directory index")
    p.add_argument("--with-bulk", action="store_true")
    p.add_argument("--out", required=True)
    v = sub.add_parser("verify")
    v.add_argument("--utoc", required=True)
    v.add_argument("--toc-index", type=int, required=True)
    v.add_argument("--fetched", required=True)
    v.add_argument("--global-utoc")
    v.add_argument("--global-ucas")
    a = ap.parse_args(argv)
    if a.cmd == "plan":
        res = plan(a.utoc, a.ucas_name, a.path, a.with_bulk)
        with open(a.out, "w") as f:
            json.dump(res, f, indent=1)
        print(f"base_fetch_plan: {len(res['rows'])} chunk rows planned, {len(res['missing'])} paths missing")
        return 0 if res["rows"] and not res["missing"] else 2
    script = io.load_script_objects(a.global_utoc, a.global_ucas) if a.global_utoc else None
    res = verify(a.utoc, a.toc_index, a.fetched, script)
    print(json.dumps(res, indent=1, default=str))
    return 0 if res["result"] == "VERIFIED" else 2


if __name__ == "__main__":
    sys.exit(main())
