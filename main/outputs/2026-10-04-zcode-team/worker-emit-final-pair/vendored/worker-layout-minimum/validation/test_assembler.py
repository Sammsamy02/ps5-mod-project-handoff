#!/usr/bin/env python3
"""Host tests: synthetic images prove the assembler refuses unvalidated entries.

Two layers, deliberately separate:
  Part 1 (synthetic-image): runs min_predicates over constructed mapped images
    and feeds the RESULTING verdict records to the assembler — no hand-written
    verdicts. This proves the end-to-end gate refuses candidates whose
    predicates did not pass.
  Part 2 (assembler unit): hand-crafted inputs exercising grammatical gate
    rules (identity completeness, width gating, size fits). Fixtures only.

Run: python3 -m unittest discover -s validation -p 'test_*.py'  (from this dir)
"""
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import min_predicates as mp          # noqa: E402
import assemble_staged_pair as asm   # noqa: E402

CANDIDATES = os.path.join(HERE, "..", "candidates", "layout-candidates.profile.json")
GOOD_IDENTITY = {"engine_revision": "scan-fact-revision", "build_id": "scan-fact-build",
                 "non_shipping": True, "stats": False}


def run_predicates(spans, image_base, goa_vaddr):
    gua = mp.probe_guobject_array(spans, image_base, goa_vaddr)
    uob = mp.probe_uobject_item_chain(spans, gua)
    return [gua, uob]


def assemble(verdicts, identity=GOOD_IDENTITY, scope=None):
    with open(CANDIDATES) as fh:
        candidates = json.load(fh)
    return asm.assemble(candidates, verdicts, identity, scope or {})


class SyntheticImageTests(unittest.TestCase):
    """Part 1: end-to-end synthetic-image gate."""

    def test_valid_hypothesis_a_emits_pair(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="A")
        verdicts = run_predicates(spans, base, vaddr)
        self.assertEqual(verdicts[0]["verdict"], "Validated")
        self.assertEqual(verdicts[0]["layout_matched"], "A")
        self.assertEqual(verdicts[1]["verdict"], "Validated")
        members, vtables, manifest = assemble(verdicts)
        self.assertIn("[NativeLayout]", members)
        self.assertIn("Format=1", members)
        self.assertIn("Target=prospero-x86_64-sysv", members)
        self.assertIn("[TUObjectArray]", members)
        self.assertIn("Objects=0x0", members)
        self.assertIn("NumElements=0x14", members)
        self.assertIn("UEP_TotalSize=0x20", members)
        self.assertIn("[FUObjectItem]", members)
        self.assertIn("[UObjectBase]", members)
        self.assertIn("ClassPrivate=0x10", members)
        self.assertEqual(members.count("[NativeLayout]"), 1)
        # vtable file is identity-only by design (exactly the 10 identity lines)
        identity_lines = members.splitlines()[:10]
        self.assertEqual(vtables, "\n".join(identity_lines) + "\n")
        self.assertEqual(manifest["hypothesis"], "A")
        self.assertTrue(any(e["class"] == "FUObjectArray" for e in manifest["emitted"]))

    def test_valid_hypothesis_b_emits_without_fuobjectarray(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="B")
        verdicts = run_predicates(spans, base, vaddr)
        self.assertEqual(verdicts[0]["verdict"], "Validated")
        self.assertEqual(verdicts[0]["layout_matched"], "B")
        members, _, manifest = assemble(verdicts)
        self.assertIn("[TUObjectArray]", members)
        self.assertNotIn("[FUObjectArray]", members)
        self.assertEqual(manifest["hypothesis"], "B")

    def test_refuted_counts_keep_everything_candidate(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="A", corrupt="counts")
        verdicts = run_predicates(spans, base, vaddr)
        self.assertEqual(verdicts[0]["verdict"], "Refuted")
        with self.assertRaises(asm.Refusal) as ctx:
            assemble(verdicts)
        self.assertIn("verdict is 'Refuted'", str(ctx.exception))

    def test_refuted_gc_order(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="A", corrupt="gc_order")
        verdicts = run_predicates(spans, base, vaddr)
        self.assertEqual(verdicts[0]["verdict"], "Refuted")
        self.assertIn("GOA_R_GC_ORDER", verdicts[0]["layout_a"]["codes"])
        with self.assertRaises(asm.Refusal):
            assemble(verdicts)

    def test_unreadable_header_is_inconclusive_and_refused(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="A", corrupt="no_header")
        verdicts = run_predicates(spans, base, vaddr)
        self.assertEqual(verdicts[0]["verdict"], "Inconclusive")
        self.assertIn("GOA_E_ADDR_UNREADABLE", verdicts[0]["codes"])
        with self.assertRaises(asm.Refusal):
            assemble(verdicts)

    def test_chunk_pointer_outside_spans_is_refuted(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="A", corrupt="no_chunks")
        verdicts = run_predicates(spans, base, vaddr)
        self.assertEqual(verdicts[0]["verdict"], "Refuted")
        self.assertIn("GOA_R_CHUNK_UNREADABLE", verdicts[0]["codes"])
        with self.assertRaises(asm.Refusal):
            assemble(verdicts)

    def test_null_objects_refuted(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="A", corrupt="null_objects")
        verdicts = run_predicates(spans, base, vaddr)
        codes = verdicts[0]["layout_a"]["codes"] + verdicts[0]["layout_b"]["codes"]
        self.assertIn("GOA_R_PTRS_BOTH_NULL", codes)
        with self.assertRaises(asm.Refusal):
            assemble(verdicts)

    def test_ambiguous_layout_refused(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="ambiguous")
        verdicts = run_predicates(spans, base, vaddr)
        self.assertEqual(verdicts[0]["verdict"], "Inconclusive")
        self.assertIn("GOA_E_LAYOUT_AMBIGUOUS", verdicts[0]["codes"])
        with self.assertRaises(asm.Refusal):
            assemble(verdicts)

    def test_no_verdict_records_at_all_refused(self):
        with self.assertRaises(asm.Refusal) as ctx:
            assemble([])
        self.assertIn("has not run", str(ctx.exception))


class AssemblerGateUnitTests(unittest.TestCase):
    """Part 2: grammatical gate rules (fixtures, not synthetic images)."""

    def _valid_verdicts(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="A")
        return run_predicates(spans, base, vaddr)

    def test_incomplete_identity_refused(self):
        for missing in ("engine_revision", "build_id", "non_shipping", "stats"):
            identity = dict(GOOD_IDENTITY)
            identity[missing] = None
            with self.assertRaises(asm.Refusal) as ctx:
                assemble(self._valid_verdicts(), identity=identity)
            self.assertIn(missing, str(ctx.exception))

    def test_nonprintable_identity_refused(self):
        identity = dict(GOOD_IDENTITY, engine_revision="bad\nrevision")
        with self.assertRaises(asm.Refusal):
            assemble(self._valid_verdicts(), identity=identity)

    def test_rung3_classes_refused_without_predicates(self):
        verdicts = self._valid_verdicts()
        with self.assertRaises(asm.Refusal) as ctx:
            assemble(verdicts, scope={"classes": ["FProperty", "UClass", "UFunction"]})
        self.assertIn("no class passed the gate", str(ctx.exception))

    def test_mixed_scope_emits_valid_and_refuses_predicateless(self):
        verdicts = self._valid_verdicts()
        members, _, manifest = assemble(
            verdicts, scope={"classes": ["TUObjectArray", "UFunction"]})
        self.assertIn("[TUObjectArray]", members)
        self.assertNotIn("[UFunction]", members)
        refused = [r for r in manifest["refused"] if r["class"] == "UFunction"]
        self.assertTrue(refused and "no predicate" in refused[0]["reason"])

    def test_null_width_members_are_skipped_and_recorded(self):
        verdicts = self._valid_verdicts()
        with open(CANDIDATES) as fh:
            candidates = json.load(fh)
        # Inject a hypothesis-B rung-3 class into scope with fabricated
        # 'Validated' records is NOT allowed by the gate; instead verify via a
        # temporary candidate variant that null-width members never reach INI:
        candidates["candidate_member_offsets"]["TUObjectArray"]["entries"]["FakeNoWidth"] = \
            {"offset": 4, "width_bytes": None, "reason": "fixture: no width"}
        members, _, manifest = asm.assemble(candidates, verdicts, GOOD_IDENTITY, {})
        self.assertNotIn("FakeNoWidth=", members)
        skipped = [s for s in manifest["skipped_no_width"] if s["member"] == "FakeNoWidth"]
        self.assertTrue(skipped)

    def test_member_outside_total_size_refused(self):
        verdicts = self._valid_verdicts()
        with open(CANDIDATES) as fh:
            candidates = json.load(fh)
        candidates["candidate_member_offsets"]["FUObjectItem"]["entries"]["Object"] = \
            {"offset": 0x18, "width_bytes": 8}  # offset == total -> must refuse
        with self.assertRaises(asm.Refusal) as ctx:
            asm.assemble(candidates, verdicts, GOOD_IDENTITY, {})
        self.assertIn("outside declared class size", str(ctx.exception))

    def test_duplicate_verdict_records_refused(self):
        verdicts = self._valid_verdicts()
        with self.assertRaises(asm.Refusal):
            assemble(verdicts + [verdicts[0]])

    def test_cli_refusal_writes_nothing(self):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "out")
            vpath = os.path.join(tmp, "v.json")
            ipath = os.path.join(tmp, "i.json")
            with open(vpath, "w") as fh:
                json.dump([], fh)
            with open(ipath, "w") as fh:
                json.dump(GOOD_IDENTITY, fh)
            rc = asm.main(["--candidates", CANDIDATES, "--verdicts", vpath,
                           "--identity", ipath, "--out-dir", out])
            self.assertEqual(rc, asm.REFUSAL_EXIT)
            self.assertFalse(os.path.exists(out))

    def test_cli_end_to_end_emit(self):
        spans, base, vaddr = mp.make_synthetic_image(hypothesis="A")
        verdicts = run_predicates(spans, base, vaddr)
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "out")
            vpath = os.path.join(tmp, "v.json")
            ipath = os.path.join(tmp, "i.json")
            with open(vpath, "w") as fh:
                json.dump(verdicts, fh)
            with open(ipath, "w") as fh:
                json.dump(GOOD_IDENTITY, fh)
            rc = asm.main(["--candidates", CANDIDATES, "--verdicts", vpath,
                           "--identity", ipath, "--out-dir", out])
            self.assertEqual(rc, 0)
            with open(os.path.join(out, "MemberVariableLayout.ini")) as fh:
                text = fh.read()
            self.assertTrue(text.startswith("[NativeLayout]"))
            self.assertIn("EngineRevision=scan-fact-revision", text)
            with open(os.path.join(out, "assembly-manifest.json")) as fh:
                manifest = json.load(fh)
            self.assertTrue(manifest["emitted"])


if __name__ == "__main__":
    unittest.main()
