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

Generic: no game or mod names; every name/hash comes from the plan and the
game's / mod's own files.
"""
import argparse
import hashlib
import json
import os
import subprocess
import sys

sys.dont_write_bytecode = True
SCHEMA = "paktx-substitute/1"
# the reviewed readers + writer (mod-manager/material-fallback/, Team F cap12)
PINNED = {
    "blake3.py": "0613633f746a8e4f6c49baee1602f411cbdc1adbb793bf401b2a8e8c17b1355e",
    "cityhash.py": "95955e5872a067d38e024b1f0bad74753842bc24eb79be689afb588898b5167a",
    "container_header.py": "e703f080adf49c9ff5bd90a41b13733b367dafe9f74e0b8306c1e1ddad964459",
    "utoc.py": "9b5bf8f12e2626fa3949d16c2292427dbdb573e20b51840ee63cdfd8379a9b6f",
    "zen.py": "b3c421d448585ab51a91af7d935ff610633968f11dc258158cae4b3bcc889f4f",
    "mic_reparent.py": "520a805580a6750e9f799c8740095d4cc5d6f3e3a0a3574e6052914e82c06af5",
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


def readers(folder):
    for fn, want in sorted(PINNED.items()):
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
    ap.add_argument("--out", required=True, help="new, empty folder")
    a = ap.parse_args(argv)
    try:
        res = run(a)
    except (Refused, ValueError, OSError) as e:
        print("derive_substitutes: FAIL: %s; 0 rows written" % e)
        return 1
    print("derive_substitutes: %d derived, %d not derived, manifest %s sha256 %s"
          % (res["derived"], res["not_derived"], res["manifest"], res["manifest_sha256"]))
    return 0 if res["derived"] > 0 else 1


def run(a):
    blake3, cityhash, container_header, utoc, zen = readers(a.material_fallback)
    mic = os.path.join(a.material_fallback, "mic_reparent.py")
    need(os.path.isfile(mic), "mic_reparent.py not found in %s" % a.material_fallback)
    need(not os.path.exists(a.out) or not os.listdir(a.out), "--out must be a new, empty folder")
    for sub in ("in", "derived", "base-headers", "base-stores"):
        os.makedirs(os.path.join(a.out, sub), exist_ok=True)
    plan_bytes = open(a.plan, "rb").read()
    plan = json.loads(plan_bytes)
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
    stores, base_pids = [], {}
    for path in a.base_store:
        data = open(path, "rb").read()
        h = blake3.iohash20(data)
        hits = [s for s, t in base.items() for r in t.rows_of_type(6) if t.metas[r][0] == h]
        need(len(hits) == 1, "%s matches %d base TOC header rows by IoHash" % (path, len(hits)))
        _c, st = container_header.parse(data)
        for p in st:
            base_pids.setdefault(p, []).append(hits[0])
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
    doc = {
        "schema": SCHEMA, "label": "Mac-only",
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
    doc["manifest_sha256"] = sha(canonical(doc))
    mpath = os.path.join(a.out, "manifest.json")
    with open(mpath, "w") as fh:
        json.dump(doc, fh, indent=1, sort_keys=True)
        fh.write("\n")
    return {"derived": len(out_rows), "not_derived": len(skipped), "manifest": mpath,
            "manifest_sha256": doc["manifest_sha256"]}


def file_sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for blk in iter(lambda: fh.read(1 << 20), b""):
            h.update(blk)
    return h.hexdigest()


if __name__ == "__main__":
    sys.exit(main())
