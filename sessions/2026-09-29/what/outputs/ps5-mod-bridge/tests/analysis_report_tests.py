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
