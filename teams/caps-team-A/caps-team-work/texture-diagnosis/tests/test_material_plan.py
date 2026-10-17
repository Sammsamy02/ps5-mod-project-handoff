"""#8 planner: options A/B/C on the real mod, plus the static-permutation check for option B."""
import json
import os
import unittest

from common import (MOD2K_UTOC, MOD2K_UCAS, BASE_UTOCS, BASE_HEADER, GLOBAL_UTOC, GLOBAL_UCAS, OUT, record,
                    require_inputs)
import material_plan


def common_args():
    a = []
    for p in BASE_UTOCS:
        a += ["--base-utoc", p]
    return a + ["--base-container-header", BASE_HEADER, "--global-utoc", GLOBAL_UTOC, "--global-ucas", GLOBAL_UCAS]


class TestMaterialPlan(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = os.path.join(OUT, "material-plan.json")
        os.makedirs(OUT, exist_ok=True)
        cls.rc = material_plan.main(["--mod-utoc", MOD2K_UTOC, "--mod-ucas", MOD2K_UCAS, "--out", cls.out]
                                    + common_args())
        cls.d = json.load(open(cls.out))

    def setUp(self):
        require_inputs(self, [MOD2K_UTOC, MOD2K_UCAS, BASE_HEADER, GLOBAL_UTOC, GLOBAL_UCAS] + BASE_UTOCS)

    def test_plan(self):
        d = self.d
        self.assertEqual(self.rc, 0)
        self.assertEqual(d["broken_materials"], 67)
        self.assertEqual(d["options"], {"A_HIDE_OVERRIDE": 43, "B_REPARENT": 11, "C_MESH_SLOT": 13})
        for r in d["rows"]:
            if r["option"] == "A_HIDE_OVERRIDE":
                self.assertTrue(r["base_pid"])
            else:
                self.assertIn("blocked_until", r)
        self.assertGreater(len(d["base_exports_to_fetch_for_parameter_check"]), 0)
        record("material_plan_rows", len(d["rows"]))

    def test_every_reparent_row_has_static_permutation_check(self):
        n = masters = 0
        for r in self.d["rows"]:
            if not r["option"].startswith("B_"):
                self.assertNotIn("static_permutation_check", r)
                continue
            spc = r["static_permutation_check"]
            # the real mod's instances carry no own permutation (0 own shader tokens, 0 own store hashes)
            self.assertFalse(spc["has_own_static_permutation"], r["package"])
            self.assertTrue(spc["reparent_can_fix_shader_map"], r["package"])
            self.assertEqual(len(spc["base_masters"]), len(r["candidate_base_masters"]))
            for m in spc["base_masters"]:
                self.assertGreaterEqual(m["game_packages_importing"], m["with_own_store_shader_map_hashes"])
                masters += 1
            n += 1
        self.assertEqual(n, 11)
        record("static_permutation_rows_checked", n)
        record("static_permutation_masters_checked", masters)

    def test_own_static_permutation_blocks_reparent(self):
        spc = material_plan.static_permutation_check
        cases = [((0, 0), True), ((4, 0), False), ((0, 2), False), ((4, 2), False)]
        for (pc, hashes), can_fix in cases:
            r = spc(pc, hashes)
            self.assertEqual(r["reparent_can_fix_shader_map"], can_fix, (pc, hashes))
            self.assertEqual(r["has_own_static_permutation"], not can_fix)
        record("static_permutation_cases", len(cases))


if __name__ == "__main__":
    unittest.main()
