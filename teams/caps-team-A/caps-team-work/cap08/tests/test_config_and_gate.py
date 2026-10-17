#!/usr/bin/env python3
"""config_disclosure.py and intake_gate.py host tests."""
import contextlib
import io
import json
import os
import shutil
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import test_intake_synthetic as ts  # noqa: E402
import config_disclosure as cd  # noqa: E402
import intake_gate as ig  # noqa: E402

N = "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations"
REAL_MOD_INI = N + "/engine-config-real-mod-intake-2026-10-01/Engine.ini"
REAL_GAME_INI = N + "/townfall-original-assets-2026-10-01/Townfall/Platforms/PS5/Config/PS5Engine.ini"


def quiet(fn, *a):
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        rc = fn(*a)
    return rc, buf.getvalue()


class ConfigTests(unittest.TestCase):
    def test_synthetic_categories(self):
        r = cd.disclose("[SystemSettings]\nr.VolumetricFog=0\nr.EyeAdaptationQuality=0\nr.Foo.D3D12Thing=1\nfoo.bar=2\n",
                        {"game": "[SystemSettings]\nr.VolumetricFog=1\n"})
        cats = {s["key"]: s["category"] for s in r["settings"]}
        self.assertEqual(cats["r.VolumetricFog"], "fog")
        self.assertEqual(cats["r.EyeAdaptationQuality"], "exposure")
        self.assertEqual(cats["r.Foo.D3D12Thing"], "pc-api")
        self.assertEqual(cats["foo.bar"], "other")
        self.assertEqual(r["settings"][0]["game_value"], "1")
        self.assertIn("LOOKS", r["headline"])

    def test_section_aware_and_repeated_keys(self):
        """F10: same key in two sections (as in a merged console ini) is two settings."""
        game = "[/Script/Engine.RendererSettings]\nr.Streaming.PoolSize=1024\n"
        merged = ("[/Script/Engine.RendererSettings]\nr.Streaming.PoolSize=1024\n"
                  "[SystemSettings]\nr.Streaming.PoolSize=1536\nr.ShadowQuality=2\nr.ShadowQuality=1\n")
        r = cd.disclose(merged, {"game": game})
        rows = [(x["section"], x["key"], x["mod_value"], x["game_value"]) for x in r["settings"]]
        self.assertIn(("/Script/Engine.RendererSettings", "r.Streaming.PoolSize", "1024", "1024"), rows)
        self.assertIn(("SystemSettings", "r.Streaming.PoolSize", "1536", None), rows)
        sys_pool = [x for x in r["settings"] if x["section"] == "SystemSettings" and x["key"] == "r.Streaming.PoolSize"][0]
        self.assertEqual(sys_pool["game_values_other_sections"],
                         [{"section": "/Script/Engine.RendererSettings", "value": "1024"}])
        sq = [x for x in r["settings"] if x["key"] == "r.ShadowQuality"]
        self.assertEqual([x["overridden_later_in_mod"] for x in sq], [True, False])

    def test_zero_settings_fails(self):
        with tempfile.NamedTemporaryFile("w", suffix=".ini", delete=False) as f:
            f.write("; nothing\n[Section]\n")
        try:
            rc, _ = quiet(cd.main, ["--mod-ini", f.name])
            self.assertEqual(rc, 2)
        finally:
            os.unlink(f.name)

    @unittest.skipUnless(os.path.isfile(REAL_MOD_INI), "real Engine.ini mod copy not present")
    def test_real_engine_ini_mod(self):
        """The Engine.ini mod installed into the game's config pak on 2026-10-02."""
        rc, out = quiet(cd.main, ["--mod-ini", REAL_MOD_INI, "--game-ini", REAL_GAME_INI])
        self.assertEqual(rc, 0)
        self.assertIn("settings checked: 30", out)
        r = cd.disclose(open(REAL_MOD_INI, encoding="utf-8-sig").read(),
                        {"g": open(REAL_GAME_INI, encoding="utf-8-sig").read()})
        self.assertTrue({"fog", "exposure", "shadows"} <= set(r["look_categories"]))
        sq = [s for s in r["settings"] if s["key"] == "r.ShadowQuality"][0]
        self.assertEqual((sq["mod_value"], sq["game_value"]), ("1", "3"))


class GateTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="cap08-gate-")
        self.w = ts.World(self.tmp)
        self.w.base([ts.base_material("/Game/Base/M_Master")])
        self.w.mod([ts.pc_material("/Game/Mod/M_PC")])
        self.w.run()
        self.report = os.path.join(self.tmp, "report.json")

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def gate(self, *extra, base=None):
        self.record = os.path.join(self.tmp, "gate-record.json")
        return quiet(ig.main, ["--report", self.report, "--mod-utoc", self.w.mod_utoc, "--mod-ucas", self.w.mod_ucas,
                               "--base-utoc", base or self.w.base_utoc, "--script-objects-utoc", self.w.so[0],
                               "--record", self.record] + list(extra))

    def test_record_is_written_with_acceptance(self):
        rc, out = self.gate("--accept-visual-loss", "user 2026-10-12")
        self.assertEqual(rc, 0, out)
        rec = json.load(open(self.record))
        self.assertEqual((rec["result"], rec["visual_loss_accepted_by"]), ("PASS", "user 2026-10-12"))
        self.assertEqual(len(rec["base_utoc_sha256"]), 1)
        rc, out = self.gate()
        self.assertEqual(json.load(open(self.record))["result"], "FAIL")

    def test_other_base_files_fail(self):
        """F8: a report made against other game files (e.g. before a game update) fails."""
        other = os.path.join(self.tmp, "other.utoc")
        shutil.copy(self.w.base_utoc, other)
        with open(other, "ab") as f:
            f.write(b"\0")
        rc, out = self.gate("--accept-visual-loss", "user", base=other)
        self.assertEqual(rc, 2)
        self.assertIn("other game base files", out)
        r = json.load(open(self.report))
        del r["inputs"]["script_objects"]["utoc_sha256"]   # a report from the older tool
        json.dump(r, open(self.report, "w"))
        rc, out = self.gate("--accept-visual-loss", "user")
        self.assertEqual(rc, 2)
        self.assertIn("ScriptObjects", out)

    def test_visual_loss_needs_acceptance(self):
        self.assertEqual(json.load(open(self.report))["verdict"], "ADMITTED-WITH-VISUAL-LOSS")
        rc, out = self.gate()
        self.assertEqual(rc, 2)
        rc, out = self.gate("--accept-visual-loss", "user 2026-10-12")
        self.assertEqual(rc, 0, out)

    def test_other_files_fail(self):
        with open(self.w.mod_ucas, "ab") as f:
            f.write(b"\0")
        rc, out = self.gate("--accept-visual-loss", "user")
        self.assertEqual(rc, 2)
        self.assertIn("different .ucas", out)

    def test_zero_items_report_fails(self):
        r = json.load(open(self.report))
        r["counts"]["items_checked_total"] = 0
        json.dump(r, open(self.report, "w"))
        rc, out = self.gate("--accept-visual-loss", "user")
        self.assertEqual(rc, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
