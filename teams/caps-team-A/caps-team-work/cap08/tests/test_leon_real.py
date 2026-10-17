#!/usr/bin/env python3
"""Real-file test: the downloaded Leon 2K mod container (unchanged) against the
game's own base TOCs / container header / ScriptObjects captured earlier.
Slow (~90 s: pure-Python BLAKE3 over ~104 MB of package chunks).  Writes its
outputs to ../evidence/leon-intake/ (Mac-only evidence)."""
import contextlib
import io
import json
import os
import shutil
import subprocess
import sys
import unittest

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import fixture_builder  # noqa: E402,F401  (path setup)
import chunk_intake as ci  # noqa: E402
import ue_formats as uf  # noqa: E402

N = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations"
MOD = N + "/leon-real-mod-intake-2026-10-01/extracted/zzz_Leon_Townfall_2K_P"
BASE = N + "/townfall-main-utoc-2026-10-01/"
OUT = os.path.join(HERE, "..", "evidence", "leon-intake")
ARGS = ["--mod-utoc", MOD + ".utoc", "--mod-ucas", MOD + ".ucas"] + \
    sum([["--base-utoc", BASE + "pakchunk%s-ps5.utoc" % c] for c in ("0", "1", "10", "11", "12")], []) + \
    ["--base-container-header", "pakchunk0-ps5=" + N + "/townfall-original-container-header-1/container-header.bin",
     "--script-objects-utoc", N + "/townfall-manifest-capture-2026-10-01/global.utoc",
     "--script-objects-ucas", N + "/townfall-global-scriptobjects-1/global.ucas"]

EXPECTED_MOD_UTOC_SHA = None  # recorded in the report; inputs are re-hashed each run


@unittest.skipUnless(os.path.isfile(MOD + ".utoc"), "local Leon mod copy not present")
class LeonRealTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        os.makedirs(OUT, exist_ok=True)
        before = {p: uf.sha256_file(p) for p in (MOD + ".utoc", MOD + ".ucas")}
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            cls.code = ci.main(ARGS + ["--out", os.path.join(OUT, "leon-intake-report.json"),
                                       "--fetch-plan", os.path.join(OUT, "leon-base-fetch-plan.json"),
                                       "--emit-diag", os.path.join(OUT, "diag")])
        cls.stdout = buf.getvalue()
        with open(os.path.join(OUT, "leon-intake-summary.txt"), "w") as f:
            f.write(cls.stdout)
        cls.after = {p: uf.sha256_file(p) for p in before}
        cls.before = before
        cls.r = json.load(open(os.path.join(OUT, "leon-intake-report.json")))
        cls.plan = json.load(open(os.path.join(OUT, "leon-base-fetch-plan.json")))["chunks"]

    def test_mod_files_unchanged(self):
        self.assertEqual(self.before, self.after)

    def test_identity_and_counts(self):
        c = self.r["counts"]
        print("  counts:", json.dumps(c, sort_keys=True))
        self.assertEqual(c["mod_packages"], 127)           # every name hashes to its chunk id + store id
        self.assertEqual(c["mod_chunks_iohash_verified"], 128)
        self.assertEqual(c["replaced_packages"], 68)
        self.assertEqual(c["added_packages"], 59)
        by = {}
        for row in self.r["replacements"]:
            for cn in row["base_containers"]:
                by[cn] = by.get(cn, 0) + 1
        self.assertEqual(by, {"pakchunk0-ps5": 66, "pakchunk10-ps5": 2})
        self.assertEqual(self.r["refusals"], [])

    def test_target_format_from_game_files(self):
        self.assertEqual(self.r["target_shader_formats"], ["SF_PS5"])

    def test_closure(self):
        cl = self.r["closure"]
        self.assertEqual(cl["broken"], [])
        self.assertEqual(cl["missing_reference_packages"], ["/InterchangeAssets/Functions/MF_PhongToMetalRoughness"])
        self.assertEqual(cl["verdict"], "PACKAGE-LEVEL-OK, EXPORT-LEVEL-PENDING")
        self.assertEqual(self.code, ci.EXIT_INCOMPLETE)

    def test_visual_loss_disclosed(self):
        mats = self.r["visual"]["materials"]
        self.assertEqual(len(mats), 67)
        self.assertTrue(all(m["verdict"] == "FALLBACK-DEFAULT-MATERIAL" for m in mats))
        roots = sorted({m["chain"][-1]["package"].rsplit("/", 1)[-1] for m in mats})
        print("  shader-source roots:", roots)
        self.assertEqual(roots, ["M_Invisible", "M_Leon_Body", "M_Leon_Hair", "M_Leon_Skin"] +
                         ["Simon_Slot%d" % i for i in range(8)])
        for m in mats:
            fm = m["chain"][-1]["inline_shader_formats"]
            self.assertEqual(fm, ["PCD3D_SM5", "PCD3D_SM6"], m["package"])
        self.assertIn("grey/white checker", self.stdout)

    def test_fetch_plan_is_exact(self):
        hdr = sorted((p["container"], p["length"]) for p in self.plan if p["priority"] == 1)
        self.assertEqual(hdr, [("pakchunk1-ps5", 11128), ("pakchunk10-ps5", 416),
                               ("pakchunk11-ps5", 544), ("pakchunk12-ps5", 552)])
        for p in self.plan:
            self.assertEqual(sum(r["length"] for r in p["physical_ranges"]), p["length"])
            self.assertEqual(len(p["expected_iohash"]), 40)
        print("  fetch plan: %d chunks, priority<=2 bytes=%d, all bytes=%d" % (
            len(self.plan), sum(p["length"] for p in self.plan if p["priority"] <= 2), sum(p["length"] for p in self.plan)))

    def test_diag_generated(self):
        d = self.r["diag"]
        self.assertGreater(d["probes"], 0)
        main = os.path.join(OUT, "diag", "zzintake", "Scripts", "main.lua")
        self.assertTrue(shutil.which("luac"), "luac is required (no silent skip)")
        r = subprocess.run(["luac", "-p", main], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        print("  diag:", d["probes"], "probes (%d replaced, %d new, %d linked),"
              % (d["replaced_probes"], d["new_probes"], d["linked_probes"]), d["main_lua_bytes"], "bytes")

    def test_diag_probes_are_linked(self):
        """F2: every new-object probe is hard-imported by a replaced probe; objects the
        reviewer found reachable only from themselves are not probed."""
        e = ci.diag_entries(self.r)
        self.assertGreater(sum(1 for x in e if x["g"] == "R"), 0)
        self.assertGreater(sum(1 for x in e if x["g"] == "N"), 0)
        importers = {j for x in e for j in x["b"]}
        for i, x in enumerate(e, 1):
            if x["g"] == "N":
                self.assertTrue(x["b"], x["o"])
            else:
                self.assertTrue(x["b"] or i in importers, x["o"])
        objs = " ".join(x["o"] for x in e)
        for self_only in ("MI_HeadSynthesized_Baked", "/Simon_Head/Face/Simon_Head_FaceMesh"):
            self.assertNotIn(self_only, objs)
        for x in e:
            print("   ", x["g"], x["k"], x["o"].rsplit(".", 1)[-1], "b=%s" % x["b"])

    def test_diag_runs_pass_under_stub(self):
        import test_diag_lua as tdl
        self.assertTrue(shutil.which("lua"), "lua is required (no silent skip)")
        import tempfile
        tmp = tempfile.mkdtemp(prefix="cap08-leon-lua-")
        try:
            main = os.path.join(OUT, "diag", "zzintake", "Scripts", "main.lua")
            objs = os.path.join(tmp, "objs.txt")
            import re
            with open(objs, "w") as f:
                for line in open(main):
                    if line.startswith(" {o="):
                        p = re.search(r'p="([^"]+)"', line)
                        f.write("%s %s\n" % (re.search(r'o="([^"]+)"', line).group(1), p.group(1) if p else ""))
            h = os.path.join(tmp, "h.lua")
            open(h, "w").write(tdl.HARNESS)
            out = subprocess.run(["lua", h, main, "all", objs], capture_output=True, text=True, timeout=30).stdout
            self.assertIn("state=PASS", [l for l in out.splitlines() if "SUM " in l][-1])
            out = subprocess.run(["lua", h, main, "drop=/Game/Leon/", objs], capture_output=True, text=True, timeout=30).stdout
            self.assertIn("state=MISSING", [l for l in out.splitlines() if "SUM " in l][-1])
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)
