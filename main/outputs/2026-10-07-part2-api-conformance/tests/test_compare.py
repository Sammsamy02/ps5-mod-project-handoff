"""Behavioral tests for the local comparator; records here are synthetic, never PC evidence."""
import copy
import csv
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

BASE = Path(__file__).resolve().parents[1]
SCHEMA = "modbridge.ue4ss-conformance/1"


class ComparatorTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.manifest = self.root / "manifest.json"
        self.manifest.write_text(json.dumps({
            "schema": "modbridge.ue4ss-conformance-plan/1", "suite_id": "test-suite",
            "release_sha256": "a" * 64, "helper_sha256": "b" * 64,
            "script_sha256": "d" * 64,
            "cases": [{"id": "one", "api": "FName", "order": 1},
                      {"id": "two", "api": "UObject.__index / __newindex", "order": 2}],
        }))

    def tearDown(self):
        self.tmp.cleanup()

    def records(self, side):
        header = {"schema": SCHEMA, "kind": "run", "run_id": side + "-run",
                  "side": side, "evidence": "engine", "suite_id": "test-suite",
                  "mod_release_sha256": "a" * 64, "helper_sha256": "b" * 64,
                  "game_id": "Townfall", "content_build": "controlled-build",
                  "scenario_id": "paused-save-1", "context_id": "fixture-context-1",
                  "runtime_build": side + "-runtime", "runtime_sha256": ("e" if side == "pc" else "f") * 64,
                  "game_binary_sha256": "c" * 64,
                  "script_sha256": "d" * 64,
                  "attestations": {"real_engine": True, "release_immutable": True,
                                   "independent_thread_witness": True,
                                   "controlled_object_context": True},
                  "selected_case_ids": ["one", "two"]}
        one = {"schema": SCHEMA, "kind": "case", "id": "one", "api": "FName",
               "order": 1, "status": "proved", "evidence": "engine",
               "value": {"text": "ByteProperty", "roundtrip": True},
               "error": None, "reason": None, "guard": {"verified": True, "in_game_thread": True}}
        two = {"schema": SCHEMA, "kind": "case", "id": "two",
               "api": "UObject.__index / __newindex", "order": 2,
               "status": "proved", "evidence": "engine", "value": {"before": 12, "written": 13,
               "readback": 13, "restored": 12}, "error": None, "reason": None,
               "guard": {"verified": True, "in_game_thread": True}}
        end = {"schema": SCHEMA, "kind": "end", "complete": True, "case_count": 2}
        return [header, one, two, end]

    def save(self, name, records, tsv=False):
        path = self.root / name
        if tsv:
            with path.open("w", newline="") as f:
                writer = csv.writer(f, delimiter="\t")
                writer.writerow(["record_json"])
                writer.writerows([[json.dumps(r, ensure_ascii=False)] for r in records])
        else:
            path.write_text("".join(json.dumps(r, ensure_ascii=False) + "\n" for r in records))
        return path

    def compare(self, reference=None, candidate=None, tsv=False):
        ref = self.save("pc.tsv" if tsv else "pc.jsonl", reference or self.records("pc"), tsv)
        cand = self.save("ps5.jsonl", candidate or self.records("ps5"))
        out = self.root / "result.json"
        diff = self.root / "diff.tsv"
        result = subprocess.run([sys.executable, str(BASE / "compare.py"), "--manifest", str(self.manifest),
                                 "--reference", str(ref), "--candidate", str(cand),
                                 "--json", str(out), "--tsv", str(diff)], capture_output=True, text=True)
        report = json.loads(out.read_text()) if out.exists() else {}
        rows = []
        if diff.exists():
            with diff.open() as stream:
                rows = list(csv.DictReader(stream, delimiter="\t"))
        return result, report, rows

    def test_complete_real_engine_records_match(self):
        # Removing the complete-case/real-engine acceptance path must fail this test.
        result, report, rows = self.compare()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(report.get("verdict"), "match")
        self.assertEqual([row["verdict"] for row in rows], ["match", "match"])

    def test_missing_pc_reference_is_pending(self):
        # Treating an absent PC path as equality must fail this test.
        cand = self.save("candidate.jsonl", self.records("ps5"))
        out = self.root / "pending.json"
        result = subprocess.run([sys.executable, str(BASE / "compare.py"), "--manifest", str(self.manifest),
                                 "--candidate", str(cand), "--json", str(out)], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(json.loads(out.read_text())["verdict"], "reference_pending")

    def test_missing_duplicate_extra_and_wrong_order_reject(self):
        # Joining cases by ID while ignoring the expected sequence/coverage must fail.
        mutations = [self.records("ps5") for _ in range(4)]
        del mutations[0][2]
        mutations[1].insert(2, copy.deepcopy(mutations[1][1]))
        mutations[2][2]["id"] = "unknown"
        mutations[3][1], mutations[3][2] = mutations[3][2], mutations[3][1]
        for records in mutations:
            with self.subTest(records=records):
                result, report, _ = self.compare(candidate=records)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(report.get("verdict"), "invalid_records")

    def test_host_or_unproved_cases_never_match(self):
        # Promoting a fake or a skipped/observed case to equality must fail.
        for status in ["skipped", "unavailable", "observed", "fixture-pass"]:
            pc, ps5 = self.records("pc"), self.records("ps5")
            pc[1]["status"] = ps5[1]["status"] = status
            result, report, _ = self.compare(pc, ps5)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(report.get("verdict"), "incomplete")
        pc, ps5 = self.records("pc"), self.records("ps5")
        pc[0]["evidence"] = "host-fixture"
        pc[1]["evidence"] = "host-fixture"
        result, report, _ = self.compare(pc, ps5)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(report.get("verdict"), "incomplete")

    def test_substantive_numeric_and_error_differences_survive(self):
        # Blanket numeric or hex-address erasure must fail.
        ps5 = self.records("ps5")
        ps5[2]["value"]["readback"] = 14
        result, report, rows = self.compare(candidate=ps5)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report.get("verdict"), "different")
        self.assertEqual(rows[1]["verdict"], "different")
        pc, ps5 = self.records("pc"), self.records("ps5")
        pc[1].update(status="failed", error="EFAULT at 0x1000")
        ps5[1].update(status="failed", error="EFAULT at 0x2000")
        result, report, _ = self.compare(pc, ps5)
        self.assertEqual(result.returncode, 1)
        self.assertIn("EFAULT at 0x2000", json.dumps(report))

    def test_only_explicit_unstable_metadata_is_normalized(self):
        # Dropping volatile values outside the whitelist or hiding ordinary payloads must fail.
        pc, ps5 = self.records("pc"), self.records("ps5")
        pc[1]["volatile"] = {"object_address": "0x1000", "duration_ms": 2, "identity": "pc-object"}
        ps5[1]["volatile"] = {"object_address": "0x2000", "duration_ms": 9, "identity": "ps5-object"}
        result, report, _ = self.compare(pc, ps5)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(report.get("normalized_fields"), ["volatile.duration_ms", "volatile.identity", "volatile.object_address"])
        ps5[1]["volatile"]["result"] = False
        result, report, _ = self.compare(pc, ps5)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(report.get("verdict"), "invalid_records")
        pc, ps5 = self.records("pc"), self.records("ps5")
        pc[1]["value"]["address"] = "0x1000"
        ps5[1]["value"]["address"] = "0x2000"
        result, report, _ = self.compare(pc, ps5)
        self.assertEqual(result.returncode, 1)

    def test_false_attestation_failed_cleanup_or_absent_end_reject(self):
        # A matching payload must not conceal missing prerequisites or a partial stream.
        pc, ps5 = self.records("pc"), self.records("ps5")
        pc[0]["attestations"]["independent_thread_witness"] = False
        result, report, _ = self.compare(pc, ps5)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(report.get("verdict"), "incomplete")
        pc, ps5 = self.records("pc"), self.records("ps5")
        pc[2]["status"] = ps5[2]["status"] = "failed"
        pc[2]["error"] = ps5[2]["error"] = "cleanup failed"
        result, report, _ = self.compare(pc, ps5)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report.get("verdict"), "different")
        ps5 = self.records("ps5")[:-1]
        result, report, _ = self.compare(candidate=ps5)
        self.assertEqual(result.returncode, 2)
        self.assertEqual(report.get("verdict"), "invalid_records")

    def test_provenance_and_context_mismatch_are_substantive(self):
        # Ignoring game/content/context or release identity must fail.
        for field in ["content_build", "scenario_id", "context_id", "mod_release_sha256", "helper_sha256", "script_sha256"]:
            ps5 = self.records("ps5")
            ps5[0][field] = "e" * 64 if field.endswith("sha256") else "different"
            result, report, _ = self.compare(candidate=ps5)
            self.assertEqual(result.returncode, 1, field)
            self.assertEqual(report.get("verdict"), "different")

    def test_tsv_record_input_is_lossless(self):
        # Parsing TSV as approximate string columns must fail on preserved nested JSON.
        result, report, _ = self.compare(tsv=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(report.get("verdict"), "match")

    def test_equal_wrong_script_hash_cannot_match_manifest(self):
        # Equal capture labels from a different runner must not satisfy this frozen plan.
        pc, ps5 = self.records("pc"), self.records("ps5")
        pc[0]["script_sha256"] = ps5[0]["script_sha256"] = "9" * 64
        result, report, _ = self.compare(pc, ps5)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report.get("verdict"), "different")
        self.assertTrue(any("manifest mismatch: script_sha256" in issue for issue in report["different"]))

    def test_nonfinite_or_unknown_status_is_invalid(self):
        # Accepting permissive NaN JSON or unknown parity status must fail.
        for field, value in [("status", "unknown"), ("value", {"bad": float("nan")})]:
            ps5 = self.records("ps5")
            ps5[1][field] = value
            result, report, _ = self.compare(candidate=ps5)
            self.assertEqual(result.returncode, 2)
            self.assertEqual(report.get("verdict"), "invalid_records")

    def test_duplicate_json_keys_are_not_silently_overwritten(self):
        # Permissive JSON last-key-wins parsing could turn failed into proved.
        pc = self.save("pc.jsonl", self.records("pc"))
        cand = self.save("candidate.jsonl", self.records("ps5"))
        cand.write_text(cand.read_text().replace('"status": "proved"', '"status": "failed", "status": "proved"', 1))
        out = self.root / "duplicate.json"
        result = subprocess.run([sys.executable, str(BASE / "compare.py"), "--manifest", str(self.manifest),
                                 "--reference", str(pc), "--candidate", str(cand), "--json", str(out)],
                                capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("duplicate JSON key", json.loads(out.read_text())["error"])

    def test_boolean_and_numeric_payloads_do_not_compare_equal(self):
        # Python considers True == 1; parity must retain JSON/Lua value types.
        ps5 = self.records("ps5")
        ps5[1]["value"]["roundtrip"] = 1
        result, report, _ = self.compare(candidate=ps5)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(report.get("verdict"), "different")

    def test_malformed_container_types_and_boolean_counts_have_durable_invalid_reports(self):
        # Guard/container assumptions and bool==int must never crash or pass schema validation.
        for change in ["root", "case", "guard", "selected", "order", "count"]:
            with self.subTest(change=change):
                self.setUp_manifest()
                if change == "root":
                    self.manifest.write_text("[]")
                elif change == "case":
                    plan = json.loads(self.manifest.read_text())
                    plan["cases"][0] = []
                    self.manifest.write_text(json.dumps(plan))
                records = self.records("ps5")
                if change == "guard": records[1]["guard"] = None
                if change == "selected": records[0]["selected_case_ids"] = [[]]
                if change == "order": records[1]["order"] = True
                if change == "count": records[-1]["case_count"] = True
                result, report, _ = self.compare(candidate=records)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(report.get("verdict"), "invalid_records")
                self.assertNotIn("Traceback", result.stderr)

    def setUp_manifest(self):
        self.manifest.write_text(json.dumps({"schema": "modbridge.ue4ss-conformance-plan/1", "suite_id": "test-suite",
            "release_sha256": "a" * 64, "helper_sha256": "b" * 64, "script_sha256": "d" * 64,
            "cases": [{"id": "one", "api": "FName", "order": 1},
                      {"id": "two", "api": "UObject.__index / __newindex", "order": 2}]}))


if __name__ == "__main__":
    unittest.main()
