"""Focused trusted title inventory fixtures; no platform, account, or game access."""
from pathlib import Path
import sys
import unittest

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "companion"))
try:
    import modbridge_inventory as inventory_module
except ImportError:
    inventory_module = None


def metadata(value, source="owned-fixture"):
    return {"value": value, "status": "observed" if value is not None else "unavailable",
            "source": source if value is not None else None}


def measured_fingerprint(value="a" * 64, *, artifact_kind="executable", complete=True,
                         byte_length=128, algorithm="sha256", confidence="exact_bytes"):
    return {"algorithm": algorithm, "value": value, "confidence": confidence,
            "source": "owned-byte-measurement",
            "provenance": {"method": "sha256_exact_bytes", "byte_length": byte_length,
                           "complete": complete, "artifact_kind": artifact_kind}}


def record(title_id="PPSA00001", *, fingerprint=None, display_name="Owned title",
           build_version="1.2.3"):
    return {"title_id": metadata(title_id), "display_name": metadata(display_name),
            "build_version": metadata(build_version), "build_fingerprint": fingerprint}


def capture(items, *, status="observed", complete=True, source_scope="fixture_processes",
            reason=None, issues=None):
    return {"status": status, "complete": complete, "source_scope": source_scope,
            "reason": reason, "issues": issues or [], "items": items}


class TitleInventoryTests(unittest.TestCase):
    def module(self):
        self.assertIsNotNone(inventory_module,
            "modbridge_inventory.py is missing the trusted snapshot contract")
        return inventory_module

    def test_default_is_explicitly_unavailable_and_has_no_selectable_reference(self):
        module = self.module()
        inventory = module.TitleInventory()
        first = inventory.snapshot()
        self.assertEqual(first["schema_version"], 1)
        self.assertEqual(first["status"], "unavailable")
        self.assertFalse(first["complete"])
        self.assertIsNone(first["snapshot_id"])
        self.assertEqual(first["items"], [])
        self.assertEqual(first["reason"], "platform_provider_unavailable")
        with self.assertRaises(module.TitleInventoryError) as caught:
            inventory.resolve({"snapshot_id": "0" * 32, "title_key": "1" * 32})
        self.assertEqual(caught.exception.code, "stale_target_ref")

    def test_complete_capture_generates_opaque_keys_and_measured_hash_only_projection(self):
        module = self.module()
        calls = []
        raw_capture = capture([record("PPSA00001", fingerprint=measured_fingerprint()),
                               record("PPSA00002", fingerprint=None, display_name=None,
                                      build_version=None)])
        def provider():
            calls.append(None)
            return raw_capture
        inventory = module.TitleInventory(provider)
        snapshot = inventory.snapshot()
        self.assertEqual(snapshot["status"], "observed")
        self.assertTrue(snapshot["complete"])
        self.assertRegex(snapshot["snapshot_id"], r"^[a-f0-9]{32}$")
        self.assertEqual(len(snapshot["items"]), 2)
        refs = [{"snapshot_id": snapshot["snapshot_id"], "title_key": item["title_key"]}
                for item in snapshot["items"]]
        self.assertEqual(len({ref["title_key"] for ref in refs}), len(refs))
        for ref in refs:
            self.assertRegex(ref["title_key"], r"^[a-f0-9]{32}$")
        self.assertNotIn("snapshot_id", raw_capture)
        self.assertNotIn("title_key", raw_capture["items"][0])
        selected = inventory.resolve(refs[0])
        self.assertEqual(selected["target_ref"], refs[0])
        self.assertEqual(selected["fingerprints"], {"build_sha256": "a" * 64})
        self.assertEqual(inventory.resolve(refs[1])["fingerprints"], {})
        self.assertEqual(len(calls), 1)

    def test_snapshots_and_resolved_records_are_copies_and_refresh_invalidates_old_refs(self):
        module = self.module()
        calls = []
        def provider():
            calls.append(None)
            return capture([record("PPSA00001", fingerprint=measured_fingerprint(
                value=("a" if len(calls) == 1 else "b") * 64))])
        inventory = module.TitleInventory(provider)
        first = inventory.snapshot()
        reference = {"snapshot_id": first["snapshot_id"], "title_key": first["items"][0]["title_key"]}
        first["items"][0]["title_id"]["value"] = "MUTATED"
        resolved = inventory.resolve(reference)
        resolved["title_id"]["value"] = "MUTATED AGAIN"
        self.assertEqual(inventory.snapshot()["items"][0]["title_id"]["value"], "PPSA00001")
        self.assertEqual(inventory.resolve(reference)["fingerprints"], {"build_sha256": "a" * 64})
        second = inventory.snapshot(refresh=True)
        self.assertNotEqual(second["snapshot_id"], reference["snapshot_id"])
        with self.assertRaises(module.TitleInventoryError) as caught:
            inventory.resolve(reference)
        self.assertEqual(caught.exception.code, "stale_target_ref")
        self.assertEqual(inventory.resolve({"snapshot_id": second["snapshot_id"],
                                            "title_key": second["items"][0]["title_key"]})["fingerprints"],
                         {"build_sha256": "b" * 64})

    def test_reference_validation_is_exact_and_typed(self):
        module = self.module()
        valid = {"snapshot_id": "a" * 32, "title_key": "b" * 32}
        self.assertEqual(module.validate_reference(valid), valid)
        bad = [None, [], {}, {**valid, "fingerprints": {"build_sha256": "c" * 64}},
               {"snapshot_id": "A" * 32, "title_key": "b" * 32},
               {"snapshot_id": "a" * 31, "title_key": "b" * 32},
               {"snapshot_id": "a" * 32, "title_key": 7}]
        for reference in bad:
            with self.subTest(reference=reference):
                with self.assertRaises(module.TitleInventoryError) as caught:
                    module.validate_reference(reference)
                self.assertEqual(caught.exception.code, "invalid_input")
        inventory = module.TitleInventory(lambda: capture([record()]))
        inventory.snapshot()
        with self.assertRaises(module.TitleInventoryError) as caught:
            inventory.resolve(valid)
        self.assertEqual(caught.exception.code, "stale_target_ref")

    def test_partial_and_empty_snapshots_are_distinct_and_not_resolvable(self):
        module = self.module()
        partial = module.TitleInventory(lambda: capture([record()], status="partial", complete=False))
        snapshot = partial.snapshot()
        self.assertEqual(snapshot["status"], "partial")
        self.assertFalse(snapshot["complete"])
        reference = {"snapshot_id": snapshot["snapshot_id"], "title_key": snapshot["items"][0]["title_key"]}
        with self.assertRaises(module.TitleInventoryError) as caught:
            partial.resolve(reference)
        self.assertEqual(caught.exception.code, "stale_target_ref")
        empty = module.TitleInventory(lambda: capture([])).snapshot()
        self.assertEqual(empty["status"], "observed")
        self.assertTrue(empty["complete"])
        self.assertRegex(empty["snapshot_id"], r"^[a-f0-9]{32}$")
        self.assertEqual(empty["items"], [])

    def test_invalid_provider_capture_is_sanitized_without_partial_inventory(self):
        module = self.module()
        bad_captures = [
            {**capture([record()]), "snapshot_id": "browser-controlled"},
            capture([{"title_key": "browser-controlled", **record()}]),
            capture([record(title_id="bad\nvalue")]),
            capture([record(fingerprint=measured_fingerprint(value="not-a-hash"))]),
            capture([record(fingerprint=measured_fingerprint(complete=False))]),
            capture([record(fingerprint=measured_fingerprint(artifact_kind="unknown"))]),
            capture([record(fingerprint=measured_fingerprint(byte_length=0))]),
            capture([record(fingerprint=measured_fingerprint(confidence="metadata_only"))]),
            capture([record(display_name="x" * 257)]),
        ]
        for bad in bad_captures:
            with self.subTest(bad=bad):
                snapshot = module.TitleInventory(lambda bad=bad: bad).snapshot()
                self.assertEqual(snapshot["status"], "error")
                self.assertFalse(snapshot["complete"])
                self.assertIsNone(snapshot["snapshot_id"])
                self.assertEqual(snapshot["items"], [])
                self.assertEqual(snapshot["reason"], "invalid_provider_capture")

    def test_provider_exception_replaces_old_snapshot_with_safe_error(self):
        module = self.module()
        calls = []
        def provider():
            calls.append(None)
            if len(calls) == 1:
                return capture([record()])
            raise RuntimeError("private path or raw provider detail")
        inventory = module.TitleInventory(provider)
        first = inventory.snapshot()
        old_ref = {"snapshot_id": first["snapshot_id"], "title_key": first["items"][0]["title_key"]}
        failed = inventory.snapshot(refresh=True)
        self.assertEqual(failed["status"], "error")
        self.assertFalse(failed["complete"])
        self.assertIsNone(failed["snapshot_id"])
        self.assertEqual(failed["items"], [])
        self.assertEqual(failed["reason"], "provider_error")
        with self.assertRaises(module.TitleInventoryError) as caught:
            inventory.resolve(old_ref)
        self.assertEqual(caught.exception.code, "stale_target_ref")
        self.assertNotIn("private path", repr(failed))

    def test_record_and_issue_limits_fail_closed(self):
        module = self.module()
        too_many = capture([record(f"TITLE{i}") for i in range(129)])
        snapshot = module.TitleInventory(lambda: too_many).snapshot()
        self.assertEqual(snapshot["status"], "error")
        self.assertFalse(snapshot["complete"])
        self.assertEqual(snapshot["items"], [])
        self.assertEqual(snapshot["reason"], "invalid_provider_capture")
        too_many_issues = capture([], issues=[{"code": f"issue_{i}"} for i in range(33)])
        issue_snapshot = module.TitleInventory(lambda: too_many_issues).snapshot()
        self.assertEqual(issue_snapshot["status"], "error")
        self.assertEqual(issue_snapshot["items"], [])
        too_large_wire = capture([record(f"TITLE{i}", display_name="x" * 256) for i in range(32)])
        wire_snapshot = module.TitleInventory(lambda: too_large_wire).snapshot()
        self.assertEqual(wire_snapshot["status"], "error")
        self.assertEqual(wire_snapshot["items"], [])


if __name__ == "__main__":
    unittest.main()
