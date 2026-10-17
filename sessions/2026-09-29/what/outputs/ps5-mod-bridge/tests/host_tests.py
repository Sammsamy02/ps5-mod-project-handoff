"""Owned local HTTP fixtures; never contact Nexus or a console."""
import dataclasses
import hashlib
import http.client
import json
import os
from pathlib import Path
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "companion"))
import modbridge_nexus as nexus
import modbridge_host as host


class FakeAccount:
    def __init__(self):
        self.requests = []
        self.download_count = 0
        self.disconnected = False

    def status(self):
        return {"connected": True, "credentials_present": True,
                "live_service": "verified", "account": {"user_id": 42,
                "display_name": "Owned fixture", "is_premium": True,
                "is_supporter": False}, "access_token": "do-not-expose"}

    def client(self):
        return self

    def games(self):
        return [{"domain_name": "ownedgame", "name": "Owned Game", "id": 1}]

    def latest_mods(self, game):
        self.requests.append(game)
        return [{"mod_id": 1, "name": "Owned mod", "summary": "A fixture"}]

    def files(self, game, mod):
        return [{"file_id": 2, "name": "Owned file", "version": "1"}]

    def download_links(self, game, mod, file):
        self.download_count += 1
        source = nexus.DownloadSource(game, mod, file)
        return [nexus.DownloadLink("https://owned.example/archive?secret=private", "Owned", "O", source)]

    def disconnect(self):
        self.disconnected = True

    def complete_sign_in(self, session, callback):
        session.consume_callback(callback)
        return nexus.UserProfile(42, "Owned fixture", True, False)


class HostTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.account = FakeAccount()

        def download(link, destination):
            destination.mkdir(mode=0o700, parents=True, exist_ok=True)
            path = destination / "owned.zip"
            path.write_bytes(b"owned archive")
            return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(),
                                        path.stat().st_size, link.source)

        def analyze(result, game):
            return {"analysis": {"status": "analyzed", "compatibility": "unresolved",
                                 "inventory_path": "/private/internal/path"}}

        self.app = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account, download=download, analyze=analyze)
        self.server = host.create_server(self.app)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.shutdown)
        self.port = self.server.server_address[1]
        self.origin = "http://127.0.0.1:" + str(self.port)
        self.cookie = None
        self.csrf = None

    def shutdown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)
        self.app.close()

    def request(self, method, path, body=None, headers=None):
        wire = http.client.HTTPConnection("127.0.0.1", self.port, timeout=2)
        merged = dict(headers or {})
        if self.cookie:
            merged.setdefault("Cookie", self.cookie)
        if body is not None:
            body = json.dumps(body).encode()
            merged.setdefault("Content-Type", "application/json")
        wire.request(method, path, body=body, headers=merged)
        response = wire.getresponse()
        content = response.read()
        cookie = response.getheader("Set-Cookie")
        if cookie:
            self.cookie = cookie.split(";", 1)[0]
        result = (response.status, content, dict(response.getheaders()))
        wire.close()
        return result

    def bootstrap(self):
        status, body, _ = self.request("GET", "/api/capabilities")
        self.assertEqual(status, 200)
        record = json.loads(body)
        self.csrf = record["csrf_token"]
        return record

    def mutation(self, path, body):
        return self.request("POST", path, body,
            {"Origin": self.origin, "X-CSRF-Token": self.csrf})

    def test_bootstrap_truth_and_private_identity(self):
        record = self.bootstrap()
        self.assertFalse(record["capabilities"]["deploy"])
        self.assertFalse(record["capabilities"]["prepared_translation"])
        self.assertFalse(record["capabilities"]["nexus_auth_start"])
        self.assertFalse(record["capabilities"]["detected_titles"])
        status, body, _ = self.request("GET", "/api/account")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["account"]["user_id"], 42)
        self.assertNotIn(b"do-not-expose", body)
        self.assertEqual(json.loads(self.request("GET", "/api/installed")[1])["items"], [])

    def test_session_origin_host_and_csrf_gate(self):
        self.assertEqual(self.request("GET", "/api/games")[0], 401)
        self.bootstrap()
        self.assertEqual(self.request("POST", "/api/auth/disconnect", {})[0], 403)
        self.assertEqual(self.request("POST", "/api/auth/disconnect", {},
            {"Origin": "https://unrelated.example", "X-CSRF-Token": self.csrf})[0], 403)
        self.assertEqual(self.request("POST", "/api/auth/disconnect", {},
            {"Origin": self.origin, "X-CSRF-Token": "wrong"})[0], 403)
        self.assertEqual(self.request("GET", "/api/capabilities", headers={"Host": "unrelated.example"})[0], 403)
        self.assertEqual(self.mutation("/api/auth/disconnect", {})[0], 200)
        self.assertTrue(self.account.disconnected)

    def test_catalog_is_generic_and_assets_are_allowlisted(self):
        self.bootstrap()
        self.assertEqual(json.loads(self.request("GET", "/api/games")[1])["items"][0]["domain_name"], "ownedgame")
        self.assertEqual(self.request("GET", "/api/games/newgame2099/mods")[0], 200)
        self.assertEqual(self.account.requests, ["newgame2099"])
        self.assertEqual(self.request("GET", "/../companion/nexus.py")[0], 404)
        self.assertEqual(self.request("GET", "/api/unknown")[0], 404)

    def test_missing_oauth_configuration_preserves_account(self):
        self.bootstrap()
        status, body, _ = self.mutation("/api/auth/start", {})
        self.assertEqual(status, 409)
        self.assertEqual(json.loads(body)["error"]["code"], "configuration")
        self.assertFalse(self.account.disconnected)

    def test_direct_mod_lookup_is_generic_and_uses_current_account(self):
        self.bootstrap()
        expected = {"mod_id": 9827, "game_domain": "newgame2099", "name": "Arbitrary catalog record",
                    "source_url": "https://www.nexusmods.com/newgame2099/mods/9827"}
        self.account.mod = mock.Mock(return_value=expected)
        status, body, _ = self.request("GET", "/api/games/newgame2099/mods/9827")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body), {"item": expected})
        self.account.mod.assert_called_once_with("newgame2099", 9827)
        self.assertEqual(self.account.requests, [])
        self.assertEqual(self.account.download_count, 0)

    def test_direct_mod_lookup_refuses_invalid_identity_before_provider(self):
        self.bootstrap()
        self.account.mod = mock.Mock()
        for path, code in (("/api/games/newgame2099/mods/0", 400),
                           ("/api/games/_bad/mods/9827", 400),
                           ("/api/games/newgame2099/mods/9827?url=https://other.example", 404),
                           ("/api/games/newgame2099/mods/9223372036854775808", 400)):
            with self.subTest(path=path):
                self.assertEqual(self.request("GET", path)[0], code)
        self.account.mod.assert_not_called()

    def test_direct_mod_lookup_requires_local_session_and_preserves_provider_denial(self):
        self.account.mod = mock.Mock(side_effect=nexus.NexusError("content_hidden"))
        self.assertEqual(self.request("GET", "/api/games/newgame2099/mods/9827")[0], 401)
        self.account.mod.assert_not_called()
        self.bootstrap()
        status, body, _ = self.request("GET", "/api/games/newgame2099/mods/9827")
        self.assertEqual(status, 400)
        self.assertEqual(json.loads(body)["error"]["code"], "content_hidden")
        self.account.mod.assert_called_once_with("newgame2099", 9827)

    def owned_inventory(self):
        first = {"snapshot_id": "1" * 32, "title_key": "2" * 32}
        second = {"snapshot_id": "1" * 32, "title_key": "3" * 32}
        records = {
            first["title_key"]: {"target_ref": first, "title_id": {"value": "PPSA98765"},
                "fingerprints": {"build_sha256": "a" * 64}},
            second["title_key"]: {"target_ref": second, "title_id": {"value": "PPSA54321"},
                "fingerprints": {}},
        }
        inventory = mock.Mock()
        inventory.snapshot.return_value = {"schema_version": 1, "status": "observed", "complete": True,
            "snapshot_id": first["snapshot_id"], "source_scope": "running_processes", "reason": None,
            "issues": [], "items": [{"title_key": key, **value} for key, value in records.items()]}
        def resolve(ref):
            from modbridge_inventory import TitleInventoryError
            if ref.get("snapshot_id") != first["snapshot_id"] or ref.get("title_key") not in records:
                raise TitleInventoryError("stale_target_ref")
            return dict(records[ref["title_key"]])
        inventory.resolve.side_effect = resolve
        self.app.inventory = inventory
        return inventory, first, second

    def await_job(self, identifier):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            job = self.app.jobs(identifier)
            if job["status"] in ("analyzed", "failed", "interrupted", "needs_nexus_authorization"):
                return job
            time.sleep(.01)
        self.fail("Owned job did not settle")

    def test_detected_titles_uses_independent_trusted_snapshot(self):
        self.bootstrap()
        inventory, _, _ = self.owned_inventory()
        status, body, _ = self.request("GET", "/api/detected-titles")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body), inventory.snapshot.return_value)
        inventory.snapshot.assert_called_once_with()
        self.assertEqual(self.account.requests, [])

    def test_title_refresh_requires_csrf_and_exact_empty_intent(self):
        self.bootstrap()
        inventory, _, _ = self.owned_inventory()
        self.assertEqual(self.request("POST", "/api/detected-titles/refresh", {})[0], 403)
        self.assertEqual(self.mutation("/api/detected-titles/refresh", {"fingerprints": {}})[0], 404)
        inventory.snapshot.assert_not_called()
        status, body, _ = self.mutation("/api/detected-titles/refresh", {})
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body), inventory.snapshot.return_value)
        inventory.snapshot.assert_called_once_with(refresh=True)
        self.assertEqual(self.account.download_count, 0)

    def test_target_bound_analysis_passes_only_server_resolved_context(self):
        self.bootstrap()
        _, reference, _ = self.owned_inventory()
        analyzer = mock.Mock(return_value={"analysis": {"status": "analyzed"}})
        self.app.analyze = analyzer
        intent = {"kind": "download_analyze", "game": "anothergame", "mod_id": 87,
            "file_id": 95, "idempotency_key": "bound-intent-001", "target_ref": reference}
        status, body, _ = self.mutation("/api/jobs", intent)
        self.assertEqual(status, 202)
        job = self.await_job(json.loads(body)["id"])
        self.assertEqual(job["status"], "analyzed")
        self.assertEqual(job["target_ref"], reference)
        self.assertEqual(analyzer.call_args.args[1], "anothergame")
        self.assertEqual(analyzer.call_args.args[2]["fingerprints"], {"build_sha256": "a" * 64})
        self.assertEqual(job["compatibility"], "unresolved")
        self.assertNotIn("fingerprints", job)
        saved = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        self.assertEqual(self.app._validated_receipt(saved, job["id"])["target_ref"], reference)

    def test_stale_or_browser_facts_are_refused_before_download(self):
        self.bootstrap()
        _, reference, _ = self.owned_inventory()
        base = {"kind": "download_analyze", "game": "anothergame", "mod_id": 87,
            "file_id": 95, "idempotency_key": "invalid-bound-001"}
        for ref, expected in ((dict(reference, snapshot_id="f" * 32), 409),
                (dict(reference, fingerprints={"build_sha256": "b" * 64}), 400),
                ({"snapshot_id": reference["snapshot_id"]}, 400), (None, 400)):
            with self.subTest(reference=ref):
                status, body, _ = self.mutation("/api/jobs", dict(base, target_ref=ref))
                self.assertEqual(status, expected)
                self.assertEqual(json.loads(body)["error"]["code"], "stale_target_ref" if expected == 409 else "invalid_input")
        self.assertEqual(self.account.download_count, 0)
        self.assertEqual(self.app.jobs(), [])

    def test_idempotency_binds_target_and_preserves_completed_receipt_on_restart(self):
        self.bootstrap()
        _, reference, second = self.owned_inventory()
        self.app.analyze = mock.Mock(return_value={"analysis": {"status": "analyzed"}})
        intent = {"kind": "download_analyze", "game": "anothergame", "mod_id": 87,
            "file_id": 95, "idempotency_key": "retry-bound-001", "target_ref": reference}
        status, body, _ = self.mutation("/api/jobs", intent)
        self.assertEqual(status, 202)
        original = self.await_job(json.loads(body)["id"])
        self.assertEqual(self.mutation("/api/jobs", dict(intent, target_ref=second))[0], 409)
        unbound = {key: value for key, value in intent.items() if key != "target_ref"}
        self.assertEqual(self.mutation("/api/jobs", unbound)[0], 409)
        self.app.close()
        restarted = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account)
        self.addCleanup(restarted.close)
        self.assertEqual(restarted.submit(intent), original)
        self.assertEqual(self.account.download_count, 1)

    def test_changed_target_after_download_cannot_publish_analysis(self):
        self.bootstrap()
        inventory, reference, _ = self.owned_inventory()
        analyzer = mock.Mock(return_value={"analysis": {"status": "analyzed"}})
        self.app.analyze = analyzer
        download = self.app.download
        def change_target(link, destination):
            result = download(link, destination)
            from modbridge_inventory import TitleInventoryError
            inventory.resolve.side_effect = TitleInventoryError("stale_target_ref")
            return result
        self.app.download = change_target
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_analyze", "game": "anothergame",
            "mod_id": 87, "file_id": 95, "idempotency_key": "changed-bound-001", "target_ref": reference})
        self.assertEqual(status, 202)
        job = self.await_job(json.loads(body)["id"])
        self.assertEqual(job["status"], "failed")
        self.assertEqual(job["stage"], "downloaded")
        self.assertEqual(job["error"]["code"], "stale_target_ref")
        analyzer.assert_not_called()

    def test_analysis_report_is_private_path_free_durable_and_immutable(self):
        from modbridge_reports import analysis_report
        self.bootstrap()
        report = analysis_report({"status": "analyzed", "inventory_complete": True,
            "provenance": {"input_stable": True}, "entries": []},
            {"unresolved": [{"code": "unknown_target", "component": "/private/source"}]})
        self.app.analyze = mock.Mock(return_value={"analysis": {"status": "analyzed", "report": report,
            "inventory_path": "/private/inventory", "plan_path": "/private/plan"}})
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_analyze", "game": "anothergame",
            "mod_id": 87, "file_id": 95, "idempotency_key": "report-owned-001"})
        self.assertEqual(status, 202)
        job = self.await_job(json.loads(body)["id"])
        self.assertEqual(job["analysis_report"], report)
        self.assertNotIn("/private/", json.dumps(job))
        job["analysis_report"]["reason_codes"].append("missing_backend")
        self.assertEqual(self.app.jobs(job["id"])["analysis_report"], report)
        self.app.close()
        restarted = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account)
        self.addCleanup(restarted.close)
        self.assertEqual(restarted.jobs(job["id"])["analysis_report"], report)
        forged = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        forged["analysis_report"]["compatibility_state"] = "supported"
        with self.assertRaises(host.HostError):
            self.app._validated_receipt(forged, job["id"])

    def test_analyzer_cannot_publish_runtime_or_install_authority(self):
        from modbridge_reports import analysis_report
        self.bootstrap()
        report = analysis_report({"status": "analyzed", "inventory_complete": True,
            "provenance": {"input_stable": True}, "entries": []}, {"unresolved": []})
        report["compatibility_state"] = "supported"
        self.app.analyze = mock.Mock(return_value={"analysis": {"status": "analyzed", "report": report}})
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_analyze", "game": "anothergame",
            "mod_id": 87, "file_id": 95, "idempotency_key": "forged-report-001"})
        self.assertEqual(status, 202)
        job = self.await_job(json.loads(body)["id"])
        self.assertEqual(job["status"], "failed")
        self.assertEqual(job["error"]["code"], "invalid_input")
        self.assertNotIn("analysis_report", job)

    def test_target_change_during_analysis_refuses_publication(self):
        self.bootstrap()
        inventory, reference, _ = self.owned_inventory()
        def changed_analysis(*arguments):
            from modbridge_inventory import TitleInventoryError
            inventory.resolve.side_effect = TitleInventoryError("stale_target_ref")
            return {"analysis": {"status": "analyzed"}}
        self.app.analyze = changed_analysis
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_analyze", "game": "anothergame",
            "mod_id": 87, "file_id": 95, "idempotency_key": "analysis-changed-001", "target_ref": reference})
        self.assertEqual(status, 202)
        job = self.await_job(json.loads(body)["id"])
        self.assertEqual(job["status"], "failed")
        self.assertEqual(job["error"]["code"], "stale_target_ref")
        self.assertNotIn("analysis_report", job)

    def test_job_idempotency_safe_progress_and_restart_receipt(self):
        self.bootstrap()
        intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1,
                  "file_id": 2, "idempotency_key": "owned-intent-0001"}
        status, body, _ = self.mutation("/api/jobs", intent)
        self.assertEqual(status, 202)
        job_id = json.loads(body)["id"]
        self.assertEqual(json.loads(self.mutation("/api/jobs", intent)[1])["id"], job_id)
        altered = dict(intent, file_id=3)
        self.assertEqual(self.mutation("/api/jobs", altered)[0], 409)
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            record = json.loads(self.request("GET", "/api/jobs/" + job_id)[1])
            if record["status"] in ("analyzed", "failed"):
                break
            time.sleep(.01)
        self.assertEqual(record["status"], "analyzed")
        self.assertEqual(record["stage"], "analyzed")
        self.assertEqual(record["compatibility"], "unresolved")
        self.assertEqual(self.account.download_count, 1)
        encoded = json.dumps(record)
        self.assertNotIn("secret=private", encoded)
        self.assertNotIn("/private/internal/path", encoded)
        self.assertNotIn(str(self.root), encoded)
        restarted = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account, download=lambda *_: None, analyze=lambda *_: None)
        try:
            self.assertEqual(restarted.jobs()[0]["status"], "analyzed")
        finally:
            restarted.close()

    def test_install_intent_is_explicitly_unavailable(self):
        self.bootstrap()
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_install",
            "game": "ownedgame", "mod_id": 1, "file_id": 2, "idempotency_key": "owned-intent-0002"})
        self.assertEqual(status, 409)
        self.assertEqual(json.loads(body)["error"]["code"], "translation_unavailable")
        self.assertEqual(self.account.download_count, 0)

    def test_configured_oauth_returns_to_clean_ui_and_rejects_replay(self):
        from urllib.parse import parse_qs, urlsplit, urlencode
        self.app.oauth_config = nexus.OAuthConfig("owned-client", self.origin + "/auth/callback")
        self.bootstrap()
        status, body, _ = self.mutation("/api/auth/start", {})
        self.assertEqual(status, 200)
        params = parse_qs(urlsplit(json.loads(body)["authorization_url"]).query)
        callback = "/auth/callback?" + urlencode({"code": "owned-private-code", "state": params["state"][0]})
        status, body, headers = self.request("GET", callback, headers={"Sec-Fetch-Site": "cross-site"})
        self.assertEqual(status, 303)
        self.assertEqual(headers["Location"], "/")
        self.assertNotIn(b"owned-private-code", body)
        self.assertEqual(self.request("GET", callback)[0], 400)

    def test_receipt_write_failure_does_not_kill_worker_or_leave_job_running(self):
        self.bootstrap()
        entered, release = threading.Event(), threading.Event()
        def blocked_download(link, destination):
            entered.set()
            release.wait(2)
            raise nexus.NexusError("network")
        self.app.download = blocked_download
        intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1,
                  "file_id": 2, "idempotency_key": "storage-failure-0001"}
        job = json.loads(self.mutation("/api/jobs", intent)[1])
        self.assertTrue(entered.wait(2))
        with mock.patch.object(self.app, "_save", side_effect=nexus.NexusError("storage")):
            release.set()
            deadline = time.monotonic() + 2
            while time.monotonic() < deadline and self.app.jobs(job["id"])["status"] == "downloading":
                time.sleep(.01)
            failed = self.app.jobs(job["id"])
            self.assertEqual(failed["status"], "failed")
            self.assertEqual(failed["error"]["code"], "storage_unavailable")
            self.assertTrue(self.app.worker.is_alive())
            self.assertEqual(self.mutation("/api/jobs", dict(intent, idempotency_key="storage-failure-0002"))[0], 503)

    def test_symlink_parent_is_refused_before_creating_any_receipts(self):
        outside = self.root / "outside"
        outside.mkdir(mode=0o700)
        alias = self.root / "alias"
        alias.symlink_to(outside, target_is_directory=True)
        with self.assertRaises(nexus.NexusError):
            host.BridgeApplication(self.root / "credentials.json", alias / "quarantine", account=self.account)
        self.assertEqual(list(outside.iterdir()), [])

    def test_loaded_receipt_cannot_claim_deployment_or_supply_external_source(self):
        record = {"id": "a" * 32, "kind": "download_analyze", "idempotency_key": "bad-receipt-0001",
            "game": "ownedgame", "mod_id": 1, "file_id": 2, "label": "Owned",
            "status": "installed", "stage": "deployed", "compatibility": "working",
            "source_url": "https://unrelated.example/?secret=private"}
        nexus.save_private_json(self.app.receipts / (record["id"] + ".json"), record)
        with self.assertRaises(host.HostError):
            host.BridgeApplication(self.root / "credentials.json", self.root / "quarantine", account=self.account)

    def test_real_account_lifecycle_is_normalized_for_ui(self):
        class Wire:
            def request(self, *_args, **_kwargs):
                return nexus.Response(200, {}, json.dumps({"user_id": 42, "name": "Owned user",
                    "is_premium": True, "is_supporter": False}).encode())
        self.app.account = nexus.AccountManager(self.app.store, transport=Wire())
        self.assertEqual(self.app.account_status()["status"], "disconnected")
        self.app.store.save_account(nexus.Credentials(api_key="owned-key"))
        self.assertEqual(self.app.account_status()["status"], "connected")
        self.assertEqual(self.app.account_status()["account"]["display_name"], "Owned user")
        with mock.patch.object(self.app.account, "status", return_value={"connected": False,
                "credentials_present": True, "live_service": "unavailable", "error": {"code": "network"}}):
            self.assertEqual(self.app.account_status()["status"], "error")

    def test_sign_in_can_be_restarted_and_old_state_is_refused(self):
        from urllib.parse import parse_qs, urlsplit, urlencode
        self.app.oauth_config = nexus.OAuthConfig("owned-client", self.origin + "/auth/callback")
        self.bootstrap()
        first = json.loads(self.mutation("/api/auth/start", {})[1])
        status, body, _ = self.mutation("/api/auth/start", {})
        self.assertEqual(status, 200)
        second = json.loads(body)
        self.assertNotEqual(first["authorization_url"], second["authorization_url"])
        state = parse_qs(urlsplit(first["authorization_url"]).query)["state"][0]
        self.assertEqual(self.request("GET", "/auth/callback?" + urlencode({"state": state, "code": "owned-code"}))[0], 400)

    def test_nexus_authorization_needed_receipt_survives_restart(self):
        self.bootstrap()
        with mock.patch.object(self.account, "download_links", side_effect=nexus.NexusError("download_authorization")):
            intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1,
                      "file_id": 2, "idempotency_key": "needs-authorization-0001"}
            identifier = json.loads(self.mutation("/api/jobs", intent)[1])["id"]
            self.app.pending.join()
        self.assertEqual(self.app.jobs(identifier)["status"], "needs_nexus_authorization")
        restarted = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account)
        try:
            self.assertEqual(restarted.jobs(identifier)["status"], "needs_nexus_authorization")
        finally:
            restarted.close()

    def test_disconnect_invalidates_already_retrieved_oauth_callback(self):
        from urllib.parse import parse_qs, urlsplit, urlencode
        self.app.oauth_config = nexus.OAuthConfig("owned-client", self.origin + "/auth/callback")
        self.bootstrap()
        authorization = json.loads(self.mutation("/api/auth/start", {})[1])
        state = parse_qs(urlsplit(authorization["authorization_url"]).query)["state"][0]
        entered, release = threading.Event(), threading.Event()
        operation = self.app._operation
        errors = []
        def delayed(action):
            if threading.current_thread().name == "owned-callback":
                entered.set()
                release.wait(2)
            return operation(action)
        def callback():
            try:
                self.app.callback(urlencode({"state": state, "code": "owned-code"}))
            except host.HostError as error:
                errors.append(error.code)
        with mock.patch.object(self.app, "_operation", side_effect=delayed), mock.patch.object(self.account, "complete_sign_in", wraps=self.account.complete_sign_in) as complete:
            thread = threading.Thread(target=callback, name="owned-callback")
            thread.start()
            self.assertTrue(entered.wait(2))
            self.assertEqual(self.mutation("/api/auth/disconnect", {})[0], 200)
            release.set()
            thread.join(2)
            self.assertFalse(thread.is_alive())
            self.assertEqual(errors, ["invalid_input"])
            complete.assert_not_called()

    def test_restart_marks_every_inflight_phase_interrupted_without_resuming(self):
        quarantine = self.root / "restart-fixture"
        for index, (status, stage) in enumerate((("queued", "requested"), ("downloading", "requested"), ("analyzing", "downloaded")), 1):
            identifier = str(index) * 32
            record = {"id": identifier, "kind": "download_analyze", "game": "ownedgame",
                "mod_id": 1, "file_id": 2, "idempotency_key": "interrupted-fixture-" + str(index),
                "label": "Mod 1 · file 2", "status": status, "stage": stage,
                "compatibility": "unresolved", "source_url": nexus.DownloadSource("ownedgame", 1, 2).source_url}
            if stage == "downloaded":
                record.update(sha256="a" * 64, bytes_downloaded=12)
            nexus.save_private_json(quarantine / ".jobs" / (identifier + ".json"), record)
        restarted = host.BridgeApplication(self.root / "credentials.json", quarantine, account=self.account)
        try:
            self.assertEqual(len(restarted.jobs()), 3)
            for record in restarted.jobs():
                self.assertEqual(record["status"], "interrupted")
                self.assertEqual(record["error"]["code"], "interrupted")
            self.assertEqual(self.account.download_count, 0)
            self.assertTrue(restarted.pending.empty())
        finally:
            restarted.close()


if __name__ == "__main__":
    unittest.main(verbosity=2)
