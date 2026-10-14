#!/usr/bin/env python3
"""Small bounded test suite for the generic build/profile validator.

Covers: the real schema + real Townfall profile validate cleanly; every guard
rule rejects a targeted mutation; generic build identification recognizes a
different (synthetic) Unreal build without any Townfall-specific values in
the tool, and refuses ambiguity/fallback.
"""
from __future__ import annotations

import copy
import importlib.util
import json
import sys
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SCHEMA_PATH = ROOT / "schema" / "unreal-build-profile.schema.json"
PROFILE_PATH = ROOT / "profiles" / "townfall-ps5-ue56.profile.json"

spec = importlib.util.spec_from_file_location("validate_profile", ROOT / "tools" / "validate_profile.py")
vp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(vp)


def load(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def findings_for(profile: dict, schema: dict) -> str:
    return vp.validate_profile(profile, schema).report()


def mutation(test_case: unittest.TestCase, base: dict, *path, value):
    """Return a deep copy of base with nested key path replaced by value."""
    working = copy.deepcopy(base)
    node = working
    for key in path[:-1]:
        node = node[key]
    node[path[-1]] = value
    return working


def synthetic_other_build_profile() -> dict:
    """A complete, valid profile for a DIFFERENT build (UE 4.27 title)."""
    return {
        "profile_format": 1,
        "profile_kind": "unreal-build-layout",
        "schema_id": "unreal-build-profile",
        "profile_id": "other-title-ps5-ue427-static",
        "title": "Other Title",
        "identity": {
            "target": "prospero-x86_64-sysv",
            "pointer_size": 8,
            "engine_major": 4,
            "engine_minor": 27,
            "engine_patch": None,
            "engine_revision": None,
            "build_id": None,
            "build_id_source": {
                "kind": "saved-capture-sha256",
                "value": "0" * 64,
                "artifact": "E-capture",
                "note": "synthetic fixture"
            },
            "build_axes": {"non_shipping": None, "stats": None},
            "build_axes_status": "unresolved-no-evidence",
            "pointer_size_origin": {"artifact": "E-capture", "detail": "synthetic", "provenance_class": "game-static-survey"},
            "engine_version_origin": {"artifact": "E-capture", "detail": "synthetic", "provenance_class": "game-static-survey"}
        },
        "evidence_sources": [
            {"id": "E-capture", "role": "capture-record", "path": "/synthetic/none.json",
             "sha256": "0" * 64, "detail": "synthetic"}
        ],
        "static_survey_facts": {
            "EngineVersion": {
                "family_status": "validated",
                "facts": {
                    "engine_version": {
                        "status": "validated-static",
                        "endpoint_status_code": 0,
                        "producer": "ps_engine_version_file_image",
                        "value": {"major": 4, "minor": 27},
                        "origin": {"artifact": "E-capture", "line": "synthetic"},
                        "provenance_class": "game-static-survey",
                        "confidence": "single-endpoint-static"
                    }
                }
            },
            "FName": {
                "family_status": "not-surveyed",
                "facts": {
                    "fname_to_string": {
                        "status": "not-surveyed",
                        "producer": None,
                        "value": None,
                        "origin": {"artifact": "E-capture", "note": "synthetic"},
                        "provenance_class": "game-static-survey",
                        "confidence": "none"
                    }
                }
            }
        },
        "compiled_port_widths": {
            "scope_statement": "synthetic port contract; not game ABI proof",
            "coverage": {"canonical_entries_total": 0, "available": 0, "unavailable": 0, "delta_source": "E-capture"},
            "classes": {}
        },
        "unresolved_list": ["synthetic"],
        "readiness": {"static_layout_ready": False, "runtime_binding_ready": False}
    }


class RealArtifactsTestCase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.schema = load(SCHEMA_PATH)
        cls.profile = load(PROFILE_PATH)

    def test_real_townfall_profile_is_valid(self):
        self.assertEqual(vp.validate_profile(self.profile, self.schema).report(), "")

    def test_cli_accepts_real_artifacts(self):
        rc = vp.main(["--schema", str(SCHEMA_PATH), "--profile", str(PROFILE_PATH)])
        self.assertEqual(rc, 0)


class ProvenanceGuards(unittest.TestCase):
    def setUp(self):
        self.schema = load(SCHEMA_PATH)
        self.profile = load(PROFILE_PATH)

    def test_pdb_default_provenance_rejected_on_fact(self):
        bad = mutation(self, self.profile, "static_survey_facts", "GUObjectArray", "facts",
                       "guobject_original", "provenance_class", value="pdb-default")
        self.assertIn("forbidden provenance_class 'pdb-default'", findings_for(bad, self.schema))

    def test_other_game_build_provenance_rejected_deep(self):
        bad = copy.deepcopy(self.profile)
        bad["compiled_port_widths"]["classes"]["UObjectBase"]["origin"]["provenance_class"] = "other-game-build"
        self.assertIn("forbidden provenance_class 'other-game-build'", findings_for(bad, self.schema))

    def test_width_provenance_outside_allowed_set_rejected(self):
        bad = mutation(self, self.profile, "compiled_port_widths", "classes", "UObjectBase",
                       "provenance_class", value="assumed")
        self.assertIn("provenance_class 'assumed' not allowed for width facts", findings_for(bad, self.schema))

    def test_numeric_fact_without_origin_rejected(self):
        bad = copy.deepcopy(self.profile)
        del bad["static_survey_facts"]["GUObjectArray"]["facts"]["guobject_original"]["origin"]
        self.assertIn("numeric facts require an origin", findings_for(bad, self.schema))

    def test_origin_referencing_unknown_artifact_rejected(self):
        bad = mutation(self, self.profile, "static_survey_facts", "GameEngineTick", "facts",
                       "gameengine_tick", "origin", "artifact", value="E-does-not-exist")
        self.assertIn("'E-does-not-exist' is not a declared evidence source", findings_for(bad, self.schema))


class AddressKindGuards(unittest.TestCase):
    def setUp(self):
        self.schema = load(SCHEMA_PATH)
        self.profile = load(PROFILE_PATH)

    def test_vaddr_copied_into_mapped_runtime_field_rejected(self):
        bad = copy.deepcopy(self.profile)
        address = bad["static_survey_facts"]["GUObjectArray"]["facts"]["guobject_original"]["value"]["address"]
        address["mapped_runtime_address"] = address["elf_image_vaddr"]
        self.assertIn("without game-runtime evidence", findings_for(bad, self.schema))

    def test_vaddr_copied_into_file_byte_offset_rejected(self):
        bad = copy.deepcopy(self.profile)
        address = bad["static_survey_facts"]["UObject"]["facts"]["static_construct_object"]["value"]["address"]
        address["file_byte_offset"] = address["elf_image_vaddr"]
        self.assertIn("copies elf_image_vaddr", findings_for(bad, self.schema))

    def test_file_byte_offset_without_derivation_rejected(self):
        bad = copy.deepcopy(self.profile)
        bad["static_survey_facts"]["UObject"]["facts"]["static_construct_object"]["value"]["address"]["file_byte_offset"] = "0x10"
        self.assertIn("requires a recorded program-header derivation", findings_for(bad, self.schema))

    def test_failure_sentinel_zero_address_rejected(self):
        bad = copy.deepcopy(self.profile)
        bad["static_survey_facts"]["UObject"]["facts"]["static_construct_object"]["value"]["address"]["elf_image_vaddr"] = "0x0"
        self.assertIn("failure sentinel", findings_for(bad, self.schema))

    def test_runtime_validated_flag_requires_game_runtime_provenance(self):
        bad = copy.deepcopy(self.profile)
        fact = bad["static_survey_facts"]["GUObjectArray"]["facts"]["guobject_original"]
        fact["runtime_validation"] = {"validated": True}
        self.assertIn("requires provenance_class 'game-runtime'", findings_for(bad, self.schema))

    def test_non_hex_address_rejected(self):
        bad = copy.deepcopy(self.profile)
        bad["static_survey_facts"]["GMalloc"]["facts"]["gmalloc_deletefile_candidate"]["value"]["address"]["elf_image_vaddr"] = 96095568
        self.assertIn("must be a hex string", findings_for(bad, self.schema))


class StatusAndFamilyGuards(unittest.TestCase):
    def setUp(self):
        self.schema = load(SCHEMA_PATH)
        self.profile = load(PROFILE_PATH)

    def test_absent_fact_with_value_rejected(self):
        bad = copy.deepcopy(self.profile)
        fact = bad["static_survey_facts"]["FName"]["facts"]["fname_to_string"]
        fact["value"] = {"address": {"elf_image_vaddr": "0x1", "file_byte_offset": None,
                                     "mapped_runtime_address": None, "load_base": None}}
        self.assertIn("status 'absent' must not carry a value", findings_for(bad, self.schema))

    def test_family_status_promotion_rejected(self):
        bad = mutation(self, self.profile, "static_survey_facts", "FName", "family_status", value="validated")
        self.assertIn("facts aggregate to 'candidate-only'", findings_for(bad, self.schema))

    def test_demoted_family_status_rejected(self):
        bad = mutation(self, self.profile, "static_survey_facts", "GUObjectArray", "family_status", value="candidate-only")
        self.assertIn("facts aggregate to 'validated'", findings_for(bad, self.schema))

    def test_unknown_family_rejected(self):
        bad = copy.deepcopy(self.profile)
        bad["static_survey_facts"]["MysteryFamily"] = copy.deepcopy(bad["static_survey_facts"]["GMalloc"])
        self.assertIn("unknown family 'MysteryFamily'", findings_for(bad, self.schema))

    def test_unknown_fact_name_rejected(self):
        bad = copy.deepcopy(self.profile)
        facts = bad["static_survey_facts"]["GUObjectArray"]["facts"]
        facts["guobject_magic"] = copy.deepcopy(facts["guobject_original"])
        self.assertIn("not in the schema vocabulary", findings_for(bad, self.schema))

    def test_engine_version_fact_must_match_identity(self):
        bad = mutation(self, self.profile, "identity", "engine_minor", value=5)
        self.assertIn("disagrees with identity.engine_major/engine_minor", findings_for(bad, self.schema))

    def test_candidate_status_with_no_value_rejected(self):
        bad = copy.deepcopy(self.profile)
        bad["static_survey_facts"]["FName"]["facts"]["fname_ctor_call_candidate"]["value"] = None
        self.assertIn("status 'candidate-only' requires a value", findings_for(bad, self.schema))


class WidthGuards(unittest.TestCase):
    def setUp(self):
        self.schema = load(SCHEMA_PATH)
        self.profile = load(PROFILE_PATH)

    def test_unknown_width_rejected(self):
        bad = copy.deepcopy(self.profile)
        bad["compiled_port_widths"]["classes"]["UObjectBase"]["entries"][0]["width_bytes"] = 3
        self.assertIn("unknown width 3", findings_for(bad, self.schema))

    def test_width_sixteen_without_alternatives_rejected(self):
        bad = copy.deepcopy(self.profile)
        entry = bad["compiled_port_widths"]["classes"]["UFunction"]["entries"]
        for item in entry:
            if item["canonical_key"] == "Func":
                del item["alternatives_compiled_bytes"]
        self.assertIn("width 16 requires alternatives_compiled_bytes", findings_for(bad, self.schema))

    def test_zero_width_for_unavailable_rejected(self):
        bad = copy.deepcopy(self.profile)
        for item in bad["compiled_port_widths"]["classes"]["FProperty"]["entries"]:
            if item["canonical_key"] == "PropertyFlags":
                item["width_bytes"] = 0
                item["reason"] = "filled with a zero default"
        self.assertIn("width 0 is never valid", findings_for(bad, self.schema))

    def test_unavailable_width_without_reason_rejected(self):
        bad = copy.deepcopy(self.profile)
        for item in bad["compiled_port_widths"]["classes"]["FProperty"]["entries"]:
            if item["canonical_key"] == "PropertyFlags":
                del item["reason"]
        self.assertIn("unavailable width requires a non-empty reason", findings_for(bad, self.schema))

    def test_declared_availability_counts_must_match_entries(self):
        bad = mutation(self, self.profile, "compiled_port_widths", "classes", "UObjectBase", "available", value=4)
        self.assertIn("declared available=4 but entries contain 5", findings_for(bad, self.schema))

    def test_missing_scope_statement_rejected(self):
        bad = copy.deepcopy(self.profile)
        del bad["compiled_port_widths"]["scope_statement"]
        self.assertIn("missing scope_statement", findings_for(bad, self.schema))


class IdentityAndReadinessGuards(unittest.TestCase):
    def setUp(self):
        self.schema = load(SCHEMA_PATH)
        self.profile = load(PROFILE_PATH)

    def test_unknown_target_rejected(self):
        bad = mutation(self, self.profile, "identity", "target", value="win64-msvc")
        self.assertIn("unknown target 'win64-msvc'", findings_for(bad, self.schema))

    def test_pointer_size_disagreement_rejected(self):
        bad = mutation(self, self.profile, "identity", "pointer_size", value=4)
        self.assertIn("disagrees with target registry", findings_for(bad, self.schema))

    def test_static_layout_ready_with_unresolved_axes_rejected(self):
        bad = copy.deepcopy(self.profile)
        bad["readiness"]["static_layout_ready"] = True
        self.assertIn("static_layout_ready=true but unresolved", findings_for(bad, self.schema))

    def test_runtime_binding_ready_cannot_be_true(self):
        bad = copy.deepcopy(self.profile)
        bad["readiness"]["runtime_binding_ready"] = True
        self.assertIn("runtime_binding_ready=true is not supported", findings_for(bad, self.schema))

    def test_defaulted_build_axis_rejected(self):
        bad = copy.deepcopy(self.profile)
        bad["identity"]["build_axes"]["non_shipping"] = "unknown-default"
        self.assertIn("must be a boolean or null", findings_for(bad, self.schema))


class GenericBuildIdentification(unittest.TestCase):
    def setUp(self):
        self.schema = load(SCHEMA_PATH)
        self.townfall = load(PROFILE_PATH)
        self.other = synthetic_other_build_profile()

    def test_other_build_profile_is_valid(self):
        self.assertEqual(vp.validate_profile(self.other, self.schema).report(), "")

    def test_identifies_other_build_by_axes_without_title(self):
        matches, error = vp.identify_build([self.townfall, self.other],
                                           {"target": "prospero-x86_64-sysv",
                                            "pointer_size": 8,
                                            "engine_major": 4, "engine_minor": 27})
        self.assertIsNone(error)
        self.assertEqual(matches, ["other-title-ps5-ue427-static"])

    def test_identifies_townfall_build_by_axes(self):
        matches, error = vp.identify_build([self.townfall, self.other],
                                           {"target": "prospero-x86_64-sysv",
                                            "engine_major": 5, "engine_minor": 6})
        self.assertIsNone(error)
        self.assertEqual(matches, ["townfall-ps5-ue56-static-20261002"])

    def test_title_discriminates(self):
        matches, error = vp.identify_build([self.townfall, dict(self.other, title="Second Title")],
                                           {"target": "prospero-x86_64-sysv", "engine_major": 4,
                                            "engine_minor": 27, "title": "Second Title"})
        self.assertIsNone(error)
        self.assertEqual(matches, ["other-title-ps5-ue427-static"])

    def test_ambiguous_matches_refused(self):
        clone = copy.deepcopy(self.other)
        clone["profile_id"] = "another-title-ps5-ue427-static"
        matches, error = vp.identify_build([self.other, clone],
                                           {"target": "prospero-x86_64-sysv",
                                            "engine_major": 4, "engine_minor": 27})
        self.assertEqual(matches, [])
        self.assertIn("ambiguous", error)

    def test_no_fallback_to_different_build(self):
        matches, error = vp.identify_build([self.townfall],
                                           {"target": "prospero-x86_64-sysv",
                                            "engine_major": 5, "engine_minor": 5})
        self.assertEqual(matches, [])
        self.assertIn("refusing to fall back", error)

    def test_missing_required_axis_refused(self):
        matches, error = vp.identify_build([self.townfall], {"engine_major": 5})
        self.assertEqual(matches, [])
        self.assertIn("lacks required axis 'target'", error)

    def test_cli_missing_input_file_is_input_error(self):
        with self.assertRaises(SystemExit):
            vp.main(["--schema", str(SCHEMA_PATH),
                     "--profile", str(ROOT / "profiles" / "nonexistent-for-cli.json")])


if __name__ == "__main__":
    unittest.main(verbosity=2)
