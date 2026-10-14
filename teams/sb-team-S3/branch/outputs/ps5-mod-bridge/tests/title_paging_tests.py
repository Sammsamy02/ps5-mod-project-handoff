"""Focused immutable title paging tests; no provider, account, or game access."""
import json
from pathlib import Path
import sys
import threading
import time
import unittest
from unittest import mock

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "companion"))
import modbridge_inventory as inventory_module


def metadata(value, source="owned-fixture"):
    return {"value": value, "status": "observed" if value is not None else "unavailable",
            "source": source if value is not None else None}


def measured_fingerprint(value="a" * 64, *, byte_length=128):
    return {"algorithm": "sha256", "value": value, "confidence": "exact_bytes",
            "source": "owned-byte-measurement",
            "provenance": {"method": "sha256_exact_bytes", "byte_length": byte_length,
                           "complete": True, "artifact_kind": "content"}}


def record(title_id="OWNED_00001", *, display_name="Owned title", fingerprint=None,
           build_version="1.2.3"):
    return {"title_id": metadata(title_id), "display_name": metadata(display_name),
            "build_version": metadata(build_version), "build_fingerprint": fingerprint}


def capture(items, *, status="observed", complete=True, source_scope="fixture_processes",
            reason=None, issues=None):
    return {"status": status, "complete": complete, "source_scope": source_scope,
            "reason": reason, "issues": issues or [], "items": items}


class TitlePagingTests(unittest.TestCase):
    def require_api(self):
        self.assertTrue(callable(getattr(inventory_module.TitleInventory, "page", None)),
                        "TitleInventory.page is not implemented")
        self.assertTrue(callable(getattr(inventory_module.TitleInventory, "current", None)),
                        "TitleInventory.current is not implemented")

    def page_value(self, inventory, cursor=None, *, refresh=False):
        raw = inventory.page(cursor, refresh=refresh)
        self.assertIs(type(raw), bytes)
        self.assertLessEqual(len(raw), 8191)
        self.assertTrue(all(byte < 128 for byte in raw))
        value = json.loads(raw)
        canonical = json.dumps(value, ensure_ascii=True, separators=(",", ":"),
                               sort_keys=True, allow_nan=False).encode("ascii")
        self.assertEqual(raw, canonical)
        return raw, value

    def test_128_generic_records_page_atomically_while_legacy_v1_size_errors(self):
        self.require_api()
        calls = []
        rows = [record(f"TITLE{index:03d}", display_name="N" * 256,
                       build_version="V" * 128,
                       fingerprint=measured_fingerprint(byte_length=0x7fffffffffffffff))
                for index in range(128)]
        provider_capture = capture(rows, issues=[{"code": "fixture_warning"}])
        def provider():
            calls.append(None)
            return provider_capture
        inventory = inventory_module.TitleInventory(provider)

        all_pages, all_items, cursor = [], [], None
        while True:
            raw, value = self.page_value(inventory, cursor)
            self.assertEqual(value["schema_version"], 2)
            self.assertEqual(value["kind"], "title_inventory_page")
            self.assertEqual(value["snapshot"]["status"], "observed")
            self.assertTrue(value["snapshot"]["complete"])
            self.assertEqual(value["snapshot"]["reason"], None)
            self.assertEqual(value["snapshot"]["issues"],
                             [{"code": "provider_observation_incomplete"}] if not all_pages else [])
            self.assertEqual(value["page"]["index"], len(all_pages))
            self.assertEqual(value["page"]["item_offset"], len(all_items))
            self.assertEqual(value["page"]["total_count"], 128)
            self.assertGreaterEqual(len(value["items"]), 1)
            self.assertLessEqual(len(value["items"]), 8)
            self.assertEqual(len(raw), len(inventory.page(cursor)))
            all_pages.append(value)
            all_items.extend(value["items"])
            cursor = value["page"]["next_cursor"]
            if cursor is None:
                break
            self.assertRegex(cursor, r"^[a-f0-9]{32}$")

        self.assertEqual(len(calls), 1)
        self.assertEqual(len(all_items), 128)
        self.assertEqual(all_pages[-1]["page"]["index"] + 1,
                         all_pages[-1]["page"]["page_count"])
        self.assertEqual({page["page"]["page_count"] for page in all_pages},
                         {len(all_pages)})
        self.assertEqual(all_items[0]["build_fingerprint"]["provenance"]["byte_length"],
                         "9223372036854775807")

        legacy = inventory.snapshot()
        self.assertEqual(legacy["schema_version"], 1)
        self.assertEqual(legacy["status"], "error")
        self.assertEqual(legacy["reason"], "invalid_provider_capture")
        self.assertEqual(len(calls), 1)
        reference = {"snapshot_id": all_pages[0]["snapshot"]["snapshot_id"],
                     "title_key": all_items[0]["title_key"]}
        resolved = inventory.resolve(reference)
        self.assertEqual(resolved["title_id"]["value"], "TITLE000")
        self.assertEqual(resolved["build_fingerprint"]["provenance"]["byte_length"],
                         0x7fffffffffffffff)

        current_raw = inventory.current(reference["snapshot_id"])
        self.assertIs(type(current_raw), bytes)
        self.assertLessEqual(len(current_raw), 8191)
        current = json.loads(current_raw)
        self.assertEqual(current, {"schema_version": 2, "kind": "title_inventory_current",
                                   "snapshot_id": reference["snapshot_id"], "current": True})
        self.assertEqual(current_raw, json.dumps(current, ensure_ascii=True, separators=(",", ":"),
                                                sort_keys=True).encode("ascii"))

    def test_empty_unavailable_and_partial_v2_are_terminal_without_prefix(self):
        self.require_api()
        unavailable = inventory_module.TitleInventory()
        _, page = self.page_value(unavailable)
        self.assertEqual(page["snapshot"], {
            "status": "unavailable", "complete": False, "snapshot_id": None,
            "source_scope": "none", "reason": "platform_provider_unavailable",
            "issues": []})
        self.assertIsNone(page["page"])
        self.assertEqual(page["items"], [])

        partial = inventory_module.TitleInventory(lambda: capture(
            [record()], status="partial", complete=False))
        _, partial_page = self.page_value(partial)
        self.assertEqual(partial_page["snapshot"]["status"], "partial")
        self.assertFalse(partial_page["snapshot"]["complete"])
        self.assertIsNone(partial_page["snapshot"]["snapshot_id"])
        self.assertEqual(partial_page["snapshot"]["reason"], "provider_observation_incomplete")
        self.assertIsNone(partial_page["page"])
        self.assertEqual(partial_page["items"], [])
        legacy_partial = partial.snapshot()
        self.assertEqual(len(legacy_partial["items"]), 1)
        reference = {"snapshot_id": legacy_partial["snapshot_id"],
                     "title_key": legacy_partial["items"][0]["title_key"]}
        with self.assertRaises(inventory_module.TitleInventoryError) as stale:
            partial.resolve(reference)
        self.assertEqual(stale.exception.code, "stale_target_ref")

        empty = inventory_module.TitleInventory(lambda: capture([]))
        _, empty_page = self.page_value(empty)
        self.assertEqual(empty_page["snapshot"]["status"], "observed")
        self.assertTrue(empty_page["snapshot"]["complete"])
        self.assertRegex(empty_page["snapshot"]["snapshot_id"], r"^[a-f0-9]{32}$")
        self.assertEqual(empty_page["page"], {"index": 0, "page_count": 1, "item_offset": 0,
                                               "total_count": 0, "next_cursor": None})
        self.assertEqual(empty_page["items"], [])

    def test_cursor_refresh_and_current_errors_are_constant_and_typed(self):
        self.require_api()
        inventory = inventory_module.TitleInventory(lambda: capture([record()]))
        first_raw, first = self.page_value(inventory)
        snapshot_id = first["snapshot"]["snapshot_id"]
        with self.assertRaises(inventory_module.TitleInventoryError) as malformed:
            inventory.page("A" * 32)
        self.assertEqual(malformed.exception.code, "invalid_input")
        with self.assertRaises(inventory_module.TitleInventoryError) as unknown:
            inventory.page("f" * 32)
        self.assertEqual(unknown.exception.code, "inventory_page_stale")
        with self.assertRaises(inventory_module.TitleInventoryError) as mixed:
            inventory.page("f" * 32, refresh=True)
        self.assertEqual(mixed.exception.code, "invalid_input")
        with self.assertRaises(inventory_module.TitleInventoryError) as malformed_current:
            inventory.current("bad")
        self.assertEqual(malformed_current.exception.code, "invalid_input")

        second_raw, second = self.page_value(inventory, refresh=True)
        self.assertNotEqual(first["snapshot"]["snapshot_id"], second["snapshot"]["snapshot_id"])
        self.assertNotEqual(first_raw, second_raw)
        with self.assertRaises(inventory_module.TitleInventoryError) as stale:
            inventory.current(snapshot_id)
        self.assertEqual(stale.exception.code, "inventory_page_stale")

    def test_continuations_and_current_never_trigger_initial_capture(self):
        self.require_api()
        calls = []
        inventory = inventory_module.TitleInventory(
            lambda: calls.append(None) or capture([record()]))
        for operation in (lambda: inventory.page("a" * 32),
                          lambda: inventory.current("b" * 32)):
            with self.assertRaises(inventory_module.TitleInventoryError) as stale:
                operation()
            self.assertEqual(stale.exception.code, "inventory_page_stale")
        self.assertEqual(calls, [])

    def test_provider_runs_outside_lock_and_refresh_ticket_replaces_cache_atomically(self):
        self.require_api()
        calls = []
        def initial():
            calls.append("initial")
            return capture([record("TITLEINITIAL")])
        inventory = inventory_module.TitleInventory(initial)
        _, first = self.page_value(inventory)
        old_snapshot_id = first["snapshot"]["snapshot_id"]
        old_ref = {"snapshot_id": old_snapshot_id, "title_key": first["items"][0]["title_key"]}

        entered, release = threading.Event(), threading.Event()
        def blocking():
            calls.append("blocking")
            entered.set()
            if not release.wait(3):
                raise RuntimeError("fixture release timeout")
            return capture([record("TITLEBLOCKING")])
        result, errors = [], []
        inventory.provider = blocking
        def refresh_worker():
            try:
                result.append(inventory.page(refresh=True))
            except Exception as error:  # surfaced in the main test thread below
                errors.append(error)
        worker = threading.Thread(target=refresh_worker)
        worker.start()
        try:
            self.assertTrue(entered.wait(1))
            started = time.monotonic()
            with self.assertRaises(inventory_module.TitleInventoryError) as in_progress:
                inventory.page()
            self.assertLess(time.monotonic() - started, 0.5)
            self.assertEqual(in_progress.exception.code, "inventory_capture_in_progress")
            with self.assertRaises(inventory_module.TitleInventoryError) as busy:
                inventory.page(refresh=True)
            self.assertEqual(busy.exception.code, "inventory_refresh_busy")
            with self.assertRaises(inventory_module.TitleInventoryError) as current_busy:
                inventory.current(old_snapshot_id)
            self.assertEqual(current_busy.exception.code, "inventory_capture_in_progress")
            with self.assertRaises(inventory_module.TitleInventoryError) as resolve_busy:
                inventory.resolve(old_ref)
            self.assertEqual(resolve_busy.exception.code, "inventory_capture_in_progress")
            inventory.provider = lambda: capture([record("TITLEREPLACED")])
        finally:
            release.set()
            worker.join(4)
        self.assertFalse(worker.is_alive())
        self.assertEqual(errors, [])
        first_refresh = json.loads(result[0])
        self.assertEqual(first_refresh["items"][0]["title_id"]["value"], "TITLEBLOCKING")
        with self.assertRaises(inventory_module.TitleInventoryError) as stale:
            inventory.current(old_snapshot_id)
        self.assertEqual(stale.exception.code, "inventory_page_stale")
        _, replacement = self.page_value(inventory, refresh=True)
        self.assertEqual(replacement["items"][0]["title_id"]["value"], "TITLEREPLACED")
        self.assertEqual(calls, ["initial", "blocking"])

    def test_failed_refresh_revokes_prior_generation_and_sanitizes_provider_error(self):
        self.require_api()
        inventory = inventory_module.TitleInventory(lambda: capture([record()]))
        _, first = self.page_value(inventory)
        old_id = first["snapshot"]["snapshot_id"]
        inventory.provider = lambda: (_ for _ in ()).throw(RuntimeError("private path detail"))
        _, failed = self.page_value(inventory, refresh=True)
        self.assertEqual(failed["snapshot"]["status"], "error")
        self.assertEqual(failed["snapshot"]["reason"], "provider_error")
        self.assertIsNone(failed["snapshot"]["snapshot_id"])
        self.assertIsNone(failed["page"])
        self.assertEqual(failed["items"], [])
        self.assertNotIn("private path", repr(failed))
        with self.assertRaises(inventory_module.TitleInventoryError) as stale:
            inventory.current(old_id)
        self.assertEqual(stale.exception.code, "inventory_page_stale")

    def test_capture_page_and_page_count_caps_fail_closed_at_boundaries(self):
        self.require_api()
        one = capture([record()])
        default_inventory = inventory_module.TitleInventory(lambda: one)
        exact_page_bytes = default_inventory.page()
        exact_capture_size = len(default_inventory._capture_bytes)

        with mock.patch.object(inventory_module, "MAX_CAPTURE_BYTES", exact_capture_size):
            exact = inventory_module.TitleInventory(lambda: one)
            _, accepted = self.page_value(exact)
            self.assertEqual(accepted["snapshot"]["status"], "observed")
        with mock.patch.object(inventory_module, "MAX_CAPTURE_BYTES", exact_capture_size - 1):
            over_capture = inventory_module.TitleInventory(lambda: one)
            _, rejected = self.page_value(over_capture)
            self.assertEqual(rejected["snapshot"]["reason"], "inventory_resource_limit")
            self.assertIsNone(rejected["snapshot"]["snapshot_id"])
            self.assertIsNone(rejected["page"])
            self.assertEqual(rejected["items"], [])

        with mock.patch.object(inventory_module, "MAX_PAGE_BYTES", len(exact_page_bytes)):
            page_exact = inventory_module.TitleInventory(lambda: one)
            self.assertEqual(len(page_exact.page()), len(exact_page_bytes))
        with mock.patch.object(inventory_module, "MAX_PAGE_BYTES", len(exact_page_bytes) - 1):
            page_over = inventory_module.TitleInventory(lambda: one)
            _, rejected_page = self.page_value(page_over)
            self.assertEqual(rejected_page["snapshot"]["reason"], "inventory_resource_limit")
            self.assertEqual(rejected_page["items"], [])
            self.assertIsNone(rejected_page["page"])

        with mock.patch.object(inventory_module, "MAX_PAGES", 1):
            too_many_pages = inventory_module.TitleInventory(
                lambda: capture([record(f"TITLE{index:03d}") for index in range(9)]))
            _, rejected_pages = self.page_value(too_many_pages)
            self.assertEqual(rejected_pages["snapshot"]["reason"], "inventory_resource_limit")
            self.assertEqual(rejected_pages["items"], [])

    def test_incremental_capture_limit_fails_before_next_record_and_text_is_bounded(self):
        self.require_api()
        one_record = capture([record()])
        baseline = inventory_module.TitleInventory(lambda: one_record)
        baseline.page()
        exact_capture_size = len(baseline._capture_bytes)

        bad_second_record = record("TITLESECOND")
        bad_second_record["unexpected"] = "must not be reached"
        two_records = capture([record(), bad_second_record])
        with mock.patch.object(inventory_module, "MAX_CAPTURE_BYTES", exact_capture_size - 1):
            _, page = self.page_value(inventory_module.TitleInventory(lambda: two_records))
        self.assertEqual(page["snapshot"]["reason"], "inventory_resource_limit")
        self.assertEqual(page["items"], [])
        with self.assertRaises(ValueError):
            inventory_module._utf8_text("x" * (inventory_module.MAX_DISPLAY_NAME_BYTES + 1),
                                        inventory_module.MAX_DISPLAY_NAME_BYTES)

    def test_v2_issue_codes_are_closed_while_legacy_projection_keeps_safe_code(self):
        self.require_api()
        inventory = inventory_module.TitleInventory(lambda: capture(
            [record()], issues=[{"code": "fixture_warning"}, {"code": "source_changed"}]))
        _, page = self.page_value(inventory)
        self.assertEqual(page["snapshot"]["issues"], [
            {"code": "provider_observation_incomplete"}, {"code": "source_changed"}])
        legacy = inventory.snapshot()
        self.assertEqual(legacy["issues"], [{"code": "fixture_warning"}, {"code": "source_changed"}])

    def test_unknown_safe_terminal_reason_is_preserved_in_v1_and_closed_in_v2(self):
        self.require_api()
        inventory = inventory_module.TitleInventory(lambda: capture(
            [], status="error", complete=False, reason="fixture_specific_failure"))
        _, page = self.page_value(inventory)
        self.assertEqual(page["snapshot"]["reason"], "provider_observation_incomplete")
        self.assertEqual(inventory.snapshot()["reason"], "fixture_specific_failure")

    def test_token_collision_budget_fails_closed_without_selectable_generation(self):
        self.require_api()
        with mock.patch.object(inventory_module.secrets, "token_hex", return_value="a" * 32):
            inventory = inventory_module.TitleInventory(lambda: capture([record()]))
            _, failed = self.page_value(inventory)
        self.assertEqual(failed["snapshot"]["status"], "error")
        self.assertEqual(failed["snapshot"]["reason"], "provider_error")
        self.assertIsNone(failed["snapshot"]["snapshot_id"])
        self.assertIsNone(failed["page"])
        self.assertEqual(failed["items"], [])


if __name__ == "__main__":
    unittest.main()
