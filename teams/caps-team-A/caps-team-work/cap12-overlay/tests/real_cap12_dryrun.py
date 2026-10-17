#!/usr/bin/env python3
"""Real-data dry run for capability #12 (Mac-only, read-only inputs).

Inputs (never written): the integrated reparent plan r3 (10 reparent-partial
rows), the fetched base headers (headers-r1..r3) and the 5 base container
headers, the integrated build record + mirror, the unchanged Leon 2K trio.

Steps:
 1. derive_substitutes.py (with the two user decisions) -> manifest + 10
    derived packages (via mic_reparent.py, unchanged).
 2. The same generator WITHOUT the decisions derives 0 rows and exits 1.
 3. paktx install --dry-run twice: plain (baseline) and with --substitute.
    The substitute plan without --accept-decision is refused (exit 2).
 4. Independent re-read with the SECOND reader set (Team F utoc / zen /
    container_header / blake3, not overlay_builder):
      - both derived indexes parse; same chunk-id set; base rows identical;
      - every chunk payload is identical between baseline and substitute,
        except the 10 derived chunks (== the derived bytes, IoHash ok) and
        the merged ContainerHeader;
      - merged headers differ in exactly the 10 entries, one import slot
        each (old mod parent -> base parent);
      - every package import of every derived package resolves at export
        level (target headers read from the derived sidecar or the
        IoHash-verified base header) and at package level (merged store or
        another base store);
      - the pakchunk10 target is byte-identical in both plans.
Outputs under tests/work/real12/ (derived targets deleted at the end) and a
JSON receipt on stdout.  Exit 0 only if everything passed and >0 items.
"""
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
MF = os.path.join(L25, "mod-manager/material-fallback")
BUILDER = os.path.join(L25, "pak-overlay/overlay_builder.py")
RECORD = os.path.join(L25, "pak-overlay/records/townfall.record.json")
MIRROR = os.path.join(L25, "pak-mirror-townfall-20261013")
PLAN = os.path.join(L25, "evidence/cap12-20261012/reparent-plan-r3-partial-opaque.json")
FET = os.path.join(L25, "evidence/cap12-fetched-20261013")
LEON = os.path.join(REPO, ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted")
STEM = "zzz_Leon_Townfall_2K_P"
TOOLS = os.path.join(TEAM, "PATCH", "tools")
INTEGRATED = os.path.join(L25, "pak-overlay", "paktx.py")   # a7446c1b..., for the A/B check
OUT = os.path.join(HERE, "work", "real12")
PY = [sys.executable, "-I"]
sys.path.insert(0, MF)
import blake3  # noqa: E402
import cityhash  # noqa: E402
import container_header  # noqa: E402
import utoc  # noqa: E402
import zen  # noqa: E402


def run(cmd):
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=3600)
    return p.returncode, p.stdout, p.stderr


def gen(out, decisions):
    cmd = PY + [os.path.join(TOOLS, "derive_substitutes.py"), "--plan", PLAN,
                "--mod-utoc", os.path.join(LEON, STEM + ".utoc"), "--mod-ucas", os.path.join(LEON, STEM + ".ucas"),
                "--mirror", MIRROR, "--headers"] + [os.path.join(FET, "headers-r%d" % i) for i in (1, 2, 3)] + \
          ["--base-store"] + [os.path.join(FET, "manifests", "%s.container-header.bin" % s) for s in
                              ("pakchunk0-ps5", "pakchunk1-ps5", "pakchunk10-ps5", "pakchunk11-ps5",
                               "pakchunk12-ps5")] + \
          ["--material-fallback", MF, "--out", out] + decisions
    return run(cmd)


def paktx(work, mod, extra, tool=None):
    cmd = PY + [tool or os.path.join(TOOLS, "paktx.py"), "--builder", BUILDER, "install", "--record", RECORD,
                "--mirror", MIRROR, "--mod", mod, "--work", work, "--dry-run"] + \
          ([] if tool else ["--readers", MF]) + extra
    rc, so, se = run(cmd)
    try:
        res = json.loads(so)
    except ValueError:
        res = {"unparsed": so[-500:], "stderr": se[-1500:]}
    return rc, res


class Side:
    """A derived index + its sidecar, read with the second reader set."""

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
    out = {"label": "Mac-only", "inputs": {"plan": PLAN, "record": RECORD, "mirror": MIRROR}}
    items = 0
    ok = True

    def check(cond, what):
        nonlocal items, ok
        items += 1
        if not cond:
            ok = False
            out.setdefault("failures", []).append(what)

    # 1 + 2: generator with / without the user decisions
    rc, so, se = gen(os.path.join(OUT, "manifest-none"), [])
    check(rc == 1 and "zero rows derived" in so, "generator without decisions must derive 0 and exit 1")
    mdir = os.path.join(OUT, "manifest")
    rc, so, se = gen(mdir, ["--allow-partial", "--accept-unproven-opaque"])
    check(rc == 0, "generator with decisions: " + so[-400:] + se[-400:])
    man = json.load(open(os.path.join(mdir, "manifest.json")))
    out["manifest_sha256"] = man["manifest_sha256"]
    out["derived_rows"] = len(man["rows"])
    check(len(man["rows"]) == 10, "10 derived rows")
    for r in man["rows"]:
        d = open(os.path.join(mdir, r["derived"]["path"]), "rb").read()
        q = zen.ZenPackage(d)
        check(q.name == r["package"] and blake3.iohash20(d) == r["derived"]["iohash"],
              "derived %s re-parses with its IoHash" % r["package"])
    # 3: dry runs
    rc0, base = paktx(os.path.join(OUT, "plan-base"), mod, [])
    check(rc0 == 0 and base.get("targets") == ["pakchunk0-ps5", "pakchunk10-ps5"], "baseline plan %r" % base)
    rcI, integ = paktx(os.path.join(OUT, "plan-integrated"), mod, [], tool=INTEGRATED)
    check(rcI == 0, "integrated paktx plain plan %r" % integ)
    sub_args = ["--substitute", os.path.join(mdir, "manifest.json"), "--reparent-plan", PLAN]
    rcn, nod = paktx(os.path.join(OUT, "plan-nodecision"), mod, sub_args)
    check(rcn == 2 and nod.get("code") == "substitute_user_decision_missing", "no-decision refusal %r" % nod)
    rcp, part = paktx(os.path.join(OUT, "plan-onedecision"), mod, sub_args + ["--accept-decision", "allow-partial"])
    check(rcp == 2 and part.get("code") == "substitute_user_decision_missing", "one-decision refusal %r" % part)
    rcq, nop = paktx(os.path.join(OUT, "plan-noplan"), mod, sub_args[:2] + [
        "--accept-decision", "allow-partial", "--accept-decision", "accept-unproven-opaque"])
    check(rcq == 2 and nop.get("code") == "substitute_needs_reparent_plan", "no-plan refusal %r" % nop)
    rc1, sub = paktx(os.path.join(OUT, "plan-sub"), mod,
                     sub_args + ["--accept-decision", "allow-partial", "--accept-decision", "accept-unproven-opaque"])
    check(rc1 == 0 and sub.get("targets") == ["pakchunk0-ps5", "pakchunk10-ps5"], "substitute plan %r" % sub)
    out["plan_sha256_baseline"] = base.get("plan_sha256")
    out["plan_sha256_substitute"] = sub.get("plan_sha256")
    if rc0 != 0 or rc1 != 0 or rcI != 0:
        out["result"] = "FAIL"
        out["items_checked"] = items
        print(json.dumps(out, indent=1, sort_keys=True))
        return 1
    plan = json.load(open(os.path.join(OUT, "plan-sub", "plan.json")))
    out["plan_substitution_post_build"] = plan["substitution"]["post_build"]
    shutil.copyfile(os.path.join(OUT, "plan-sub", "plan.json"), os.path.join(OUT, "plan-sub.json"))
    shutil.copyfile(os.path.join(OUT, "plan-base", "plan.json"), os.path.join(OUT, "plan-base.json"))
    # 4: independent re-read
    tb = os.path.join(OUT, "plan-base", "targets", "pakchunk0-ps5")
    ts = os.path.join(OUT, "plan-sub", "targets", "pakchunk0-ps5")
    a, b = Side(tb, "pakchunk0-ps5"), Side(ts, "pakchunk0-ps5")
    check(a.toc.chunk_ids == b.toc.chunk_ids, "same chunk-id order")
    hid = a.toc.container_id.to_bytes(8, "little") + bytes.fromhex("00000006")
    derived = {bytes.fromhex(r["chunk_id"]): open(os.path.join(mdir, r["derived"]["path"]), "rb").read()
               for r in man["rows"]}
    base_rows_same = side_same = side_derived = 0
    for i, c in enumerate(a.toc.chunk_ids):
        if not a.in_sidecar(i):
            same = (a.toc.offlen[i] == b.toc.offlen[i] and a.toc.metas[i] == b.toc.metas[i] and
                    not b.in_sidecar(i))
            check(same, "base row %s identical" % c.hex())
            base_rows_same += same
            continue
        check(b.in_sidecar(i), "row %s in sidecar in both" % c.hex())
        pb = b.read(i)
        check(blake3.iohash20(pb) == b.toc.metas[i][0], "IoHash of %s" % c.hex())
        if c in derived:
            check(pb == derived[c], "derived payload %s" % c.hex())
            side_derived += 1
        elif c == hid:
            continue
        else:
            check(pb == a.read(i), "unchanged payload %s" % c.hex())
            side_same += 1
    check(side_derived == 10, "10 derived chunks shipped")
    _c, ha = container_header.parse(a.read(a.row[hid]))
    _c, hb = container_header.parse(b.read(b.row[hid]))
    check(set(ha) == set(hb), "same merged package set")
    diff = sorted(p for p in ha if ha[p] != hb[p])
    want = sorted(int(r["package_id"], 16) for r in man["rows"])
    check(diff == want, "merged store differs in exactly the 10 entries")
    # base stores (for package level), IoHash-tied as the manifest says
    known = set(hb)
    for s in man["base_stores"]:
        data = open(os.path.join(mdir, s["path"]), "rb").read()
        bt = utoc.load(os.path.join(MIRROR, s["container"] + ".utoc"))
        hrow = bt.rows_of_type(6)[0]
        check(bt.metas[hrow][0] == blake3.iohash20(data), "base store %s IoHash" % s["container"])
        known |= set(container_header.parse(data)[1])
    bh = {}
    for h in man["base_headers"]:
        bh[int(h["package_id"], 16)] = zen.ZenPackage(open(os.path.join(mdir, h["path"]), "rb").read())
    export_level = 0
    for r in man["rows"]:
        pid = int(r["package_id"], 16)
        ea, eb = ha[pid]["imports"], hb[pid]["imports"]
        slots = [k for k in range(len(ea)) if ea[k] != eb[k]]
        check(len(slots) == 1 and eb[slots[0]] == cityhash.ue_name_hash(r["reparent"]["new_parent"]) and
              ea[slots[0]] == cityhash.ue_name_hash(r["reparent"]["old_parent"]),
              "one slot repointed for %s" % r["package"])
        check(ha[pid]["shader_maps"] == hb[pid]["shader_maps"], "shader maps kept for %s" % r["package"])
        q = zen.ZenPackage(derived[bytes.fromhex(r["chunk_id"])])
        for k, v in q.imports:
            if k != "PackageImport":
                continue
            tname = q.imported_packages[v[0]]
            tpid = cityhash.ue_name_hash(tname)
            check(tpid == eb[v[0]] and tpid in known, "%s import %s package level" % (r["package"], tname))
            tc = tpid.to_bytes(8, "little") + bytes.fromhex("00000001")
            if tpid in bh:
                tp = bh[tpid]
            else:
                check(tc in b.row and b.in_sidecar(b.row[tc]), "%s import %s served by the mod" % (r["package"], tname))
                tp = zen.ZenPackage(b.read(b.row[tc]))
            check(q.import_hashes[v[1]] in {e["public_hash"] for e in tp.exports},
                  "%s import %s export level" % (r["package"], tname))
            export_level += 1
    check(export_level == 40, "40 package imports resolved at export level")
    # A/B: the plain (no --substitute) install is byte-identical to the integrated paktx's
    ab = 0
    for dest in ("pakchunk0-ps5", "pakchunk10-ps5"):
        for n in (dest + ".utoc", dest + "_s1.ucas"):
            x = open(os.path.join(OUT, "plan-integrated", "targets", dest, n), "rb").read()
            y = open(os.path.join(OUT, "plan-base", "targets", dest, n), "rb").read()
            check(x == y, "plain install %s identical to integrated paktx" % n)
            ab += x == y
    out["plain_install_files_identical_to_integrated"] = ab
    for n in ("pakchunk10-ps5.utoc", "pakchunk10-ps5_s1.ucas"):
        x = open(os.path.join(OUT, "plan-base", "targets", "pakchunk10-ps5", n), "rb").read()
        y = open(os.path.join(OUT, "plan-sub", "targets", "pakchunk10-ps5", n), "rb").read()
        check(x == y, "pakchunk10 %s identical" % n)
    out.update({"base_rows_identical": base_rows_same, "sidecar_rows_identical": side_same,
                "derived_rows_verified": side_derived, "imports_resolved_export_level": export_level,
                "merged_store_entries_changed": len(diff)})
    for d in ("plan-base", "plan-sub", "plan-integrated"):
        shutil.rmtree(os.path.join(OUT, d, "targets"))
    out["items_checked"] = items
    out["result"] = "PASS" if ok and items > 0 else "FAIL"
    print(json.dumps(out, indent=1, sort_keys=True))
    return 0 if out["result"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
