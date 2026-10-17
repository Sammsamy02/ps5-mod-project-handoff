#!/usr/bin/env python3
"""#8 planner: what each PC-only mod material can become on PS5 (Mac-only, read-only).

For every mod material whose shader map is PC-only (or whose parent chain is),
emit the data-only options that keep the downloaded mod files untouched:

  A  HIDE_OVERRIDE   the mod overrides a base package that has its own PS5
                     cook -> suppress the override in the DERIVED overlay so the
                     game's own asset renders (overlay_builder action "hide").
  B  REPARENT        a new mod material instance whose parent is a PC-only mod
                     material -> point it at a base master material that the
                     game itself uses for the meshes this mod replaces. Needs a
                     DERIVED package copy (zen_import_rewrite.py) + the store
                     entry repoint (overlay_builder "closest-match"). Parameter
                     names must be checked against the base master first.
                     An instance with its OWN static permutation (own
                     inline shader map or own store shader-map hashes) is
                     B_BLOCKED_OWN_STATIC_PERMUTATION: reparenting cannot
                     give that permutation a PS5 shader map.
  C  MESH_SLOT       a new mod UMaterial referenced directly by mod meshes ->
                     rewrite the mesh's import (same tool) to a base master, or
                     leave as is (renders as the engine default checker).

Base masters are chosen ONLY from data: base store entries of the base meshes
that the mod overrides -> their material imports (store entries carrying PS5
shader-map hashes) -> those materials' own material imports. Nothing is named
in code. Output says exactly which base exports must be fetched to check
parameter names before any option B/C directive may be emitted (fail closed).
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import iostore_lib as io  # noqa: E402
import texture_diag as td  # noqa: E402
from zen_import_rewrite import public_export_hash  # noqa: E402


def to_base_path(game_path: str, mount_roots) -> str:
    """'/Game/X/Y' -> '<Project>/Content/X/Y.uasset' using roots derived from the mod's own data."""
    for virt, real in mount_roots:
        if game_path.lower().startswith(virt.lower()):
            return real + game_path[len(virt):] + ".uasset"
    return ""


def mount_roots_from(mod_pkgs) -> list:
    """Derive '/Game/' -> '<Project>/Content/' from the mod's own path/name pairs."""
    roots = collections.Counter()
    for p in mod_pkgs.values():
        name, path = p["name"], p["path"]
        stem = path.rsplit(".", 1)[0]
        parts = name.split("/")
        if len(parts) > 2:
            virt = "/" + parts[1] + "/"
            rest = name[len(virt):]
            if stem.endswith(rest):
                roots[(virt, stem[: len(stem) - len(rest)])] += 1
    return [k for k, _ in roots.most_common()]


def static_permutation_check(own_pc_tokens: int, own_store_shader_map_hashes: int,
                             base_masters=()) -> dict:
    """Can reparenting fix this material instance's shader map? (fail closed)

    A material instance with static switch / static parameter overrides carries
    its OWN shader map (its own permutation): either inline in its own export
    (PC shader tokens in the instance itself) or as hashes in its own store
    entry. Reparenting a derived copy to a base master does not give that
    permutation a PS5 shader map, so it would still draw the checker.
    base_masters: per candidate master, how many of the GAME's own packages
    import it and how many of those carry their own store shader-map hashes
    (i.e. the game itself uses static permutations on that master)."""
    own = own_pc_tokens > 0 or own_store_shader_map_hashes > 0
    return {"own_inline_shader_tokens": own_pc_tokens,
            "own_store_shader_map_hashes": own_store_shader_map_hashes,
            "has_own_static_permutation": own,
            "base_masters": list(base_masters),
            "reparent_can_fix_shader_map": not own}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mod-utoc", required=True)
    ap.add_argument("--mod-ucas", required=True)
    ap.add_argument("--base-utoc", action="append", required=True)
    ap.add_argument("--base-container-header", required=True)
    ap.add_argument("--global-utoc", required=True)
    ap.add_argument("--global-ucas", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args(argv)

    script = io.load_script_objects(a.global_utoc, a.global_ucas)
    toc = io.parse_toc(a.mod_utoc)
    pkgs = {}
    mod_store = {}
    with open(a.mod_ucas, "rb") as fh:
        for i in range(toc.entry_count):
            if toc.chunk_type(i) == 6:
                mod_store = io.parse_container_header(io.read_chunk(toc, a.mod_ucas, i, fh))["entries"]
            if toc.chunk_type(i) != 1:
                continue
            z = io.parse_zen(io.read_chunk(toc, a.mod_ucas, i, fh))
            short = z.name.rsplit("/", 1)[-1]
            main_e = next((e for e in z.exports if e["name"] == short), z.exports[0])
            cls = io.resolve_index(z, main_e["class"], script)
            pc = sum(sum(td.count_tokens(z.export_bytes(e), td.PC_TOKENS).values()) for e in z.exports
                     if io.resolve_index(z, e["class"], script) in td.MATERIAL_CLASSES)
            pkgs[z.name] = {"name": z.name, "path": toc.paths.get(i, ""), "class": cls, "pc_tokens": pc,
                            "imports": z.imported_package_names,
                            "param_names": [n for n in z.names if n and n[0] != "/" and n != "None"],
                            "pid": toc.chunk_u64(i)}
    roots = mount_roots_from(pkgs)
    base_paths, pid_to_path, _ = td.base_index(a.base_utoc)
    path_to_pid = {v.lower(): k for k, v in pid_to_path.items()}
    with open(a.base_container_header, "rb") as f:
        store = io.parse_container_header(f.read())["entries"]
    base_importers = collections.defaultdict(list)
    for bpid, ent in store.items():
        for imp in ent["imports"]:
            base_importers[imp].append(bpid)

    def base_pid(game_path):
        bp = to_base_path(game_path, roots)
        return path_to_pid.get(bp.lower()), bp

    def is_mat(pkg_name):
        return pkg_name in pkgs and "Material" in pkgs[pkg_name]["class"] and \
            not pkgs[pkg_name]["class"].endswith("MaterialFunction")

    broken = {}
    for n, p in pkgs.items():
        if not is_mat(n):
            continue
        chain_bad = p["pc_tokens"] > 0 or any(is_mat(x) and pkgs[x]["pc_tokens"] > 0 for x in p["imports"])
        if chain_bad:
            broken[n] = p

    # base masters from data: base meshes overridden by the mod -> material imports -> their parents
    def base_material_imports(pid):
        out = []
        for imp in store.get(pid, {}).get("imports", []):
            if store.get(imp, {}).get("shader_map_hashes"):
                out.append(imp)
        return out

    masters_for_mesh = {}
    for n, p in pkgs.items():
        if "SkeletalMesh" not in p["class"] and "StaticMesh" not in p["class"]:
            continue
        pid, bp = base_pid(n)
        if pid is None:
            continue
        cand = collections.Counter()
        for mi in base_material_imports(pid):
            parents = base_material_imports(mi) or [mi]
            for par in parents:
                cand[par] += 1
        masters_for_mesh[n] = cand

    rows, fetch = [], set()
    for n, p in sorted(broken.items()):
        pid, bp = base_pid(n)
        users = [m for m, mp in pkgs.items() if n in mp["imports"]]
        row = {"package": n, "class": p["class"].split(".")[-1], "pc_tokens": p["pc_tokens"],
               "used_by": users, "param_names": p["param_names"] if p["class"].endswith("Constant") else []}
        if pid is not None:
            row["option"] = "A_HIDE_OVERRIDE"
            row["base_path"] = bp
            row["base_pid"] = f"{pid:016x}"
            row["base_has_ps5_shader_map"] = bool(store.get(pid, {}).get("shader_map_hashes"))
            row["note"] = "game's own PS5 asset returns for this slot (original look, not the mod's)"
        else:
            cand = collections.Counter()
            closure = {n, *users}
            for m, c in masters_for_mesh.items():
                if closure & set(pkgs[m]["imports"]):
                    cand.update(c)
            ranked = [{"base_path": pid_to_path.get(k, f"{k:016x}"), "base_pid": f"{k:016x}", "votes": v,
                       "export_name": pid_to_path.get(k, "").rsplit("/", 1)[-1].rsplit(".", 1)[0]}
                      for k, v in cand.most_common(6)]
            for r in ranked:
                r["public_export_hash"] = f"{public_export_hash(r['export_name']):016x}" if r["export_name"] else None
                fetch.add(r["base_path"])
            if p["class"].endswith("Constant"):
                parent = [x for x in p["imports"] if is_mat(x)]
                row["option"] = "B_REPARENT"
                row["mod_parent"] = parent
                row["from_export_hash"] = [f"{public_export_hash(x.rsplit('/', 1)[-1]):016x}" for x in parent]
                masters = []
                for r in ranked:
                    k = int(r["base_pid"], 16)
                    imps = base_importers.get(k, [])
                    masters.append({"base_pid": r["base_pid"], "game_packages_importing": len(imps),
                                    "with_own_store_shader_map_hashes":
                                        sum(1 for x in imps if store.get(x, {}).get("shader_map_hashes"))})
                spc = static_permutation_check(p["pc_tokens"],
                                               len(mod_store.get(p["pid"], {}).get("shader_map_hashes", [])),
                                               masters)
                row["static_permutation_check"] = spc
                if not spc["reparent_can_fix_shader_map"]:
                    row["option"] = "B_BLOCKED_OWN_STATIC_PERMUTATION"
                    row["blocked_until"] = ("this instance has its own static permutation (own shader map); "
                                            "reparenting cannot give it a PS5 shader map -- never a #8 option-B row")
            else:
                row["option"] = "C_MESH_SLOT"
            row["candidate_base_masters"] = ranked
            row.setdefault("blocked_until", ("base master parameter names verified (fetch the listed base "
                                             "exports); no directive is emitted for B/C before that"))
        rows.append(row)

    res = {"tool": "material_plan.py", "mod_utoc_sha256": toc.sha256, "mount_roots": roots,
           "broken_materials": len(rows),
           "options": collections.Counter(r["option"] for r in rows),
           "rows": rows, "base_exports_to_fetch_for_parameter_check": sorted(fetch)}
    with open(a.out, "w") as f:
        json.dump(res, f, indent=1)
    n_spc = sum(1 for r in rows if "static_permutation_check" in r)
    print(f"material_plan: {len(pkgs)} mod packages read, {len(rows)} PC-only materials planned "
          f"({dict(res['options'])}), {len(fetch)} base exports to fetch, "
          f"{n_spc} instances checked for own static permutation")
    if not pkgs or not masters_for_mesh:
        print("FAIL: zero mod packages or zero overridden base meshes checked")
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
