"""Team G driver tests: the two GPT driver tests superseded by Team G's changes,
rewritten for the new behavior. (1) make_new_units now registers THREE cap10
units (owner, boundary, offline-net). (2) finalize_variant's fixed P7/P8 gate
passes a 350-identity fixture and rejects a dropped row or a SceNet name.
usage: python3 -I test_team_g_drivers.py
"""
import ast, copy, hashlib, importlib.util, json, tempfile, unittest
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


def extracted(path, names, env):
    tree = ast.parse(Path(path).read_text())
    defs = [n for n in tree.body if isinstance(n, (ast.FunctionDef, ast.AsyncFunctionDef)) and n.name in names]
    assert {n.name for n in defs} == set(names), "missing driver defs"
    exec(compile(ast.Module(body=defs, type_ignores=[]), str(path), "exec"), env)
    return env


class MakeNewUnitsTests(unittest.TestCase):
    def test_three_cap10_units_with_pins(self):
        global CHECKED
        m = load_mod(); c = m.load(RT)
        base = json.loads((MAIN / "outputs/2026-10-08-l25-build-variants/l25-A/pass1/COMPOSITION-RECEIPT.json").read_text())
        env = {"copy": copy, "sha": lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest(), "NEW_UNITS": c["units"]}
        extracted(RT / "build_variants.py", {"make_new_units"}, env)
        made = env["make_new_units"](base["selected_units"], 1, {}, Path("/unused"))
        CHECKED += 1
        self.assertEqual([x["name"] for x in made], list(c["units"]))
        self.assertEqual(len(made), 3)
        self.assertIn("cap10-offline-net", c["units"])
        for u in made:
            spec = c["units"][u["name"]]
            self.assertEqual(u["source_sha256"], spec["source_sha256"])
            argv = m.compile_argv(u["argv"], u["name"], c)
            self.assertIn("-DMODBRIDGE_CAP10_ADMITTED_MONO_PORT=1", argv)
            self.assertNotIn("MODBRIDGE_CAP10_MONO_HOST_VALIDATION", " ".join(argv))


class FinalizerFixedGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.m = load_mod()
        cls.c = cls.m.load(RT)

    def admitted_ids(self):
        """Synthesize a 350-row EXACT-IMPORT-IDENTITIES set = R15e 207 + admitted 143."""
        base = json.loads((RT / "cap10/baseline/R15E-EXACT-IMPORT-IDENTITIES.json").read_text())
        rows = list(base["imports"])
        d = json.loads((RT / "cap10/ADMITTED-IMPORTS.json").read_text())
        for r in d["final_added"]:
            rows.append({"name": r["name"], "published": r["name"],
                         "module": {"name": r["module"]["name"], "version": r["module"]["version"], "value_offset": 0},
                         "library": {"name": r["library"]["name"], "version": r["library"]["version"], "module": 1, "value_offset": 0},
                         "type": r["type"]})
        return {"status": "PASS_MAC_ONLY_EXACT_ELF_IDENTITIES", "count": len(rows), "imports": rows, "native_resolution_proven": False}

    def fixture(self, root, ids):
        rt = RT
        pkg = root / "pkg"; build = root / "build"; pkg.mkdir(); (build / "pass1").mkdir(parents=True)
        def write(p, x): p.parent.mkdir(parents=True, exist_ok=True); p.write_text(json.dumps(x) + "\n")
        parent = MAIN / "outputs/2026-10-08-l24-package/pass-1"
        for passno in (1, 2):
            out = pkg / f"pass-{passno}"; out.mkdir()
            (out / "STATIC-CHECKS.json").write_bytes((parent / "STATIC-CHECKS.json").read_bytes())
            arrays = json.loads((parent / "CONSTRUCTOR-ARRAY-CENSUS.json").read_text())
            arrays["combined"][".init_array"]["slots"] = 240  # R15e fixed count
            arrays["combined"][".fini_array"]["slots"] = 2
            write(out / "CONSTRUCTOR-ARRAY-CENSUS.json", arrays)
            write(out / "EXACT-IMPORT-IDENTITIES.json", ids)
            write(out / "CAP10-CONSTRUCTOR-CONTRACT.json", {"status": "PASS_EXACT_INPUT_ARRAY_SUM", "expected": {n: arrays["combined"][n]["slots"] for n in (".init_array", ".fini_array")}})
            # package contract final_rows must equal the 350 identities for the computed cross-check
            final_rows = json.loads((parent / "supplier-contract.json").read_text())["contract"]["imports"]
            write(out / "CAP10-PACKAGE-CONTRACT.json", {"gaps": [], "final_rows": final_rows + [{"ImportSymbol": {"Name": r["name"]}, "symbol_type": "FUNC" if r["type"] == 2 else "OBJECT"} for r in json.loads((RT / "cap10/ADMITTED-IMPORTS.json").read_text())["final_added"]], "additional_rows": [], "expected_final_count": ids["count"]})
            write(out / "BUILD-RECEIPT.json", {"status": "MAC_PACKAGE_CHECKS_PASS"})
            (out / "modbridge_supplier_composition.prx").write_bytes(b"fixture package bytes")
        write(build / "VARIANT-MARKER.json", {"label": "fixture", "define": "fixture", "delete_listener_registration": "fixture"})
        write(build / "TWIN-RECEIPT.json", {"fixture": True})
        (build / "pass1/runtime.o").write_bytes(b"fixture runtime")
        write(pkg / "CAP10-RUNTIME-ASSETS.json", {"fixture": True})
        env = {"Path": Path, "json": json, "sha": lambda p: self.m.pin(p)["sha256"], "pinrow": self.m.pin,
               "write": write, "cap10": self.m, "CAP10": self.c, "L24PKG": MAIN / "outputs/2026-10-08-l24-package",
               "TOKEN": "fixture", "UNSAFE_NOTE": "fixture", "emit_symbols_sidecar": lambda *a: {"fixture": 1}}
        extracted(rt / "l26_packaging_driver.py", {"finalize_variant"}, env)
        return env, {"pkg": pkg, "build": build, "key": "A", "unsafe": False, "artifact": "runtime.o"}

    def test_350_identity_fixture_passes(self):
        global CHECKED
        with tempfile.TemporaryDirectory() as d:
            env, v = self.fixture(Path(d), self.admitted_ids()); gates = []
            final = env["finalize_variant"](v, lambda *x: gates.append(x), [])
            CHECKED += 1
            self.assertEqual(final["exact_import_count"], 350)
            self.assertTrue((v["pkg"] / "FINAL-RECEIPT-VARIANT-A.json").exists())
            self.assertTrue(all(x[1] for x in gates))

    def test_dropped_row_is_rejected(self):
        global CHECKED
        ids = self.admitted_ids(); ids["imports"].pop(); ids["count"] = len(ids["imports"])
        with tempfile.TemporaryDirectory() as d:
            env, v = self.fixture(Path(d), ids)
            CHECKED += 1
            with self.assertRaises((ValueError, RuntimeError)):
                env["finalize_variant"](v, lambda *x: None, [])
            self.assertFalse((v["pkg"] / "FINAL-RECEIPT-VARIANT-A.json").exists())

    def test_sceNet_name_is_rejected(self):
        global CHECKED
        ids = self.admitted_ids()
        ids["imports"][-1] = {"name": "sceNetPoolCreate", "published": "sceNetPoolCreate",
                              "module": {"name": "libSceNet", "version": 257, "value_offset": 0},
                              "library": {"name": "libSceNet", "version": 1, "module": 1, "value_offset": 0}, "type": 2}
        with tempfile.TemporaryDirectory() as d:
            env, v = self.fixture(Path(d), ids)
            CHECKED += 1
            with self.assertRaises((ValueError, RuntimeError)):
                env["finalize_variant"](v, lambda *x: None, [])
            self.assertFalse((v["pkg"] / "FINAL-RECEIPT-VARIANT-A.json").exists())


if __name__ == "__main__":
    import sys
    r = unittest.TextTestRunner(verbosity=2).run(unittest.defaultTestLoader.loadTestsFromModule(sys.modules[__name__]))
    print(f"test_team_g_drivers items_checked={CHECKED}")
    if CHECKED == 0:
        print("FAIL: zero cases checked"); sys.exit(2)
    sys.exit(0 if r.wasSuccessful() else 1)
