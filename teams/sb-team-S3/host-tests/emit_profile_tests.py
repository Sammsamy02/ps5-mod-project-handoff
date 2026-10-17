#!/usr/bin/env python3
"""Team S3 tests for the profile-file emitter (readiness item B4).

Synthetic images with numbers that are not any game's, plus the real Townfall two-source check (skipped
cleanly when the executable is absent). Prints a count and FAILS on zero tests, mirroring the gate rule
"a gate that scans nothing is a failed gate, not a pass".

    python3 -I host-tests/emit_profile_tests.py
"""
import hashlib
import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.join(HERE, "..", "generator", "outputs", "ps5-mod-bridge")
sys.path.insert(0, PROJECT)
sys.path.insert(0, HERE)                                            # so the Townfall test can import check_equivalence

from tools.profilegen import emit_profile                           # noqa: E402

S1_ORACLE = os.environ.get("S3_S1_ORACLE",
                           "/WORKSPACE/.codex/worktrees/translation-core/sb-team-S1/gen/townfall-expected.json")
TOWNFALL_EBOOT = os.environ.get(
    "MODBRIDGE_TOWNFALL_EBOOT",
    "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/"
    "townfall-static-executable-2026-10-02/decrypted-eboot.bin")
TOWNFALL_PARAM = os.environ.get(
    "MODBRIDGE_TOWNFALL_PARAM",
    "/WORKSPACE/ps5-claude/outputs/2026-10-04-zcode-team/worker-game-version/param.json")


def env(value, conf="high", method="t", ev=None):
    return {"value": value, "confidence": conf, "method": method, "evidence": ev or []}


class FakeSeg:
    def __init__(self, vaddr, size, w=False, x=False):
        self.vaddr, self._size, self._w, self._x = vaddr, size, w, x

    @property
    def writable(self):
        return self._w

    @property
    def executable(self):
        return self._x

    @property
    def file_end_va(self):
        return self.vaddr + self._size

    @property
    def mem_end_va(self):
        return self.vaddr + self._size


class FakeElf:
    """Just enough of ElfImage for the emitter's prefix reads and vtable derivation (no capstone)."""

    def __init__(self, reads=None, exec_lo=0x1000, exec_hi=0x2000):
        self.reads = reads or {}
        self.loads = [FakeSeg(exec_lo, exec_hi - exec_lo, x=True)]

    def read(self, va, n):
        return self.reads.get((va, n))

    def is_mapped(self, va):
        return True

    def is_exec(self, va):
        return 0x1000 <= va < 0x2000

    def is_writable(self, va):
        return False

    @property
    def exec_segments(self):
        return self.loads

    def relative_relocs(self):
        from array import array
        return array("Q"), array("q")


def minimal_doc(**over):
    """A synthetic profile document with invented numbers (no game's values)."""
    doc = {
        "profile_id": "AAAA00000/01.000.000/deadbeef",
        "profile_digest": "a" * 64,
        "binding": {"title_id": env("AAAA00000"),
                    "executable": {"sha256": env("b" * 64), "bytes": env("0x1000")}},
        "load": {"base": env("0x111000", "medium"), "image_extent": {"vaddr_end": env("0x222000")}},
        "build": {"engine": {"major": env("0x4"), "minor": env("0x7"), "revision_string": env("4.7.2-9+++UEx")},
                  "build_id": env("cafef00d"), "configuration": env("development"), "stats": env(True),
                  "non_shipping": env(True), "allocator_family": env("FMallocSynthetic", "medium")},
        "structs": {
            "FNamePool": {"current_block": env("0x1"), "current_byte_offset": env("0x2"), "blocks": env("0x3"),
                          "none_id": env("0x100"), "byte_property_id": env("0x104"),
                          "block_offset_bits": env("0x11"), "max_blocks": env("0x4")},
            "FName": {"size": env("0x8", "medium")},
            # a complete FUObjectArray + TUObjectArray so the layout block renders (invented offsets)
            "FUObjectArray": {"ObjFirstGCIndex": env("0x0"), "ObjLastNonGCIndex": env("0x4"),
                              "MaxObjectsNotConsideredByGC": env("0x8"), "OpenForDisregardForGC": env("0xc"),
                              "ObjObjects": env("0x10"), "UObjectCreateListeners": env("0x50"),
                              "UObjectDeleteListeners": env("0x60"), "PrimarySerialNumber": env("0x88"),
                              "bShouldRecycleObjectIndices": env("0x8c"), "UEP_TotalSize": env("0x90")},
            "TUObjectArray": {"Objects": env("0x0"), "PreAllocatedObjects": env("0x8"), "MaxElements": env("0x10"),
                              "NumElements": env("0x14"), "MaxChunks": env("0x18"), "NumChunks": env("0x1c"),
                              "UEP_TotalSize": env("0x20")},
        },
        "symbols": {
            "name_pool": {"label": env("0x300000")},
            "object_array": {"label": env("0x300100")},
            "fname_ctor_wchar": {"label": env("0x1100")},
            "gmalloc": {"label": env("0x300200")},
            "gmalloc_base_slot": {"label": env("0x300300", "medium")},
            "gmalloc_instance": {"label": env("0x300400", "medium")},
            "static_construct_object_internal": {"label": env("0x1200")},
        },
        "vtables": {"UObject": {"ProcessEvent": env("0x268")}},
        "unresolved": [],
    }
    doc.update(over)
    return doc


class EmitProfileTests(unittest.TestCase):
    def _elf(self):
        # fname ctor @0x1100 (32 bytes) and sco @0x1200 (25 bytes) live in the exec segment
        return FakeElf(reads={(0x1100, 32): bytes(range(32)), (0x1200, 25): bytes(range(25))})

    def test_core_profile_is_complete_and_seeds_arm(self):
        m = emit_profile.build(minimal_doc(), self._elf())
        text = emit_profile.render(m)
        self.assertTrue(m.required_complete)
        self.assertIn("required_set_complete=1", text)
        self.assertIn("[profile]", text)
        self.assertIn("[seeds]", text)
        self.assertIn("[layout.members]", text)

    def test_gmalloc_label_is_the_base_slot_not_the_dispatch_variable(self):
        """THE headline fix: this runtime seeds the base allocator slot (READINESS B5)."""
        text = emit_profile.render(emit_profile.build(minimal_doc(), self._elf()))
        self.assertIn("gmalloc_label=0x300300", text)                 # base_slot
        self.assertNotIn("gmalloc_label=0x300200", text)              # NOT the dispatch variable

    def test_dispatch_override_is_honoured(self):
        text = emit_profile.render(emit_profile.build(minimal_doc(), self._elf(),
                                                      emit_profile.ProfileOptions(gmalloc="dispatch")))
        self.assertIn("gmalloc_label=0x300200", text)

    def test_build_configuration_and_stats_map_to_the_runtime_numbers(self):
        text = emit_profile.render(emit_profile.build(minimal_doc(), self._elf()))
        self.assertIn("build_configuration=2", text)                  # development -> 2
        self.assertIn("non_shipping=1", text)
        self.assertIn("stats=2", text)                                # True -> 2 (on)
        self.assertIn("stats_layout=1", text)

    def test_optional_features_are_off_when_their_detectors_did_not_fire(self):
        text = emit_profile.render(emit_profile.build(minimal_doc(), self._elf()))
        self.assertIn("delete_listener=0", text)
        self.assertIn("thread_name=0", text)
        self.assertIn("pak_mount=0", text)

    def test_a_missing_required_seed_turns_required_set_complete_off_fail_closed(self):
        doc = minimal_doc()
        del doc["symbols"]["object_array"]                            # drop a required slot
        m = emit_profile.build(doc, self._elf())
        self.assertFalse(m.required_complete)
        self.assertIn("required_set_complete=0", emit_profile.render(m))

    def test_body_sha_is_the_hash_of_the_file_without_that_line(self):
        text = emit_profile.render(emit_profile.build(minimal_doc(), self._elf()))
        lines = text.splitlines()
        body = next(l for l in lines if l.startswith("body_sha256="))
        without = [l for l in lines if l != body]
        recomputed = hashlib.sha256(("\n".join(without) + "\n").encode()).hexdigest()
        self.assertEqual(body.split("=", 1)[1], recomputed)

    def test_missing_image_hash_is_refused_not_guessed(self):
        doc = minimal_doc()
        doc["binding"]["executable"]["sha256"] = env("not-a-sha")
        with self.assertRaises(emit_profile.ProfileRefused):
            emit_profile.build(doc, self._elf())

    def test_a_verification_marker_in_the_profile_is_refused(self):
        doc = minimal_doc()
        doc["build"]["configuration"] = env("development", ev=["console-verified by hand"])
        with self.assertRaises(emit_profile.ProfileRefused):
            emit_profile.build(doc, self._elf())

    def test_exe_path_is_emitted_only_when_supplied(self):
        no = emit_profile.render(emit_profile.build(minimal_doc(), self._elf()))
        self.assertNotIn("exe_path=", no)
        yes = emit_profile.render(emit_profile.build(minimal_doc(), self._elf(),
                                                     emit_profile.ProfileOptions(exe_path="/data/x/eboot.bin")))
        self.assertIn("exe_path=/data/x/eboot.bin", yes)


@unittest.skipUnless(os.path.isfile(TOWNFALL_EBOOT) and os.path.isfile(S1_ORACLE),
                     "Townfall executable or S1 oracle not available")
class TownfallTwoSourceTest(unittest.TestCase):
    def test_every_runtime_used_field_agrees_with_the_s1_compiled_header_oracle(self):
        from tools.profilegen import cli
        import tempfile
        import check_equivalence
        with tempfile.TemporaryDirectory() as d:
            res = cli.generate(TOWNFALL_EBOOT, TOWNFALL_PARAM, d, utc="2026-10-13T00:00:00Z",
                               profile_ini=os.path.join(d, "profile.ini"))
            self.assertIsNotNone(res.profile)
            self.assertTrue(res.profile["required_set_complete"])
            rc = check_equivalence.main(["--ini", os.path.join(d, "profile.ini"), "--oracle", S1_ORACLE, "--quiet"])
            self.assertEqual(rc, 0, "two-source check reported an unexpected mismatch")
            # the headline fix, proven on the real image:
            text = open(os.path.join(d, "profile.ini")).read()
            oracle = json.load(open(S1_ORACLE))
            self.assertIn("gmalloc_label=%s" % hex(oracle["gmalloc_label"]), text)
            self.assertIn("gmalloc_vtable_label=%s" % hex(oracle["gmalloc_vtable_label"]), text)


if __name__ == "__main__":
    loader = unittest.TestLoader()
    suite = unittest.TestSuite()
    for cls in (EmitProfileTests, TownfallTwoSourceTest):
        suite.addTests(loader.loadTestsFromTestCase(cls))
    n = suite.countTestCases()
    if n == 0:
        print("emit_profile_tests: FAIL: zero tests collected", file=sys.stderr)
        sys.exit(1)
    print("emit_profile_tests: %d tests" % n)
    result = unittest.TextTestRunner(verbosity=2).run(suite)
    sys.exit(0 if result.wasSuccessful() else 1)
