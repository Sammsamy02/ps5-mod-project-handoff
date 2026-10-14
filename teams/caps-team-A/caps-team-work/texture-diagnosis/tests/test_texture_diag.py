"""End-to-end diagnosis on the real downloaded mod (2K + 4K) vs the game's own files."""
import contextlib
import io as _stdio
import json
import os
import shutil
import subprocess
import sys
import unittest

from common import (MOD2K_UTOC, MOD2K_UCAS, MOD4K_UTOC, MOD4K_UCAS, BASE_UTOCS, BASE_HEADER, GLOBAL_UTOC,
                    GLOBAL_UCAS, BASE_PAK_LISTING, OUT, RETOC, TOOLS, OVERLAY_PATCHED, fresh_tmp, record,
                    require_inputs)
import texture_diag


def common_args():
    a = []
    for p in BASE_UTOCS:
        a += ["--base-utoc", p]
    return a + ["--base-container-header", BASE_HEADER, "--global-utoc", GLOBAL_UTOC, "--global-ucas", GLOBAL_UCAS]


class TestDiagnosis(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.out = os.path.join(OUT, "texture-diag-report.json")
        os.makedirs(OUT, exist_ok=True)
        cls.rc = texture_diag.main(["--mod-utoc", MOD2K_UTOC, "--mod-ucas", MOD2K_UCAS,
                                    "--mod-utoc", MOD4K_UTOC, "--mod-ucas", MOD4K_UCAS,
                                    "--base-pak-listing", BASE_PAK_LISTING, "--out", cls.out] + common_args())
        cls.rep = json.load(open(cls.out))

    def setUp(self):
        require_inputs(self, [MOD2K_UTOC, MOD2K_UCAS, MOD4K_UTOC, MOD4K_UCAS, BASE_HEADER, GLOBAL_UTOC,
                              GLOBAL_UCAS, BASE_PAK_LISTING] + BASE_UTOCS)

    def test_exit_and_counts(self):
        self.assertEqual(self.rc, 0)
        c = self.rep["counts"]
        for k, v in c.items():
            self.assertGreater(v, 0, k)
        record("diag_mod_packages", c["mod_packages"])

    def test_game_ships_its_own_ps5_shader_platform_only(self):
        toks = self.rep["inputs"]["game_shader_platforms"]["platform_tokens"]
        self.assertIn("SP_PS5", toks)
        self.assertNotIn("PCD3D_SM6", toks)
        record("game_shader_platform_files", len(self.rep["inputs"]["game_shader_platforms"]["files"]))

    def test_every_mod_material_lacks_a_ps5_shader_map(self):
        n = 0
        for m in self.rep["mods"]:
            self.assertEqual(m["store"]["shader_map_hashes"], 0)  # no game-library shader maps
            self.assertEqual(m["game_platform_tokens_in_mod_materials"], 0)  # no PS5 inline data
            self.assertEqual(m["chunk_type_counts"].get("ShaderCodeLibrary", 0), 0)
            self.assertEqual(m["chunk_type_counts"].get("ShaderCode", 0), 0)
            mats = m["materials"]
            self.assertEqual(len(mats), 67)
            for r in mats:
                self.assertIn(r["verdict"], ("NO_PS5_SHADER_MAP", "INHERITS_PC_ONLY_PARENT"), r["package"])
                self.assertTrue(r["will_draw_checker"], r["package"])
                n += 1
            umat = [r for r in mats if r["class"] == "Material"]
            self.assertEqual(len(umat), 13)
            for r in umat:
                self.assertGreater(r["tokens"]["PCD3D_SM5"] + r["tokens"]["PCD3D_SM6"], 0)
                self.assertGreater(r["tokens"]["DXBC"], 0)
        record("materials_judged", n)

    def test_textures_are_pc_linear_and_base_is_not(self):
        n = 0
        for m in self.rep["mods"]:
            for t in m["textures"]:
                self.assertEqual(t["status"], "PARSED", t["package"])
                self.assertTrue(t["all_mips_linear_unpadded_size"], t["package"])
                self.assertIn(t["pixel_format"], ("PF_DXT1", "PF_DXT5", "PF_BC5"))
                n += 1
            self.assertEqual(m["linear_chain_control"]["checked"], m["linear_chain_control"]["matches"])
        base = self.rep["base_texture_ubulk_linear_test"]
        self.assertGreater(base["checked"], 1000)
        self.assertLess(base["match_fraction"], 0.5)
        record("mod_textures_parsed", n)
        record("base_texture_bulk_rows", base["checked"])

    def test_no_scene_wide_asset_in_mod(self):
        n = 0
        for m in self.rep["mods"]:
            for r in m["non_mod_root_packages"]:
                self.assertEqual(r["scene_wide_class"], [], r["package"])
                n += 1
        self.assertGreater(n, 0)
        record("non_mod_root_packages_checked", n)

    def test_zero_count_gate_fails(self):
        tmp = fresh_tmp("zero-gate")
        empty = os.path.join(tmp, "empty-listing.txt")
        open(empty, "w").close()
        buf = _stdio.StringIO()
        with contextlib.redirect_stdout(buf):
            rc = texture_diag.main(["--mod-utoc", MOD2K_UTOC, "--mod-ucas", MOD2K_UCAS, "--base-pak-listing",
                                    empty, "--out", os.path.join(tmp, "r.json")] + common_args())
        for line in buf.getvalue().splitlines():
            print("(expected, negative test) " + line)
        self.assertEqual(rc, 2)
        self.assertIn("FAIL: zero items checked in: game_shader_platform_files", buf.getvalue())
        record("zero_count_gate_cases", 1)

    def test_outside_mod_dependents_use_package_ids(self):
        """Blast radius is computed from PackageIds, so it is the same for every game (no path rewrite)."""
        n = 0
        for m in self.rep["mods"]:
            for r in m["non_mod_root_packages"]:
                self.assertLessEqual(len(r["base_dependents_outside_mod"]), r["base_dependents_total"])
                n += r["base_dependents_total"]
        self.assertGreater(n, 0)
        record("base_dependents_checked", n)


    def test_positive_control_real_container_with_planted_store_hashes(self):
        """End to end: the real mod container, with shader-map hashes copied from the GAME's own store
        into the mod's masters' store entries (scratch copy only). The gate must now call those
        masters, and the instances that only use them, OK. Everything else stays as before."""
        require_inputs(self, [RETOC, MOD2K_UTOC, MOD2K_UCAS, os.path.join(OVERLAY_PATCHED, "overlay_builder.py")])
        sys.path.insert(0, OVERLAY_PATCHED)
        import overlay_builder as ob
        tmp = fresh_tmp("planted-store-hashes")
        raw = os.path.join(tmp, "raw")
        p = subprocess.run([RETOC, "unpack-raw", MOD2K_UTOC, raw], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr[-1500:])
        hdr = [f for f in os.listdir(os.path.join(raw, "chunks")) if f.endswith("00000006")]
        self.assertEqual(len(hdr), 1)
        hdr_path = os.path.join(raw, "chunks", hdr[0])
        mod_header = ob.parse_container_header(open(hdr_path, "rb").read())
        base_header = ob.parse_container_header(open(BASE_HEADER, "rb").read())
        donor = next(e.shader_map_hashes for e in base_header.packages.values() if e.shader_map_hashes)
        before = {r["package"]: r for r in self.rep["mods"][0]["materials"]}
        masters = {r["package"] for r in before.values() if r["class"] == "Material"}
        self.assertEqual(len(masters), 13)
        from cityhash64 import cityhash64
        # plant on every other master only, so the test also proves the rest stay negative
        chosen = set(sorted(masters)[::2])
        planted = 0
        for name in chosen:
            pid = cityhash64(name.lower().encode("utf-16-le"))
            self.assertIn(pid, mod_header.packages, name)
            e = mod_header.packages[pid]
            mod_header.packages[pid] = ob.StoreEntry(e.imported_packages, donor)
            planted += 1
        os.remove(hdr_path)
        open(hdr_path, "wb").write(ob.serialize_container_header(mod_header))
        pack = os.path.join(tmp, "pack")
        os.makedirs(pack)
        utoc = os.path.join(pack, os.path.basename(MOD2K_UTOC))
        p = subprocess.run([RETOC, "pack-raw", raw, utoc], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr[-1500:])
        shutil.rmtree(raw)
        out = os.path.join(tmp, "r.json")
        with contextlib.redirect_stdout(_stdio.StringIO()):
            rc = texture_diag.main(["--mod-utoc", utoc, "--mod-ucas", os.path.splitext(utoc)[0] + ".ucas",
                                    "--base-pak-listing", BASE_PAK_LISTING, "--out", out] + common_args())
        self.assertEqual(rc, 0)
        after = {r["package"]: r for r in json.load(open(out))["mods"][0]["materials"]}
        self.assertEqual(set(after), set(before))
        ok_after = unchanged = 0
        for name, r in after.items():
            if name in chosen:
                self.assertEqual(r["verdict"], "HAS_GAME_SHADER_MAP_IN_STORE", name)
                self.assertFalse(r["will_draw_checker"])
                ok_after += 1
            elif r["mod_parents"] and all(x in chosen for x in r["mod_parents"]) and \
                    sum(r["tokens"].values()) == 0:
                self.assertEqual(r["verdict"], "USES_MOD_PARENT_WITH_GAME_SHADER_MAP", name)
                self.assertFalse(r["will_draw_checker"])
                ok_after += 1
            else:
                self.assertEqual(r["verdict"], before[name]["verdict"], name)
                self.assertTrue(r["will_draw_checker"], name)
                unchanged += 1
        self.assertGreater(ok_after, planted)
        self.assertGreater(unchanged, 0)
        record("positive_control_materials_ok", ok_after)
        record("positive_control_materials_still_negative", unchanged)
        shutil.rmtree(tmp)

    def test_tools_name_no_game_or_mod(self):
        """The tools must not name the game or the mod. The words to look for come from the data
        (the mod's own mount root and its new content folder), not from a list in this test."""
        import material_plan
        import iostore_lib as io
        toc = io.parse_toc(MOD2K_UTOC)
        pairs = {}
        with open(MOD2K_UCAS, "rb") as fh:
            for i in range(toc.entry_count):
                if toc.chunk_type(i) == 1:
                    z = io.parse_zen(io.read_chunk(toc, MOD2K_UCAS, i, fh))
                    pairs[z.name] = {"name": z.name, "path": toc.paths.get(i, "")}
        roots = material_plan.mount_roots_from(pairs)
        self.assertTrue(roots)
        words = {roots[0][1].split("/")[0].lower()}                    # the project folder
        words.add(self.rep["mods"][0]["new_content_root"].strip("/").split("/")[-1])  # the mod's own folder
        self.assertTrue(all(len(w) >= 3 for w in words), words)
        files = [f for f in os.listdir(TOOLS) if f.endswith(".py")]
        self.assertGreater(len(files), 0)
        for f in files:
            src = open(os.path.join(TOOLS, f)).read().lower()
            for w in words:
                self.assertFalse(w in src, f"{f} names {w!r}")
        record("tool_files_scanned_for_game_names", len(files))


class TestMaterialVerdict(unittest.TestCase):
    """The material gate must be able to say OK (positive controls), not only 'checker'."""

    def test_verdict_table(self):
        V = texture_diag.material_verdict
        OK = texture_diag.MATERIAL_OK_VERDICTS
        cases = [
            # (is_master, pc, game, store_hashes, mod_parent_verdicts, base_parents) -> ok?
            ((True, 0, 0, 3, (), ()), True),             # cooked for the game: hashes in its store entry
            ((True, 5, 0, 2, (), ()), True),             # store hashes win (library shader map present)
            ((True, 0, 4, 0, (), ()), True),             # inline shader map for the game's platform
            ((True, 7, 0, 0, (), ()), False),            # PC only
            ((True, 0, 0, 0, (), ()), False),            # no shader map at all
            ((False, 0, 0, 1, (), ()), True),            # instance with its own game permutation
            ((False, 0, 0, 0, ("HAS_GAME_SHADER_MAP_IN_STORE",), ()), True),
            ((False, 0, 0, 0, ("NO_PS5_SHADER_MAP",), ()), False),
            ((False, 0, 0, 0, ("HAS_GAME_SHADER_MAP_IN_STORE", "NO_PS5_SHADER_MAP"), ()), False),
            ((False, 0, 0, 0, (), ("/Game/Base/M_Master",)), True),
            ((False, 3, 0, 0, (), ("/Game/Base/M_Master",)), False),  # own PC-only permutation
            ((False, 0, 0, 0, (), ()), False),           # unresolved parent fails closed
        ]
        for args, ok in cases:
            self.assertEqual(V(*args) in OK, ok, args)
        record("material_verdict_cases", len(cases))
        record("material_verdict_positive_cases", sum(1 for _, ok in cases if ok))

if __name__ == "__main__":
    unittest.main()
