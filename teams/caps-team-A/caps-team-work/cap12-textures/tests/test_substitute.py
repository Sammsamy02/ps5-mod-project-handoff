#!/usr/bin/env python3
"""Host tests for capability #12: `paktx install --substitute` + derive_substitutes.py.

ROUND 5b ADAPTATION (Team A cap12-textures): this is Team A's cap12-overlay suite, run against
the round-5 tools. The integrator rule "a shipped instance points at derived PS5-layout textures
only" retires MIC-only manifests, so the fixture's plan row now also derives the instance's 3
textures ('keep' channels, PS5 tile layout; their BulkData chunks are in the mod), the readers
folder is the staged PATCH/tools (it carries the texture tools) and Team B's pinned oracle is
passed. Every other check and expectation is unchanged, except where noted "round 5b".

Mac-only. A synthetic game install (the cap07 fixture writers) whose packages
are REAL bytes: the base parent material package is the fetched, IoHash-
verified M_SkinBasic_Hands chunk; the mod holds the real Leon MI_Leon_Eye,
its 3 texture packages and its old parent M_Leon_Body (read-only extraction
from the unchanged Leon trio), with their real store entries.  The manifest
is made by derive_substitutes.py -> mic_reparent.py (unchanged) and then
installed / restored on a local folder standing in for the console.

Positive: plan, ship, independent re-read of the installed files, byte-exact
restore (also after every interruption point), determinism.  Negative: every
refusal names its code, the console folder is unchanged, nothing is planned.
"""
import copy
import hashlib
import io
import json
import os
import shutil
import struct
import subprocess
import sys
import unittest
from contextlib import redirect_stderr, redirect_stdout

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import test_paktx as T  # noqa: E402  (loads the staged paktx + pinned builder)

paktx, B = T.paktx, T.B
REPO = T.REPO
L25 = os.path.join(REPO, "outputs/2026-10-08-l25-prep")
MF_INTEGRATED = os.path.join(L25, "mod-manager/material-fallback")
MF = os.path.join(T.TEAM, "PATCH", "tools")   # round 5b: staged tools (incl. the texture tools)
TILEREF = "/WORKSPACE/.codex/worktrees/translation-core/caps-team-B/caps-team-work/cap12-layout-invisible/PATCH/tools"
TOOLS = os.path.join(T.TEAM, "PATCH", "tools")
GEN = os.path.join(TOOLS, "derive_substitutes.py")
LEON = os.path.join(REPO, ".superpowers/native-observations/leon-real-mod-intake-2026-10-01/extracted",
                    "zzz_Leon_Townfall_2K_P")
PARENT_HDR = os.path.join(L25, "evidence/cap12-fetched-20261013/headers-r3/09fc94d29f805ce3.hdr.bin")
WORK = os.path.join(HERE, "work", "sub")
sys.path.insert(0, MF)
import blake3 as FB3  # noqa: E402
import cityhash  # noqa: E402
import container_header as FCH  # noqa: E402
import utoc as FUTOC  # noqa: E402
import zen as FZEN  # noqa: E402

LAW = cityhash.ue_name_hash
MI = "/Game/Leon/MI_Leon_Eye"
OLD = "/Game/Leon/M_Leon_Body"
NEW = "/Game/Townfall/Characters/Materials/M_SkinBasic_Hands"
TEX = ["/Game/Leon/Textures/T_Leon_Eye_N", "/Game/Leon/Textures/T_Leon_Eye_D",
       "/Game/Leon/Textures/T_Leon_Eye_ORM"]
DECIDE = ["--accept-decision", "allow-partial", "--accept-decision", "accept-unproven-opaque"]
_LEON = {}


def leon():
    """Real Leon export chunks + store entries (read-only)."""
    if not _LEON:
        t = FUTOC.load(LEON + ".utoc")

        def rd(o, n):
            with open(LEON + ".ucas", "rb") as fh:
                fh.seek(o)
                return fh.read(n)
        _c, st = FCH.parse(t.read_chunk(t.rows_of_type(6)[0], rd))
        for name in [MI, OLD] + TEX:
            pid = LAW(name)
            c = pid.to_bytes(8, "little") + bytes.fromhex("00000001")
            b = pid.to_bytes(8, "little") + bytes.fromhex("00000002")
            _LEON[name] = (pid, t.read_chunk(t.chunk_ids.index(c), rd), list(st[pid]["imports"]),
                           t.read_chunk(t.chunk_ids.index(b), rd) if b in t.chunk_ids else None)
    return _LEON


def parent():
    data = open(PARENT_HDR, "rb").read()
    return data, FZEN.ZenPackage(data)


def rel_of(name, ext=".uasset"):
    return name.replace("/Game/", "Townfall/Content/", 1).lstrip("/") + ext


class SubGame:
    """console/ (live paks), mirror/ (+sizes, headers), mod/, store files, plan."""

    def __init__(self, name, *, mod_override_parent=False, wrong_texture=False, pak10_pkg=True,
                 second_mod_overrides_parent=False, unproven=True, mi_shader_map=False,
                 second_mod=None):
        self.root = os.path.join(WORK, name)
        shutil.rmtree(self.root, ignore_errors=True)
        self.paks = os.path.join(self.root, "console", T.PAKS_REMOTE.lstrip("/"))
        self.mirror = os.path.join(self.root, "mirror")
        self.headers = os.path.join(self.root, "headers")
        self.fetched = os.path.join(self.root, "fetched")
        self.mod = os.path.join(self.root, "mod")
        self.out = os.path.join(self.root, "work")
        for d in (self.paks, self.mirror, self.headers, self.fetched, self.mod):
            os.makedirs(d)
        pdata, ppkg = parent()
        self.to_pid = LAW(NEW)
        pimps = [LAW(n) for n in ppkg.imported_packages]
        # base pakchunk0: the parent + the parent's own imports + fillers
        fill0 = list(range(0x1000, 0x1004))
        pk0 = {self.to_pid: pimps}
        pk0.update({p: [] for p in pimps[:12] + fill0})
        self._base("pakchunk0-ps5", 0xA0A0A0A000000001, pk0, {self.to_pid: pdata}, NEW)
        # base pakchunk1: the rest of the parent's imports (a second base store)
        pk1 = {p: [] for p in pimps[12:]}
        pk1.update({0x4000: []})
        self._base("pakchunk1-ps5", 0xC0C0C0C000000003, pk1, {}, None)
        # base pakchunk10: a small secondary container
        self._base("pakchunk10-ps5", 0xB0B0B0B000000002, {0x2000: [], 0x2001: []}, {}, None)
        # mirror = every non-.ucas file; sizes for .ucas
        self.sizes = {}
        for n in os.listdir(self.paks):
            p = os.path.join(self.paks, n)
            if n.endswith(".ucas"):
                self.sizes[n] = os.path.getsize(p)
            else:
                shutil.copyfile(p, os.path.join(self.mirror, n))
        with open(os.path.join(self.fetched, "%016x.hdr.bin" % self.to_pid), "wb") as f:
            f.write(pdata)
        self.mi_shader_map = mi_shader_map
        self._mod(mod_override_parent, wrong_texture, pak10_pkg)
        if second_mod_overrides_parent:
            self._mod2(self.to_pid, T.prng(0x9004, 7000), rel_of(NEW))
        if second_mod == "mi":
            self._mod2(LAW(MI), leon()[MI][1], rel_of(MI) + ".x")
        if second_mod == "texture":
            self._mod2(LAW(TEX[1]), leon()[TEX[0]][1], rel_of(TEX[1]) + ".x", winner=True)
        self.unproven = unproven
        self._plan()

    def _base(self, stem, cid_value, pkgs, real, real_name):
        c_id = struct.pack("<Q", cid_value)
        hb = B.serialize_container_header(B.ContainerHeader(
            cid_value, {p: B.StoreEntry(tuple(i), ()) for p, i in pkgs.items()}, False, b""))
        chunks = [(c_id + bytes.fromhex("00000006"), hb)]
        files = []
        for p in sorted(pkgs):
            # fillers big enough that the sidecar stays below the base .ucas size
            chunks.append((T.cid(p), real.get(p) or T.prng(p, 60000 + p % 5000)))
            nm = rel_of(real_name) if p in real else "Townfall/Content/%s/P%016x.uasset" % (stem, p)
            files.append((nm, T.cid(p)))
        u, c = T.container(c_id, chunks, files)
        T.write(os.path.join(self.paks, stem + ".utoc"), u)
        T.write(os.path.join(self.paks, stem + ".ucas"), c)
        if stem != "pakchunk1-ps5":  # like the real record: some headers only via the manifest
            T.write(os.path.join(self.headers, stem + ".container-header.bin"), hb)
        T.write(os.path.join(self.root, "stores", stem + ".container-header.bin"), hb)

    def _mod(self, override_parent, wrong_texture, pak10_pkg):
        lv = leon()
        c_id = struct.pack("<Q", 0xE0E0E0E000000005)
        pkgs, chunks, files = {}, [], []
        for name in [MI, OLD] + TEX:
            pid, data, imps, bulk = lv[name]
            if bulk is not None and not (wrong_texture and name == TEX[1]):   # round 5b: textures ship bulk
                chunks.append((T.cid(pid, kind=2), bulk))
                files.append((rel_of(name, ".ubulk"), T.cid(pid, kind=2)))
            if wrong_texture and name == TEX[1]:
                chunks.append((T.cid(pid, kind=2), lv[TEX[0]][3]))
                files.append((rel_of(name, ".ubulk"), T.cid(pid, kind=2)))
            if wrong_texture and name == TEX[1]:
                data = lv[TEX[0]][1]  # a valid package that exports other hashes
            if name == OLD:
                imps = [i for i in imps if i in {lv[t][0] for t in TEX}]
            pkgs[pid] = imps
            chunks.append((T.cid(pid), data))
            files.append((rel_of(name), T.cid(pid)))
        # one replaced base package (so the primary container is found by the rule)
        for rp in (0x1001, 0x1002):  # two replaced pakchunk0 packages: primary by the rule
            pkgs[rp] = []
            chunks.append((T.cid(rp), T.prng(0x9000 + rp, 4000)))
            files.append(("Townfall/Content/pakchunk0-ps5/P%016x.uasset" % rp, T.cid(rp)))
        if pak10_pkg:
            pkgs[0x2001] = []
            chunks.append((T.cid(0x2001), T.prng(0x9002, 5000)))
            files.append(("Townfall/Content/pakchunk10-ps5/P%016x.uasset" % 0x2001, T.cid(0x2001)))
        if override_parent:
            pkgs[self.to_pid] = []
            chunks.append((T.cid(self.to_pid), T.prng(0x9003, 6000)))
            files.append((rel_of(NEW), T.cid(self.to_pid)))
        shm = {LAW(MI): (bytes(range(20)),)} if self.mi_shader_map else {}
        hb = B.serialize_container_header(B.ContainerHeader(
            0xE0E0E0E000000005, {p: B.StoreEntry(tuple(i), shm.get(p, ())) for p, i in pkgs.items()},
            False, b""))
        chunks.insert(0, (c_id + bytes.fromhex("00000006"), hb))
        u, c = T.container(c_id, chunks, files)
        self.mod_utoc = os.path.join(self.mod, "zzz_Leon_Test_P.utoc")
        self.mod_ucas = os.path.join(self.mod, "zzz_Leon_Test_P.ucas")
        T.write(self.mod_utoc, u)
        T.write(self.mod_ucas, c)

    def _mod2(self, pid, data, path, winner=False):
        """A second mod in the same folder providing `pid`.  winner=True picks a
        name whose effective order puts it after the first mod (it wins)."""
        c_id = struct.pack("<Q", 0xF0F0F0F000000006)
        hb = B.serialize_container_header(B.ContainerHeader(
            0xF0F0F0F000000006, {pid: B.StoreEntry((), ())}, False, b""))
        chunks = [(c_id + bytes.fromhex("00000006"), hb), (T.cid(pid), data)]
        u, c = T.container(c_id, chunks, [(path, T.cid(pid))])
        first = "zzz_Leon_Test_P"
        name = "zzz_Other_P"
        if winner:
            k = 0
            while sorted([first, name], key=lambda m: (0, B.stable_mod_hash(m), m))[-1] != name:
                k += 1
                name = "zzz_Other%d_P" % k
        self.mod2_name = name
        T.write(os.path.join(self.mod, name + ".utoc"), u)
        T.write(os.path.join(self.mod, name + ".ucas"), c)

    def _plan(self):
        pid = LAW(MI)
        self.reparent_plan = os.path.join(self.root, "reparent-plan.json")
        row = {"mod_package": MI, "action": "reparent-partial", "old_parent": OLD, "new_parent": NEW,
               "new_export": NEW.rsplit("/", 1)[1], "param_renames": {"BaseColor": "Albedo"},
               "mod_class": {"unproven_opaque": self.unproven},
               # round 5b: every bound texture ships as a derived PS5-layout copy
               "texture_derivations": [
                   {"texture": TEX[1], "op": "keep", "channels": "keep", "layout": "ps5", "format": "PF_DXT1",
                    "parameter": "BaseColor", "slot": "Albedo"},
                   {"texture": TEX[0], "op": "keep", "channels": "keep", "layout": "ps5", "format": "PF_BC5",
                    "parameter": "Normal", "slot": None},
                   {"texture": TEX[2], "op": "keep", "channels": "keep", "layout": "ps5", "format": "PF_DXT1",
                    "parameter": "ORM", "slot": None}],
               "look_notes": ["USER DECISION (--accept-unproven-opaque): test"],
               "store_entry_repoint": {"package_id": "%016x" % pid, "from_package_id": "%016x" % LAW(OLD),
                                       "to_package_id": "%016x" % self.to_pid}}
        with open(self.reparent_plan, "w") as f:
            json.dump({"label": "Mac-only", "rows": [row, {"mod_package": OLD, "action": "known-checker"}]}, f)

    def record(self):
        rec = paktx.make_record(B, self.mirror, "TEST00012", "01.000.000", T.PAKS_REMOTE,
                                self.sizes, self.headers)
        path = os.path.join(self.root, "record.json")
        paktx.write_json(path, rec)
        return path

    def generate(self, decisions=("--allow-partial", "--accept-unproven-opaque"), out="manifest"):
        o = os.path.join(self.root, out)
        cmd = [sys.executable, "-I", GEN, "--plan", self.reparent_plan, "--mod-utoc", self.mod_utoc,
               "--mod-ucas", self.mod_ucas, "--mirror", self.mirror, "--headers", self.fetched,
               "--base-store"] + [os.path.join(self.root, "stores", s + ".container-header.bin")
                                  for s in ("pakchunk0-ps5", "pakchunk1-ps5", "pakchunk10-ps5")] + \
              ["--material-fallback", MF, "--tiling-reference", TILEREF, "--out", o] + list(decisions)
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        return p.returncode, p.stdout + p.stderr, os.path.join(o, "manifest.json")


def run_cli(*argv):
    out, err = io.StringIO(), io.StringIO()
    with redirect_stdout(out), redirect_stderr(err):
        code = paktx.main(["--builder", T.BUILDER, *argv])
    return code, json.loads(out.getvalue()), err.getvalue()


def plan_sub(g, rp, man, *extra, decide=True, plan=True):
    return run_cli("install", "--record", rp, "--mirror", g.mirror, "--mod", g.mod, "--work", g.out,
                   "--substitute", man, "--readers", MF, "--tiling-reference", TILEREF,
                   *(DECIDE if decide else []),
                   *(["--reparent-plan", g.reparent_plan] if plan else []), *extra)


def edit_plan(g, man_path, edit):
    """Change the reparent plan AND re-bind the manifest to it (resealed): the
    byte-level checks must then catch the disagreement."""
    doc = json.load(open(g.reparent_plan))
    edit(doc["rows"][0])
    with open(g.reparent_plan, "w") as f:
        json.dump(doc, f)
    sha = paktx.sha256_file(g.reparent_plan)
    reseal(man_path, lambda m: m["source_plan"].__setitem__("sha256", sha))


def reseal(man_path, edit):
    doc = json.load(open(man_path))
    edit(doc)
    doc["manifest_sha256"] = paktx.manifest_self_sha(doc)
    with open(man_path, "w") as f:
        json.dump(doc, f, indent=1, sort_keys=True)


def rewrite_file(man_path, rel, data, key):
    """Replace a manifest-listed file and fix its sha/bytes in the manifest (resealed)."""
    d = os.path.dirname(man_path)
    with open(os.path.join(d, rel), "wb") as f:
        f.write(data)

    def edit(doc):
        for ent in key(doc):
            if ent["path"] == rel:
                ent["bytes"], ent["sha256"] = len(data), hashlib.sha256(data).hexdigest()
    reseal(man_path, edit)


class Base(unittest.TestCase):
    game_kw = {}

    def setUp(self):
        self.g = SubGame(self.id().rsplit(".", 1)[1][:40], **self.game_kw)
        self.rp = self.g.record()
        code, out, self.man = self.g.generate()
        self.assertEqual(code, 0, out)
        self.console = os.path.join(self.g.root, "console")
        self.before = T.snapshot(self.console)

    def refused(self, code_want, *extra, man=None, decide=True, plan=True):
        code, res, err = plan_sub(self.g, self.rp, man or self.man, "--dry-run", *extra, decide=decide,
                                  plan=plan)
        self.assertEqual(code, 2, (res, err[-800:]))
        self.assertEqual(res["code"], code_want, res)
        self.assertIs(res["console_changed"], False)
        self.assertEqual(T.snapshot(self.console), self.before)
        return res

    def derived_rel(self):
        return json.load(open(self.man))["rows"][0]["derived"]["path"]


class S01Positive(Base):
    def test_plan_ships_derived_bytes(self):
        code, res, err = plan_sub(self.g, self.rp, self.man, "--dry-run")
        self.assertEqual(code, 0, (res, err[-1500:]))
        plan = json.load(open(res["plan"]))
        sub = plan["substitution"]
        self.assertEqual(len(sub["rows"]), 1)
        r = sub["rows"][0]
        self.assertEqual((r["package"], r["new_parent"], r["old_parent"]), (MI, NEW, OLD))
        self.assertEqual(r["new_public_hash"], "%016x" % LAW("M_SkinBasic_Hands"))
        self.assertEqual(r["imports_resolved_export_level"], 4)
        self.assertEqual(r["renamed_parameters"], [["BaseColor", "Albedo"]])
        self.assertEqual(sub["user_decisions"], ["accept-unproven-opaque", "allow-partial"])
        self.assertEqual([x["container"] for x in sub["post_build"]], ["pakchunk0-ps5"])
        # round 5b: the instance (1 chunk) + its 3 derived PS5 textures (export + bulk each)
        self.assertEqual(sub["post_build"][0]["substituted_chunks_verified"], 7)
        t0 = [t for t in plan["targets"] if t["container"] == "pakchunk0-ps5"][0]
        self.assertEqual(t0["substituted_packages"], sorted("%016x" % LAW(x) for x in [MI] + TEX))
        t10 = [t for t in plan["targets"] if t["container"] == "pakchunk10-ps5"][0]
        self.assertEqual(t10["substituted_packages"], [])
        self.assertIn("SUBSTITUTE (capability #12)", err)
        # mod files untouched
        for f in plan["mod"]["files"]:
            self.assertEqual(paktx.sha256_file(os.path.join(self.g.mod, f["name"])), f["sha256"])

    def test_install_reread_restore_byte_exact(self):
        mod_before = T.snapshot(self.g.mod)
        code, res, err = plan_sub(self.g, self.rp, self.man, "--dry-run")
        self.assertEqual(code, 0, res)
        sha = res["plan_sha256"]
        code, res, err = plan_sub(self.g, self.rp, self.man, "--local", self.g.paks, "--confirm", sha[:12],
                                  "--game-closed")
        self.assertEqual(code, 0, (res, err[-1500:]))
        code, v, _ = run_cli("verify", "--plan", res["plan"], "--local", self.g.paks)
        self.assertEqual(v["state"]["verdict"], "INSTALLED")
        # independent re-read of the INSTALLED files with the second reader set
        toc = FUTOC.load(os.path.join(self.g.paks, "pakchunk0-ps5.utoc"))
        side = open(os.path.join(self.g.paks, "pakchunk0-ps5_s1.ucas"), "rb").read()
        base = toc.partition_size

        def rd(o, n):
            return side[o - base:o - base + n]
        der = open(os.path.join(os.path.dirname(self.man), self.derived_rel()), "rb").read()
        c = LAW(MI).to_bytes(8, "little") + bytes.fromhex("00000001")
        row = toc.chunk_ids.index(c)
        got = toc.read_chunk(row, rd)
        self.assertEqual(got, der)
        self.assertEqual(toc.metas[row][0], FB3.iohash20(der))
        hrow = toc.chunk_ids.index(toc.container_id.to_bytes(8, "little") + bytes.fromhex("00000006"))
        _c, st = FCH.parse(toc.read_chunk(hrow, rd))
        imps = list(st[LAW(MI)]["imports"])
        self.assertIn(LAW(NEW), imps)
        self.assertNotIn(LAW(OLD), imps)
        q = FZEN.ZenPackage(got)
        self.assertIn(NEW, q.imported_packages)
        # restore: original back by rename, byte-exact, sidecar removed with cleanup
        code, r2, err2 = run_cli("restore", "--plan", res["plan"], "--local", self.g.paks, "--confirm",
                                 sha[:12], "--game-closed", "--cleanup", "--allow-delete")
        self.assertEqual(code, 0, (r2, err2))
        self.assertEqual(T.snapshot(self.console), self.before)
        self.assertEqual(T.snapshot(self.g.mod), mod_before)

    def test_cycle_command_byte_exact(self):
        code, r, err = run_cli("cycle", "--record", self.rp, "--mirror", self.g.mirror, "--mod", self.g.mod,
                               "--work", self.g.out, "--local", self.g.paks, "--substitute", self.man,
                               "--readers", MF, "--tiling-reference", TILEREF, "--reparent-plan",
                               self.g.reparent_plan, *DECIDE)
        self.assertEqual(code, 0, (r, err[-1500:]))
        self.assertEqual(r["after_install"]["verdict"], "INSTALLED")
        self.assertEqual(r["after_restore"]["verdict"], "PRISTINE")
        self.assertEqual(T.snapshot(self.console), self.before)

    def test_every_interruption_point_restores_exactly(self):
        code, res, _ = plan_sub(self.g, self.rp, self.man, "--dry-run")
        self.assertEqual(code, 0, res)
        plan = json.load(open(res["plan"]))
        pristine = os.path.join(self.g.root, "pristine")
        shutil.copytree(self.console, pristine)
        mutating = sum(1 for op in plan["operations"] if op["op"] in ("upload", "rename"))
        self.assertEqual(mutating, 2 * 2 + 3 * 2)
        for k in range(mutating):
            shutil.rmtree(self.console)
            shutil.copytree(pristine, self.console)
            jp = os.path.join(self.g.root, "j%d.json" % k)
            with self.assertRaises(paktx.Stopped):
                paktx.execute(plan, plan["operations"], paktx.LocalBackend(self.g.paks, fail_after=k),
                              paktx.Journal(jp, plan, "install"), lambda s: None)
            be = paktx.LocalBackend(self.g.paks)
            ops, _ = paktx.restore_operations(plan, be, cleanup=True)
            paktx.execute(plan, ops, be, paktx.Journal(jp, plan, "restore"), lambda s: None)
            after = T.snapshot(self.console)
            for n, d in self.before.items():
                self.assertEqual(after.get(n), d, "k=%d %s" % (k, n))
            for n in set(after) - set(self.before):
                self.assertTrue(".paktx-" in n and n.endswith(".tmp"), "k=%d %s" % (k, n))

    def test_plan_deterministic_and_bound_to_manifest(self):
        c1, r1, _ = plan_sub(self.g, self.rp, self.man, "--dry-run")
        c2, r2, _ = plan_sub(self.g, self.rp, self.man, "--dry-run")
        self.assertEqual((c1, c2), (0, 0))
        self.assertEqual(r1["plan_sha256"], r2["plan_sha256"])
        c3, r3, _ = T.run_cli("install", "--record", self.rp, "--mirror", self.g.mirror, "--mod", self.g.mod,
                              "--work", self.g.out, "--dry-run")
        self.assertEqual(c3, 0, r3)
        self.assertNotEqual(r3["plan_sha256"], r1["plan_sha256"])
        p1 = json.load(open(r1["plan"])) if os.path.isfile(r1["plan"]) else None
        self.assertIsNotNone(p1)
        # a regenerated manifest (same inputs, other folder) has the same sha
        code, out, man2 = self.g.generate(out="manifest-again")
        self.assertEqual(code, 0, out)
        self.assertEqual(json.load(open(man2))["manifest_sha256"], json.load(open(self.man))["manifest_sha256"])

    def test_generator_needs_both_user_decisions(self):
        mod_before = T.snapshot(self.g.mod)
        code, out, _ = self.g.generate(decisions=(), out="m-none")
        self.assertEqual(code, 1, out)
        self.assertIn("zero rows derived", out)
        code, out, _ = self.g.generate(decisions=("--allow-partial",), out="m-one")
        self.assertEqual(code, 1, out)
        code, out, _ = self.g.generate(decisions=("--accept-unproven-opaque",), out="m-two")
        self.assertEqual(code, 1, out)
        self.assertEqual(T.snapshot(self.g.mod), mod_before)

    def test_generator_refuses_nonempty_out_and_bad_header(self):
        code, out, _ = self.g.generate(out="manifest")  # already used
        self.assertEqual(code, 1, out)
        self.assertIn("new, empty folder", out)
        hp = os.path.join(self.g.fetched, "%016x.hdr.bin" % self.g.to_pid)
        data = open(hp, "rb").read()
        with open(hp, "wb") as f:
            f.write(data[:-1] + bytes([data[-1] ^ 1]))
        code, out, _ = self.g.generate(out="m-badhdr")
        self.assertEqual(code, 1, out)
        self.assertIn("IoHash", out)
        with open(hp, "wb") as f:
            f.write(data[:4096])
        code, out, _ = self.g.generate(out="m-prefix")
        self.assertEqual(code, 1, out)
        self.assertIn("not the whole base chunk", out)


class S02Refusals(Base):
    def test_user_decisions_missing_or_partial_or_unknown(self):
        self.refused("substitute_user_decision_missing", decide=False)
        self.refused("substitute_user_decision_missing", "--accept-decision", "accept-unproven-opaque",
                     decide=False)
        self.refused("substitute_user_decision_missing", "--accept-decision", "allow-partial", decide=False)
        self.refused("substitute_user_decision_unknown", "--accept-decision", "yolo", *DECIDE, decide=False)
        code, res, _ = T.run_cli("install", "--record", self.rp, "--mirror", self.g.mirror, "--mod", self.g.mod,
                                 "--work", self.g.out, "--dry-run", "--accept-decision", "allow-partial")
        self.assertEqual((code, res["code"]), (2, "accept_decision_needs_substitute"))

    def test_install_without_decision_changes_nothing(self):
        code, res, _ = plan_sub(self.g, self.rp, self.man, "--local", self.g.paks, "--confirm", "0" * 12,
                                "--game-closed", decide=False)
        self.assertEqual((code, res["code"]), (2, "substitute_user_decision_missing"))
        self.assertEqual(T.snapshot(self.console), self.before)

    def test_manifest_tampered_without_reseal(self):
        doc = json.load(open(self.man))
        doc["rows"][0]["reparent"]["new_export"] = "M_Other"
        json.dump(doc, open(self.man, "w"))
        self.refused("substitute_manifest_self_hash_mismatch")

    def test_derived_bytes_changed(self):
        p = os.path.join(os.path.dirname(self.man), self.derived_rel())
        d = bytearray(open(p, "rb").read())
        d[-1] ^= 1
        open(p, "wb").write(bytes(d))
        self.refused("substitute_derived_sha_mismatch")

    def _derived_edit(self, fn, code):
        p = os.path.join(os.path.dirname(self.man), self.derived_rel())
        d = bytearray(open(p, "rb").read())
        fn(d)
        rewrite_file(self.man, self.derived_rel(), bytes(d), lambda doc: [r["derived"] for r in doc["rows"]])

        def fix(doc):
            doc["rows"][0]["derived"]["iohash"] = FB3.iohash20(bytes(d))
        reseal(self.man, fix)
        self.refused(code)

    def test_derived_wrong_public_hash(self):
        def fn(d):
            q = FZEN.ZenPackage(bytes(d))
            iph = struct.unpack_from("<i", d, 24)[0]
            slot = [v[1] for k, v in q.imports if k == "PackageImport" and
                    q.imported_packages[v[0]] == NEW][0]
            struct.pack_into("<Q", d, iph + 8 * slot, LAW("M_Something_Else"))
        self._derived_edit(fn, "substitute_public_hash_law_mismatch")

    def test_derived_export_bytes_changed(self):
        def fn(d):
            q = FZEN.ZenPackage(bytes(d))
            o = q.exports[0]["data_offset"] + 2
            d[o] ^= 0xFF
        self._derived_edit(fn, "substitute_export_bytes_changed")

    def test_derived_tail_after_exports_changed(self):
        self._derived_edit(lambda d: d.__setitem__(len(d) - 3, d[len(d) - 3] ^ 0xFF),
                           "substitute_payload_after_header_changed")

    def test_derived_texture_hash_changed(self):
        def fn(d):
            q = FZEN.ZenPackage(bytes(d))
            iph = struct.unpack_from("<i", d, 24)[0]
            slot = [v[1] for k, v in q.imports if k == "PackageImport" and
                    q.imported_packages[v[0]] == TEX[0]][0]
            struct.pack_into("<Q", d, iph + 8 * slot, LAW("T_Wrong"))
        self._derived_edit(fn, "substitute_unexpected_public_hash_change")

    def test_original_not_this_mod(self):
        reseal(self.man, lambda doc: doc["rows"][0]["original"].__setitem__("sha256", "0" * 64))
        self.refused("substitute_original_mismatch")

    def test_manifest_for_another_mod_build(self):
        reseal(self.man, lambda doc: doc["mod"].__setitem__("ucas_sha256", "1" * 64))
        self.refused("substitute_mod_identity_mismatch")

    def test_old_parent_claim_wrong(self):
        reseal(self.man, lambda doc: doc["rows"][0]["reparent"].__setitem__("old_parent", "/Game/Leon/M_Leon_Skin"))
        self.refused("substitute_row_disagrees_with_plan")
        # plan and manifest agree, the bytes do not
        edit_plan(self.g, self.man, lambda r: r.__setitem__("old_parent", "/Game/Leon/M_Leon_Skin"))
        self.refused("substitute_unexpected_import_package_change")

    def test_undeclared_name_change(self):
        reseal(self.man, lambda doc: doc["rows"][0]["reparent"].__setitem__("renamed_parameters", []))
        self.refused("substitute_row_disagrees_with_plan")
        # round 5b: the plan's texture derivation names the renamed slot too; drop it everywhere so the
        # byte-level check is the one that catches the undeclared rename in the derived bytes
        def plan_edit(r):
            r["param_renames"] = {}
            for d in r["texture_derivations"]:
                if d["parameter"] == "BaseColor":
                    d["slot"] = None
        edit_plan(self.g, self.man, plan_edit)

        def man_edit(m):
            for t in m["textures"]:
                for c in t["consumers"]:
                    if c["parameter"] == "BaseColor":
                        c["slot"] = None
        reseal(self.man, man_edit)
        self.refused("substitute_name_change_not_declared")

    def test_decisions_stripped_from_manifest(self):
        def strip(doc):
            doc["user_decisions"] = []
            for r in doc["rows"]:
                r["user_decisions"] = []
        reseal(self.man, strip)
        self.refused("substitute_row_decisions_disagree_with_plan", decide=False)

    def test_plan_binding(self):
        self.refused("substitute_needs_reparent_plan", plan=False)
        with open(self.g.reparent_plan, "a") as f:
            f.write(" ")
        self.refused("substitute_plan_sha_mismatch")

    def test_row_action_differs_from_plan(self):
        reseal(self.man, lambda doc: doc["rows"][0].__setitem__("action", "reparent"))
        self.refused("substitute_row_disagrees_with_plan")

    def test_old_public_hash_claim_wrong(self):
        reseal(self.man, lambda doc: doc["rows"][0]["reparent"].__setitem__("old_public_hash", "0" * 16))
        self.refused("substitute_old_public_hash_disagrees")

    def test_raw_name_hash_tampered(self):
        def fn(d):
            n = struct.unpack_from("<I", d, 60)[0]
            pos = 60 + 4 + 12 + 8 * (n - 1)   # stored hash of the last name
            d[pos] ^= 1
        self._derived_edit(fn, "substitute_raw_name_hash_law")

    def test_raw_unread_region_tampered(self):
        def fn(d):
            offs = struct.unpack_from("<9i", d, 24)
            iph, imp, exp, bundle, dep_h, dep_e, ipn, cimp, cexp = offs
            for lo, hi in ((dep_h, dep_e), (dep_e, ipn), (cimp, cexp), (cexp, bundle)):
                if hi > lo:
                    d[lo] ^= 0x40
                    return
            raise AssertionError("no unread region in this package")
        self._derived_edit(fn, "substitute_raw_maps_changed")

    def test_store_imports_claim_wrong(self):
        def edit(doc):
            doc["rows"][0]["store_entry"]["imports_after"][0] = "%016x" % 0x1002
        reseal(self.man, edit)
        self.refused("substitute_store_imports_disagree")

    def test_duplicate_row(self):
        reseal(self.man, lambda doc: doc["rows"].append(copy.deepcopy(doc["rows"][0])))
        self.refused("substitute_duplicate_package")

    def test_base_header_tampered(self):
        rel = json.load(open(self.man))["base_headers"][0]["path"]
        data = bytearray(open(os.path.join(os.path.dirname(self.man), rel), "rb").read())
        data[100] ^= 1
        rewrite_file(self.man, rel, bytes(data), lambda doc: doc["base_headers"])
        self.refused("substitute_base_header_iohash_mismatch")

    def test_base_header_prefix_only(self):
        rel = json.load(open(self.man))["base_headers"][0]["path"]
        data = open(os.path.join(os.path.dirname(self.man), rel), "rb").read()[:8192]
        rewrite_file(self.man, rel, data, lambda doc: doc["base_headers"])
        self.refused("substitute_base_header_not_whole_chunk")

    def test_base_header_dropped(self):
        reseal(self.man, lambda doc: doc.__setitem__("base_headers", []))
        self.refused("substitute_base_header_missing")

    def test_base_store_tampered(self):
        ent = [s for s in json.load(open(self.man))["base_stores"] if s["container"] == "pakchunk1-ps5"][0]
        data = bytearray(open(os.path.join(os.path.dirname(self.man), ent["path"]), "rb").read())
        data[-8] ^= 1
        rewrite_file(self.man, ent["path"], bytes(data), lambda doc: doc["base_stores"])
        self.refused("substitute_base_store_iohash_mismatch")

    def test_base_store_disagrees_with_record(self):
        ent = [s for s in json.load(open(self.man))["base_stores"] if s["container"] == "pakchunk0-ps5"][0]
        other = open(os.path.join(os.path.dirname(self.man), "base-stores",
                                  "pakchunk1-ps5.container-header.bin"), "rb").read()
        rewrite_file(self.man, ent["path"], other, lambda doc: doc["base_stores"])
        self.refused("substitute_base_store_disagrees_with_record")

    def test_parent_imports_store_missing(self):
        # without pakchunk1's store the parent's own imports are not all known
        reseal(self.man, lambda doc: doc.__setitem__(
            "base_stores", [s for s in doc["base_stores"] if s["container"] != "pakchunk1-ps5"]))
        self.refused("substitute_parent_imports_unresolved")

    def test_reader_pin(self):
        rd = os.path.join(self.g.root, "readers")
        shutil.copytree(MF, rd)
        with open(os.path.join(rd, "zen.py"), "a") as f:
            f.write("\n# changed\n")
        self.refused("substitute_reader_sha_mismatch", "--readers", rd)

    def test_only_into_drops_substituted_package(self):
        self.refused("substitute_package_not_installed", "--only-into", "pakchunk10-ps5")

    def test_post_build_check_catches_unshipped_bytes(self):
        real = paktx.project_mod

        def no_sub(B_, mod, pids, out_stem, target_dir=None, substitute=None, store_imports=None):
            return real(B_, mod, pids, out_stem, target_dir=target_dir)
        paktx.project_mod = no_sub
        try:
            self.refused("substitute_derived_payload_mismatch")
        finally:
            paktx.project_mod = real

    def test_post_build_check_catches_store_not_repointed(self):
        real = paktx.project_mod

        def no_store(B_, mod, pids, out_stem, target_dir=None, substitute=None, store_imports=None):
            return real(B_, mod, pids, out_stem, target_dir=target_dir, substitute=substitute)
        paktx.project_mod = no_store
        try:
            self.refused("substitute_merged_store_entry_mismatch")
        finally:
            paktx.project_mod = real


class S03MoreRefusals(unittest.TestCase):
    def test_extra_decision_refused_and_one_decision_plan(self):
        g = SubGame("s03-onedec", unproven=False)
        rp = g.record()
        code, out, man = g.generate(decisions=("--allow-partial",))
        self.assertEqual(code, 0, out)
        code, res, _ = plan_sub(g, rp, man, "--dry-run")  # restates both: one is not needed
        self.assertEqual((code, res.get("code")), (2, "substitute_user_decision_not_needed"), res)
        code, res, err = plan_sub(g, rp, man, "--dry-run", "--accept-decision", "allow-partial", decide=False)
        self.assertEqual(code, 0, (res, err[-800:]))
        self.assertEqual(json.load(open(res["plan"]))["substitution"]["user_decisions"], ["allow-partial"])

    def test_rows_not_derived_are_listed(self):
        g = SubGame("s03-listed")
        rp = g.record()
        code, out, man = g.generate()
        self.assertEqual(code, 0, out)
        d = json.load(open(g.reparent_plan))
        extra = dict(d["rows"][0], mod_package="/Game/Leon/MI_Not_Derived")
        d["rows"].append(extra)
        json.dump(d, open(g.reparent_plan, "w"))
        sha = paktx.sha256_file(g.reparent_plan)
        reseal(man, lambda m: m["source_plan"].__setitem__("sha256", sha))
        code, res, err = plan_sub(g, rp, man, "--dry-run")
        self.assertEqual(code, 0, (res, err[-800:]))
        ns = json.load(open(res["plan"]))["substitution"]["plan_rows_not_shipped"]
        self.assertEqual([x["package"] for x in ns], ["/Game/Leon/MI_Not_Derived"])
        self.assertIn("NOT SUBSTITUTED", err)
        # the generator refuses a plan row whose package the mod does not have
        code, out, _ = g.generate(out="m-extra")
        self.assertEqual(code, 1, out)
        self.assertIn("MI_Not_Derived", out)

    def test_own_shader_maps_refused(self):
        g = SubGame("s03-shm", mi_shader_map=True)
        rp = g.record()
        code, out, _ = g.generate()
        self.assertEqual(code, 1, out)
        self.assertIn("own shader maps", out)
        # a generator without that check (scratch copy) -> paktx still refuses
        src = open(GEN).read()
        old = '        need(not mstore[pid]["shader_maps"], "%s has its own shader maps; reparenting cannot fix it" % name)\n'
        self.assertEqual(src.count(old), 1)
        gen2 = os.path.join(g.root, "derive_nocheck.py")
        open(gen2, "w").write(src.replace(old, ""))
        o = os.path.join(g.root, "m-nocheck")
        cmd = [sys.executable, "-I", gen2, "--plan", g.reparent_plan, "--mod-utoc", g.mod_utoc,
               "--mod-ucas", g.mod_ucas, "--mirror", g.mirror, "--headers", g.fetched, "--base-store"] + \
              [os.path.join(g.root, "stores", s_ + ".container-header.bin")
               for s_ in ("pakchunk0-ps5", "pakchunk1-ps5", "pakchunk10-ps5")] + \
              ["--material-fallback", MF, "--tiling-reference", TILEREF, "--out", o, "--allow-partial",
               "--accept-unproven-opaque"]
        p = subprocess.run(cmd, capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stdout + p.stderr)
        code, res, _ = plan_sub(g, rp, os.path.join(o, "manifest.json"), "--dry-run")
        self.assertEqual((code, res.get("code")), (2, "substitute_entry_has_own_shader_maps"), res)

    def test_substituted_package_in_two_mods(self):
        g = SubGame("s03-mi2", second_mod="mi")
        rp = g.record()
        code, out, man = g.generate()
        self.assertEqual(code, 0, out)
        code, res, _ = plan_sub(g, rp, man, "--dry-run")
        self.assertEqual((code, res.get("code")), (2, "substitute_package_in_several_mods"), res)

    def test_import_target_won_by_another_mod(self):
        g = SubGame("s03-tex2", second_mod="texture")
        rp = g.record()
        code, out, man = g.generate()
        self.assertEqual(code, 0, out)
        code, res, _ = plan_sub(g, rp, man, "--dry-run")
        self.assertEqual((code, res.get("code")), (2, "substitute_import_unresolvable"), res)
        self.assertIn("T_Leon_Eye_D", res["detail"])

    def test_parent_overridden_by_another_mod(self):
        g = SubGame("s03-mod2", second_mod_overrides_parent=True)
        rp = g.record()
        code, out, man = g.generate()
        self.assertEqual(code, 0, out)  # the generator sees one mod; paktx sees the whole folder
        code, res, _ = plan_sub(g, rp, man, "--dry-run")
        self.assertEqual((code, res.get("code")), (2, "substitute_parent_overridden_by_mod"), res)

    def test_generator_refuses_parent_overridden_by_same_mod(self):
        g = SubGame("s03-ovr", mod_override_parent=True)
        code, out, _ = g.generate()
        self.assertEqual(code, 1, out)
        self.assertIn("overridden by the mod", out)

    def test_import_unresolvable_at_export_level(self):
        g = SubGame("s03-tex", wrong_texture=True)
        code, out, man = g.generate()
        # round 5b: the wrong texture is now itself a derived texture row, so the generator already
        # refuses it (its chunk holds another texture of another format); nothing is written
        self.assertEqual(code, 1, out)
        self.assertIn("T_Leon_Eye_D: format PF_BC5, plan says PF_DXT1", out)

    def test_parent_not_in_base_store(self):
        g = SubGame("s03-nostore")
        rp = g.record()
        code, out, man = g.generate()
        self.assertEqual(code, 0, out)
        real = paktx.base_stores

        def drop(B_, R, rec, mirror, m):
            st = real(B_, R, rec, mirror, m)
            st["pakchunk0-ps5"]["packages"] = {k: v for k, v in st["pakchunk0-ps5"]["packages"].items()
                                               if k != g.to_pid}
            return st
        paktx.base_stores = drop
        try:
            code, res, _ = plan_sub(g, rp, man, "--dry-run")
        finally:
            paktx.base_stores = real
        self.assertEqual((code, res.get("code")), (2, "substitute_parent_not_in_base_store"), res)

    def test_zero_row_manifest_refused(self):
        g = SubGame("s03-zero")
        rp = g.record()
        code, out, man = g.generate()
        self.assertEqual(code, 0, out)
        reseal(man, lambda doc: doc.__setitem__("rows", []))
        code, res, _ = plan_sub(g, rp, man, "--dry-run")
        self.assertEqual((code, res.get("code")), (2, "substitute_manifest_has_zero_rows"), res)


class S04Units(unittest.TestCase):
    def test_project_mod_multiblock_substitute(self):
        g = T.Game("s04-proj")
        inv = paktx.inventory_mod(B, g.mod)
        mod = inv["iostore"][0]
        hdr = B.parse_container_header(B.read_mod_header_bytes(mod))
        pid = 0x3000
        c = T.cid(pid)
        blob = T.prng(77, 3 * T.BS + 1234)          # 4 stored blocks
        out = os.path.join(g.root, "proj")
        spec = paktx.project_mod(B, mod, set(hdr.packages), out, substitute={c: blob},
                                 store_imports={pid: (0x1001, 0x2001)})
        row = spec.toc.ids.index(c)
        with open(spec.ucas_path, "rb") as fh:
            data = fh.read()
        self.assertEqual(B.extract_chunk_bytes(spec.toc, row, data), blob)
        self.assertEqual(spec.toc.meta(row)[:20], B.blake3_20(blob))
        self.assertEqual(spec.toc.offset_length(row)[1], len(blob))
        first = spec.toc.offset_length(row)[0] // T.BS
        self.assertEqual([spec.toc.block(i)[1] for i in range(first, first + 4)], [T.BS] * 3 + [1234])
        h2 = B.parse_container_header(B.read_mod_header_bytes(spec))
        self.assertEqual(h2.packages[pid].imported_packages, (0x1001, 0x2001))
        for cid_, pl in g.mod_chunks.items():
            if cid_ in spec.toc.ids and cid_ != c and cid_ != g.mod_header_id:
                self.assertEqual(B.extract_chunk_bytes(spec.toc, spec.toc.ids.index(cid_), data), pl)
        with self.assertRaises(paktx.Refused) as cm:
            paktx.project_mod(B, mod, {0x1001}, out + "x", substitute={c: blob})
        self.assertEqual(cm.exception.code, "substitute_chunk_not_projected")

    def test_default_readers_path(self):
        old = os.environ.pop("PAKTX_READERS", None)
        try:
            want = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(paktx.__file__))),
                                "mod-manager", "material-fallback")
            self.assertEqual(paktx.default_readers_path(), want)
            os.environ["PAKTX_READERS"] = MF_INTEGRATED
            self.assertEqual(paktx.default_readers_path(), MF_INTEGRATED)
            self.assertEqual(sorted(vars(paktx.load_readers(MF_INTEGRATED))),
                             ["blake3", "cityhash", "container_header", "zen"])
        finally:
            os.environ.pop("PAKTX_READERS", None)
            if old is not None:
                os.environ["PAKTX_READERS"] = old

    def test_plain_install_bytes_identical_to_integrated_paktx(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location("paktx_integrated",
                                                      os.path.join(L25, "pak-overlay", "paktx.py"))
        orig = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(orig)
        self.assertEqual(paktx.sha256_file(os.path.join(L25, "pak-overlay", "paktx.py")),
                         "a7446c1b750718d1f75f5d92877e8e07724c303d5b3b5025f081b5ed042012dd")
        checked = 0
        for name, game in (("s04-ab-cap07", lambda: T.Game("s04-ab-cap07")),
                           ("s04-ab-sub", lambda: SubGame("s04-ab-sub"))):
            g = game()
            rp = g.record()[0] if isinstance(g, T.Game) else g.record()
            res = []
            for mod_, w in ((orig, "w-orig"), (paktx, "w-staged")):
                rec = mod_.load_record(rp)
                plan = mod_.make_install_plan(B, rec, g.mirror, g.mod, os.path.join(g.root, w),
                                              None, T.REPAK, "x")
                res.append({t["container"]: (t["derived"]["sha256"], (t["sidecar"] or {}).get("sha256"))
                            for t in plan["targets"]})
            self.assertEqual(res[0], res[1], name)
            checked += len(res[0])
        self.assertGreater(checked, 0)


if __name__ == "__main__":
    os.makedirs(WORK, exist_ok=True)
    for name, p in (("BUILDER", T.BUILDER), ("REPAK", T.REPAK), ("LEON", LEON + ".ucas"),
                    ("PARENT_HDR", PARENT_HDR), ("MF", os.path.join(MF, "mic_reparent.py"))):
        if not os.path.isfile(p):
            print("MISSING required input %s=%s" % (name, p))
            sys.exit(1)
    suite = unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__])
    print("cap12 substitute host tests: %d test cases" % suite.countTestCases())
    res = unittest.TextTestRunner(verbosity=2).run(suite)
    ran, bad, skipped = res.testsRun, len(res.failures) + len(res.errors), len(res.skipped)
    print("SUMMARY tests_run=%d failed=%d skipped=%d passed=%d" % (ran, bad, skipped, ran - bad - skipped))
    if ran == 0 or bad or skipped or ran != suite.countTestCases():
        print("GATE FAIL (zero tests, failures, skips, or count mismatch)")
        sys.exit(1)
    print("GATE PASS")
