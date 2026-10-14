"""Derived-copy import rewrite: invariants, refusals, retoc acceptance, and the
patched overlay_builder closest-match guard (refuse original, accept rewritten)."""
import importlib.util
import json
import os
import shutil
import struct
import subprocess
import unittest

from common import (MOD2K_UTOC, MOD2K_UCAS, MOD2K_PAK, GLOBAL_UTOC, GLOBAL_UCAS, RETOC, OVERLAY_PATCHED,
                    BASE_UTOCS, BASE_HEADER, fresh_tmp, record, require_inputs)
import iostore_lib as io
import zen_import_rewrite as zr
import blake3_min
from cityhash64 import cityhash64

TARGET_PKG = "/Game/Example/Materials/M_BaseMaster"   # synthetic target name: rewrite is name-agnostic
TARGET_EXP = "M_BaseMaster"


def pid_of(name):
    return cityhash64(name.lower().encode("utf-16-le"))


class TestRewrite(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.toc = io.parse_toc(MOD2K_UTOC)
        cls.mats = {}
        cls.pkgs = []
        with open(MOD2K_UCAS, "rb") as fh:
            for i in range(cls.toc.entry_count):
                if cls.toc.chunk_type(i) == 1:
                    b = io.read_chunk(cls.toc, MOD2K_UCAS, i, fh)
                    cls.pkgs.append((i, b, io.parse_zen(b)))

    def setUp(self):
        require_inputs(self, [MOD2K_UTOC, MOD2K_UCAS])

    def instances_with_mod_parent(self):
        names = {z.name for _, _, z in self.pkgs}
        for i, b, z in self.pkgs:
            parents = [x for x in z.imported_package_names
                       if x in names and x.rsplit("/", 1)[-1].startswith("M_")]
            if parents:
                yield i, b, z, parents[0]

    def test_rewrite_every_instance_keeps_payload_identical(self):
        n = 0
        for i, b, z, parent in self.instances_with_mod_parent():
            out, rec = zr.rewrite_import(b, parent, parent.rsplit("/", 1)[-1], TARGET_PKG, TARGET_EXP)
            z2 = io.parse_zen(out)
            self.assertIn(TARGET_PKG, z2.imported_package_names)
            self.assertNotIn(parent, z2.imported_package_names)
            self.assertIn(zr.public_export_hash(TARGET_EXP), z2.imported_hashes)
            self.assertEqual([e["size"] for e in z2.exports], [e["size"] for e in z.exports])
            self.assertEqual(rec["bytes_after"] - rec["bytes_before"],
                             rec["header_size_after"] - rec["header_size_before"])
            self.assertEqual(blake3_min.io_hash20(b), self.toc.metas[i][0])  # source bytes untouched
            n += 1
        self.assertGreaterEqual(n, 40)
        record("rewrites_verified", n)

    def test_refusals(self):
        i, b, z, parent = next(self.instances_with_mod_parent())
        cases = 0
        with self.assertRaises(io.FormatError):   # wrong export name -> hash mismatch, never guessed
            zr.rewrite_import(b, parent, "NotTheExport", TARGET_PKG, TARGET_EXP)
        cases += 1
        with self.assertRaises(io.FormatError):   # package not imported
            zr.rewrite_import(b, "/Game/Nope/M_Nope", "M_Nope", TARGET_PKG, TARGET_EXP)
        cases += 1
        with self.assertRaises(io.FormatError):   # target already imported
            other = [x for x in z.imported_package_names if x != parent][0]
            zr.rewrite_import(b, parent, parent.rsplit("/", 1)[-1], other, other.rsplit("/", 1)[-1])
        cases += 1
        with self.assertRaises(io.FormatError):   # truncated package
            zr.rewrite_import(b[:100], parent, parent.rsplit("/", 1)[-1], TARGET_PKG, TARGET_EXP)
        cases += 1
        record("rewrite_refusal_cases", cases)

    def test_retoc_accepts_rewritten_container_and_guard(self):
        require_inputs(self, [RETOC, MOD2K_PAK, GLOBAL_UTOC, GLOBAL_UCAS,
                              os.path.join(OVERLAY_PATCHED, "overlay_builder.py")])
        tmp = fresh_tmp("retoc-rewrite")
        raw = os.path.join(tmp, "raw")
        p = subprocess.run([RETOC, "unpack-raw", MOD2K_UTOC, raw], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr[-1500:])
        manifest = json.load(open(os.path.join(raw, "manifest.json")))
        rewritten = []
        for i, b, z, parent in self.instances_with_mod_parent():
            cid = self.toc.chunk_hex(i)
            fn = os.path.join(raw, "chunks", cid)
            self.assertTrue(os.path.exists(fn), cid)
            out, _ = zr.rewrite_import(open(fn, "rb").read(), parent, parent.rsplit("/", 1)[-1],
                                       TARGET_PKG, TARGET_EXP)
            os.remove(fn)
            open(fn, "wb").write(out)
            rewritten.append((self.toc.chunk_u64(i), pid_of(parent)))
        pack = os.path.join(tmp, "pack")
        os.makedirs(pack)
        utoc = os.path.join(pack, os.path.basename(MOD2K_UTOC))
        p = subprocess.run([RETOC, "pack-raw", raw, utoc], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr[-1500:])
        p = subprocess.run([RETOC, "verify", utoc], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stdout[-800:] + p.stderr[-800:])
        for src in (GLOBAL_UTOC, GLOBAL_UCAS, MOD2K_PAK):
            os.symlink(src, os.path.join(pack, os.path.basename(src)))
        leg = os.path.join(tmp, "legacy")
        p = subprocess.run([RETOC, "to-legacy", "--no-shaders", pack, leg], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr[-1500:])
        self.assertIn("(0 failed)", p.stdout + p.stderr)
        # patched overlay_builder guard: original refuses, rewritten admits
        spec = importlib.util.spec_from_file_location("ob_patched",
                                                      os.path.join(OVERLAY_PATCHED, "overlay_builder.py"))
        ob = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(ob)
        orig = ob.ModSpec("orig", MOD2K_UTOC, MOD2K_UCAS)
        new = ob.ModSpec("new", utoc, os.path.splitext(utoc)[0] + ".ucas")
        n = 0
        for pkg_pid, from_pid in rewritten:
            with self.assertRaises(ob.Refused) as caught:
                ob.require_package_import_rewritten(orig, pkg_pid, from_pid, pid_of(TARGET_PKG))
            self.assertEqual(caught.exception.code, "closest_match_package_import_not_rewritten")
            ob.require_package_import_rewritten(new, pkg_pid, from_pid, pid_of(TARGET_PKG))
            n += 1
        self.assertGreaterEqual(n, 40)
        record("guard_original_refused_rewritten_admitted", n)
        shutil.rmtree(raw)


    def test_build_overlay_admits_rewritten_container_end_to_end(self):
        """Finding 5: one full build_overlay run with closest-match rows over the REWRITTEN real
        container, toward a real base master chosen from data (material_plan's first candidate).

        Ordering this test pins down: retoc pack-raw keeps the OLD parent in the derived
        container's own store entry, while the package header names the NEW parent. build_overlay
        (1) finds from_package_id in that store entry, (2) the guard reads the package header
        and admits it because it names to_package_id, (3) the repoint fixes the store entry."""
        require_inputs(self, [RETOC, BASE_HEADER, BASE_UTOCS[0], os.path.join(OVERLAY_PATCHED,
                                                                               "overlay_builder.py")])
        import material_plan
        tmp = fresh_tmp("e2e-overlay")
        plan_out = os.path.join(tmp, "plan.json")
        args = ["--mod-utoc", MOD2K_UTOC, "--mod-ucas", MOD2K_UCAS, "--out", plan_out,
                "--base-container-header", BASE_HEADER, "--global-utoc", GLOBAL_UTOC, "--global-ucas", GLOBAL_UCAS]
        for b in BASE_UTOCS:
            args += ["--base-utoc", b]
        self.assertEqual(material_plan.main(args), 0)
        plan = json.load(open(plan_out))
        b_rows = [r for r in plan["rows"] if r["option"] == "B_REPARENT"]
        self.assertGreater(len(b_rows), 0)
        cand = b_rows[0]["candidate_base_masters"][0]
        # base TOC path -> package name, with the mount roots derived from the mod's own data
        target_pkg = None
        for virt, real in plan["mount_roots"]:
            if cand["base_path"].startswith(real) and cand["base_path"].endswith(".uasset"):
                target_pkg = virt + cand["base_path"][len(real):-len(".uasset")]
        self.assertIsNotNone(target_pkg)
        target_pid = pid_of(target_pkg)
        self.assertEqual("%016x" % target_pid, cand["base_pid"])   # the mapping is proven, not assumed
        target_exp = cand["export_name"]

        raw = os.path.join(tmp, "raw")
        p = subprocess.run([RETOC, "unpack-raw", MOD2K_UTOC, raw], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr[-1500:])
        name_to_row = {z.name: i for i, _, z in self.pkgs}
        sub_rows, rewritten = [], []
        for r in b_rows:
            parent = r["mod_parent"][0]
            i = name_to_row[r["package"]]
            fn = os.path.join(raw, "chunks", self.toc.chunk_hex(i))
            out, _ = zr.rewrite_import(open(fn, "rb").read(), parent, parent.rsplit("/", 1)[-1],
                                       target_pkg, target_exp)
            os.remove(fn)
            open(fn, "wb").write(out)
            pkg_pid = self.toc.chunk_u64(i)
            rewritten.append((pkg_pid, pid_of(parent)))
            sub_rows.append({"mod_id": "leon-derived", "package_id": "%016x" % pkg_pid,
                             "action": "closest-match", "from_package_id": "%016x" % pid_of(parent),
                             "to_package_id": "%016x" % target_pid})
        pack = os.path.join(tmp, "pack")
        os.makedirs(pack)
        utoc = os.path.join(pack, os.path.basename(MOD2K_UTOC))
        p = subprocess.run([RETOC, "pack-raw", raw, utoc], capture_output=True, text=True)
        self.assertEqual(p.returncode, 0, p.stderr[-1500:])
        shutil.rmtree(raw)

        spec = importlib.util.spec_from_file_location("ob_patched_e2e",
                                                      os.path.join(OVERLAY_PATCHED, "overlay_builder.py"))
        ob = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(ob)
        derived = ob.ModSpec("leon-derived", utoc, os.path.splitext(utoc)[0] + ".ucas")
        # ordering fact: the derived container's own store entry still names the OLD parent
        dtoc = io.parse_toc(utoc)
        with open(derived.ucas_path, "rb") as fh:
            hdr = next(io.read_chunk(dtoc, derived.ucas_path, i, fh) for i in range(dtoc.entry_count)
                       if dtoc.chunk_type(i) == 6)
        own_store = io.parse_container_header(hdr)["entries"]
        for pkg_pid, from_pid in rewritten:
            self.assertIn(from_pid, own_store[pkg_pid]["imports"])
            self.assertNotIn(target_pid, own_store[pkg_pid]["imports"])

        rows = ob.parse_substitutions({"schema": 1, "rows": sub_rows})
        base_bytes = open(BASE_UTOCS[0], "rb").read()
        header_chunk = open(BASE_HEADER, "rb").read()
        derived_utoc, sidecar, receipt = ob.build_overlay(base_bytes, LEON_BASE_UCAS_SIZE, header_chunk,
                                                          [derived], "reject", rows)
        ledger = receipt["substitution_ledger"]
        self.assertEqual(ledger["rows"], len(sub_rows))
        self.assertEqual(ledger["actions"]["closest-match"], len(sub_rows))
        self.assertTrue(all(r["store_entry"] == "derived-import-repoint" for r in ledger["applied"]))
        self.assertTrue(receipt["byte_identity"]["mod_payloads_verbatim"])
        out_toc = ob.parse_toc(derived_utoc)
        ob.validate_lookup(out_toc)
        n = 0
        for r in ledger["applied"]:
            after = [int(x, 16) for x in r["store_entry_imports_after"]]
            self.assertIn(target_pid, after)
            self.assertNotIn(int(r["from_package_id"], 16), after)
            n += 1
        self.assertEqual(n, len(sub_rows))
        record("e2e_build_overlay_closest_match_admitted", n)
        shutil.rmtree(tmp)


LEON_BASE_UCAS_SIZE = 128240377200  # the game's pakchunk0 .ucas size (same pin as the overlay suite)


if __name__ == "__main__":
    unittest.main()
