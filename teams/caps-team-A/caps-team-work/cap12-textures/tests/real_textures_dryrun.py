#!/usr/bin/env python3
"""Real-data dry run for capability #12 derived textures (Mac-only, read-only inputs).

Inputs (never written): the game's own files already on the Mac (fetched headers
r1-r3, the 5 container headers, the asset registry, the shader libraries), the
integrated build record + mirror, the unchanged Leon 2K trio.

 1. reparent_plan.py --derive-textures (staged tools): the plan must equal the
    evidence copy byte for byte, keep ORM on 10 parts, and WITHOUT the flag the
    plan must equal Team A's round-4 plan (A/B).
 2. derive_substitutes.py: without the user decision 0 rows (exit 1); with it
    10 material rows + 30 derived PS5-layout textures (10 with the R<->B swap,
    proven by both decoders; all 30 through the four tiling gates).
 3. paktx install --dry-run: plain, plain with the INTEGRATED paktx (A/B, must
    be byte-identical), substitute without the decision (refused), substitute.
 4. Independent re-read with the second reader set (utoc / zen / container_header
    / blake3, not overlay_builder): base rows identical to the plain plan; every
    sidecar row identical except the 70 derived chunks (10 material + 30 texture
    export + 30 texture bulk), which equal the manifest bytes with matching
    IoHash; the merged store differs in exactly the 10 material entries; each
    shipped texture, read back from the derived sidecar and de-tiled with Team
    B's oracle, gives back Leon's exact original bytes (keep) or is re-proven
    R<->B exact against them (swap, Python reference decoder); all 40
    package imports of the derived materials resolve at export level; the
    pakchunk10 target is byte-identical.
JSON receipt on stdout; exit 0 only if everything passed and > 0 items.
"""
import hashlib
import json
import os
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
TEAM = os.path.dirname(HERE)
REPO = os.environ.get("PAKTX_REPO", "/WORKSPACE/.codex/worktrees/translation-core/what")
L25 = os.path.join(REPO, "outputs/2026-10-08-l25-prep")
BUILDER = os.path.join(L25, "pak-overlay/overlay_builder.py")
RECORD = os.path.join(L25, "pak-overlay/records/townfall.record.json")
MIRROR = os.path.join(L25, "pak-mirror-townfall-20261013")
LEON = os.path.join(REPO, ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted")
STEM = "zzz_Leon_Townfall_2K_P"
TOOLS = os.path.join(TEAM, "PATCH", "tools")
INTEGRATED = os.path.join(L25, "pak-overlay", "paktx.py")
R4 = os.path.join(TEAM, "..", "cap12-fulllook", "evidence", "reparent-plan-r4-fulllook.json")
EVPLAN = os.path.join(TEAM, "evidence", "reparent-plan-r5-textures.json")
OUT = os.path.join(HERE, "work", "real")
TILEREF = os.environ.get("TILEREF", "/WORKSPACE/.codex/worktrees/translation-core/caps-team-B/caps-team-work/"
                                    "cap12-layout-invisible/PATCH/tools")
PY = [sys.executable, "-I"]
sys.path.insert(0, TOOLS)
import bcn_decode  # noqa: E402
import blake3  # noqa: E402
import cityhash  # noqa: E402
import container_header  # noqa: E402
import texture_package as tpkg  # noqa: E402
import utoc  # noqa: E402
import zen  # noqa: E402
import derive_substitutes as DS  # noqa: E402
PROBE, ORACLE = DS.load_oracle(TILEREF)


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=7200)
    return p.returncode, p.stdout, p.stderr


def paktx(work, mod, extra, tool=None):
    cmd = PY + [tool or os.path.join(TOOLS, "paktx.py"), "--builder", BUILDER, "install", "--record", RECORD,
                "--mirror", MIRROR, "--mod", mod, "--work", work, "--dry-run"] + \
          ([] if tool else ["--readers", TOOLS, "--tiling-reference", TILEREF]) + extra
    rc, so, se = run(cmd)
    try:
        res = json.loads(so)
    except ValueError:
        res = {"unparsed": so[-500:], "stderr": se[-1500:]}
    return rc, res


class Side:
    def __init__(self, tdir, stem):
        self.toc = utoc.load(os.path.join(tdir, stem + ".utoc"))
        self.side = open(os.path.join(tdir, stem + "_s1.ucas"), "rb").read()
        self.base = self.toc.partition_size
        self.row = {c: i for i, c in enumerate(self.toc.chunk_ids)}

    def in_sidecar(self, row):
        rng = self.toc.physical_ranges(row)
        return bool(rng) and all(o >= self.base for o, _ in rng)

    def read(self, row):
        def r(o, n):
            assert o >= self.base, "base partition read requested"
            return self.side[o - self.base:o - self.base + n]
        return self.toc.read_chunk(row, r)


def main():
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(OUT)
    mod = os.path.join(OUT, "mod")
    os.makedirs(mod)
    for ext in ("utoc", "ucas", "pak"):
        os.symlink(os.path.join(LEON, STEM + "." + ext), os.path.join(mod, STEM + "." + ext))
    out = {"label": "Mac-only", "inputs": {"record": RECORD, "mirror": MIRROR}}
    items = 0
    ok = True

    def check(cond, what):
        nonlocal items, ok
        items += 1
        if not cond:
            ok = False
            out.setdefault("failures", []).append(what)
        return cond

    # 1: the plan
    plan_path = os.path.join(OUT, "reparent-plan-r5-textures.json")   # same name as the evidence copy
    rc, so, se = run(["sh", os.path.join(TEAM, "run_plan.sh"), plan_path, "--accept-unproven-opaque",
                      "--derive-textures"])
    check(rc == 0 and "30 derived PS5-layout texture(s), 10 of them with an exact R<->B swap" in so,
          "plan r5: " + so[-600:] + se[-400:])
    pb = open(plan_path, "rb").read()
    out["plan_sha256"] = hashlib.sha256(pb).hexdigest()
    check(pb == open(EVPLAN, "rb").read(), "plan r5 == evidence copy")
    plan = json.loads(pb)
    reps = [r for r in plan["rows"] if r["action"] == "reparent"]
    check(len(reps) == 10 and all(r["coverage"] == 1.0 and "ORM" in r["param_map"] and
                                  r["param_map"]["ORM"]["how"] == "derived:swap_rb" for r in reps),
          "10 parts keep ORM through a derived texture")
    off_path = os.path.join(OUT, "plan-off.json")
    rc, so, se = run(["sh", os.path.join(TEAM, "run_plan.sh"), off_path, "--allow-partial",
                      "--accept-unproven-opaque"])
    off = json.load(open(off_path))
    check(off.pop("derive_textures") is False and off.pop("derive_rules") is None and off == json.load(open(R4)),
          "without --derive-textures the plan equals Team A's round-4 plan")
    # 2: the manifest
    gen = ["sh", os.path.join(TEAM, "run_derive.sh"), plan_path]
    rc, so, se = run(gen + [os.path.join(OUT, "manifest-none")])
    check(rc == 1 and "zero rows derived" in so, "generator without the decision derives 0 and exits 1")
    mdir = os.path.join(OUT, "manifest")
    rc, so, se = run(gen + [mdir, "--accept-unproven-opaque"])
    check(rc == 0, "generator: " + so[-600:] + se[-400:])
    man = json.load(open(os.path.join(mdir, "manifest.json")))
    out["manifest_sha256"] = man["manifest_sha256"]
    check(len(man["rows"]) == 10 and len(man["textures"]) == 30, "10 material rows + 30 derived PS5 textures")
    check(sum(t["channels"] == "swap_rb" for t in man["textures"]) == 10, "10 of them with the R<->B swap")
    tex_texels = 0
    for t in man["textures"]:
        check(t["layout"]["form"] == "ps5" and t["proof"]["tiling"]["gate3_oracle"].startswith("tile_reference"),
              "four tiling gates recorded for %s" % t["package"])
        if t["channels"] != "swap_rb":
            continue
        pyp, gp = t["proof"]["python"], t["proof"]["gpu"]
        check(pyp["exact"] and gp["exact"] and pyp["texels_compared"] == gp["texels_compared"] > 0,
              "both decoders exact for %s" % t["package"])
        tex_texels += pyp["texels_compared"]
    out["texels_proven_at_derive_time_per_decoder"] = tex_texels
    # 3: dry runs
    rc0, base = paktx(os.path.join(OUT, "plan-base"), mod, [])
    check(rc0 == 0 and base.get("targets") == ["pakchunk0-ps5", "pakchunk10-ps5"], "baseline plan %r" % base)
    rcI, integ = paktx(os.path.join(OUT, "plan-integrated"), mod, [], tool=INTEGRATED)
    check(rcI == 0, "integrated paktx plain plan %r" % integ)
    sub_args = ["--substitute", os.path.join(mdir, "manifest.json"), "--reparent-plan", plan_path]
    rcn, nod = paktx(os.path.join(OUT, "plan-nodecision"), mod, sub_args)
    check(rcn == 2 and nod.get("code") == "substitute_user_decision_missing", "no-decision refusal %r" % nod)
    rc1, sub = paktx(os.path.join(OUT, "plan-sub"), mod, sub_args + ["--accept-decision", "accept-unproven-opaque"])
    check(rc1 == 0 and sub.get("targets") == ["pakchunk0-ps5", "pakchunk10-ps5"], "substitute plan %r" % sub)
    out["plan_sha256_baseline"] = base.get("plan_sha256")
    out["plan_sha256_substitute"] = sub.get("plan_sha256")
    if rc0 != 0 or rc1 != 0 or rcI != 0:
        out["result"] = "FAIL"
        out["items_checked"] = items
        print(json.dumps(out, indent=1, sort_keys=True))
        return 1
    psub = json.load(open(os.path.join(OUT, "plan-sub", "plan.json")))
    out["plan_substitution_post_build"] = psub["substitution"]["post_build"]
    check(psub["substitution"]["post_build"][0]["substituted_chunks_verified"] == 70, "70 chunks verified post-build")
    check(len(psub["substitution"]["textures"]) == 30, "30 texture ledger rows")
    shutil.copyfile(os.path.join(OUT, "plan-sub", "plan.json"), os.path.join(OUT, "plan-sub.json"))
    # 4: independent re-read
    a = Side(os.path.join(OUT, "plan-base", "targets", "pakchunk0-ps5"), "pakchunk0-ps5")
    b = Side(os.path.join(OUT, "plan-sub", "targets", "pakchunk0-ps5"), "pakchunk0-ps5")
    check(a.toc.chunk_ids == b.toc.chunk_ids, "same chunk-id order")
    hid = a.toc.container_id.to_bytes(8, "little") + bytes.fromhex("00000006")
    derived = {bytes.fromhex(r["chunk_id"]): open(os.path.join(mdir, r["derived"]["path"]), "rb").read()
               for r in man["rows"]}
    for t in man["textures"]:
        for c in t["chunks"]:
            derived[bytes.fromhex(c["chunk_id"])] = open(os.path.join(mdir, c["derived"]["path"]), "rb").read()
    check(len(derived) == 70, "70 derived chunks in the manifest")
    base_same = side_same = side_derived = 0
    for i, c in enumerate(a.toc.chunk_ids):
        if not a.in_sidecar(i):
            same = (a.toc.offlen[i] == b.toc.offlen[i] and a.toc.metas[i] == b.toc.metas[i] and not b.in_sidecar(i))
            if not check(same, "base row %s identical" % c.hex()):
                continue
            base_same += 1
            continue
        check(b.in_sidecar(i), "row %s in sidecar in both" % c.hex())
        pb_ = b.read(i)
        check(blake3.iohash20(pb_) == b.toc.metas[i][0], "IoHash of %s" % c.hex())
        if c in derived:
            check(pb_ == derived[c], "derived payload %s" % c.hex())
            side_derived += 1
        elif c != hid:
            check(pb_ == a.read(i), "unchanged payload %s" % c.hex())
            side_same += 1
    check(side_derived == 70, "70 derived chunks shipped")
    _c, ha = container_header.parse(a.read(a.row[hid]))
    _c, hb = container_header.parse(b.read(b.row[hid]))
    check(set(ha) == set(hb), "same merged package set")
    diff = sorted(p for p in ha if ha[p] != hb[p])
    check(diff == sorted(int(r["package_id"], 16) for r in man["rows"]),
          "merged store differs in exactly the 10 material entries (texture entries unchanged)")
    # re-prove every shipped texture from the derived sidecar against Leon's original
    lt = utoc.load(os.path.join(LEON, STEM + ".utoc"))

    def lrd(o, n):
        with open(os.path.join(LEON, STEM + ".ucas"), "rb") as fh:
            fh.seek(o)
            return fh.read(n)
    reproven = 0
    for t in man["textures"]:
        got, orig = {}, {}
        for c in t["chunks"]:
            cid = bytes.fromhex(c["chunk_id"])
            got[c["kind"]] = b.read(b.row[cid])
            orig[c["kind"]] = lt.read_chunk(lt.chunk_ids.index(cid), lrd)
        lay = tpkg.layout(got["export"], len(got["bulk"]) if "bulk" in got else None, allow_ps5=True)
        olay = tpkg.layout(orig["export"], len(orig["bulk"]) if "bulk" in orig else None)
        w, h = lay["size"]
        lin = ORACLE.detile_chain(tpkg.payload(lay, got["export"], got.get("bulk")), lay["format"], w, h, 0,
                                  lay["ps5"]["ext_data"])          # Team B's de-tiler on the shipped bytes
        olin = tpkg.payload(olay, orig["export"], orig.get("bulk"))
        if t["channels"] == "swap_rb":
            pr = bcn_decode.swap_proof(lay["format"], b"".join(olin), b"".join(lin))
            check(pr["exact"] and pr["texels_compared"] == t["proof"]["python"]["texels_compared"],
                  "shipped %s re-proven (de-tiled by the oracle, R<->B exact)" % t["package"])
            reproven += pr["texels_compared"]
        else:
            check(lin == olin, "shipped %s de-tiles to Leon's exact original bytes" % t["package"])
            reproven += sum(len(x) for x in lin)
        check(lay["ps5"]["ext_data"] == t["layout"]["ext_data"] and
              lay["ps5"]["num_mips_in_tail"] == t["layout"]["num_mips_in_tail"],
              "shipped %s header fields" % t["package"])
    out["texels_or_bytes_reproven_from_shipped_sidecar"] = reproven
    known = set(hb)
    for s in man["base_stores"]:
        data = open(os.path.join(mdir, s["path"]), "rb").read()
        bt = utoc.load(os.path.join(MIRROR, s["container"] + ".utoc"))
        check(bt.metas[bt.rows_of_type(6)[0]][0] == blake3.iohash20(data), "base store %s IoHash" % s["container"])
        known |= set(container_header.parse(data)[1])
    bh = {int(h["package_id"], 16): zen.ZenPackage(open(os.path.join(mdir, h["path"]), "rb").read())
          for h in man["base_headers"]}
    export_level = 0
    for r in man["rows"]:
        pid = int(r["package_id"], 16)
        ea, eb = ha[pid]["imports"], hb[pid]["imports"]
        slots = [k for k in range(len(ea)) if ea[k] != eb[k]]
        check(len(slots) == 1 and eb[slots[0]] == cityhash.ue_name_hash(r["reparent"]["new_parent"]),
              "one slot repointed for %s" % r["package"])
        q = zen.ZenPackage(derived[bytes.fromhex(r["chunk_id"])])
        check("MRO" in q.names and "ORM" not in q.names, "%s binds MRO" % r["package"])
        for k, v in q.imports:
            if k != "PackageImport":
                continue
            tname = q.imported_packages[v[0]]
            tpid = cityhash.ue_name_hash(tname)
            check(tpid == eb[v[0]] and tpid in known, "%s import %s package level" % (r["package"], tname))
            tc = tpid.to_bytes(8, "little") + bytes.fromhex("00000001")
            tp = bh.get(tpid) or zen.ZenPackage(b.read(b.row[tc]))
            check(q.import_hashes[v[1]] in {e["public_hash"] for e in tp.exports},
                  "%s import %s export level" % (r["package"], tname))
            export_level += 1
    check(export_level == 40, "40 package imports resolved at export level")
    ab = 0
    for dest in ("pakchunk0-ps5", "pakchunk10-ps5"):
        for n in (dest + ".utoc", dest + "_s1.ucas"):
            x = open(os.path.join(OUT, "plan-integrated", "targets", dest, n), "rb").read()
            y = open(os.path.join(OUT, "plan-base", "targets", dest, n), "rb").read()
            ab += check(x == y, "plain install %s identical to the integrated paktx" % n)
    for n in ("pakchunk10-ps5.utoc", "pakchunk10-ps5_s1.ucas"):
        x = open(os.path.join(OUT, "plan-base", "targets", "pakchunk10-ps5", n), "rb").read()
        y = open(os.path.join(OUT, "plan-sub", "targets", "pakchunk10-ps5", n), "rb").read()
        check(x == y, "pakchunk10 %s identical" % n)
    out.update({"base_rows_identical": base_same, "sidecar_rows_identical": side_same,
                "derived_chunks_verified": side_derived, "imports_resolved_export_level": export_level,
                "merged_store_entries_changed": len(diff), "plain_install_files_identical_to_integrated": ab})
    for d in ("plan-base", "plan-sub", "plan-integrated"):
        shutil.rmtree(os.path.join(OUT, d, "targets"), ignore_errors=True)
    out["items_checked"] = items
    out["result"] = "PASS" if ok and items > 0 else "FAIL"
    print(json.dumps(out, indent=1, sort_keys=True))
    return 0 if out["result"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
