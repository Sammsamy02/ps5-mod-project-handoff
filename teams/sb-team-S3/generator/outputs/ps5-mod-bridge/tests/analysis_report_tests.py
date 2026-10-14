"""Focused report projection checks; no provider, device or game operations."""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "companion"))


class AnalysisReports(unittest.TestCase):
    def module(self):
        path = Path(__file__).resolve().parents[1] / "companion" / "modbridge_reports.py"
        self.assertTrue(path.is_file(), "Missing bounded analysis report projection")
        import modbridge_reports
        return modbridge_reports

    def inventory(self):
        return {"status": "analyzed", "inventory_complete": True,
            "provenance": {"input_stable": True}, "entries": []}

    def test_projection_never_grants_runtime_or_install_support(self):
        reports = self.module()
        result = reports.analysis_report(self.inventory(), {"unresolved": []})
        self.assertEqual(result["operation_state"], "supported")
        self.assertEqual(result["compatibility_state"], "unverified")
        self.assertIn("platform_acceptance_missing", result["reason_codes"])
        self.assertEqual(result["install_authority"],
            {"state": "unsupported", "reason": "install_operation_unavailable", "scope": []})
        self.assertEqual(reports.validate_report(result), result)

    def test_blockers_are_bounded_allowlisted_codes_without_paths_or_details(self):
        reports = self.module()
        plan = {"unresolved": [{"code": "unknown_target", "component": "/private/game"},
            {"code": "missing_requirements", "details": "private token"},
            {"code": "unknown_target"}, {"code": "invented_private_message"}]}
        result = reports.analysis_report(self.inventory(), plan)
        self.assertEqual(result["reason_codes"],
            ["analysis_detail_unavailable", "missing_requirements", "platform_acceptance_missing", "unknown_target"])
        self.assertNotIn("/private/game", str(result))
        self.assertNotIn("private token", str(result))
        self.assertNotIn("invented_private_message", str(result))

    def test_structural_pe_runtime_is_unsupported_but_filename_hints_are_not(self):
        reports = self.module()
        inventory = self.inventory()
        entry = {"kind": "native_pe", "facts": {"classification_confidence": "structural"},
            "requirements": [{"id": "runtime.windows"}]}
        inventory["entries"] = [entry]
        result = reports.analysis_report(inventory, {"unresolved": []})
        self.assertEqual(result["compatibility_state"], "unsupported")
        self.assertIn("windows_runtime_unavailable", result["reason_codes"])
        entry["facts"]["classification_confidence"] = "hint"
        self.assertEqual(reports.analysis_report(inventory, {"unresolved": []})["compatibility_state"], "unverified")
        entry["facts"]["classification_confidence"] = "structural"
        entry["kind"] = "managed_pe"
        entry["requirements"] = [{"id": "runtime.clr"}]
        self.assertIn("managed_runtime_unavailable", reports.analysis_report(inventory, {"unresolved": []})["reason_codes"])

    def test_incomplete_inventory_cannot_publish_supported_operation(self):
        reports = self.module()
        for field in ("inventory_complete", "provenance"):
            inventory = self.inventory()
            inventory[field] = False if field == "inventory_complete" else {"input_stable": False}
            with self.subTest(field=field):
                self.assertEqual(reports.analysis_report(inventory, {"unresolved": []})["operation_state"], "unverified")

    def test_blocked_prerequisites_project_only_planner_blockers(self):
        inventory = self.inventory()
        inventory["entries"] = [{"requirements": [{"id": "declared.only", "version": "9"}]}]
        plan = {"unresolved": [
            {"code": "missing_requirements", "component": "/private/game", "requirements": [
                {"code": "missing_capability", "requirement": {"id": "runtime.shared", "version": ">=1.2", "consumer": "private"}},
                {"code": "missing_capability", "requirement": {"id": "runtime.shared"}}]},
            {"code": "unbound_package_requirement", "requirement": {"id": "engine.profile", "version": "v2+build-1"}},
            {"code": "missing_backend", "candidates": [{"backend_id": "private", "requirements": [
                {"code": "missing_capability", "requirement": {"id": "runtime.shared", "version": ">=1.2"}}]},
                {"code": "analyzer_only", "backend_id": "private"}]},
            {"code": "missing_dependency", "to": "/private/dependency"}],
            "conflicts": [{"code": "version_conflict", "components": ["private"], "requirements": [
                {"component": "private", "requirement": {"id": "runtime.shared", "version": "2.0"}},
                {"id": "runtime.shared", "version": "1.0"}]}]}
        result = self.module().analysis_report(inventory, plan)
        self.assertEqual(result["blocked_prerequisites"], {"complete": True, "items": [
            {"id": "engine.profile", "version": "v2+build-1"},
            {"id": "runtime.shared", "version": None},
            {"id": "runtime.shared", "version": "1.0"},
            {"id": "runtime.shared", "version": "2.0"},
            {"id": "runtime.shared", "version": ">=1.2"}]})
        self.assertNotIn("private", str(result["blocked_prerequisites"]))
        self.assertNotIn("declared.only", str(result))

    def test_nested_requirement_conflicts_and_safe_namespace_tokens(self):
        result = self.module().analysis_report(self.inventory(), {"unresolved": [
            {"code": "missing_requirements", "requirements": [
                {"code": "version_conflict", "requirements": [
                    {"id": "runtime.shared-runtime", "version": "1"},
                    {"id": "runtime.shared-runtime", "version": ">=2"}]},
                {"requirement": {"id": "shared_runtime"}}]}]})
        self.assertEqual(result["blocked_prerequisites"], {"complete": True, "items": [
            {"id": "runtime.shared-runtime", "version": "1"},
            {"id": "runtime.shared-runtime", "version": ">=2"},
            {"id": "shared_runtime", "version": None}]})

    def test_empty_projection_is_complete_even_when_other_dependencies_are_blocked(self):
        result = self.module().analysis_report(self.inventory(), {"unresolved": [
            {"code": "missing_dependency", "to": "private"}, {"code": "unknown_dependency_type"}]})
        self.assertEqual(result["blocked_prerequisites"], {"complete": True, "items": []})

    def test_unsafe_or_malformed_projection_erases_all_items_without_truncation(self):
        cases = [{"id": "Runtime.shared"}, {"id": "runtime..shared"}, {"id": "runtime/path"},
            {"id": "runtime." + "a" * 90}, {"id": "runtime.shared", "version": ">=v1"},
            {"id": "runtime.shared", "version": "https://private"},
            {"id": "runtime.shared", "version": "1 2"},
            {"id": "runtime.shared", "version": "1\n2"},
            {"id": "runtime.shared", "version": "x" * 65},
            {"id": "runtime.shared", "version": 1}, None]
        for requirement in cases:
            with self.subTest(requirement=requirement):
                plan = {"unresolved": [{"code": "missing_requirements", "requirements": [
                    {"requirement": {"id": "runtime.valid"}}, {"requirement": requirement}]}]}
                result = self.module().analysis_report(self.inventory(), plan)
                self.assertEqual(result["blocked_prerequisites"], {"complete": False, "items": []})
                self.assertIn("analysis_detail_unavailable", result["reason_codes"])
        for record in ({"code": "missing_requirements"},
                {"code": "missing_backend", "candidates": [{"requirements": "private"}]},
                {"code": "unbound_package_requirement", "requirement": []}):
            with self.subTest(record=record):
                result = self.module().analysis_report(self.inventory(), {"unresolved": [record]})
                self.assertEqual(result["blocked_prerequisites"], {"complete": False, "items": []})
        many = [{"requirement": {"id": "runtime.cap" + str(i)}} for i in range(129)]
        result = self.module().analysis_report(self.inventory(), {"unresolved": [
            {"code": "missing_requirements", "requirements": many}]})
        self.assertEqual(result["blocked_prerequisites"], {"complete": False, "items": []})
        self.assertIn("analysis_detail_unavailable", result["reason_codes"])
        result = self.module().analysis_report(self.inventory(), {"unresolved": [
            {"code": "missing_requirements", "requirements": many[:128] + many[:128]}]})
        self.assertTrue(result["blocked_prerequisites"]["complete"])
        self.assertEqual(len(result["blocked_prerequisites"]["items"]), 128)
        repeated = [{"requirement": {"id": "runtime.shared"}}] * 10001
        result = self.module().analysis_report(self.inventory(), {"unresolved": [
            {"code": "missing_requirements", "requirements": repeated}]})
        self.assertEqual(result["blocked_prerequisites"], {"complete": False, "items": []})
        self.assertIn("analysis_detail_unavailable", result["reason_codes"])

    def test_projection_validator_is_optional_exact_bounded_and_owning(self):
        reports = self.module()
        original = reports.analysis_report(self.inventory(), {"unresolved": []})
        legacy = dict(original)
        legacy.pop("blocked_prerequisites")
        self.assertEqual(reports.validate_report(legacy), legacy)
        valid = dict(original, blocked_prerequisites={"complete": True, "items": [
            {"id": "runtime.shared", "version": None}, {"id": "runtime.shared", "version": "1.0"}]})
        copied = reports.validate_report(valid)
        copied["blocked_prerequisites"]["items"][0]["id"] = "runtime.changed"
        self.assertEqual(valid["blocked_prerequisites"]["items"][0]["id"], "runtime.shared")
        invalid = [{"complete": 1, "items": []}, {"complete": True, "items": [], "extra": 1},
            {"complete": False, "items": [{"id": "runtime.shared", "version": None}]},
            {"complete": True, "items": [{"id": "runtime.shared"}]},
            {"complete": True, "items": [{"id": "runtime.shared", "version": None, "consumer": "private"}]},
            {"complete": True, "items": [{"id": "runtime.Shared", "version": None}]},
            {"complete": True, "items": [{"id": "runtime.shared", "version": None}] * 2},
            {"complete": True, "items": [{"id": "runtime.z", "version": None}, {"id": "runtime.a", "version": None}]},
            {"complete": True, "items": [{"id": "runtime.cap" + str(i), "version": None} for i in range(129)]}]
        for projection in invalid:
            with self.subTest(projection=projection), self.assertRaises(ValueError):
                reports.validate_report(dict(original, blocked_prerequisites=projection))

    def test_receipt_validator_rejects_authority_and_unbounded_diagnostics(self):
        reports = self.module()
        original = reports.analysis_report(self.inventory(), {"unresolved": []})
        changes = [dict(original, compatibility_state="supported"), dict(original, extra="ignored"),
            dict(original, reason_codes=["/private/path"]), dict(original, reason_codes=["unknown_target"] * 33),
            dict(original, reason_codes=["unknown_target", "unknown_target"]),
            dict(original, schema_version=True), dict(original, execution_context="ps5_native"),
            dict(original, install_authority={"state": "supported", "reason": "validated", "scope": ["writes"]})]
        for value in changes:
            with self.subTest(value=value), self.assertRaises(ValueError):
                reports.validate_report(value)
        copied = reports.validate_report(original)
        copied["reason_codes"].append("unknown_target")
        copied["install_authority"]["scope"].append("writes")
        self.assertEqual(original, reports.analysis_report(self.inventory(), {"unresolved": []}))


class CompatibilityAssessments(unittest.TestCase):
    CODES = ("unknown_console_version", "unknown_build", "file_ambiguous", "file_no_fit", "file_older_build",
             "file_newer_than_build", "file_undated", "missing_references", "unverified_references",
             "dynamic_references", "no_index", "index_for_other_build", "assessment_failed")

    def reports(self):
        import modbridge_reports
        return modbridge_reports

    def assessment(self, **changes):
        value = {"verdict": "partially_compatible",
            "reasons": [{"code": "missing_references", "detail": "1 of 2 references missing"}],
            "console_content_version": "02.003.000", "pc_build": "1.03", "selected_file_id": 95,
            "checked": 2, "present": 1,
            "missing": {"assets": ["/Game/Gone"], "classes": [], "functions": [], "short_class_names": []}}
        value.update(changes)
        return value

    def rejected(self, value):
        with self.assertRaises(ValueError):
            self.reports().validate_compat(value)

    def test_accepts_every_verdict_code_and_nullable_field(self):
        reports = self.reports()
        for verdict in ("compatible", "partially_compatible", "incompatible", "unknown"):
            value = self.assessment(verdict=verdict)
            self.assertEqual(reports.validate_compat(value), value)
        for code in self.CODES:
            with self.subTest(code=code):
                value = self.assessment(reasons=[{"code": code, "detail": ""}])
                self.assertEqual(reports.validate_compat(value), value)
        value = self.assessment(console_content_version=None, pc_build=None, selected_file_id=None,
                                checked=0, present=0, reasons=[])
        self.assertEqual(reports.validate_compat(value), value)
        self.assertIsNot(reports.validate_compat(value)["missing"], value["missing"])

    def test_rejects_unknown_keys_codes_verdicts_and_wrong_types(self):
        good = self.assessment()
        for key in good:
            missing = dict(good)
            del missing[key]
            self.rejected(missing)
        self.rejected(dict(good, extra=1))
        self.rejected(dict(good, verdict="working"))
        self.rejected(dict(good, verdict=None))
        self.rejected(dict(good, reasons=[{"code": "invented", "detail": "x"}]))
        self.rejected(dict(good, reasons=[{"code": "no_index", "detail": "x", "extra": 1}]))
        self.rejected(dict(good, reasons=[{"code": "no_index"}]))
        self.rejected(dict(good, reasons=[{"code": "no_index", "detail": 5}]))
        self.rejected(dict(good, reasons=["no_index"]))
        self.rejected(dict(good, reasons={}))
        self.rejected(dict(good, selected_file_id=0))
        self.rejected(dict(good, selected_file_id=True))
        self.rejected(dict(good, selected_file_id="95"))
        for field in ("checked", "present"):
            self.rejected(dict(good, **{field: -1}))
            self.rejected(dict(good, **{field: True}))
            self.rejected(dict(good, **{field: 1.0}))
        self.rejected(dict(good, console_content_version=2))
        self.rejected(dict(good, pc_build=[]))
        self.rejected(dict(good, missing={"assets": [], "classes": [], "functions": []}))
        self.rejected(dict(good, missing={**good["missing"], "extra": []}))
        self.rejected(dict(good, missing={**good["missing"], "assets": [1]}))
        self.rejected(dict(good, missing={**good["missing"], "classes": "x"}))
        self.rejected([])
        self.rejected(None)

    def test_size_caps_are_enforced_exactly(self):
        reports = self.reports()
        reason = {"code": "no_index", "detail": "x" * 512}
        reports.validate_compat(self.assessment(reasons=[reason] * 32))
        self.rejected(self.assessment(reasons=[reason] * 33))
        self.rejected(self.assessment(reasons=[{"code": "no_index", "detail": "x" * 513}]))
        for key in ("assets", "classes", "functions", "short_class_names"):
            missing = {"assets": [], "classes": [], "functions": [], "short_class_names": []}
            reports.validate_compat(self.assessment(missing=dict(missing, **{key: ["n" * 1024] * 50})))
            self.rejected(self.assessment(missing=dict(missing, **{key: ["n"] * 51})))
            self.rejected(self.assessment(missing=dict(missing, **{key: ["n" * 1025]})))
        self.rejected(self.assessment(pc_build="v" * 257))
        self.rejected(self.assessment(console_content_version="v" * 257))

    def test_projection_sorts_truncates_and_never_defaults_an_unknown_code(self):
        reports = self.reports()
        result = {"verdict": "partially_compatible",
            "reasons": [{"code": "missing_references", "detail": "d" * 600}] * 40,
            "game_version": {"known": True, "content_version": "02.003.000"},
            "file_selection": {"console_content_version": "02.003.000", "selected_file_id": 7,
                               "pc_build": {"label": "1.03", "released_utc": 5}},
            "content": {"checked": 90, "present": 5,
                        "missing": {"assets": ["/b"] * 3 + ["/Game/%03d" % number for number in range(99, -1, -1)],
                                    "classes": [], "functions": [], "short_class_names": ["Z", "A"]}}}
        slim = reports.compat_projection(result)
        self.assertEqual(len(slim["reasons"]), 32)
        self.assertEqual(len(slim["reasons"][0]["detail"]), 512)
        self.assertEqual(slim["missing"]["assets"], sorted(set(result["content"]["missing"]["assets"]))[:50])
        self.assertEqual(slim["missing"]["short_class_names"], ["A", "Z"])
        self.assertEqual((slim["pc_build"], slim["selected_file_id"], slim["console_content_version"]), ("1.03", 7, "02.003.000"))
        for bad in (dict(result, verdict="maybe"), dict(result, reasons=[{"code": "other", "detail": "x"}])):
            with self.assertRaises(ValueError):
                reports.compat_projection(bad)
        self.assertEqual(reports.unknown_compat("no_index", "no game profile for this title")["verdict"], "unknown")

    def test_lua_reference_union_is_sorted_unique_and_dynamic_when_unknowable(self):
        reports = self.reports()
        refs = lambda assets, dynamic=False: {"assets": assets, "classes": [], "functions": [],
                                              "short_class_names": ["Pawn"], "dynamic": dynamic}
        entries = [{"kind": "lua", "facts": {"lua_references": refs(["/b", "/a"])}},
                   {"kind": "lua", "facts": {"lua_references": refs(["/a", "/c"])}},
                   {"kind": "data", "facts": {}}]
        union = reports.lua_reference_union({"entries": entries})
        self.assertEqual(union, {"assets": ["/a", "/b", "/c"], "classes": [], "functions": [],
                                 "short_class_names": ["Pawn"], "dynamic": False})
        entries.append({"kind": "lua", "facts": {"lua_references": refs([], True)}})
        self.assertTrue(reports.lua_reference_union({"entries": entries})["dynamic"])
        for facts in ({"lua_references_unavailable": "too_large"}, {}):
            self.assertTrue(reports.lua_reference_union({"entries": [{"kind": "lua", "facts": facts}]})["dynamic"])
        self.assertFalse(reports.lua_reference_union({"entries": []})["dynamic"])


class LuaApiUnion(unittest.TestCase):
    def reports(self):
        import modbridge_reports
        return modbridge_reports

    def lua(self, functions=None, modules=(), shadowed=(), dynamic=False):
        return {"kind": "lua", "facts": {"lua_api": {"functions": dict(functions or {}), "modules": list(modules),
                                                      "shadowed": list(shadowed), "dynamic_require": dynamic}}}

    def union(self, *entries):
        return self.reports().lua_api_union({"entries": list(entries)})

    def test_none_when_the_package_has_no_lua(self):
        self.assertIsNone(self.union())
        self.assertIsNone(self.union({"kind": "data", "facts": {}}, {"kind": "native_pe", "facts": {"lua_api": {}}}, "x"))

    def test_sums_uses_and_unions_sorted_modules_and_shadowed_and_ors_dynamic_require(self):
        union = self.union(
            self.lua({"RegisterKeyBind": 2, "ExecuteInGameThread": 1}, ["UEHelpers", "b/mod"], ["RegisterHook"]),
            {"kind": "data", "facts": {}},
            self.lua({"RegisterKeyBind": 3, "FText": 4}, ["UEHelpers", "a.mod"], ["FindFirstOf", "RegisterHook"]))
        self.assertEqual(union, {"functions": {"ExecuteInGameThread": 1, "FText": 4, "RegisterKeyBind": 5},
                                 "modules": ["UEHelpers", "a.mod", "b/mod"], "shadowed": ["FindFirstOf", "RegisterHook"],
                                 "dynamic_require": False})
        self.assertEqual(list(union["functions"]), sorted(union["functions"]))
        self.assertTrue(self.union(self.lua(), self.lua(dynamic=True))["dynamic_require"])
        # A Lua entry that uses nothing is still Lua: an empty union, not None.
        self.assertEqual(self.union(self.lua()), {"functions": {}, "modules": [], "shadowed": [], "dynamic_require": False})

    def test_a_name_used_by_one_script_and_shadowed_by_another_stays_a_use(self):
        union = self.union(self.lua({"RegisterHook": 1}), self.lua({}, [], ["RegisterHook", "FText"]))
        self.assertEqual(union["functions"], {"RegisterHook": 1})
        self.assertEqual(union["shadowed"], ["FText"])

    def test_counts_cap_and_module_overflow_fall_back_to_dynamic_require(self):
        union = self.union(self.lua({"FText": 0xFFFFFFFF}), self.lua({"FText": 5}))
        self.assertEqual(union["functions"], {"FText": 0xFFFFFFFF})
        many = ["m%04d" % number for number in range(600)]
        union = self.union(self.lua({}, many[:500]), self.lua({}, many[300:]))
        self.assertEqual(len(union["modules"]), 600)
        self.assertFalse(union["dynamic_require"])
        union = self.union(self.lua({}, many), self.lua({}, ["z%04d" % number for number in range(500)]))
        self.assertEqual(len(union["modules"]), 1024)
        self.assertEqual(union["modules"], sorted(union["modules"]))
        self.assertTrue(union["dynamic_require"])

    def test_any_unreadable_lua_entry_makes_the_whole_union_unavailable(self):
        reports = self.reports()
        bad = [{"kind": "lua", "facts": {"lua_api_unavailable": "too_large"}},
               {"kind": "lua", "facts": {"lua_api_unavailable": "unreadable", "lua_api": self.lua()["facts"]["lua_api"]}},
               {"kind": "lua", "facts": {}}, {"kind": "lua"}, {"kind": "lua", "facts": None},
               {"kind": "lua", "facts": {"lua_api": None}}, {"kind": "lua", "facts": {"lua_api": []}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {}, "modules": [], "shadowed": [],
                                                      "dynamic_require": False, "extra": 1}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {}, "modules": [], "shadowed": []}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {"FText": 0}, "modules": [], "shadowed": [], "dynamic_require": False}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {"FText": True}, "modules": [], "shadowed": [], "dynamic_require": False}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {"FText": 2 ** 32}, "modules": [], "shadowed": [], "dynamic_require": False}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {"bad name": 1}, "modules": [], "shadowed": [], "dynamic_require": False}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": [], "modules": [], "shadowed": [], "dynamic_require": False}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {}, "modules": ["has space"], "shadowed": [], "dynamic_require": False}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {}, "modules": [3], "shadowed": [], "dynamic_require": False}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {}, "modules": [], "shadowed": [3], "dynamic_require": False}}},
               {"kind": "lua", "facts": {"lua_api": {"functions": {}, "modules": [], "shadowed": [], "dynamic_require": 1}}}]
        for entry in bad:
            with self.subTest(entry=entry):
                # Readable scripts around it never make up for it.
                result = self.union(self.lua({"FText": 1}), entry, self.lua({"RegisterKeyBind": 1}))
                self.assertEqual(result, {"unavailable": True})
                self.assertTrue(reports.lua_api_unavailable(result))
        self.assertFalse(reports.lua_api_unavailable(self.union(self.lua())))
        self.assertFalse(reports.lua_api_unavailable(None))
        self.assertFalse(reports.lua_api_unavailable({"unavailable": False}))
        # The marker is a fresh object each time.
        first = self.union(bad[0])
        first["unavailable"] = False
        self.assertEqual(self.union(bad[0]), {"unavailable": True})

    def test_invalid_inventories_raise(self):
        for inventory in ({}, {"entries": "x"}, None, []):
            with self.subTest(inventory=inventory), self.assertRaises(ValueError):
                self.reports().lua_api_union(inventory)


class LuaApiOptionalUnion(unittest.TestCase):
    """A name is optional for the package only when every script that uses it calls it through pcall."""

    def union(self, *usages):
        import modbridge_reports
        return modbridge_reports.lua_api_union({"entries": [{"kind": "lua", "facts": {"lua_api": usage}} for usage in usages]})

    def usage(self, functions, optional=None):
        value = {"functions": functions, "modules": [], "shadowed": [], "dynamic_require": False}
        if optional is not None:
            value["optional"] = optional
        return value

    def test_optional_only_when_no_script_calls_it_plainly(self):
        union = self.union(self.usage({"RegisterLoadMapPreHook": 1, "FName": 1}, ["RegisterLoadMapPreHook"]),
                           self.usage({"RegisterLoadMapPreHook": 1, "LoadAsset": 2}, ["RegisterLoadMapPreHook", "LoadAsset"]))
        self.assertEqual(union["optional"], ["LoadAsset", "RegisterLoadMapPreHook"])
        mixed = self.union(self.usage({"LoadAsset": 1}, ["LoadAsset"]), self.usage({"LoadAsset": 1}))
        self.assertNotIn("optional", mixed)
        self.assertNotIn("optional", self.union(self.usage({"FName": 1})))

    def test_malformed_optional_lists_make_the_package_unread(self):
        import modbridge_reports
        for optional in ([], ["NotCounted"], ["FName", "FName"], "FName", [3]):
            with self.subTest(optional=optional):
                self.assertTrue(modbridge_reports.lua_api_unavailable(self.union(self.usage({"FName": 1}, optional))))


class RuntimeCoverageOptional(unittest.TestCase):
    def validate(self, **changes):
        import modbridge_reports
        value = {"status": "partial", "reason": None, "runtime": "ps5-ue4ss-port", "console_wired": False,
                 "missing": [], "partial": ["ExecuteInGameThread"], "missing_modules": [], "partial_modules": [],
                 "dynamic_require": False, "optional_missing": ["RegisterLoadMapPreHook"]}
        value.update(changes)
        return modbridge_reports.validate_runtime_coverage(value)

    def test_optional_missing_alone_makes_partial_and_is_kept(self):
        self.assertEqual(self.validate()["optional_missing"], ["RegisterLoadMapPreHook"])
        self.assertEqual(self.validate(partial=[])["status"], "partial")
        self.assertEqual(self.validate(status="blocked", missing=["FName"])["optional_missing"], ["RegisterLoadMapPreHook"])

    def test_bad_optional_missing_is_refused(self):
        for changes in ({"optional_missing": []}, {"optional_missing": ["b", "a"]}, {"optional_missing": ["a", "a"]},
                        {"optional_missing": ["bad name"]}, {"optional_missing": "x"},
                        {"status": "covered", "partial": []},
                        {"status": "blocked", "missing": ["RegisterLoadMapPreHook"]},
                        {"status": "unknown", "reason": "assessment_failed", "runtime": None, "console_wired": None,
                         "partial": []}):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.validate(**changes)

    def test_projection_keeps_optional_missing_from_the_core_result(self):
        import modbridge_reports
        core = {"status": "partial", "runtime": "ps5-ue4ss-port", "console_wired": False, "dynamic_require": False,
                "missing": [], "partial": ["ExecuteInGameThread"], "missing_modules": [], "partial_modules": [],
                "optional_missing": ["RegisterLoadMapPreHook"]}
        self.assertEqual(modbridge_reports.runtime_coverage_projection(core)["optional_missing"], ["RegisterLoadMapPreHook"])
        core.pop("optional_missing")
        self.assertNotIn("optional_missing", modbridge_reports.runtime_coverage_projection(core))


class RuntimeCoverageRecords(unittest.TestCase):
    def reports(self):
        import modbridge_reports
        return modbridge_reports

    def record(self, **changes):
        value = {"status": "blocked", "reason": None, "runtime": "ps5-ue4ss-portable", "console_wired": False,
                 "missing": ["ExecuteInGameThread", "FindFirstOf"], "partial": ["ExecuteAsync"],
                 "missing_modules": [], "partial_modules": ["UEHelpers"], "dynamic_require": False}
        value.update(changes)
        return value

    def rejected(self, value):
        with self.assertRaises(ValueError):
            self.reports().validate_runtime_coverage(value)

    def test_accepts_each_status_and_the_unknown_reasons_and_returns_an_owned_copy(self):
        reports = self.reports()
        clean = dict(missing=[], partial=[], missing_modules=[], partial_modules=[])
        for value in (self.record(),
                      self.record(status="partial", missing=[], missing_modules=[]),
                      self.record(status="partial", dynamic_require=True, **clean),
                      self.record(status="covered", dynamic_require=False, **clean),
                      self.record(status="blocked", console_wired=True, missing=[], missing_modules=["mod/x.y-z_1"])):
            with self.subTest(status=value["status"]):
                self.assertEqual(reports.validate_runtime_coverage(value), value)
        for reason in ("usage_unavailable", "assessment_failed"):
            value = reports.unknown_runtime_coverage(reason)
            self.assertEqual(value["status"], "unknown")
            self.assertEqual(value, {"status": "unknown", "reason": reason, "runtime": None, "console_wired": None,
                                     "missing": [], "partial": [], "missing_modules": [], "partial_modules": [],
                                     "dynamic_require": False})
        with self.assertRaises(ValueError):
            reports.unknown_runtime_coverage("other")
        with self.assertRaises(ValueError):
            reports.unknown_runtime_coverage(None)
        original = self.record()
        copied = reports.validate_runtime_coverage(original)
        copied["missing"].append("Changed")
        self.assertEqual(original["missing"], ["ExecuteInGameThread", "FindFirstOf"])

    def test_rejects_unknown_or_missing_keys_wrong_types_and_inconsistent_claims(self):
        good = self.record()
        for key in good:
            missing = dict(good)
            del missing[key]
            self.rejected(missing)
        for bad in (dict(good, extra=1), dict(good, status="works"), dict(good, status=None), dict(good, status=["blocked"]),
                    dict(good, reason="assessment_failed"), dict(good, reason="other"), dict(good, runtime=None),
                    dict(good, runtime="Bad Id"), dict(good, runtime="/private/x"), dict(good, runtime=7),
                    dict(good, console_wired=None), dict(good, console_wired=0), dict(good, console_wired="false"),
                    dict(good, dynamic_require=None), dict(good, dynamic_require=1),
                    # covered or partial beside names that contradict it
                    dict(good, status="covered"), dict(good, status="partial"),
                    dict(good, status="covered", missing=[], missing_modules=[], partial=[], partial_modules=["UEHelpers"]),
                    dict(good, status="covered", missing=[], missing_modules=[], partial=[], partial_modules=[], dynamic_require=True),
                    dict(good, status="blocked", missing=[], missing_modules=[]),
                    # unknown carries a reason and no data
                    dict(good, status="unknown"), dict(good, status="unknown", reason="assessment_failed"),
                    dict(self.reports().unknown_runtime_coverage("assessment_failed"), runtime="ps5-ue4ss-portable"),
                    dict(self.reports().unknown_runtime_coverage("assessment_failed"), console_wired=False),
                    dict(self.reports().unknown_runtime_coverage("assessment_failed"), missing=["FText"]),
                    dict(self.reports().unknown_runtime_coverage("assessment_failed"), dynamic_require=True),
                    dict(self.reports().unknown_runtime_coverage("assessment_failed"), reason=None),
                    [], None, "blocked", [good]):
            with self.subTest(bad=str(bad)[:80]):
                self.rejected(bad)

    def test_lists_are_bounded_and_names_have_the_catalog_and_module_shapes(self):
        reports = self.reports()
        for key, name in (("missing", "A" * 128), ("partial", "_a1"), ("missing_modules", "m" * 128), ("partial_modules", "a/b.c-d_e")):
            full = [name] * 64
            self.assertEqual(reports.validate_runtime_coverage(self.record(**{key: full}))[key], full)
            self.rejected(self.record(**{key: [name] * 65}))
            self.rejected(self.record(**{key: ["x" * 129]}))
        for key in ("missing", "partial"):
            for bad in ("1abc", "a-b", "a b", "a.b", "", "a/b", "name\n", "<img>", "é"):
                with self.subTest(key=key, name=bad):
                    self.rejected(self.record(**{key: [bad]}))
        for key in ("missing_modules", "partial_modules"):
            for bad in ("a b", "", "<b>", "a\n", "é", "a:b", "a\\b"):
                with self.subTest(key=key, name=bad):
                    self.rejected(self.record(**{key: [bad]}))
        for key in ("missing", "partial", "missing_modules", "partial_modules"):
            for bad in ("FText", ["FText", 1], [None], {"a": 1}, None):
                self.rejected(self.record(**{key: bad}))

    def test_projection_sorts_bounds_and_never_defaults_an_unknown_status(self):
        reports = self.reports()
        core = {"runtime": "ps5-ue4ss-portable", "runtime_version": "readme-1", "console_wired": False, "status": "blocked",
                "functions": [{"name": "X", "uses": 1, "status": "absent", "note": "ignored"}],
                "missing": ["FindFirstOf", "ExecuteInGameThread", "FindFirstOf"], "partial": [],
                "missing_modules": ["m%03d" % n for n in range(99, -1, -1)], "partial_modules": ["UEHelpers"],
                "shadowed": ["RegisterHook"], "dynamic_require": True}
        slim = reports.runtime_coverage_projection(core)
        self.assertEqual(slim, {"status": "blocked", "reason": None, "runtime": "ps5-ue4ss-portable", "console_wired": False,
            "missing": ["ExecuteInGameThread", "FindFirstOf"], "partial": [],
            "missing_modules": ["m%03d" % n for n in range(64)], "partial_modules": ["UEHelpers"], "dynamic_require": True})
        for forbidden in ("functions", "shadowed", "runtime_version"):
            self.assertNotIn(forbidden, slim)
        for bad in (dict(core, status="working"), dict(core, status="unknown"), dict(core, status=None),
                    dict(core, console_wired=None), dict(core, runtime="/private/x"), dict(core, missing="FText"),
                    dict(core, missing=[1]), dict(core, partial=None), dict(core, dynamic_require="yes"),
                    {key: value for key, value in core.items() if key != "missing_modules"},
                    dict(core, status="covered"), "x", None, []):
            with self.subTest(bad=str(bad)[:60]), self.assertRaises(ValueError):
                reports.runtime_coverage_projection(bad)


if __name__ == "__main__":
    unittest.main()
