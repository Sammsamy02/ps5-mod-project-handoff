#!/usr/bin/env python3
"""Mod-on-PS5 texture / material diagnosis (Mac-only, read-only).

Generic: works for any UE5 IoStore pak mod against any game's captured TOCs.
Nothing game- or mod-specific is hard-coded; every per-game fact comes from the
input files given on the command line.

Inputs (all read-only):
  --mod-utoc/--mod-ucas      the downloaded mod container (any number of pairs)
  --base-utoc                the game's own TOCs (directory index + chunk ids)
  --base-container-header    the game's own ContainerHeader chunk (store entries)
  --global-utoc/--global-ucas the game's own global container (script objects)
  --base-pak-listing         optional: text listing of the game's classic .pak
                             (used only to read the game's own shader-platform
                             file names, e.g. GlobalShaderCache-<platform>.bin)
Output: one JSON report (+ a printed summary with item counts). Exits non-zero
if any section checked zero items.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iostore_lib as io  # noqa: E402

# Byte tokens that label which shader platform a cooked material payload was
# built for. PC D3D tokens are the cook's own platform names; the PS5 names are
# read from the GAME'S OWN file names at runtime (see game_shader_platforms()).
PC_TOKENS = [b"PCD3D_SM5", b"PCD3D_SM6", b"DXBC", b"DXIL", b"SF_VULKAN", b"VULKAN_SM", b"METAL_SM"]

MATERIAL_CLASSES = {"/Script/Engine.Material", "/Script/Engine.MaterialInstanceConstant",
                    "/Script/Engine.MaterialFunction", "/Script/Engine.MaterialParameterCollection"}
TEXTURE_CLASSES = {"/Script/Engine.Texture2D", "/Script/Engine.TextureCube", "/Script/Engine.Texture2DArray",
                   "/Script/Engine.VolumeTexture"}
# Classes whose objects shape the whole scene's light/fog/exposure if a pak overrides them.
SCENE_WIDE_CLASS_HINTS = ("PostProcess", "Fog", "SkyAtmosphere", "SkyLight", "DirectionalLight",
                          "MaterialParameterCollection", "VolumetricCloud", "LocalFogVolume", "World",
                          "Level", "CurveLinearColor", "ColorGradingLUT", "ExposureCurve")

BLOCK_BYTES = {  # pixel format -> (block_w, block_h, bytes_per_block)
    "PF_DXT1": (4, 4, 8), "PF_BC4": (4, 4, 8), "PF_ETC2_RGB": (4, 4, 8),
    "PF_DXT3": (4, 4, 16), "PF_DXT5": (4, 4, 16), "PF_BC5": (4, 4, 16), "PF_BC6H": (4, 4, 16),
    "PF_BC7": (4, 4, 16),
    "PF_G8": (1, 1, 1), "PF_R8": (1, 1, 1), "PF_G16": (1, 1, 2), "PF_R8G8": (1, 1, 2),
    "PF_B8G8R8A8": (1, 1, 4), "PF_R8G8B8A8": (1, 1, 4), "PF_FloatRGBA": (1, 1, 8),
    "PF_A32B32G32R32F": (1, 1, 16),
}
BULK_SEPARATE_FILE = 0x0100  # payload lives in the package's BulkData chunk (.ubulk)


def count_tokens(blob: bytes, tokens) -> dict:
    return {t.decode(): blob.count(t) for t in tokens}


def linear_mip_bytes(fmt: str, w: int, h: int) -> int:
    bw, bh, bb = BLOCK_BYTES[fmt]
    return max(1, -(-w // bw)) * max(1, -(-h // bh)) * bb


def parse_texture_platform_data(pkg: io.ZenPackage, exp: dict) -> dict:
    """Find the cooked FTexturePlatformData by its PF_* FString and walk the mips.

    Self-validating: every mip's SizeX/SizeY must halve and every mip's bulk
    record must exist; otherwise status=UNPARSED (never a guessed value)."""
    blob = pkg.export_bytes(exp)
    for m in re.finditer(rb"PF_[A-Za-z0-9_]{1,24}\x00", blob):
        s = m.start()
        if s < 16:
            continue
        ln = int.from_bytes(blob[s - 4:s], "little")
        if ln != m.end() - s:
            continue
        r = io.Reader(blob, s - 16)
        sx, sy, packed = r.i32(), r.i32(), r.u32()
        fmt = r.fstring()
        num_slices = packed & ((1 << 29) - 1)
        has_opt = bool(packed & (1 << 30))
        if not (0 < sx <= 16384 and 0 < sy <= 16384) or num_slices != 1:
            continue
        try:
            opt = None
            if has_opt:
                opt = {"ext_data": r.u32(), "num_mips_in_tail": r.u32()}
            first_mip = r.i32()
            nmips = r.i32()
            if not 0 < nmips <= 16:
                continue
            mips = []
            for k in range(nmips):
                bi = r.u32()
                if bi >= len(pkg.bulk_map):
                    raise io.FormatError("mip bulk index out of range")
                be = pkg.bulk_map[bi]
                separate = bool(be["flags"] & BULK_SEPARATE_FILE)
                if not separate:
                    r.raw(be["size"])
                mx, my, mz = r.i32(), r.i32(), r.i32()
                mips.append({"w": mx, "h": my, "d": mz, "bytes": be["size"], "in_ubulk": separate,
                             "bulk_flags": be["flags"]})
            ok = all(mips[k]["w"] == max(1, sx >> k) and mips[k]["h"] == max(1, sy >> k) for k in range(nmips))
            if not ok:
                continue
            linear = None
            if fmt in BLOCK_BYTES:
                linear = all(mp["bytes"] == linear_mip_bytes(fmt, mp["w"], mp["h"]) for mp in mips)
            return {"status": "PARSED", "pixel_format": fmt, "size": [sx, sy], "mips": nmips,
                    "first_mip": first_mip, "opt_data": opt,
                    "ubulk_bytes": sum(mp["bytes"] for mp in mips if mp["in_ubulk"]),
                    "inline_mips": sum(1 for mp in mips if not mp["in_ubulk"]),
                    "all_mips_linear_unpadded_size": linear,
                    "mip_bytes": [mp["bytes"] for mp in mips]}
        except io.FormatError:
            continue
    return {"status": "UNPARSED"}


# Verdicts that mean "this material has a shader map the game's platform can use".
MATERIAL_OK_VERDICTS = ("HAS_GAME_SHADER_MAP_IN_STORE", "HAS_GAME_PLATFORM_INLINE_SHADER_MAP",
                        "USES_MOD_PARENT_WITH_GAME_SHADER_MAP", "USES_BASE_PARENT (shader map from game)")


def material_verdict(is_master: bool, pc_tokens: int, game_tokens: int, store_shader_map_hashes: int,
                     mod_parent_verdicts=(), base_parents=()) -> str:
    """One material's verdict from data only (positive and negative cases).

    is_master: a UMaterial (has its own shader map) vs a material instance.
    pc_tokens / game_tokens: PC vs the GAME'S OWN shader-platform tokens in the
    export bytes (inline shader map). store_shader_map_hashes: hashes in the
    mod's own store entry (shader maps cooked into the game's shader library).
    mod_parent_verdicts: verdicts of parents that are inside the mod.
    base_parents: imports that resolve outside the mod (the game's packages)."""
    if store_shader_map_hashes > 0:
        return "HAS_GAME_SHADER_MAP_IN_STORE"
    if game_tokens > 0:
        return "HAS_GAME_PLATFORM_INLINE_SHADER_MAP"
    if is_master:
        return "NO_PS5_SHADER_MAP" if pc_tokens else "NO_SHADER_MAP_AT_ALL"
    if pc_tokens:
        return "NO_PS5_SHADER_MAP"  # own static permutation, PC only
    if any(v not in MATERIAL_OK_VERDICTS for v in mod_parent_verdicts):
        return "INHERITS_PC_ONLY_PARENT"
    if mod_parent_verdicts:
        return "USES_MOD_PARENT_WITH_GAME_SHADER_MAP"
    if base_parents:
        return "USES_BASE_PARENT (shader map from game)"
    return "UNRESOLVED_PARENT"


def unversioned_property_indices(blob: bytes):
    """Decode the unversioned property header fragments (indices only, no names).

    Returns (indices, header_len) or (None, 0). Property NAMES need the class
    schema, which is not on the Mac; indices are reported as-is."""
    r = io.Reader(blob)
    idx, cur, zero_mask_bits = [], 0, 0
    try:
        while True:
            v = r.u16()
            skip, has_zero, last, num = v & 0x7F, bool(v & 0x80), bool(v & 0x100), v >> 9
            cur += skip
            idx.extend(range(cur, cur + num))
            if has_zero:
                zero_mask_bits += num
            cur += num
            if last:
                break
            if len(idx) > 512:
                return None, 0
        if zero_mask_bits:
            return None, 0  # zero mask present: value layout not decoded here
        return idx, r.p
    except io.FormatError:
        return None, 0


def load_mod(utoc: str, ucas: str, script: dict, game_token_bytes=()) -> dict:
    toc = io.parse_toc(utoc)
    out = {"utoc": utoc, "utoc_sha256": toc.sha256, "ucas": ucas, "ucas_bytes": os.path.getsize(ucas),
           "container_id": f"{toc.container_id:016x}", "chunks": toc.entry_count,
           "chunk_type_counts": {}, "packages": {}, "store": {}}
    for i in range(toc.entry_count):
        t = io.CHUNK_TYPES.get(toc.chunk_type(i), str(toc.chunk_type(i)))
        out["chunk_type_counts"][t] = out["chunk_type_counts"].get(t, 0) + 1
    with open(ucas, "rb") as fh:
        for i in range(toc.entry_count):
            ct = toc.chunk_type(i)
            if ct == 6:
                ch = io.parse_container_header(io.read_chunk(toc, ucas, i, fh))
                out["store"] = {f"{k:016x}": v for k, v in ch["entries"].items()}
            if ct != 1:
                continue
            data = io.read_chunk(toc, ucas, i, fh)
            pkg = io.parse_zen(data)
            pid = f"{toc.chunk_u64(i):016x}"
            rec = {"path": toc.paths.get(i, ""), "package_id": pid, "package_name": pkg.name,
                   "bytes": len(data), "imported_packages": pkg.imported_package_names, "exports": []}
            for e in pkg.exports:
                cls = io.resolve_index(pkg, e["class"], script)
                blob = pkg.export_bytes(e)
                er = {"name": e["name"], "class": cls, "size": e["size"]}
                if cls in MATERIAL_CLASSES:
                    er["tokens"] = count_tokens(blob, PC_TOKENS)
                    # the GAME's own shader-platform tokens (read from its files, never hard-coded)
                    er["game_tokens"] = sum(blob.count(t) for t in game_token_bytes)
                if cls in TEXTURE_CLASSES:
                    er["texture"] = parse_texture_platform_data(pkg, e)
                    props, _ = unversioned_property_indices(blob)
                    er["unversioned_property_indices"] = props
                rec["exports"].append(er)
            out["packages"][pid] = rec
    return out


def game_shader_platforms(listing_path: str) -> dict:
    """Shader platform names the GAME ships, read from its own pak file names."""
    plats, files = set(), []
    if listing_path and os.path.exists(listing_path):
        for line in open(listing_path, encoding="utf-8", errors="replace"):
            line = line.strip()
            m = re.search(r"(?:GlobalShaderCache-|ShaderTypeInfo-[^-]+-|ShaderArchive-[^-]+-)(S[FP]_[A-Za-z0-9_]+?)(?:-|\.)", line)
            if m:
                files.append(line)
                for tok in re.findall(r"S[FP]_[A-Za-z0-9]+(?:_[A-Za-z0-9]+)*", line):
                    plats.add(tok.split(".")[0])
    return {"files": files, "platform_tokens": sorted(plats)}


def base_index(base_utocs):
    paths, pid_to_path, per_toc = {}, {}, []
    for p in base_utocs:
        toc = io.parse_toc(p)
        n_ubulk = 0
        rec = {"utoc": p, "sha256": toc.sha256, "entries": toc.entry_count, "paths": len(toc.paths),
               "shader_code_chunks": sum(1 for i in range(toc.entry_count) if toc.chunk_type(i) == 9),
               "shader_library_chunks": sum(1 for i in range(toc.entry_count) if toc.chunk_type(i) == 8)}
        for i, path in toc.paths.items():
            low = path.lower()
            paths.setdefault(low, []).append((os.path.basename(p), i, toc.offlens[i][1], toc.chunk_type(i)))
            if toc.chunk_type(i) == 1:
                pid_to_path[toc.chunk_u64(i)] = path
            if low.endswith(".ubulk"):
                n_ubulk += 1
        rec["ubulk_rows"] = n_ubulk
        per_toc.append(rec)
    return paths, pid_to_path, per_toc


def linear_chain_sum_set(max_log2: int = 14) -> dict:
    """Every byte total a contiguous top-down chain of linear, unpadded mips can have.

    Keyed by total -> one example (format class, w, h, mips). Covers BC 8/16-byte
    blocks and 1/2/4/8/16-byte pixels, power-of-two sizes 4..16384, aspect <= 16."""
    classes = {"bc8": (4, 4, 8), "bc16": (4, 4, 16), "px1": (1, 1, 1), "px2": (1, 1, 2), "px4": (1, 1, 4),
               "px8": (1, 1, 8), "px16": (1, 1, 16)}
    out = {}
    for cname, (bw, bh, bb) in classes.items():
        for lw in range(2, max_log2 + 1):
            for lh in range(2, max_log2 + 1):
                if abs(lw - lh) > 4:
                    continue
                w, h, total, k = 1 << lw, 1 << lh, 0, 0
                while True:
                    total += max(1, -(-w // bw)) * max(1, -(-h // bh)) * bb
                    k += 1
                    out.setdefault(total, (cname, 1 << lw, 1 << lh, k))
                    if w == 1 and h == 1:
                        break
                    w, h = max(1, w >> 1), max(1, h >> 1)
    return out


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mod-utoc", action="append", required=True)
    ap.add_argument("--mod-ucas", action="append", required=True)
    ap.add_argument("--base-utoc", action="append", required=True)
    ap.add_argument("--base-container-header", required=True)
    ap.add_argument("--global-utoc", required=True)
    ap.add_argument("--global-ucas", required=True)
    ap.add_argument("--base-pak-listing")
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)
    if len(a.mod_utoc) != len(a.mod_ucas):
        ap.error("mod utoc/ucas count mismatch")

    counts = {}
    script = io.load_script_objects(a.global_utoc, a.global_ucas)
    counts["script_objects"] = len(script)
    plats = game_shader_platforms(a.base_pak_listing)
    counts["game_shader_platform_files"] = len(plats["files"])
    game_tokens = sorted({t for t in plats["platform_tokens"]})
    game_token_bytes = [t.encode() for t in game_tokens]

    paths, pid_to_path, per_toc = base_index(a.base_utoc)
    counts["base_paths"] = len(paths)
    with open(a.base_container_header, "rb") as f:
        base_ch = io.parse_container_header(f.read())
    counts["base_store_entries"] = len(base_ch["entries"])
    reverse = {}
    for pid, ent in base_ch["entries"].items():
        for imp in ent["imports"]:
            reverse.setdefault(imp, []).append(pid)

    report = {"tool": "texture_diag.py", "inputs": {
        "base_utocs": per_toc, "base_container_header": {"path": a.base_container_header,
                                                          "sha256": io.sha256_file(a.base_container_header),
                                                          "entries": len(base_ch["entries"])},
        "global_utoc_sha256": io.sha256_file(a.global_utoc), "global_ucas_sha256": io.sha256_file(a.global_ucas),
        "game_shader_platforms": plats}, "mods": []}

    chain_sums = linear_chain_sum_set()
    for utoc, ucas in zip(a.mod_utoc, a.mod_ucas):
        mod = load_mod(utoc, ucas, script, game_token_bytes)
        mod["ucas_sha256"] = io.sha256_file(ucas)
        counts.setdefault("mod_packages", 0)
        counts["mod_packages"] += len(mod["packages"])
        mod_by_name = {p["package_name"].lower(): p for p in mod["packages"].values()}
        rows_mat, rows_tex, rows_shared = [], [], []
        mod_root = _mod_root(mod, paths)
        mod["new_content_root"] = mod_root
        # material verdicts, from data only. A material instance also looks at the verdicts of its
        # parents inside the mod (resolved recursively; a parent cycle is UNRESOLVED_PARENT).
        mat_cls = ("/Script/Engine.Material", "/Script/Engine.MaterialInstanceConstant")
        mat_exports = {}
        for pid, p in mod["packages"].items():
            for e in p["exports"]:
                if e["class"] in mat_cls:
                    mat_exports.setdefault(p["package_name"].lower(), (pid, p, e))
        verdicts = {}

        def resolve(name, stack=()):
            if name in verdicts:
                return verdicts[name]
            pid, p, e = mat_exports[name]
            if name in stack:
                return "UNRESOLVED_PARENT"
            n_hashes = len(mod["store"].get(pid, {}).get("shader_map_hashes", []))
            parents = [n for n in p["imported_packages"] if n.lower() in mat_exports]
            base_parents = [n for n in p["imported_packages"] if n.lower() not in mod_by_name]
            is_master = e["class"] == mat_cls[0]
            parent_v = [] if is_master else [resolve(n.lower(), stack + (name,)) for n in parents]
            v = material_verdict(is_master, sum(e["tokens"].values()), e.get("game_tokens", 0), n_hashes,
                                 parent_v, base_parents)
            verdicts[name] = v
            rows_mat.append({"package": p["package_name"], "class": e["class"].split(".")[-1],
                             "overrides_base": bool(paths.get(p["path"].lower())),
                             "store_shader_map_hashes": n_hashes, "tokens": e["tokens"],
                             "game_tokens": e.get("game_tokens", 0),
                             "mod_parents": parents, "other_imports": base_parents, "verdict": v,
                             "will_draw_checker": v not in MATERIAL_OK_VERDICTS})
            return v

        for name in mat_exports:
            resolve(name)
        for pid, p in mod["packages"].items():
            base_rows = paths.get(p["path"].lower(), [])
            for e in p["exports"]:
                if e["class"] in TEXTURE_CLASSES:
                    t = e["texture"]
                    rows_tex.append({"package": p["package_name"], **t,
                                     "unversioned_property_indices": e["unversioned_property_indices"],
                                     "overrides_base": bool(base_rows)})
            if not p["package_name"].lower().startswith(mod_root):
                classes = sorted({e["class"] for e in p["exports"]})
                dep_ids = reverse.get(int(pid, 16), [])
                dependents = [pid_to_path.get(d, f"{d:016x}") for d in dep_ids]
                # outside the mod = the dependent's PackageId is not one of the mod's own package ids
                # (ids are data: CityHash64 of the package name; no path or game-name rewriting)
                outside_mod = [pid_to_path.get(d, f"{d:016x}") for d in dep_ids if f"{d:016x}" not in mod["packages"]]
                scene_wide = [c for c in classes if any(h.lower() in c.lower() for h in SCENE_WIDE_CLASS_HINTS)]
                rows_shared.append({"package": p["package_name"], "classes": classes,
                                    "overrides_base_path": bool(base_rows),
                                    "base_store_has_entry": int(pid, 16) in base_ch["entries"],
                                    "base_store_shader_map_hashes": len(base_ch["entries"].get(int(pid, 16), {})
                                                                        .get("shader_map_hashes", [])),
                                    "base_dependents_total": len(dependents),
                                    "base_dependents_outside_mod": sorted(outside_mod),
                                    "scene_wide_class": scene_wide})
        # base comparison for game-platform tokens in mod material exports
        mod["game_platform_tokens_in_mod_materials"] = 0
        if game_token_bytes:
            with open(ucas, "rb") as fh:
                toc = io.parse_toc(utoc)
                for i in range(toc.entry_count):
                    if toc.chunk_type(i) == 1:
                        blob = io.read_chunk(toc, ucas, i, fh)
                        mod["game_platform_tokens_in_mod_materials"] += sum(blob.count(t) for t in game_token_bytes)
        mod["materials"] = rows_mat
        mod["textures"] = rows_tex
        mod["non_mod_root_packages"] = rows_shared
        # mod ubulk positive control for the linear-size test
        mod_ubulk = [r["ubulk_bytes"] for r in rows_tex if r.get("status") == "PARSED" and r["ubulk_bytes"]]
        mod["linear_chain_control"] = {"checked": len(mod_ubulk),
                                       "matches": sum(1 for b in mod_ubulk if b in chain_sums)}
        counts.setdefault("mod_materials", 0)
        counts["mod_materials"] += len(rows_mat)
        counts.setdefault("mod_textures", 0)
        counts["mod_textures"] += len(rows_tex)
        del mod["packages"]  # keep the report compact; tables above carry the facts
        mod["store"] = {"entries": len(mod["store"]),
                        "shader_map_hashes": sum(len(v["shader_map_hashes"]) for v in mod["store"].values())}
        report["mods"].append(mod)

    # base texture bulk sizes: are they exact linear unpadded mip chains?
    base_tex = []
    for low, rows in paths.items():
        name = os.path.basename(low)
        if low.endswith(".ubulk") and name.startswith("t_"):
            for (tocname, i, length, ct) in rows:
                if length >= 4096:
                    base_tex.append((low, tocname, length))
    matched = [b for b in base_tex if b[2] in chain_sums]
    report["base_texture_ubulk_linear_test"] = {
        "checked": len(base_tex), "exact_linear_chain_matches": len(matched),
        "match_fraction": round(len(matched) / len(base_tex), 4) if base_tex else None,
        "examples_unmatched": [{"path": b[0], "toc": b[1], "bytes": b[2]} for b in base_tex if b not in matched][:15],
        "examples_matched": [{"path": b[0], "toc": b[1], "bytes": b[2], "as": chain_sums[b[2]]} for b in matched][:15]}
    counts["base_texture_ubulk_rows"] = len(base_tex)
    report["counts"] = counts

    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, "w") as f:
        json.dump(report, f, indent=1, sort_keys=False, default=str)
    print("texture_diag counts:", json.dumps(counts))
    zero = [k for k, v in counts.items() if not v]
    if zero:
        print("FAIL: zero items checked in:", ", ".join(zero))
        return 2
    return 0


def _mod_root(mod: dict, base_paths: dict) -> str:
    """The mod's own new top folder = the most common first two path segments of
    packages that do NOT override a base path (derived from data, not a name list)."""
    from collections import Counter
    c = Counter()
    for p in mod["packages"].values():
        if p["path"].lower() in base_paths:
            continue
        parts = p["package_name"].lower().split("/")
        if len(parts) > 3:
            c["/".join(parts[:3]) + "/"] += 1
    return c.most_common(1)[0][0] if c else "\0"


if __name__ == "__main__":
    sys.exit(main())
