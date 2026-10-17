#!/usr/bin/env python3
"""test_bridge.py — host tests for the batch-3 assembler bridge.

Covers: width-contract loading (widths-1 + widths-2 delta), the candidates
transcription cross-check (SuperStruct et al.), the promotable-row decisions,
and the v2 assembler's gate behavior (variant gate, SuperStruct refusal,
UFunction refusal, missing-predicate refusals, emitted-member expectations).
Run: python3 -m unittest discover -s bridge -p 'test_*.py'  (from the worker
directory) or directly: python3 bridge/test_bridge.py
"""
from __future__ import annotations

import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
TEAM = os.path.join(ROOT, "outputs", "2026-10-04-zcode-team")
W1 = os.path.join(ROOT, ".superpowers", "native-dependency-spike",
                  "ue4ss-native-layout-widths-1", "coverage.json")
DELTA = os.path.join(ROOT, ".superpowers", "native-dependency-spike",
                     "ue4ss-native-layout-widths-2", "coverage-delta.json")
CANDIDATES = os.path.join(TEAM, "worker-layout-minimum", "candidates",
                          "layout-candidates.profile.json")

sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(TEAM, "worker-layout-minimum", "validation"))

import width_contract as wc  # noqa: E402
import assemble_staged_pair as base  # noqa: E402
import assemble_staged_pair_v2 as v2  # noqa: E402


def load(path):
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def full_verdicts():
    """Synthetic VALIDATED verdict records in the exact worker-5 vocabulary,
    one per retained predicate (host-test facts, not target facts)."""
    return [
        {"probe": "guobject_array", "verdict": "Validated",
         "layout_matched": "A", "codes": []},
        {"probe": "uobject_item_chain", "verdict": "Validated", "codes": []},
        {"probe": "ustruct_chain", "verdict": "Validated", "codes": []},
        {"probe": "ffield_variant", "verdict": "Validated",
         "variant": "UE5_FField", "codes": []},
    ]


def identity_ok():
    return {"engine_revision": "SYNTHETIC-DRYRUN-REV", "build_id":
            "SYNTHETIC-DRYRUN-BUILD", "non_shipping": 0, "stats": 0}


class WidthContractTests(unittest.TestCase):
    def setUp(self):
        self.contract = wc.load_contract(load(W1), load(DELTA))

    def test_delta_added_func(self):
        self.assertEqual(self.contract["UFunction"]["Func"], 16)

    def test_superstruct_stays_null(self):
        # widths-2 explicitly retained the SuperStruct refusal.
        self.assertIsNone(self.contract["UStruct"].get("SuperStruct"))

    def test_fname_absent(self):
        self.assertNotIn("FName", self.contract)

    def test_known_widths(self):
        self.assertEqual(self.contract["UObjectBase"]["NamePrivate"], 8)
        self.assertEqual(self.contract["UStruct"]["ChildProperties"], 8)
        self.assertEqual(self.contract["FProperty"]["PropertyFlags"], None)
        self.assertEqual(self.contract["UFunction"]["FunctionFlags"], None)
        self.assertEqual(self.contract["FField"]["Owner"], None)
        self.assertEqual(self.contract["UClass"]["FuncMap"], 80)

    def test_cross_check_finds_transcription_errors(self):
        rows = wc.cross_check_candidates(load(CANDIDATES), self.contract)
        by_key = {(r["class"], r["member"]): r for r in rows}
        # Candidates profile claims a width where the contract has none.
        for cls, member in (("UStruct", "SuperStruct"),
                            ("UClass", "ClassFlags"),
                            ("UClass", "ClassCastFlags"),
                            ("UClass", "ReferenceSchema")):
            self.assertIn((cls, member), by_key, (cls, member))
            self.assertEqual(by_key[(cls, member)]["kind"], "overstated")
        # Both exist but disagree: contract wins for staging/use.
        for cls, member in (("UClass", "ClassUnique"),
                            ("UClass", "FuncMap"),
                            ("UClass", "Interfaces"),
                            ("UClass", "NativeFunctionLookupTable"),
                            ("UClass", "NetFields")):
            self.assertEqual(by_key[(cls, member)]["kind"], "width_diff")

    def test_promotable_rows_decisions(self):
        rows = wc.promotable_rows(load(CANDIDATES), self.contract)
        dec = {(r["class"], r["member"]): r["decision"] for r in rows}
        self.assertEqual(dec[("UStruct", "SuperStruct")], "BLOCKED_NO_WIDTH")
        self.assertEqual(dec[("FField", "Owner")], "BLOCKED_NO_WIDTH")
        self.assertEqual(dec[("FField", "ClassPrivate")], "STAGEABLE_TODAY")
        self.assertEqual(dec[("FProperty", "PropertyFlags")],
                         "BLOCKED_NO_WIDTH")
        self.assertEqual(dec[("FProperty", "Offset_Internal")],
                         "STAGEABLE_TODAY")
        self.assertEqual(dec[("UFunction", "FunctionFlags")],
                         "BLOCKED_NO_PREDICATE_AND_WIDTH")
        self.assertEqual(dec[("UFunction", "Func")], "BLOCKED_NO_PREDICATE")
        self.assertEqual(dec[("UClass", "ClassDefaultObject")],
                         "STAGEABLE_TODAY")
        summary = wc.summarize(rows)
        self.assertEqual(summary.get("BLOCKED_NO_PREDICATE"), 9)
        self.assertEqual(summary.get("BLOCKED_NO_PREDICATE_AND_WIDTH"), 1)


class AssemblerV2Tests(unittest.TestCase):
    def setUp(self):
        self.candidates = load(CANDIDATES)
        self.contract = wc.load_contract(load(W1), load(DELTA))

    def _assemble(self, verdicts=None, scope=None, identity=None):
        return v2.assemble_v2(self.candidates,
                              verdicts if verdicts is not None
                              else full_verdicts(),
                              identity if identity is not None
                              else identity_ok(),
                              scope or {}, self.contract)

    def test_full_scope_maximal_emission(self):
        members, vtables, manifest = self._assemble()
        classes = [e["class"] for e in manifest["emitted"]]
        self.assertEqual(classes, ["FUObjectArray", "TUObjectArray",
                                   "FUObjectItem", "UObjectBase", "UStruct",
                                   "UClass", "FField", "FProperty"])
        refused = {r["class"] for r in manifest["refused"]}
        self.assertEqual(refused, {"UFunction"})
        by_cls = {e["class"]: e for e in manifest["emitted"]}
        # Contract-gated staging: SuperStruct/ClassFlags/ClassCastFlags/
        # ReferenceSchema skipped; FuncMap emitted despite the width_diff.
        self.assertNotIn("SuperStruct", by_cls["UStruct"]["members"])
        self.assertIn("ChildProperties", by_cls["UStruct"]["members"])
        self.assertNotIn("ClassFlags", by_cls["UClass"]["members"])
        self.assertNotIn("ClassCastFlags", by_cls["UClass"]["members"])
        self.assertNotIn("ReferenceSchema", by_cls["UClass"]["members"])
        self.assertIn("FuncMap", by_cls["UClass"]["members"])
        self.assertIn("ClassDefaultObject", by_cls["UClass"]["members"])
        self.assertEqual(by_cls["FField"]["members"],
                         ["ClassPrivate", "FlagsPrivate", "UEP_TotalSize"])
        self.assertNotIn("PropertyFlags",
                         by_cls["FProperty"]["members"])
        self.assertIn("RepNotifyFunc", by_cls["FProperty"]["members"])
        self.assertNotIn("ObjAvailableList", by_cls["FUObjectArray"]["members"])
        # The pair grammar: identity header + sections with Member=0xHEX.
        self.assertTrue(members.startswith("[NativeLayout]\nFormat=1\n"))
        self.assertIn("[UStruct]\nChildren=0x48\n", members)
        self.assertIn("ChildProperties=0x50\n", members)
        self.assertIn("[FField]\nClassPrivate=0x8\n", members)
        self.assertTrue(vtables.startswith("[NativeLayout]\n"))
        # Mismatch transparency.
        mm = {(r["class"], r["member"]) for r in
              manifest["width_contract_mismatches"]}
        self.assertIn(("UStruct", "SuperStruct"), mm)
        self.assertIn(("UClass", "FuncMap"), mm)

    def test_variant_refutes_506_table(self):
        verdicts = full_verdicts()
        for rec in verdicts:
            if rec["probe"] == "ffield_variant":
                rec["variant"] = "UE4_UProperty"
        members, _, manifest = self._assemble(verdicts=verdicts)
        emitted = {e["class"] for e in manifest["emitted"]}
        self.assertNotIn("FField", emitted)
        self.assertNotIn("FProperty", emitted)
        self.assertIn("UStruct", emitted)  # variant-independent
        reasons = " | ".join(r["reason"] for r in manifest["refused"])
        self.assertIn("UE4_UProperty", reasons)

    def test_variant_unsettled_refuses_fields(self):
        verdicts = full_verdicts()
        for rec in verdicts:
            if rec["probe"] == "ffield_variant":
                rec["verdict"] = "Inconclusive"
                rec["codes"] = ["FLD_E_AMBIGUOUS"]
        _, _, manifest = self._assemble(verdicts=verdicts)
        emitted = {e["class"] for e in manifest["emitted"]}
        self.assertNotIn("FField", emitted)
        self.assertNotIn("FProperty", emitted)

    def test_missing_rung3_predicate_refuses(self):
        verdicts = [r for r in full_verdicts()
                    if r["probe"] != "ustruct_chain"]
        _, _, manifest = self._assemble(verdicts=verdicts)
        emitted = {e["class"] for e in manifest["emitted"]}
        self.assertEqual(emitted, {"FUObjectArray", "TUObjectArray",
                                   "FUObjectItem"})

    def test_no_gua_verdict_refuses_everything(self):
        with self.assertRaises(base.Refusal):
            self._assemble(verdicts=[])

    def test_identity_incomplete_refuses(self):
        with self.assertRaises(base.Refusal):
            self._assemble(identity={"engine_revision": None,
                                     "build_id": "X", "non_shipping": 0,
                                     "stats": 0})

    def test_strengthening_verdicts_recorded_not_gating(self):
        verdicts = full_verdicts() + [
            {"probe": "childproperties_walk", "verdict": "Validated",
             "codes": ["CP_I_CHAIN_OK"]},
            {"probe": "uclass_links", "verdict": "Inconclusive",
             "codes": ["UC_I_WALK_TRUNCATED"]},
        ]
        _, _, manifest = self._assemble(verdicts=verdicts)
        sv = manifest["strengthening_verdicts"]
        self.assertEqual(sv["childproperties_walk"]["verdict"], "Validated")
        self.assertEqual(sv["uclass_links"]["verdict"], "Inconclusive")
        # UClass stageability comes from ustruct_chain, not uclass_links.
        emitted = {e["class"] for e in manifest["emitted"]}
        self.assertIn("UClass", emitted)

    def test_inconclusive_gua_refuses_everything(self):
        verdicts = [{"probe": "guobject_array", "verdict": "Inconclusive",
                     "codes": ["GOA_E_AMBIGUOUS_LAYOUT"]}]
        with self.assertRaises(base.Refusal):
            self._assemble(verdicts=verdicts)


if __name__ == "__main__":
    unittest.main(verbosity=2)
