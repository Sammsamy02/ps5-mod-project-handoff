#!/usr/bin/env python3
"""derive_substitutes.py -- capability #12: reparent plan -> derived-packages manifest (Mac-only).

For every `reparent` / `reparent-partial` row of a reparent plan
(reparent_plan.py), this tool:
  1. reads the mod package's export chunk from the mod container (read-only;
     IoHash checked against the mod's own TOC),
  2. finds the new parent's fetched base header (`<package id>.hdr.bin` in the
     --headers folders) and proves it is the WHOLE base chunk: same length as
     the base TOC row and the same BLAKE3-20 IoHash as that row (two sources),
  3. runs mic_reparent.py (the cap12 tool, unchanged) with --base-header and
     the row's --rename pairs, writing the DERIVED package bytes (new file;
     the mod file is never touched),
  4. cross-checks mic_reparent's receipt with the plan row and the mod's own
     store entry, and writes one manifest row.
The manifest (`manifest.json`, schema paktx-substitute/1) is self-contained:
derived packages, the parent headers and the base container headers are
copied next to it (relative paths), so its sha does not depend on where it
lives.  paktx.py `install --substitute manifest.json` re-checks everything.

User decisions are explicit: a `reparent-partial` row needs --allow-partial,
a row whose mod class is `unproven_opaque` needs --accept-unproven-opaque.
A row needing a decision that was not given is NOT derived (listed with the
reason).  Exit 1 when zero rows are derived, on any refusal, or on any input
that cannot be verified; nothing is written on refusal except the work folder.

ROUND 5b (Team B finding): every derived texture is written in the game's PS5
tile layout (ps5_tiling.py via texture_package.derive_ps5), channels kept or
R<->B swapped, and the round trip is proven: de-tiling the result with the
writer's own de-tiler AND with Team B's independent de-tiler (pinned
texture_tiling_probe.py) gives back exactly the linear mips (after the swap).
Every texture a shipped instance imports from the mod must be derived.
ROUND 5 (derived textures, schema paktx-substitute/2): for every texture a
plan row lists under `texture_derivations` (reparent_plan.py --derive-textures),
the tool also
  5. reads the mod texture's export chunk and BulkData chunk (IoHash checked
     against the mod TOC), proves the texel layout (texture_package.py: header
     bulk map + export mip table), applies the exact R<->B block swap
     (bcn_swap.py) to every mip payload,
  6. PROVES the result with TWO independent decoders, each on every texel of
     every block of every mip: the Python reference decoder (bcn_decode.py)
     and the Mac's GPU (metal_bcn_check.swift, compiled from its pinned
     source); both must report decode(derived) == decode(original) with R and
     B exchanged, the same texel count, and > 0 texels,
  7. writes the derived chunks + a manifest `textures` row (the derived bytes
     keep the texture's path and package id: the mod's own copy is replaced in
     the overlay only; the mod file is never touched).
A texture is derived only when every plan row that routes it agrees on the
operation; any refusal exits 1 (nothing usable is written).

Generic: no game or mod names; every name/hash comes from the plan and the
game's / mod's own files.
"""
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import types

sys.dont_write_bytecode = True
SCHEMA = "paktx-substitute/1"
SCHEMA_TEXTURES = "paktx-substitute/2"
# the reviewed readers + writer (mod-manager/material-fallback/, Team F cap12)
PINNED = {
    "blake3.py": "0613633f746a8e4f6c49baee1602f411cbdc1adbb793bf401b2a8e8c17b1355e",
    "cityhash.py": "95955e5872a067d38e024b1f0bad74753842bc24eb79be689afb588898b5167a",
    "container_header.py": "e703f080adf49c9ff5bd90a41b13733b367dafe9f74e0b8306c1e1ddad964459",
    "utoc.py": "9b5bf8f12e2626fa3949d16c2292427dbdb573e20b51840ee63cdfd8379a9b6f",
    "zen.py": "b3c421d448585ab51a91af7d935ff610633968f11dc258158cae4b3bcc889f4f",
    "mic_reparent.py": "520a805580a6750e9f799c8740095d4cc5d6f3e3a0a3574e6052914e82c06af5",
}
# round 5: the texture writer, the reference decoder, the layout reader, the GPU decoder
TEXTURE_PINNED = {
    "bcn_swap.py": "25e1896dbee2852e2716a1c9ca55589fa3f3801a089d82a084d6edca76ea9249",
    "bcn_decode.py": "d5058cd77788abac42b9dd7974a1cdd97485b69aee049694fc7947d506797a54",
    "texture_package.py": "f821b133e3e6bfe0740e0132e72381c97b54986d4fd5ce14bbf81e948e84b2be",
    "metal_bcn_check.swift": "778791f85e34bf77f1774bc9bd2c98f27cb5e786f162895098f57d347c615286",
    "ps5_tiling.py": "5c0148d7a4746d87df09564c223fad85850bb9bf298f1c1c6b2fa279705dbd6b",
    "ps5_layout_census.json": "f0a958a9fb339e492a90229a3ba935a4995c35571e4f696b709322c2550fea5c",
}
# Team B's tiling probe (cap12-layout-invisible, integrated in pak-intake/): its de-tiler is the
# INDEPENDENT second implementation for the round-trip proof of every PS5-layout texture.
TILING_REFERENCE_SHA256 = {
    "texture_tiling_probe.py": "ec0d8db08ea1ca232911b6ca6bc520b15b3bc46ad179bc9938cdf201f2185bf5",
    "tile_reference.py": "3aa856217071c79555427765d2c56245f7a99ec0763d041a0e3df46b01250dab",
}


class Refused(Exception):
    pass


def need(ok, why):
    if not ok:
        raise Refused(why)


def sha(b):
    return hashlib.sha256(b).hexdigest()


def canonical(obj):
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=True).encode("ascii")


def readers(folder, textures=False):
    """Pin-check the readers (and, for textures, the texture tools; those are exec'd
    later from their verified bytes, never imported)."""
    pins = dict(PINNED, **(TEXTURE_PINNED if textures else {}))
    for fn, want in sorted(pins.items()):
        p = os.path.join(folder, fn)
        need(os.path.isfile(p), "%s missing in %s" % (fn, folder))
        got = sha(open(p, "rb").read())
        need(got == want, "%s is %s, pinned %s" % (fn, got, want))
    sys.path.insert(0, folder)
    import blake3, cityhash, container_header, utoc, zen  # noqa: E401,E402
    return blake3, cityhash, container_header, utoc, zen


def reader_of(path):
    def r(o, n):
        with open(path, "rb") as fh:
            fh.seek(o)
            return fh.read(n)
    return r


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--plan", required=True)
    ap.add_argument("--mod-utoc", required=True)
    ap.add_argument("--mod-ucas", required=True)
    ap.add_argument("--mirror", required=True, help="folder holding the base game's .utoc files")
    ap.add_argument("--headers", nargs="+", required=True, help="folders of fetched <pid>.hdr.bin")
    ap.add_argument("--base-store", nargs="+", required=True,
                    help="base container-header chunk files (all containers the game mounts)")
    ap.add_argument("--material-fallback", default=os.path.dirname(os.path.abspath(__file__)),
                    help="folder with mic_reparent.py and the readers (zen, utoc, ...)")
    ap.add_argument("--allow-partial", action="store_true", help="USER DECISION")
    ap.add_argument("--accept-unproven-opaque", action="store_true", help="USER DECISION")
    ap.add_argument("--tiling-reference", help="folder with Team B's texture_tiling_probe.py + tile_reference.py "
                                               "(independent oracle, sha-pinned); default: "
                                               "<material-fallback>/../../pak-intake")
    ap.add_argument("--out", required=True, help="new, empty folder")
    a = ap.parse_args(argv)
    try:
        res = run(a)
    except (Refused, ValueError, OSError) as e:
        print("derive_substitutes: FAIL: %s; 0 rows written" % e)
        return 1
    print("derive_substitutes: %d derived, %d not derived, %d derived textures, manifest %s sha256 %s"
          % (res["derived"], res["not_derived"], res["textures"], res["manifest"], res["manifest_sha256"]))
    return 0 if res["derived"] > 0 else 1


def run(a):
    plan_bytes = open(a.plan, "rb").read()
    plan = json.loads(plan_bytes)
    want_tex = any(r.get("texture_derivations") for r in plan.get("rows", [])
                   if r.get("action") in ("reparent", "reparent-partial"))
    blake3, cityhash, container_header, utoc, zen = readers(a.material_fallback, textures=want_tex)
    mic = os.path.join(a.material_fallback, "mic_reparent.py")
    need(os.path.isfile(mic), "mic_reparent.py not found in %s" % a.material_fallback)
    need(not os.path.exists(a.out) or not os.listdir(a.out), "--out must be a new, empty folder")
    for sub in ("in", "derived", "base-headers", "base-stores"):
        os.makedirs(os.path.join(a.out, sub), exist_ok=True)
    rows = plan.get("rows")
    need(isinstance(rows, list) and rows, "plan has zero rows")
    law = cityhash.ue_name_hash
    # mod (read-only)
    mt = utoc.load(a.mod_utoc)
    mrd = reader_of(a.mod_ucas)
    hrows = mt.rows_of_type(6)
    need(len(hrows) == 1, "mod needs exactly one container header")
    _cid, mstore = container_header.parse(mt.read_chunk(hrows[0], mrd))
    # base TOCs (export chunk rows + header rows), from the mirror
    base, skipped_tocs = {}, []
    for fn in sorted(os.listdir(a.mirror)):
        if fn.endswith(".utoc"):
            try:
                t = utoc.load(os.path.join(a.mirror, fn))
            except ValueError as e:  # listed in the manifest; paktx re-checks against the record
                skipped_tocs.append({"name": fn, "reason": str(e)})
                continue
            base[fn[:-5]] = t
    need(base, "zero base TOCs in --mirror")
    print("derive_substitutes: %d base TOCs read, %d not readable (listed): %s"
          % (len(base), len(skipped_tocs), ", ".join(x["name"] for x in skipped_tocs) or "none"))
    # base stores: each tied to a TOC header row by IoHash
    stores, base_pids, base_entries = [], {}, {}
    for path in a.base_store:
        data = open(path, "rb").read()
        h = blake3.iohash20(data)
        hits = [s for s, t in base.items() for r in t.rows_of_type(6) if t.metas[r][0] == h]
        need(len(hits) == 1, "%s matches %d base TOC header rows by IoHash" % (path, len(hits)))
        _c, st = container_header.parse(data)
        for p in st:
            base_pids.setdefault(p, []).append(hits[0])
            base_entries[p] = st[p]
        rel = "base-stores/%s.container-header.bin" % hits[0]
        need(not os.path.exists(os.path.join(a.out, rel)), "two base-store files for %s" % hits[0])
        with open(os.path.join(a.out, rel), "wb") as fh:
            fh.write(data)
        stores.append({"container": hits[0], "path": rel, "bytes": len(data), "sha256": sha(data)})
    out_rows, skipped, headers = [], [], {}
    mod_pids = set(mstore)
    for r in rows:
        act = r.get("action")
        if act not in ("reparent", "reparent-partial"):
            continue
        decisions = []
        if act == "reparent-partial":
            decisions.append("allow-partial")
        if (r.get("mod_class") or {}).get("unproven_opaque"):
            decisions.append("accept-unproven-opaque")
        missing = [d for d in decisions
                   if not getattr(a, d.replace("-", "_"))]
        if missing:
            skipped.append({"package": r.get("mod_package"), "reason": "needs user decision: " + ", ".join(missing)})
            continue
        name, old, new, exp = r["mod_package"], r["old_parent"], r["new_parent"], r["new_export"]
        pid, from_pid, to_pid = law(name), law(old), law(new)
        rep = r.get("store_entry_repoint") or {}
        need(rep.get("package_id") == "%016x" % pid and rep.get("from_package_id") == "%016x" % from_pid
             and rep.get("to_package_id") == "%016x" % to_pid, "%s: plan store_entry_repoint disagrees "
             "with the name hash law" % name)
        need(pid in mstore, "%s not in the mod store" % name)
        need(not mstore[pid]["shader_maps"], "%s has its own shader maps; reparenting cannot fix it" % name)
        need(to_pid in base_pids, "%s: new parent %s in no base store" % (name, new))
        need(to_pid not in mod_pids, "%s: new parent %s is overridden by the mod" % (name, new))
        cid = (pid.to_bytes(8, "little") + bytes.fromhex("00000001"))
        crow = [i for i, c in enumerate(mt.chunk_ids) if c == cid]
        need(len(crow) == 1, "%s: export chunk missing" % name)
        orig = mt.read_chunk(crow[0], mrd)
        need(blake3.iohash20(orig) == mt.metas[crow[0]][0], "%s: mod chunk IoHash mismatch" % name)
        # parent header: whole chunk, IoHash-tied to its base TOC row
        if to_pid not in headers:
            cands = {}
            for d in a.headers:
                p = os.path.join(d, "%016x.hdr.bin" % to_pid)
                if os.path.isfile(p):
                    cands[sha(open(p, "rb").read())] = p
            need(len(cands) == 1, "%s: %d distinct fetched headers for %s" % (name, len(cands), new))
            hp = list(cands.values())[0]
            hb = open(hp, "rb").read()
            hits = [(s, i) for s, t in base.items() for i, c in enumerate(t.chunk_ids) if c ==
                    to_pid.to_bytes(8, "little") + bytes.fromhex("00000001")]
            need(len(hits) == 1, "%s: new parent chunk in %d base TOCs" % (name, len(hits)))
            s, i = hits[0]
            need(base[s].offlen[i][1] == len(hb), "%s: fetched header is not the whole base chunk" % new)
            need(blake3.iohash20(hb) == base[s].metas[i][0], "%s: fetched header IoHash != base TOC" % new)
            need(s in base_pids[to_pid], "%s: base store container differs from TOC container" % new)
            rel = "base-headers/%016x.hdr.bin" % to_pid
            with open(os.path.join(a.out, rel), "wb") as fh:
                fh.write(hb)
            headers[to_pid] = {"package_id": "%016x" % to_pid, "package": new, "container": s,
                               "chunk_id": base[s].chunk_ids[i].hex(), "path": rel,
                               "bytes": len(hb), "sha256": sha(hb), "iohash": base[s].metas[i][0]}
        inp = os.path.join(a.out, "in", "%016x.bin" % pid)
        with open(inp, "wb") as fh:
            fh.write(orig)
        drel = "derived/%016x.bin" % pid
        dpath = os.path.join(a.out, drel)
        ren = ["%s=%s" % kv for kv in sorted((r.get("param_renames") or {}).items())]
        cmd = [sys.executable, "-I", mic, "--in", inp, "--old-parent", old, "--new-parent", new,
               "--new-export", exp, "--base-header", os.path.join(a.out, headers[to_pid]["path"]),
               "--out", dpath]
        if ren:
            cmd += ["--rename"] + ren
        pr = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        need(pr.returncode == 0, "%s: mic_reparent refused: %s" % (name, (pr.stdout + pr.stderr)[-600:]))
        os.remove(inp)  # a copy of mod bytes; the manifest pins its sha instead
        rec = json.load(open(dpath + ".receipt.json"))
        der = open(dpath, "rb").read()
        q, p = zen.ZenPackage(der), zen.ZenPackage(orig)
        sec = rec["store_entry_import_change"]
        need(rec["package"] == name and rec["old_parent"] == old and rec["new_parent"] == new and
             sec["old_package_id"] == "%016x" % from_pid and sec["new_package_id"] == "%016x" % to_pid and
             rec["new_public_hash"] == "%016x" % law(exp) and rec["export_bytes_identical"] is True,
             "%s: mic_reparent receipt disagrees with the plan" % name)
        before = list(mstore[pid]["imports"])
        need(before == [law(n) for n in p.imported_packages], "%s: store import order != header names" % name)
        after = [law(n) for n in q.imported_packages]
        slot = rec["parent_import_slot"]
        need([i for i in range(len(before)) if before[i] != after[i]] == [slot] and
             before[slot] == from_pid and after[slot] == to_pid, "%s: store repoint is not one slot" % name)
        out_rows.append({
            "package": name, "package_id": "%016x" % pid, "chunk_id": cid.hex(), "action": act,
            "user_decisions": sorted(decisions),
            "original": {"bytes": len(orig), "sha256": sha(orig), "iohash": mt.metas[crow[0]][0]},
            "derived": {"path": drel, "bytes": len(der), "sha256": sha(der), "iohash": blake3.iohash20(der)},
            "reparent": {"old_parent": old, "new_parent": new, "new_export": exp,
                         "old_public_hash": rec["old_public_hash"], "new_public_hash": rec["new_public_hash"],
                         "public_hash_slot": rec["public_hash_slot"], "parent_import_slot": slot,
                         "renamed_parameters": rec["renamed_parameters"]},
            "store_entry": {"slot": slot, "imports_before": ["%016x" % v for v in before],
                            "imports_after": ["%016x" % v for v in after]},
            "mic_reparent_receipt_sha256": sha(open(dpath + ".receipt.json", "rb").read()),
            "look_notes": r.get("look_notes", []),
        })
    os.rmdir(os.path.join(a.out, "in"))
    need(out_rows, "zero rows derived (%d need a user decision)" % len(skipped))
    # integrator rule (round 5b): a shipped instance must point at derived PS5 textures only, so every mod
    # package it imports (its old mod parent is replaced) must be a derived texture of this plan
    planned = {law(d["texture"]) for r in rows if r.get("action") in ("reparent", "reparent-partial")
               for d in (r.get("texture_derivations") or [])}
    for r in out_rows:
        pid = int(r["package_id"], 16)
        for i in mstore[pid]["imports"]:
            if i in mstore and i != law(r["reparent"]["old_parent"]):
                need(i in planned, "%s imports mod package %016x that has no derived PS5 copy in the plan "
                     "(plan made without --derive-textures?)" % (r["package"], i))
    textures = []
    if want_tex:
        textures = derive_textures(a, rows, out_rows, mt, mrd, mstore, blake3, cityhash, base_pids, base_entries)
        need(textures, "the plan routes derived textures but zero were derived")
    doc = {
        "schema": SCHEMA_TEXTURES if textures else SCHEMA, "label": "Mac-only",
        "tool_sha256": sha(open(os.path.abspath(__file__), "rb").read()),
        "mic_reparent_sha256": sha(open(mic, "rb").read()),
        "source_plan": {"name": os.path.basename(a.plan), "sha256": sha(plan_bytes)},
        "mod": {"utoc_name": os.path.basename(a.mod_utoc), "utoc_sha256": sha(open(a.mod_utoc, "rb").read()),
                "ucas_name": os.path.basename(a.mod_ucas), "ucas_sha256": file_sha(a.mod_ucas)},
        "user_decisions": sorted({d for x in out_rows for d in x["user_decisions"]}),
        "base_headers": [headers[k] for k in sorted(headers)],
        "base_stores": sorted(stores, key=lambda x: x["container"]),
        "rows": out_rows, "not_derived": skipped, "skipped_base_tocs": skipped_tocs,
    }
    if textures:
        doc["textures"] = textures
        doc["texture_tools_sha256"] = {fn: sha(open(os.path.join(a.material_fallback, fn), "rb").read())
                                       for fn in sorted(TEXTURE_PINNED)}
    doc["manifest_sha256"] = sha(canonical(doc))
    mpath = os.path.join(a.out, "manifest.json")
    with open(mpath, "w") as fh:
        json.dump(doc, fh, indent=1, sort_keys=True)
        fh.write("\n")
    return {"derived": len(out_rows), "not_derived": len(skipped), "manifest": mpath,
            "manifest_sha256": doc["manifest_sha256"], "textures": len(textures)}


def verified_module(folder, fn, want, inject=None):
    """Exec a pinned tool from its verified bytes (no import path, no bytecode cache)."""
    p = os.path.join(folder, fn)
    need(os.path.isfile(p), "%s missing" % p)
    data = open(p, "rb").read()
    need(sha(data) == want, "%s is %s, pinned %s" % (p, sha(data), want))
    mod = types.ModuleType("derive_" + fn.rsplit(".", 1)[0])
    mod.__file__ = p
    mod.__dict__.update(inject or {})
    exec(compile(data, p, "exec"), mod.__dict__)
    return mod, data


def gpu_checker(folder, work):
    """Compile the pinned Metal decoder from its VERIFIED bytes copied into work/."""
    p = os.path.join(folder, "metal_bcn_check.swift")
    data = open(p, "rb").read()
    need(sha(data) == TEXTURE_PINNED["metal_bcn_check.swift"], "metal_bcn_check.swift changed after the pin check")
    src = os.path.join(work, "metal_bcn_check.swift")
    with open(src, "wb") as fh:
        fh.write(data)
    exe = os.path.join(work, "metal_bcn_check")
    pr = subprocess.run(["swiftc", "-O", src, "-o", exe], capture_output=True, text=True, timeout=600)
    need(pr.returncode == 0 and os.path.isfile(exe), "GPU decoder did not compile: %s" % (pr.stderr[-400:]))
    ver = subprocess.run(["swiftc", "--version"], capture_output=True, text=True, timeout=60)
    return exe, ((ver.stdout or ver.stderr).strip().splitlines() or ["unknown"])[0][:200]


def gpu_proof(exe, work, fmt, before, after, bb):
    a_, b_ = os.path.join(work, "before.bin"), os.path.join(work, "after.bin")
    with open(a_, "wb") as fh:
        fh.write(before)
    with open(b_, "wb") as fh:
        fh.write(after)
    pr = subprocess.run([exe, "compare", fmt, a_, b_], capture_output=True, text=True, timeout=600)
    os.remove(a_)
    os.remove(b_)
    try:
        res = json.loads(pr.stdout.strip().splitlines()[-1])
    except (ValueError, IndexError):
        raise Refused("GPU decoder gave no result: %s" % (pr.stdout + pr.stderr)[-300:])
    need(pr.returncode == 0 and res.get("exact") is True, "GPU proof failed: %s" % res)
    need(res.get("blocks") == len(before) // bb, "GPU decoded %s blocks, the payload has %d"
         % (res.get("blocks"), len(before) // bb))
    return res


def decoded_agree(py, gp):
    """The GPU really decoded the data: its count of texels where red != blue matches the
    reference decoder's (BC1-3 interpolation rounding may differ slightly)."""
    tol = max(16, py["texels_compared"] // 200)
    return gp["texels_compared"] == py["texels_compared"] > 0 and \
        abs(gp["texels_where_r_differs_from_b"] - py["texels_where_r_differs_from_b"]) <= tol


def load_oracle(ref_dir):
    """Team B's tiling probe + oracle (tile_reference.py) from their VERIFIED bytes; the oracle's
    `import texture_tiling_probe` is served the verified module, never a file from the path."""
    probe, _ = verified_module(ref_dir, "texture_tiling_probe.py", TILING_REFERENCE_SHA256["texture_tiling_probe.py"])
    saved, saved_path = sys.modules.get("texture_tiling_probe"), list(sys.path)
    sys.modules["texture_tiling_probe"] = probe
    try:
        oracle, _ = verified_module(ref_dir, "tile_reference.py", TILING_REFERENCE_SHA256["tile_reference.py"])
    finally:
        sys.path[:] = saved_path           # the oracle prepends its own folder; undo that
        if saved is None:
            sys.modules.pop("texture_tiling_probe", None)
        else:
            sys.modules["texture_tiling_probe"] = saved
    return probe, oracle


def tiling_gates(pt, probe, oracle, choice, fmt, size, lin, ps5_payloads, export_bytes, names):
    """Team B spec section 7, all four gates; any failure (or error) refuses. -> proof record."""
    try:
        return _tiling_gates(pt, probe, oracle, choice, fmt, size, lin, ps5_payloads, export_bytes, names)
    except Refused:
        raise
    except (ValueError, KeyError, IndexError, TypeError) as e:     # TilingRefused / TileError are ValueErrors
        raise Refused("tiling gates: %s: %s" % (type(e).__name__, e))


def _tiling_gates(pt, probe, oracle, choice, fmt, size, lin, ps5_payloads, export_bytes, names):
    sx, sy = size
    mode = choice["mode"]
    # 1 lossless (own de-tiler)
    need(pt.detile(fmt, sx, sy, ps5_payloads, choice) == lin, "gate 1: own de-tiler does not give back the linear mips")
    # 2 rule: stored sizes and NumMipsInTail as the oracle's public rule says
    ref = oracle.layout(fmt, sx, sy, 0, len(lin), mode)
    need(ref["sizes"] == [len(x) for x in ps5_payloads] and ref["num_mips_in_tail"] == choice["num_mips_in_tail"]
         and ref["first_tail"] == choice["chain"]["first_tail"], "gate 2: sizes / tail differ from the oracle's rule")
    # 3 oracle agreement: byte-equal to the oracle's own tiling, and the oracle de-tiles it back exactly
    need(oracle.tile_chain(lin, fmt, sx, sy, 0, mode) == ps5_payloads, "gate 3: output differs from the oracle")
    need(oracle.detile_chain(ps5_payloads, fmt, sx, sy, 0, mode) == lin,
         "gate 3: the oracle's de-tile does not give back the linear mips")
    # 4 read-back: the pixel probe reads the derived texture in the mode its header claims
    h = probe.platform_header(export_bytes, names)
    need(h is not None and h["has_opt_data"] and h["ext_data"] == mode and
         h["num_mips_in_tail"] == choice["num_mips_in_tail"], "gate 4: probe reads other header fields %r" % (h,))
    v = probe.judge(h, {k: x for k, x in enumerate(ps5_payloads) if x})
    verdict = v.get("verdict", "")
    need(verdict.startswith(choice["mode_name"] + " (header and pixels agree") or
         verdict.startswith("undecided (texture too uniform") or verdict.startswith("no decodable mip pair"),
         "gate 4: probe verdict %r (only 'pixels agree', 'too uniform' or 'no decodable pair' pass)" % verdict)
    return {"gate1_self_detile": "byte-exact", "gate2_rule": {"sizes": ref["sizes"],
                                                               "num_mips_in_tail": ref["num_mips_in_tail"]},
            "gate3_oracle": "tile_reference.tile_chain == output (byte-exact); detile_chain(output) == linear",
            "gate4_probe_verdict": verdict, "mips": len(lin), "bytes": sum(len(x) for x in lin),
            "reference_sha256": dict(TILING_REFERENCE_SHA256)}


def can_draw(start, entries, depth=16):
    """True if a package (or anything it imports, transitively through the merged mod +
    base stores) has shader maps: a material instance without its own shader maps still
    draws with its parent's."""
    seen, frontier = set(), [start]
    for _ in range(depth):
        nxt = []
        for p_ in frontier:
            if p_ in seen:
                continue
            seen.add(p_)
            e = entries.get(p_)
            if e is None:
                return True        # unknown package: cannot prove it does not draw (fail closed)
            if e["shader_maps"]:
                return True
            nxt.extend(e["imports"])
        frontier = nxt
        if not frontier:
            return False
    return True   # too deep: fail closed


def derive_textures(a, rows, out_rows, mt, mrd, mstore, blake3, cityhash, base_pids, base_entries):
    """Derived PS5-layout texture packages for the plan's texture_derivations: channels kept
    or R<->B swapped (exact), re-laid in the game's PS5 tile order (lossless), proven."""
    zen_mod = sys.modules.get("zen")
    bs, _ = verified_module(a.material_fallback, "bcn_swap.py", TEXTURE_PINNED["bcn_swap.py"])
    bd, _ = verified_module(a.material_fallback, "bcn_decode.py", TEXTURE_PINNED["bcn_decode.py"])
    pt, _ = verified_module(a.material_fallback, "ps5_tiling.py", TEXTURE_PINNED["ps5_tiling.py"])
    tp, _ = verified_module(a.material_fallback, "texture_package.py", TEXTURE_PINNED["texture_package.py"],
                            {"ZEN": zen_mod, "PS5_TILING": pt})
    ref_dir = a.tiling_reference or os.path.join(os.path.dirname(os.path.dirname(
        os.path.abspath(a.material_fallback))), "pak-intake")
    probe, oracle = load_oracle(ref_dir)
    cpath = os.path.join(a.material_fallback, "ps5_layout_census.json")
    cbytes = open(cpath, "rb").read()
    need(sha(cbytes) == TEXTURE_PINNED["ps5_layout_census.json"], "ps5_layout_census.json is not the pinned census")
    census = json.loads(cbytes)

    def choose(f, w_, h_, n_):
        return pt.choose(f, w_, h_, n_, census)
    law = cityhash.ue_name_hash
    shipped = {r["package"] for r in out_rows}
    wanted = {}
    for r in rows:
        if r.get("action") not in ("reparent", "reparent-partial"):
            continue
        for d in r.get("texture_derivations") or []:
            ch = d.get("channels")
            need(ch in ("keep", "swap_rb") and d.get("layout") == "ps5" and d.get("op") == ch,
                 "%s: unknown texture derivation %r" % (r["mod_package"], d))
            if r["mod_package"] not in shipped:
                continue        # row not derived (e.g. its user decision is missing): its textures are not shipped
            if d.get("slot") and d.get("parameter") != d.get("slot"):
                need((r.get("param_renames") or {}).get(d.get("parameter")) == d.get("slot"),
                     "routing row %s does not rename %s to %s" % (r["mod_package"], d.get("parameter"),
                                                                   d.get("slot")))
            key = (d["op"], ch, d["layout"], d.get("format"), d.get("from_letters"), d.get("to_letters"))
            w = wanted.setdefault(d["texture"], {"key": key, "consumers": []})
            need(w["key"] == key, "%s: plan rows disagree on its derivation" % d["texture"])
            w["consumers"].append({"package": r["mod_package"], "parameter": d["parameter"], "slot": d.get("slot")})
    # every texture-like mod package a shipped instance imports must be derived (PS5 copies only)
    for r in out_rows:
        pid = int(r["package_id"], 16)
        old = law(r["reparent"]["old_parent"])
        for i in mstore[pid]["imports"]:
            if i in mstore and i != old:
                need(any(law(t) == i for t in wanted), "%s imports mod package %016x that has no derived PS5 "
                     "copy" % (r["package"], i))
    importers = {}
    for pid_, e in mstore.items():
        for i in e["imports"]:
            importers.setdefault(i, set()).add(pid_)
    merged = dict(base_entries)
    merged.update(mstore)
    work = tempfile.mkdtemp(prefix="derive-tex-")
    out = []
    try:
        exe, swiftc = gpu_checker(a.material_fallback, work)
        for tex in sorted(wanted):
            w = wanted[tex]
            op, ch, lay_kind, fmt, frm, to = w["key"]
            pid = law(tex)
            need(pid in mstore, "%s not in the mod store" % tex)
            need(not mstore[pid]["shader_maps"], "%s has shader maps" % tex)
            need(pid not in base_pids, "%s has the id of a base package (base materials would read the derived "
                 "copy)" % tex)
            rows_ = {c[8:].hex(): i for i, c in enumerate(mt.chunk_ids) if int.from_bytes(c[:8], "little") == pid}
            need(set(rows_) <= {"00000001", "00000002"} and "00000001" in rows_,
                 "%s: chunk kinds %s (need export [+ bulk])" % (tex, sorted(rows_)))
            chunks = {}
            for kind, i in sorted(rows_.items()):
                data = mt.read_chunk(i, mrd)
                need(blake3.iohash20(data) == mt.metas[i][0], "%s: mod chunk %s IoHash mismatch" % (tex, kind))
                need(mt.metas[i][1] == 0, "%s: chunk %s has meta flags" % (tex, kind))
                chunks[kind] = (i, data)
            chunk = chunks["00000001"][1]
            bulk = chunks["00000002"][1] if "00000002" in chunks else None
            transform = bs.swap_rb if ch == "swap_rb" else keep_channels
            try:
                nc, nb, lay, facts, choice, lin = tp.derive_ps5(chunk, bulk, transform, choose)
            except pt.TilingRefused as e:
                raise Refused("%s: PS5 tile layout refused: %s" % (tex, e))
            except (tp.LayoutError, bs.SwapRefused, ValueError) as e:
                raise Refused("%s: not derivable: %s" % (tex, e))
            need(lay["format"] == fmt, "%s: format %s, plan says %s" % (tex, lay["format"], fmt))
            need(lay["package"] == tex, "%s: package name in the chunk is %s" % (tex, lay["package"]))
            olay = tp.layout(chunk, None if bulk is None else len(bulk))
            orig_lin = tp.payload(olay, chunk, bulk)
            before, after = b"".join(orig_lin), b"".join(lin)
            proof = {}
            if ch == "swap_rb":
                py = bd.swap_proof(fmt, before, after)
                need(py["exact"], "%s: Python decoder proof failed: %s" % (tex, py))
                gp = gpu_proof(exe, work, fmt, before, after, bd.BLOCK[fmt])
                need(decoded_agree(py, gp), "%s: the GPU's decode does not track the reference decoder (%s vs %s)"
                     % (tex, gp, py))
                gp["swiftc"] = swiftc
                proof.update(python=py, gpu=gp)
            else:
                need(lin == orig_lin, "%s: channels 'keep' changed texel bytes" % tex)
            ps5_payloads = tp.payload(lay, nc, nb)
            exp_bytes = nc[lay["export_offset"]:lay["export_offset"] + lay["export_size"]]
            try:
                proof["tiling"] = tiling_gates(pt, probe, oracle, choice, fmt, lay["size"], lin, ps5_payloads,
                                               exp_bytes, zen_mod.ZenPackage(nc).names)
            except Refused as e:
                raise Refused("%s: %s" % (tex, e))
            users = sorted(importers.get(pid, ()))
            routing = {law(c["package"]) for c in w["consumers"]}
            others = []
            for u in users:
                if u in routing:
                    continue
                need(not can_draw(u, merged), "%s: importer %016x can draw (it or its parents have shader maps)"
                     % (tex, u))
                others.append("%016x" % u)
            need(routing <= set(users), "%s: a routing row does not import it" % tex)
            cks = []
            for kind, new in (("00000001", nc), ("00000002", nb)):
                if kind not in chunks:
                    need(new is None, "%s: derived bulk chunk but the mod has none" % tex)
                    continue
                need(new is not None, "%s: derived copy has no bulk chunk" % tex)
                i, orig = chunks[kind]
                rel = "derived/%016x.%s.bin" % (pid, "export" if kind == "00000001" else "bulk")
                with open(os.path.join(a.out, rel), "wb") as fh:
                    fh.write(new)
                cks.append({"chunk_id": mt.chunk_ids[i].hex(), "kind": "export" if kind == "00000001" else "bulk",
                            "original": {"bytes": len(orig), "sha256": sha(orig), "iohash": mt.metas[i][0]},
                            "derived": {"path": rel, "bytes": len(new), "sha256": sha(new),
                                        "iohash": blake3.iohash20(new)}})
            out.append({
                "package": tex, "package_id": "%016x" % pid, "op": op, "channels": ch, "format": fmt,
                "from_letters": frm, "to_letters": to,
                "consumers": sorted(w["consumers"], key=lambda c: (c["package"], c["parameter"])),
                "other_importers_cannot_draw": others,
                "store_imports": ["%016x" % v for v in mstore[pid]["imports"]],
                "chunks": cks,
                "layout": {"form": "ps5", "size": lay["size"], "mode": choice["mode_name"],
                           "ext_data": choice["ext_data"], "num_mips_in_tail": choice["num_mips_in_tail"],
                           "first_tail": choice["chain"]["first_tail"],
                           "game_layout_source": choice["game_layout_source"],
                           "census_sha256": TEXTURE_PINNED["ps5_layout_census.json"],
                           "order_source": choice["order_source"],
                           "mips": [[m["x"], m["y"], m["where"], m["offset"], m["size"]] for m in lay["mips"]]},
                "swap": ({"blocks": sum(f["blocks"] for f in facts),
                          "endpoint_order_fixed": sum(f["endpoint_order_fixed"] for f in facts),
                          "bc7_rotation_remapped": sum(f["bc7_rotation_remapped"] for f in facts)}
                         if ch == "swap_rb" else None),
                "proof": proof,
            })
            print("derive_substitutes: texture %s %s channels=%s -> PS5 %s (tail %d mips): %s4 tiling gates pass "
                  "(probe: %s)" % (tex, fmt, ch, choice["mode_name"], choice["num_mips_in_tail"],
                                      ("%d texels exact on both decoders, " % proof["python"]["texels_compared"])
                                      if ch == "swap_rb" else "", proof["tiling"]["gate4_probe_verdict"][:40]))
    finally:
        shutil.rmtree(work, ignore_errors=True)
    return out


def keep_channels(fmt, data):
    return data, {"format": fmt, "blocks": None, "endpoint_order_fixed": 0, "bc7_rotation_remapped": 0}


def file_sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for blk in iter(lambda: fh.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


if __name__ == "__main__":
    sys.exit(main())
