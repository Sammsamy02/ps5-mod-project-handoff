#!/usr/bin/env python3
"""Consumer-facing checks against the real compiled command-line program."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zipfile

PROJECT = Path(__file__).resolve().parents[1]
BINARY = Path(os.environ.get("MODBRIDGE_BIN", PROJECT / "build" / "modbridge"))


class CliTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(BINARY.is_file(), "CLI implementation is missing")
        self.temp = tempfile.TemporaryDirectory(prefix="modbridge-cli-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def run_cli(self, *args, expected=0):
        result = subprocess.run([str(BINARY), *map(str, args)], text=True,
                                capture_output=True, timeout=30)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result

    def fixture(self):
        package = self.root / "mixed.zip"
        with zipfile.ZipFile(package, "w") as archive:
            archive.writestr("Example/Mods/main.lua", "RegisterHook('/Script/Test.Event', function() end)\n")
            archive.writestr("Example/settings.ini", "enabled=true\n")
        return package

    def write_json(self, name, value):
        path = self.root / name
        path.write_text(json.dumps(value))
        return path

    def graph_plan(self, edge_type, ring=False, supported=True):
        # Flat valid inputs must not exhaust the stack in SCC or component ordering.
        names = [f"node-{i:05d}" for i in range(6500)]
        entries = [{"path": name, "kind": "data", "sha256": hashlib.sha256(b"abc").hexdigest()}
                   for name in names]
        edges = [{"from": names[i], "to": names[i + 1], "type": edge_type}
                 for i in range(len(names) - 1)]
        if ring:
            edges.append({"from": names[-1], "to": names[0], "type": edge_type})
        inventory = self.write_json("graph.json", {"entries": entries, "dependencies": edges})
        target = self.write_json("graph-target.json", {"fingerprints": {"executable": "owned-target"},
                                                       "capabilities": []})
        backends = self.write_json("graph-backends.json", [{"id": "owned.data", "version": "1",
            "mode": "transformer", "kinds": ["data"], "capabilities": [],
            "target_fingerprints": ["executable"], "supports_reference_cycles": supported}])
        result = self.run_cli("plan", inventory, "--target", target, "--backends", backends)
        return names, json.loads(result.stdout), len(result.stdout)

    def test_large_acyclic_graph_has_complete_dependency_order(self):
        names, plan, _ = self.graph_plan("order")
        self.assertEqual(plan["status"], "planned")
        self.assertEqual(plan["ordered_components"], names[::-1])
        self.assertEqual([step["component"] for step in plan["eligible_steps"]], names[::-1])

    def test_large_reference_ring_uses_one_shared_group(self):
        names, plan, output_bytes = self.graph_plan("reference", ring=True)
        self.assertEqual(plan["status"], "planned")
        self.assertEqual(plan["ordered_components"], names[::-1])
        self.assertEqual(len(plan["reference_groups"]), 1)
        group = plan["reference_groups"][0]
        self.assertEqual(group["components"], names)
        self.assertEqual(group["status"], "eligible")
        self.assertEqual({step["group_id"] for step in plan["eligible_steps"]}, {group["group_id"]})
        self.assertLess(output_bytes, 5_000_000)

    def test_large_strict_ring_reports_one_conflict_without_quadratic_output(self):
        names, plan, output_bytes = self.graph_plan("order", ring=True)
        self.assertEqual(plan["status"], "unresolved")
        self.assertEqual(plan["eligible_steps"], [])
        self.assertEqual(plan["ordered_components"], [])
        self.assertEqual(len(plan["conflicts"]), 1)
        conflict = plan["conflicts"][0]
        self.assertEqual(conflict["code"], "ordering_cycle")
        self.assertEqual(conflict["components"], names)
        self.assertEqual(len(plan["unresolved"]), len(names))
        self.assertEqual({item["group_id"] for item in plan["unresolved"]}, {conflict["group_id"]})
        self.assertTrue(all("components" not in item for item in plan["unresolved"]))
        self.assertLess(output_bytes, 3_000_000)

    def test_analyze_produces_real_hash_and_mixed_inventory(self):
        # Catches CLI dropping entries, reporting the wrong input, or upgrading evidence.
        package = self.fixture()
        report = self.root / "report.json"
        self.run_cli("analyze", package, "--output", report)
        data = json.loads(report.read_text())
        self.assertEqual(data["package_sha256"], hashlib.sha256(package.read_bytes()).hexdigest())
        self.assertEqual(data["status"], "analyzed")
        self.assertEqual({entry["path"] for entry in data["entries"]},
                         {"Example/Mods/main.lua", "Example/settings.ini"})
        self.assertNotIn("deployed", data)

    def test_unknown_target_reports_unresolved_runtime(self):
        package = self.fixture()
        inventory = self.root / "inventory.json"
        self.run_cli("analyze", package, "--output", inventory)
        target = self.write_json("target.json", {"platform": "ps5", "title": "Stellar Blade",
                                                 "fingerprints": {}, "capabilities": []})
        backends = self.write_json("backends.json", [])
        result = self.run_cli("plan", inventory, "--target", target, "--backends", backends)
        plan = json.loads(result.stdout)
        self.assertEqual(plan["status"], "unresolved")
        self.assertEqual(plan["stage"], "analyzed")
        self.assertTrue(plan["unresolved"])
        self.assertEqual(plan["eligible_steps"], [])

    def test_invalid_arguments_and_json_have_structured_errors(self):
        cases = [(), ("unknown-command",), ("analyze",),
                 ("analyze", self.root / "missing.zip"),
                 ("analyze", self.fixture(), "--bogus")]
        for args in cases:
            with self.subTest(args=args):
                result = self.run_cli(*args, expected=2)
                error = json.loads(result.stderr)["error"]
                self.assertTrue(error["code"])
                self.assertTrue(error["message"])
        invalid = self.root / "invalid.json"
        invalid.write_text("{")
        result = self.run_cli("cache-key", invalid, expected=2)
        self.assertIn("error", json.loads(result.stderr))

    def test_scratch_roundtrip_and_modified_file_preservation(self):
        scratch = self.root / "scratch"
        prepared = self.root / "prepared"
        prepared.mkdir()
        self.run_cli("scratch-init", scratch)
        original = scratch / "config.txt"
        original.write_text("original")
        (prepared / "replacement.txt").write_text("installed")
        manifest = self.write_json("manifest.json", {"files": [{"source": "replacement.txt",
                         "destination": "config.txt", "sha256": hashlib.sha256(b"installed").hexdigest()}]})
        result = self.run_cli("scratch-deploy", scratch, prepared, manifest)
        transaction = json.loads(result.stdout)["transaction_id"]
        self.assertEqual(original.read_text(), "installed")
        self.run_cli("scratch-remove", scratch, transaction)
        self.assertEqual(original.read_text(), "original")
        result = self.run_cli("scratch-deploy", scratch, prepared, manifest)
        transaction = json.loads(result.stdout)["transaction_id"]
        original.write_text("user edit")
        self.run_cli("scratch-remove", scratch, transaction)
        self.assertEqual(original.read_text(), "user edit")
        recovered = json.loads(self.run_cli("scratch-recover", scratch).stdout)
        self.assertIsInstance(recovered, dict)

    def test_json_nesting_and_node_budget_fail_without_crashing(self):
        # Parsing must reject the structure before copying a deeply nested DOM.
        excessive = self.root / "excessive.json"
        excessive.write_text('{"metadata":' + '[' * 20000 + '0' + ']' * 20000 + '}')
        result = self.run_cli("cache-key", excessive, expected=2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "invalid_input")
        excessive.write_text('[' + ','.join('0' for _ in range(100001)) + ']')
        result = self.run_cli("cache-key", excessive, expected=2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "invalid_input")

    def test_existing_nonempty_directory_is_not_silently_marked(self):
        scratch = self.root / "ordinary-files"
        scratch.mkdir()
        (scratch / "keep.txt").write_text("keep")
        self.run_cli("scratch-init", scratch, expected=2)
        self.assertFalse((scratch / ".modbridge-scratch").exists())
        self.assertEqual((scratch / "keep.txt").read_text(), "keep")

    def test_output_failure_does_not_mutate_input(self):
        package = self.fixture()
        before = package.read_bytes()
        directory = self.root / "destination-directory"
        directory.mkdir()
        self.run_cli("analyze", package, "--output", directory, expected=2)
        self.assertEqual(package.read_bytes(), before)

    def test_plan_report_cannot_replace_any_input(self):
        inventory = self.root / "inventory.json"
        self.run_cli("analyze", self.fixture(), "--output", inventory)
        target = self.write_json("target.json", {"fingerprints": {}, "capabilities": []})
        backends = self.write_json("backends.json", [])
        for destination in (inventory, target, backends):
            with self.subTest(destination=destination):
                before = destination.read_bytes()
                self.run_cli("plan", inventory, "--target", target, "--backends", backends,
                             "--output", destination, expected=2)
                self.assertEqual(destination.read_bytes(), before)

    def test_directory_report_cannot_change_package_members(self):
        package = self.root / "package-directory"
        package.mkdir()
        member = package / "owned.txt"
        member.write_text("keep my package content")
        for destination in (member, package / "new-report.json"):
            with self.subTest(destination=destination):
                self.run_cli("analyze", package, "--output", destination, expected=2)
                self.assertEqual(member.read_text(), "keep my package content")
                self.assertFalse((package / "new-report.json").exists())

    def test_stdout_write_failure_returns_nonzero(self):
        output = self.root / "readonly-output"
        output.write_text("preserve")
        with output.open("rb") as readonly:
            result = subprocess.run([str(BINARY), "--help"], stdout=readonly,
                                    stderr=subprocess.PIPE, text=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "write_failed")
        self.assertEqual(output.read_text(), "preserve")

    @unittest.skipUnless(os.name == "posix", "POSIX path byte handling")
    def test_non_utf8_path_error_is_serializable(self):
        result = subprocess.run([os.fsencode(BINARY), b"cache-key", b"missing-\xff.json"],
                                capture_output=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "invalid_input")


if __name__ == "__main__":
    unittest.main(verbosity=2)
