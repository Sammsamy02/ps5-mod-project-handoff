#!/usr/bin/env python3
"""End-to-end host tests of chunk_intake.py on synthetic base + mod containers."""
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
import fixture_builder as fb  # noqa: E402
import ue_formats as uf  # noqa: E402
import chunk_intake as ci  # noqa: E402

S = fb.script_index
MAT, MIC, TEX, MESH = (S("/Script/Engine.Material"), S("/Script/Engine.MaterialInstanceConstant"),
                       S("/Script/Engine.Texture2D"), S("/Script/Engine.SkeletalMesh"))
SCRIPT_PATHS = ["/Script/Engine.Material", "/Script/Engine.MaterialInstanceConstant",
                "/Script/Engine.Texture2D", "/Script/Engine.SkeletalMesh"]
TARGET_FMT = "SF_TGT"  # synthetic target format: only ever learned from the base shader archive name


def pub(pkg):
    return fb.public_hash(pkg + "." + pkg.rsplit("/", 1)[-1])


def fstr(s):
    b = s.encode() + b"\0"
    return len(b).to_bytes(4, "little") + b


class World:
    def __init__(self, root):
        self.root = root
        self.base_dir = os.path.join(root, "base")
        self.mod_dir = os.path.join(root, "mod")
        self.fetch_dir = os.path.join(root, "fetched")
        for d in (self.base_dir, self.mod_dir, self.fetch_dir):
            os.makedirs(d, exist_ok=True)
        self.so = fb.write_script_objects(os.path.join(root, "global"), SCRIPT_PATHS)

    def base(self, pkgs, archive_fmt=TARGET_FMT, block_size=65536, stem="pakchunk0-tgt"):
        extra = []
        if archive_fmt:
            extra.append((fb.chunk_id(0xABC, 0, 8), b"LIB" * 10,
                          "Game/Content/ShaderArchive-Game_Chunk0-%s-SP_%s.ushaderbytecode" % (archive_fmt, archive_fmt[3:])))
        self.base_stem = os.path.join(self.base_dir, stem)
        self.base_utoc, self.base_ucas = fb.write_package_container(self.base_stem, 0x1111, pkgs, extra_chunks=extra,
                                                                    block_size=block_size)
        t = uf.parse_toc(uf.read_bytes(self.base_utoc))
        row = [i for i, c in enumerate(t.chunk_ids) if uf.Toc.chunk_type(c) == 6][0]
        self.base_header = os.path.join(self.root, "base-header.bin")
        with open(self.base_header, "wb") as f:
            f.write(uf.ChunkReader(t, self.base_ucas).read_verified(row))

    def mod(self, pkgs, block_size=65536):
        self.mod_utoc, self.mod_ucas = fb.write_package_container(os.path.join(self.mod_dir, "zzz_mod_P"), 0x2222, pkgs,
                                                                  block_size=block_size)

    def run(self, fetched=False, extra=()):
        out = os.path.join(self.root, "report.json")
        plan = os.path.join(self.root, "plan.json")
        argv = ["--mod-utoc", self.mod_utoc, "--mod-ucas", self.mod_ucas,
                "--base-utoc", self.base_utoc,
                "--base-container-header", "pakchunk0-tgt=" + self.base_header,
                "--script-objects-utoc", self.so[0], "--script-objects-ucas", self.so[1],
                "--out", out, "--fetch-plan", plan, "--verify-bulk"] + list(extra)
        if fetched:
            argv += ["--base-fetched", self.fetch_dir]
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            code = ci.main(argv)
        self.stdout = buf.getvalue()
        self.report = json.load(open(out))
        self.plan = json.load(open(plan))
        return code

    def do_fetch(self, corrupt=False):
        """Simulates the integrator: read exactly the planned physical ranges from the base UCAS."""
        n = 0
        with open(self.base_ucas, "rb") as f:
            for c in self.plan["chunks"]:
                data = b""
                for r in c["physical_ranges"]:
                    f.seek(r["offset"])
                    data += f.read(r["length"])
                if corrupt:
                    data = data[:-1] + bytes([data[-1] ^ 0xFF])
                dst = os.path.join(self.fetch_dir, c["save_as"])
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                with open(dst, "wb") as g:
                    g.write(data)
                n += 1
        return n


def base_material(name, hashes=1):
    return fb.Pkg(name, [{"name": name.rsplit("/", 1)[-1], "cls": MAT, "payload": b"basemat"}],
                  imports=[MAT], shader_hashes=hashes)


def tex(name, pf="PF_DXT1", opt=None, size=64):
    packed = 1 | ((1 << 30) if opt is not None else 0)
    mips = [max(1, size // 4) ** 2 * 8]
    return fb.Pkg(name, [{"name": name.rsplit("/", 1)[-1], "cls": TEX,
                          "payload": fb.texture_payload(pf, size, size, packed=packed, opt=opt)}],
                  imports=[TEX], bulk_sizes=mips, extra_names=[pf])


def mic(name, parent, textures=()):
    imported = [(parent, [pub(parent)])] + [(t, [pub(t)]) for t in textures]
    imports = [MIC] + [fb.package_import(i, i) for i in range(len(imported))]
    return fb.Pkg(name, [{"name": name.rsplit("/", 1)[-1], "cls": MIC, "payload": b"mic"}],
                  imports=imports, imported_packages=imported)


def pc_material(name, textures=()):
    payload = b"\x01" * 8 + fstr("PCD3D_SM6") + fstr("PCD3D_SM5") + b"DXBC" + b"\0" * 32 + b"DXIL"
    imported = [(t, [pub(t)]) for t in textures]
    return fb.Pkg(name, [{"name": name.rsplit("/", 1)[-1], "cls": MAT, "payload": payload}],
                  imports=[MAT] + [fb.package_import(i, i) for i in range(len(imported))],
                  imported_packages=imported)


def mesh(name, materials):
    imported = [(m, [pub(m)]) for m in materials]
    return fb.Pkg(name, [{"name": name.rsplit("/", 1)[-1], "cls": MESH, "payload": b"mesh"}],
                  imports=[MESH] + [fb.package_import(i, i) for i in range(len(imported))],
                  imported_packages=imported)


class IntakeTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.mkdtemp(prefix="cap08-")
        self.w = World(self.tmp)

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_native_parent_needs_fetch_then_admits(self):
        w = self.w
        w.base([base_material("/Game/Base/M_Master"), tex("/Game/Base/T_Base")])
        w.mod([mic("/Game/Mod/MI_New", "/Game/Base/M_Master", ["/Game/Mod/T_New"]), tex("/Game/Mod/T_New"),
               mesh("/Game/Mod/SK_New", ["/Game/Mod/MI_New"])])
        self.assertEqual(w.run(), ci.EXIT_INCOMPLETE)
        self.assertEqual(w.report["closure"]["verdict"], "PACKAGE-LEVEL-OK, EXPORT-LEVEL-PENDING")
        self.assertTrue(any(c["virtual_path"] and "M_Master" in c["virtual_path"] for c in w.plan["chunks"]))
        mats = {m["package"]: m for m in w.report["visual"]["materials"]}
        self.assertEqual(mats["/Game/Mod/MI_New"]["verdict"], "NATIVE")
        self.assertGreater(w.do_fetch(), 0)
        code = w.run(fetched=True)
        self.assertEqual(code, ci.EXIT_OK, w.stdout)
        self.assertEqual(w.report["closure"]["verdict"], "CLOSED")
        self.assertEqual(w.report["target_shader_formats"], [TARGET_FMT])
        self.assertGreater(w.report["counts"]["items_checked_total"], 0)

    def test_pc_only_material_is_disclosed_with_meshes(self):
        w = self.w
        w.base([base_material("/Game/Base/M_Master")])
        w.mod([pc_material("/Game/Mod/M_PC", ["/Game/Mod/T_A"]), tex("/Game/Mod/T_A"),
               mic("/Game/Mod/MI_Child", "/Game/Mod/M_PC"), mesh("/Game/Mod/SK_Char", ["/Game/Mod/MI_Child"])])
        code = w.run()
        self.assertEqual(code, ci.EXIT_VISUAL_LOSS, w.stdout)
        mats = {m["package"]: m for m in w.report["visual"]["materials"]}
        self.assertEqual(mats["/Game/Mod/M_PC"]["verdict"], "FALLBACK-DEFAULT-MATERIAL")
        self.assertEqual(mats["/Game/Mod/MI_Child"]["verdict"], "FALLBACK-DEFAULT-MATERIAL")
        self.assertEqual(mats["/Game/Mod/M_PC"]["chain"][0]["inline_shader_formats"], ["PCD3D_SM5", "PCD3D_SM6"])
        self.assertEqual(w.report["visual"]["meshes"][0]["materials_with_visual_loss"], ["/Game/Mod/MI_Child"])
        self.assertIn("grey/white checker", w.stdout)
        t = w.report["visual"]["textures"]["mod"][0]
        self.assertTrue(t["only_used_by_fallback_materials"])

    def test_target_format_is_learned_from_base_files(self):
        """Same mod material, but carrying the base's own format name inline -> NATIVE."""
        w = self.w
        payload = b"\x01" * 8 + fstr(TARGET_FMT)
        p = fb.Pkg("/Game/Mod/M_T", [{"name": "M_T", "cls": MAT, "payload": payload}], imports=[MAT])
        w.base([base_material("/Game/Base/M_Master")])
        w.mod([p])
        self.assertEqual(w.run(), ci.EXIT_OK, w.stdout)
        self.assertEqual(w.report["visual"]["materials"][0]["verdict"], "NATIVE")
        # base without a shader archive: target unknown -> incomplete, never NATIVE by guess
        w2 = World(os.path.join(self.tmp, "w2"))
        w2.base([base_material("/Game/Base/M_Master")], archive_fmt=None)
        w2.mod([p])
        self.assertEqual(w2.run(), ci.EXIT_INCOMPLETE)
        self.assertNotEqual(w2.report["visual"]["materials"][0]["verdict"], "NATIVE")

    def test_structural_missing_import_refuses(self):
        w = self.w
        w.base([base_material("/Game/Base/M_Master")])
        missing = "/Game/Nowhere/BP_Parent"
        p = fb.Pkg("/Game/Mod/BP_X", [{"name": "BP_X", "cls": fb.package_import(0, 0), "payload": b"x"}],
                   imports=[fb.package_import(0, 0)], imported_packages=[(missing, [pub(missing)])])
        w.mod([p])
        self.assertEqual(w.run(), ci.EXIT_REFUSED)
        self.assertEqual(w.report["closure"]["verdict"], "BROKEN")

    def test_missing_script_class_refuses(self):
        w = self.w
        w.base([base_material("/Game/Base/M_Master")])
        cls = S("/Script/NotInGame.Widget")
        w.mod([fb.Pkg("/Game/Mod/W", [{"name": "W", "cls": cls, "payload": b"x"}], imports=[cls])])
        self.assertEqual(w.run(), ci.EXIT_REFUSED)

    def test_missing_plain_reference_is_disclosed_not_refused(self):
        w = self.w
        w.base([base_material("/Game/Base/M_Master")])
        ghost = "/EditorOnlyPlugin/Functions/MF_Thing"
        p = fb.Pkg("/Game/Mod/M_Ref", [{"name": "M_Ref", "cls": MAT, "payload": b"\x01" * 4 + fstr(TARGET_FMT)}],
                   imports=[MAT, fb.package_import(0, 0)], imported_packages=[(ghost, [pub(ghost)])])
        w.mod([p])
        code = w.run()
        self.assertEqual(code, ci.EXIT_OK, w.stdout)
        self.assertEqual(w.report["closure"]["missing_reference_packages"], [ghost])
        self.assertIn("missing references", w.stdout)

    def test_replacement_dropping_public_export_refuses(self):
        w = self.w
        base_pkg = fb.Pkg("/Game/Base/MI_Old", [{"name": "MI_Old", "cls": MIC, "payload": b"a"},
                                                 {"name": "Extra", "cls": MIC, "payload": b"b"}],
                          imports=[MIC, fb.package_import(0, 0)],
                          imported_packages=[("/Game/Base/M_Master", [pub("/Game/Base/M_Master")])])
        w.base([base_material("/Game/Base/M_Master"), base_pkg])
        w.mod([mic("/Game/Base/MI_Old", "/Game/Base/M_Master")])
        self.assertEqual(w.run(), ci.EXIT_INCOMPLETE)
        w.do_fetch()
        self.assertEqual(w.run(fetched=True), ci.EXIT_REFUSED, w.stdout)
        rep = w.report["replacements"][0]
        self.assertEqual(len(rep["public_exports"]["missing_in_mod"]), 1)

    def test_corrupt_fetch_is_not_used(self):
        w = self.w
        w.base([base_material("/Game/Base/M_Master")])
        w.mod([mic("/Game/Mod/MI_New", "/Game/Base/M_Master")])
        w.run()
        w.do_fetch(corrupt=True)
        # F6: a bad transfer is INCOMPLETE (re-fetch), never REFUSED, and the chunk is planned again
        self.assertEqual(w.run(fetched=True), ci.EXIT_INCOMPLETE)
        self.assertEqual(w.report["refusals"], [])
        self.assertTrue(any("fails its TOC IoHash" in r and "re-fetch" in r for r in w.report["incomplete"]))
        self.assertTrue(any(c["virtual_path"] and "M_Master" in c["virtual_path"] for c in w.plan["chunks"]))
        self.assertEqual(w.report["fetched_used"], [])
        w.do_fetch()
        self.assertEqual(w.run(fetched=True), ci.EXIT_OK, w.stdout)

    def test_base_parent_chain_plans_each_missing_header(self):
        """F7: mod MI -> base MI_Mid -> base MI_Top -> base M_Master.  The base instances
        carry no shader maps, so each step needs its header; every round must plan the
        next one (never UNKNOWN with nothing left to fetch)."""
        w = self.w
        top = fb.Pkg("/Game/Base/MI_Top", [{"name": "MI_Top", "cls": MIC, "payload": b"t"}],
                     imports=[MIC, fb.package_import(0, 0)],
                     imported_packages=[("/Game/Base/M_Master", [pub("/Game/Base/M_Master")])])
        mid = fb.Pkg("/Game/Base/MI_Mid", [{"name": "MI_Mid", "cls": MIC, "payload": b"m"}],
                     imports=[MIC, fb.package_import(0, 0)],
                     imported_packages=[("/Game/Base/MI_Top", [pub("/Game/Base/MI_Top")])])
        w.base([base_material("/Game/Base/M_Master"), top, mid])
        w.mod([mic("/Game/Mod/MI_New", "/Game/Base/MI_Mid")])
        rounds = []
        code = w.run()
        for _ in range(4):
            rounds.append((code, sorted(c["virtual_path"].rsplit("/", 1)[-1] for c in w.plan["chunks"] if c["virtual_path"])))
            if code != ci.EXIT_INCOMPLETE:
                break
            self.assertTrue(w.plan["chunks"], "INCOMPLETE but nothing planned")
            w.do_fetch()
            code = w.run(fetched=True)
        print("  rounds:", rounds)
        self.assertEqual(code, ci.EXIT_OK, w.stdout)
        self.assertTrue(any(any("MI_Top" in n for n in r[1]) for r in rounds))
        mats = {m["package"]: m for m in w.report["visual"]["materials"]}
        self.assertEqual(mats["/Game/Mod/MI_New"]["verdict"], "NATIVE")

    def test_unexpected_error_writes_fresh_refused_report(self):
        """F8: a crash outside the format errors still fails closed and never leaves an old report."""
        w = self.w
        w.base([base_material("/Game/Base/M_Master")])
        w.mod([pc_material("/Game/Mod/M_PC")])
        self.assertEqual(w.run(), ci.EXIT_VISUAL_LOSS)
        orig = ci.Intake.visual
        try:
            def boom(self):
                raise KeyError("planted")
            ci.Intake.visual = boom
            self.assertEqual(w.run(), ci.EXIT_REFUSED)
        finally:
            ci.Intake.visual = orig
        self.assertEqual(w.report["verdict"], "REFUSED")
        self.assertTrue(any("unexpected KeyError" in r for r in w.report["refusals"]))

    def test_profile_mismatch_refuses(self):
        w = self.w
        w.base([base_material("/Game/Base/M_Master")], block_size=65536)
        w.mod([pc_material("/Game/Mod/M_PC")], block_size=32768)
        self.assertEqual(w.run(), ci.EXIT_REFUSED)
        self.assertTrue(w.report["format_profile"]["mismatches"])

    def test_mod_chunk_corruption_refuses(self):
        w = self.w
        w.base([base_material("/Game/Base/M_Master")])
        w.mod([pc_material("/Game/Mod/M_PC")])
        b = bytearray(uf.read_bytes(w.mod_ucas))
        b[100] ^= 0xFF
        with open(w.mod_ucas, "wb") as f:
            f.write(bytes(b))
        self.assertEqual(w.run(), ci.EXIT_REFUSED)

    def test_texture_layout_differs_when_base_carries_platform_layout(self):
        w = self.w
        base_mi = fb.Pkg("/Game/Base/MI_Old", [{"name": "MI_Old", "cls": MIC, "payload": b"a"}],
                         imports=[MIC, fb.package_import(0, 0), fb.package_import(1, 1)],
                         imported_packages=[("/Game/Base/M_Master", [pub("/Game/Base/M_Master")]),
                                            ("/Game/Base/T_Base", [pub("/Game/Base/T_Base")])])
        w.base([base_material("/Game/Base/M_Master"), tex("/Game/Base/T_Base", opt=(5, 2)), base_mi])
        w.mod([mic("/Game/Base/MI_Old", "/Game/Base/M_Master", ["/Game/Mod/T_New"]), tex("/Game/Mod/T_New")])
        w.run()
        self.assertIn("UNVERIFIED", w.report["visual"]["textures"]["verdict"])
        self.assertTrue(any("texture sample" in c["why"] for c in w.plan["chunks"]))
        w.do_fetch()
        self.assertEqual(w.run(fetched=True), ci.EXIT_VISUAL_LOSS, w.stdout)
        self.assertIn("LAYOUT-DIFFERS", w.report["visual"]["textures"]["verdict"], w.stdout)

    def test_texture_no_difference_when_base_is_linear(self):
        w = self.w
        base_mi = fb.Pkg("/Game/Base/MI_Old", [{"name": "MI_Old", "cls": MIC, "payload": b"a"}],
                         imports=[MIC, fb.package_import(0, 0), fb.package_import(1, 1)],
                         imported_packages=[("/Game/Base/M_Master", [pub("/Game/Base/M_Master")]),
                                            ("/Game/Base/T_Base", [pub("/Game/Base/T_Base")])])
        w.base([base_material("/Game/Base/M_Master"), tex("/Game/Base/T_Base"), base_mi])
        w.mod([mic("/Game/Base/MI_Old", "/Game/Base/M_Master", ["/Game/Mod/T_New"]), tex("/Game/Mod/T_New")])
        w.run()
        w.do_fetch()
        self.assertEqual(w.run(fetched=True), ci.EXIT_OK, w.stdout)
        self.assertIn("NO-DIFFERENCE-FOUND", w.report["visual"]["textures"]["verdict"], w.stdout)

    def test_zero_packages_refuses(self):
        w = self.w
        w.base([base_material("/Game/Base/M_Master")])
        w.mod([])
        self.assertEqual(w.run(), ci.EXIT_REFUSED)
        self.assertIn("zero packages", " ".join(w.report["refusals"]))


if __name__ == "__main__":
    unittest.main(verbosity=2)
