#!/usr/bin/env python3
"""classify_materials.py — install-time PC-shader fallback analyzer (PROTOTYPE).

Part of outputs/2026-10-08-l25-prep/shader-fallback/ (agent GM, 2026-10-08).

Reads ONLY retained local evidence (no network, no console, no builds):
  * the Leon mod UTOC (virtual paths + chunk table),
  * the Leon mod raw ExportBundleData chunks (already captured, read-only),
  * the mod's own ContainerHeader chunk (store entries),
  * the captured PS5 base pakchunk0 ContainerHeader (38,439 store entries),
  * the five captured PS5 base UTOCs (package id -> path),
  * the pinned dependency graph proof-3/graph.json (mod-local import edges).

For every mod .uasset package it emits:
  role               name-heuristic asset family (material / instance / mesh / texture / other)
  pc_inline_tokens   counts of PCD3D_SM5/PCD3D_SM6/DXBC/DXIL bytes in the package chunk
  ps5_tokens         counts of SF_PS5/SP_PS5 (expected 0 across this corpus)
  mod_store_hashes   container-header ShaderMapHashes for the package (0 for this mod)
  base_replaced      True if the same package id exists in the captured base store
  base_store_hashes  base store ShaderMapHashes count for that package id
  invisible_intent   own name or any mod-local dependency name contains 'invisible'
  verdict            PS5-UNKNOWN-BYTES-ONLY / PC-SHADER-INLINE / CHAIN-PC-PARENT / ...
  directive          overlay-builder action (substitute/hide/suppress/pass/flag)

It hard-fails if any pinned cross-check (recorded in the observation records)
does not reproduce exactly. Classification is byte-driven; 'PS5-OK' can never
be proven from mod bytes and is never claimed.

Usage: python3 classify_materials.py [--out leon-classification.json]
"""

import argparse
import json
import struct
import sys
from pathlib import Path

WHAT = Path("/WORKSPACE/.codex/worktrees/translation-core/what")

MOD_TOC = WHAT / ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P.utoc"
MOD_CHUNKS = WHAT / ".superpowers/native-dependency-spike/leon-iostore-adapter-trial-1/raw/chunks"
MOD_HEADER_CHUNK = MOD_CHUNKS / "156a95060856399000000006"  # LE(0x9039560806956a15)+0000+06
BASE_HEADER = WHAT / ".superpowers/native-observations/townfall-original-container-header-1/container-header.bin"
BASE_TOCS = [
    WHAT / ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc",
    WHAT / ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk1-ps5.utoc",
    WHAT / ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk10-ps5.utoc",
    WHAT / ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk11-ps5.utoc",
    WHAT / ".superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk12-ps5.utoc",
]
GRAPH = WHAT / ".superpowers/native-dependency-spike/unreal-mod-local-dependency-closure-1/proof-3/graph.json"

TOKENS = [b"PCD3D_SM6", b"PCD3D_SM5", b"DXBC", b"DXIL", b"SF_PS5", b"SP_PS5"]

# Base-game fallback/invisible material candidates (paths verified in the five
# captured base UTOCs; store hash counts verified in the base container header).
BASE_INVISIBLE_CANDIDATES = [
    ("/Game/FluidFlux/Surface/Materials/M_FluxSurfaceInvisible", "hide"),
    ("/Engine/EngineMaterials/RemoveSurfaceMaterial", "hide-engine"),
    ("/Engine/EngineMaterials/DefaultDeferredDecalMaterial", "decal-fallback"),
    ("/Engine/EngineMaterials/WorldGridMaterial", "default-grid"),
    ("/Engine/EngineMaterials/DefaultMaterial", "default"),
]


# ---------------------------------------------------------------- UTOC parsing
def parse_utoc_directory(data: bytes):
    """Return {package_id: (virtual_path, chunk_type)} plus counts, using the
    layout validated against the pinned intake facts (mount '../../../',
    29 dirs / 161 files for the mod TOC; directory range [39084, 46476))."""
    (chunks, blocks, width, methods, name_width, block_bytes,
     dir_bytes, parts) = struct.unpack_from("<8I", data, 24)
    seeds, = struct.unpack_from("<I", data, 84)
    overflow, = struct.unpack_from("<I", data, 96)
    directory = (144 + 12 * chunks + 10 * chunks + 4 * seeds + 4 * overflow
                 + width * blocks + methods * name_width)
    b = data[directory:directory + dir_bytes]
    mlen, = struct.unpack_from("<I", b, 0)
    mount = b[4:4 + mlen - 1].decode("utf-8")
    off = 4 + mlen
    ndir, = struct.unpack_from("<I", b, off); off += 4
    dirs = [struct.unpack_from("<4i", b, off + 16 * i) for i in range(ndir)]
    off += 16 * ndir
    nfile, = struct.unpack_from("<I", b, off); off += 4
    files = [struct.unpack_from("<3i", b, off + 12 * i) for i in range(nfile)]
    off += 12 * nfile
    nstr, = struct.unpack_from("<I", b, off); off += 4
    strs = []
    for _ in range(nstr):
        ln, = struct.unpack_from("<I", b, off); off += 4
        strs.append(b[off:off + ln - 1].decode("utf-8", "replace"))
        off += ln
    if off != dir_bytes:
        raise ValueError("directory index not exactly consumed: %d != %d" % (off, dir_bytes))
    dpath = {}

    def walk(idx, prefix):
        name_idx, first_child, _next_sib, _first_file = dirs[idx]
        name = mount if idx == 0 else prefix + strs[name_idx] + "/"
        dpath[idx] = name
        c = first_child
        while c != -1:
            walk(c, name)
            c = dirs[c][2]

    walk(0, "")
    owner = {}
    for di, (_n, _fc, _ns, first_file) in enumerate(dirs):
        fi = first_file
        while fi != -1:
            owner[fi] = di
            fi = files[fi][1]
    out = {}
    for i, (n, _nf, toc) in enumerate(files):
        cid = data[144 + 12 * toc:144 + 12 * toc + 12]
        pid = struct.unpack("<Q", cid[:8])[0]
        entry = out.get(pid, {})
        # .uasset (type 1) and .ubulk (type 2) share the package id; keep both.
        entry[1 if cid[11] == 1 else 2] = dpath[owner[i]] + strs[n]
        out[pid] = entry
    return out, mount, ndir, nfile


def parse_container_store(buf: bytes):
    """Parse FIoContainerHeader v5 package ids + store entries per the pinned
    CUE4Parse FFilePackageStoreEntry schema (NoExportInfo+): per entry a
    CArrayView of FPackageId imports then a CArrayView of FSHAHash(20B)
    ShaderMapHashes; data blobs sit at headerStart+offsetToDataFromThis."""
    sig, ver = struct.unpack_from("<II", buf, 0)
    if sig != 0x496F436E:
        raise ValueError("bad container header signature")
    n_pkgs, = struct.unpack_from("<I", buf, 16)
    pids = struct.unpack_from("<%dQ" % n_pkgs, buf, 20)
    buf_num, = struct.unpack_from("<I", buf, 20 + n_pkgs * 8)
    body = buf[20 + n_pkgs * 8 + 4:]
    if len(body) < buf_num:
        raise ValueError("store buffer truncated")
    store = {}
    total_imports = total_hashes = 0
    for i, pid in enumerate(pids):
        p = i * 16
        n1, o1 = struct.unpack_from("<ii", body, p)
        n2, o2 = struct.unpack_from("<ii", body, p + 8)
        imports = struct.unpack_from("<%dQ" % n1, body, p + o1) if n1 > 0 else ()
        hashes = ([body[p + 8 + o2 + k * 20:p + 8 + o2 + (k + 1) * 20].hex()
                   for k in range(n2)] if n2 > 0 else [])
        store[pid] = {"imports": [hex(x) for x in imports], "hashes": hashes}
        total_imports += n1
        total_hashes += n2
    return store, total_imports, total_hashes


# ---------------------------------------------------------------- token scan
def token_counts(chunk: bytes):
    return {tok.decode(): chunk.count(tok) for tok in TOKENS}


# ---------------------------------------------------------------- role rules
def role_of(basename: str) -> str:
    stem = basename.rsplit(".", 1)[0]
    if stem.startswith("MI_") or stem.endswith("_Inst"):
        return "material_instance"
    if stem.startswith(("M_", "MM_")) or "Material" in stem:
        return "material"
    if stem.startswith("MF_"):
        return "material_function"
    if stem.startswith("T_"):
        return "texture"
    if stem.startswith(("SK_", "SKM_")):
        return "skeletal_mesh"
    return "other"


def game_name(virtual_path: str) -> str:
    # '../../../Townfall/Content/X.uasset' -> '/Game/X' (engine content too).
    p = virtual_path
    for stem in (".uasset", ".ubulk"):
        if p.endswith(stem):
            p = p[: -len(stem)]
    for prefix, repl in (("../../../Townfall/Content/", "/Game/"),
                         ("../../../Engine/Content/", "/Engine/")):
        if p.startswith(prefix):
            return repl + p[len(prefix):]
    return p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", type=Path,
                    default=Path(__file__).resolve().parent / "leon-classification.json")
    args = ap.parse_args()

    checks = []

    def check(name, got, want):
        ok = got == want
        checks.append({"check": name, "got": got, "want": want, "pass": ok})
        if not ok:
            raise SystemExit("PINNED CHECK FAILED: %s got %r want %r" % (name, got, want))

    # --- mod TOC
    mod_map, mount, ndir, nfile = parse_utoc_directory(MOD_TOC.read_bytes())
    check("mod directory facts (mount,dirs,files)", (mount, ndir, nfile),
          ("../../../", 29, 161))
    mod_pkgs = {pid: paths.get(1) for pid, paths in mod_map.items() if 1 in paths}
    check("mod type-1 package count", len(mod_pkgs), 127)

    # --- base TOCs (per-TOC file counts are recorded facts from the intake /
    #     overlap analysis; the parser must reproduce them exactly)
    base_paths = {}
    for toc, want_files in zip(BASE_TOCS, (56898, 196, 15, 23, 21)):
        m, _mount, _ndir, nfile = parse_utoc_directory(toc.read_bytes())
        check("base toc file count %s" % toc.name, nfile, want_files)
        for pid, paths in m.items():
            if 1 in paths:
                base_paths[pid] = paths[1]

    # --- stores
    base_store, base_imports, base_hashes = parse_container_store(BASE_HEADER.read_bytes())
    check("base store totals", (len(base_store), base_imports, base_hashes),
          (38439, 155248, 5387))
    glasses = base_store.get(0x96CD0F10E42AF022)
    check("base glasses store entry",
          (glasses["imports"], glasses["hashes"]),
          (["0x60ec7194dd276154", "0xec45dde72b6e9f27"],
           ["233d367d112a9b677e3e29f960b6d633a97cddf4"]))
    mod_store, mod_imports, mod_hashes = parse_container_store(MOD_HEADER_CHUNK.read_bytes())
    check("mod store totals", (len(mod_store), mod_imports, mod_hashes),
          (127, 367, 0))

    # --- graph (mod-local dependency edges, hash-pinned by the closure proof)
    graph = json.loads(GRAPH.read_text())
    deps = {}
    for pkg in graph["packages"]:
        names = []
        for d in pkg.get("dependency_names", []):
            names.append((d["name"] if isinstance(d, dict) else d).lower())
        deps[pkg["package_name"].lower()] = names
    mod_game_names = {game_name(p).lower(): game_name(p) for p in mod_pkgs.values() if p}

    # --- 68/68 overlap oracle against the intake's recorded summary
    overlap = json.loads((WHAT / ".superpowers/native-observations/leon-real-mod-intake-2026-10-01"
                          "/base-pak-metadata/townfall-leon-overlap-summary.json").read_text())
    ok_rows = 0
    for r in overlap["path_overlaps"]:
        want = r["virtual_path"].lower()
        pid = int(r["package_id"], 16)
        if base_paths.get(pid, "").lower() == want and mod_pkgs.get(pid, "").lower() == want:
            ok_rows += 1
    check("overlap path+package-id rows reproduced", ok_rows, 68)

    # --- per-package scan
    rows = []
    for pid, path in sorted(mod_pkgs.items(), key=lambda kv: kv[1]):
        chunk_file = MOD_CHUNKS / ("%016x00000001" % _le_hex(pid))
        if not chunk_file.exists():
            raise SystemExit("missing chunk file %s" % chunk_file)
        chunk = chunk_file.read_bytes()
        toks = token_counts(chunk)
        base_entry = base_store.get(pid)
        row = {
            "package_id": "%016x" % pid,
            "virtual_path": path,
            "role": role_of(path.rsplit("/", 1)[-1]),
            "chunk_bytes": len(chunk),
            "tokens": toks,
            "pc_inline": any(toks[t] for t in ("PCD3D_SM5", "PCD3D_SM6", "DXBC", "DXIL")),
            "ps5_tokens": any(toks[t] for t in ("SF_PS5", "SP_PS5")),
            "mod_store_hashes": len(mod_store[pid]["hashes"]),
            "base_replaced": base_entry is not None,
            "base_store_hashes": len(base_entry["hashes"]) if base_entry else 0,
            "base_path": base_paths.get(pid),
        }
        gname = game_name(path).lower()
        dep_names = deps.get(gname, [])
        row["invisible_intent"] = ("invisible" in gname
                                   or any("invisible" in d for d in dep_names))
        # parent-chain: any mod-local ancestor carries a PC inline payload?
        chain_pc = False
        seen, stack = set(), list(dep_names)
        while stack:
            cur = stack.pop()
            if cur in seen:
                continue
            seen.add(cur)
            anc = mod_game_names.get(cur)
            if anc is None:
                continue
            anc_pid = next(p for p, q in mod_pkgs.items() if game_name(q).lower() == cur)
            anc_chunk = (MOD_CHUNKS / ("%016x00000001" % _le_hex(anc_pid))).read_bytes()
            at = token_counts(anc_chunk)
            if any(at[t] for t in ("PCD3D_SM5", "PCD3D_SM6", "DXBC", "DXIL")):
                chain_pc = True
                break
            stack.extend(deps.get(anc.lower(), []))
        row["chain_pc_parent"] = chain_pc

        # verdict + directive
        if row["role"] in ("material", "material_instance", "material_function"):
            if row["pc_inline"]:
                row["verdict"] = "PC-SHADER-INLINE"
            elif chain_pc:
                row["verdict"] = "CHAIN-PC-PARENT"
            else:
                row["verdict"] = "NO-PC-EVIDENCE-IN-BYTES"
            if row["verdict"] in ("PC-SHADER-INLINE", "CHAIN-PC-PARENT"):
                if row["base_replaced"]:
                    if row["invisible_intent"]:
                        row["directive"] = "HIDE_VIA_BASE_INVISIBLE"
                        row["substitute"] = BASE_INVISIBLE_CANDIDATES[0][0]
                    else:
                        row["directive"] = "SUBSTITUTE_BASE_EXPORT"
                else:
                    row["directive"] = "ADDED-PC-ONLY-NEEDS-REPOINT-OR-DROP"
            else:
                row["directive"] = "PASS"
        else:
            row["verdict"] = "NOT-MATERIAL"
            row["directive"] = "PASS" if row["role"] in ("texture", "other") \
                else "FLAG-MESH-PATH-BLOCKED"
        rows.append(row)

    # --- reproduce the observation's four byte-token records exactly
    def row_for(game_path):
        return next(r for r in rows if game_name(r["virtual_path"]).lower() == game_path.lower())

    for game_path, want in (
        ("/Game/Leon/MI_Leon_Props", dict(PCD3D_SM6=0, PCD3D_SM5=0, DXBC=0, DXIL=0, SF_PS5=0, SP_PS5=0)),
        ("/Game/Leon/M_Leon_Body", dict(PCD3D_SM6=1, PCD3D_SM5=1, DXBC=15, DXIL=10, SF_PS5=0, SP_PS5=0)),
        ("/Game/Townfall/Characters/Simon/Materials/MI_Simon_Accessories_Glass", dict(PCD3D_SM6=0, PCD3D_SM5=0, DXBC=0, DXIL=0, SF_PS5=0, SP_PS5=0)),
        ("/Game/Townfall/Characters/Simon/Materials/M_Invisible", dict(PCD3D_SM6=1, PCD3D_SM5=1, DXBC=296, DXIL=205, SF_PS5=0, SP_PS5=0)),
    ):
        r = row_for(game_path)
        check("observation tokens %s" % game_path,
              {k: r["tokens"][k] for k in want}, want)

    # --- summary
    mat = [r for r in rows if r["role"] in ("material", "material_instance", "material_function")]
    overlap_rows = [r for r in rows if r["base_replaced"]]
    summary = {
        "mod_packages": len(rows),
        "material_family": len(mat),
        "material_family_pc_inline": sum(1 for r in mat if r["verdict"] == "PC-SHADER-INLINE"),
        "material_family_chain_pc_parent": sum(1 for r in mat if r["verdict"] == "CHAIN-PC-PARENT"),
        "mod_store_hash_total": mod_hashes,
        "base_replaced_packages": len(overlap_rows),
        "base_replaced_with_base_shader_hash": sum(1 for r in overlap_rows if r["base_store_hashes"]),
        "overlap_material_family": sum(1 for r in overlap_rows
                                       if r["role"] in ("material", "material_instance", "material_function")),
        "overlap_material_family_broken": sum(1 for r in overlap_rows
                                              if r["role"] in ("material", "material_instance", "material_function")
                                              and r["verdict"] in ("PC-SHADER-INLINE", "CHAIN-PC-PARENT")),
        "directives": {},
        "base_invisible_candidates": [
            {"game_path": p, "tag": tag,
             "store_hash_count": len(base_store[_pid_of(base_paths, p)]["hashes"])}
            for p, tag in BASE_INVISIBLE_CANDIDATES],
    }
    for r in rows:
        summary["directives"][r["directive"]] = summary["directives"].get(r["directive"], 0) + 1

    out = {
        "tool": "classify_materials.py (prototype, outputs/2026-10-08-l25-prep/shader-fallback)",
        "scope": "offline classification of retained Leon mod bytes + captured base metadata; no console contact",
        "inputs": {
            "mod_toc": str(MOD_TOC), "mod_chunks_dir": str(MOD_CHUNKS),
            "mod_container_header_chunk": str(MOD_HEADER_CHUNK),
            "base_container_header": str(BASE_HEADER),
            "base_utocs": [str(p) for p in BASE_TOCS], "graph": str(GRAPH),
        },
        "pinned_checks": checks,
        "summary": summary,
        "packages": rows,
    }
    args.out.write_text(json.dumps(out, indent=1) + "\n")
    print("wrote %s" % args.out)
    print(json.dumps(summary, indent=1))
    print("pinned checks passed: %d" % len(checks))


def _le_hex(pid: int) -> int:
    """chunk file name = raw FIoChunkId hex: 8-byte package id little-endian,
    2-byte chunk index 0000, 1-byte type 01. Returns the int whose 16-nibble
    zero-padded hex IS the file name."""
    return int.from_bytes(struct.pack("<Q", pid), "big")


def _pid_of(base_paths, game_path):
    for pid, p in base_paths.items():
        if game_name(p) == game_path:
            return pid
    raise KeyError(game_path)


if __name__ == "__main__":
    sys.exit(main())
