"""Team G host tests for the task-2/3/4 changes (fixed gates, offline net,
admitted-import list). Imports the integration helper from the scratch tree and
exercises cap10.admitted and the fixed-expectation data. No build, no console.
Prints how many cases it checked and fails on zero.
usage: python3 -I test_team_g_gates.py
"""
import copy, hashlib, importlib.util, json, tempfile, unittest
from pathlib import Path

G = Path(__file__).resolve().parents[1]
import os
RT = Path(os.environ.get("CAP10_TEST_RT", str(G / "scratch/after10/outputs/2026-10-08-l26-runtime")))
MAIN = Path("/WORKSPACE/.codex/worktrees/translation-core/what")
CHECKED = 0


def load_mod():
    spec = importlib.util.spec_from_file_location("cap10_integration", RT / "cap10_integration.py")
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


def tick(n=1):
    global CHECKED
    CHECKED += n


class AdmittedImportListTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m = load_mod()
        cls.c = cls.m.load(RT)

    def test_admitted_list_passes_and_counts_are_fixed(self):
        a = self.m.admitted(self.c)
        tick()
        self.assertEqual(a["final_count"], 350)
        self.assertEqual(a["init"], 240)
        self.assertEqual(a["fini"], 2)
        self.assertEqual(len(a["identity_set"]), a["final_count"])
        self.assertTrue(a["raw_added"])
        self.assertFalse(a["raw_added"] & a["raw_removed"])

    def test_sceNet_names_are_not_in_the_admitted_set(self):
        a = self.m.admitted(self.c)
        tick()
        names = {t[0] for t in a["identity_set"]}
        for n in ("sceNetErrnoLoc", "sceNetPoolCreate", "sceNetResolverStartNtoa"):
            self.assertNotIn(n, names)

    def _run_patched(self, mutate, stack):
        # Build a temp cap10/ dir mirror so admitted() resolves the baseline
        # relative to the patched list; run the check inside the stack.
        d = json.loads((RT / "cap10/ADMITTED-IMPORTS.json").read_text())
        mutate(d)
        t = Path(stack.enter_context(tempfile.TemporaryDirectory()))
        cap = t / "cap10"
        (cap / "baseline").mkdir(parents=True)
        (cap / "baseline/R15E-EXACT-IMPORT-IDENTITIES.json").write_bytes((RT / "cap10/baseline/R15E-EXACT-IMPORT-IDENTITIES.json").read_bytes())
        p = cap / "ADMITTED-IMPORTS.json"
        p.write_text(json.dumps(d))
        c = copy.deepcopy(self.c)
        c["admitted_imports"] = {"path": str(p), "sha256": hashlib.sha256(p.read_bytes()).hexdigest(), "bytes": p.stat().st_size}
        return self.m.admitted(c, _check_chain=False)

    def test_refuses_sdk_stub_only_row(self):
        def mut(d):
            d["final_added"][0]["evidence"]["class"] = "sdk-stub-only"
        tick()
        import contextlib
        with self.assertRaisesRegex(ValueError, "without captured provider evidence"), contextlib.ExitStack() as s:
            self._run_patched(mut, s)

    def test_refuses_pending_identity_row(self):
        def mut(d):
            d["final_added"][0]["evidence"]["class"] = "pending-identity-decode"
        tick()
        import contextlib
        with self.assertRaisesRegex(ValueError, "without captured provider evidence"), contextlib.ExitStack() as s:
            self._run_patched(mut, s)

    def test_refuses_count_mismatch(self):
        def mut(d):
            d["expected"]["final_import_count"] += 1
        tick()
        import contextlib
        with self.assertRaisesRegex(ValueError, "count zero/mismatch"), contextlib.ExitStack() as s:
            self._run_patched(mut, s)

    def test_refuses_provider_file_drift(self):
        def mut(d):
            d["final_added"][0]["evidence"]["provider"]["sha256"] = "0" * 64
        tick()
        import contextlib
        with self.assertRaisesRegex(ValueError, "missing/pin drift"), contextlib.ExitStack() as s:
            self._run_patched(mut, s)

    def test_every_added_row_has_two_sources_and_captured_evidence(self):
        d = json.loads((RT / "cap10/ADMITTED-IMPORTS.json").read_text())
        for r in d["final_added"]:
            tick()
            ev = r["evidence"]
            self.assertEqual(ev["class"], "captured-provider-export")
            self.assertIn("source_1", ev)
            self.assertIn("source_2", ev)
            self.assertTrue(Path(ev["provider"]["path"]).is_file())


class IncludeExemptionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m = load_mod()
        cls.c = cls.m.load(RT)

    def test_exact_csignal_row_accepted_and_others_refused(self):
        row = self.c["include_exemptions"][0]
        tick()
        hit, unused = self.m.include_exempt("cap10-mono-owner", [row["path"]], self.c)
        self.assertEqual(hit, [row["path"]])
        self.assertEqual(unused, [])
        tick()
        hit2, _ = self.m.include_exempt("cap10-invocation-boundary", [row["path"]], self.c)
        self.assertEqual(hit2, [], "exemption must be scoped to the one unit")
        tick()
        hit3, _ = self.m.include_exempt("cap10-mono-owner", ["/usr/include/stdio.h"], self.c)
        self.assertEqual(hit3, [], "a different path must not match")


import shutil  # noqa: E402


if __name__ == "__main__":
    import sys
    r = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__]))
    print(f"test_team_g_gates items_checked={CHECKED}")
    if CHECKED == 0:
        print("FAIL: zero cases checked")
        sys.exit(2)
    sys.exit(0 if r.wasSuccessful() else 1)
