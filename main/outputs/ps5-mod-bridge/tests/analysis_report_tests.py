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


if __name__ == "__main__":
    unittest.main()
