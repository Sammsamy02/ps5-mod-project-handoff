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
from urllib.parse import parse_qs, urlsplit

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


class FreeNexusWireFixture:
    """External provider boundary only; use the real entitlement/NXM client."""
    def __init__(self):
        self.download_queries = []

    def request(self, method, uri, **kwargs):
        parsed = urlsplit(uri)
        if parsed.path == "/v1/users/validate":
            body = {"user_id": 42, "name": "Owned free account",
                    "is_premium": False, "is_supporter": True}
        elif parsed.path == "/v1/games/ownedgame/mods/1":
            body = {"mod_id": 1, "game_id": 1, "domain_name": "ownedgame",
                    "name": "Owned mod", "author": "Owned author", "version": "1",
                    "contains_adult_content": False, "available": True, "status": "published"}
        elif parsed.path == "/v1/games/ownedgame/mods/1/files/2/download_link":
            self.download_queries.append(parse_qs(parsed.query))
            body = [{"URI": "https://owned.example/archive?signed=fixture-only",
                     "name": "Owned", "short_name": "O"}]
        else:
            raise AssertionError("Unexpected fixture provider endpoint")
        return nexus.Response(200, {}, json.dumps(body).encode())


class _KeyWire:
    """The provider boundary for API-key sign-in: GET /v1/users/validate answers for exactly one key."""
    GOOD = "owned-personal-key-0123456789abcdef"

    def __init__(self):
        self.requests = []
        self.error = None
        self.status = None

    def request(self, method, url, **kwargs):
        headers = dict(kwargs.get("headers", {}))
        self.requests.append((method, url, headers))
        if self.error is not None:
            raise self.error
        if (method, url) != ("GET", "https://api.nexusmods.com/v1/users/validate"):
            raise AssertionError("Unexpected fixture provider endpoint")
        if self.status is not None:
            return nexus.Response(self.status, {}, b"{}")
        if headers.get("apikey") != self.GOOD:
            return nexus.Response(401, {}, b'{"message":"Please provide a valid API Key"}')
        return nexus.Response(200, {}, json.dumps({"user_id": 42, "name": "Key owner", "is_premium": False, "is_supporter": True}).encode())


class _GraphqlWire:
    """The provider boundary for the v2 GraphQL listing: every POST answers with the same page of nodes."""
    def __init__(self, nodes, total):
        self.nodes, self.total, self.requests = nodes, total, []

    def request(self, method, url, **kwargs):
        self.requests.append((method, url, kwargs))
        page = {"data": {"mods": {"nodes": self.nodes, "nodesCount": len(self.nodes), "totalCount": self.total}}}
        return nexus.Response(200, {}, json.dumps(page).encode())


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
        self.assertTrue(record["capabilities"]["file_updates"])
        status, body, _ = self.request("GET", "/api/account")
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)["account"]["user_id"], 42)
        self.assertNotIn(b"do-not-expose", body)
        self.assertEqual(json.loads(self.request("GET", "/api/installed")[1])["items"], [])

    def test_job_capacity_is_advertised_from_the_hosts_own_limits(self):
        record = self.bootstrap()
        capacity = record["capabilities"]["job_capacity"]
        self.assertEqual(capacity, {"max_queued": 16, "max_records": 100})
        self.assertEqual(self.app.capabilities()["capabilities"]["job_capacity"], capacity)
        self.assertEqual((host.JOB_QUEUE_MAX, host.JOB_RECORDS_MAX), (capacity["max_queued"], capacity["max_records"]))
        self.assertEqual(self.app.pending.maxsize, capacity["max_queued"])
        for value in capacity.values():
            self.assertIs(type(value), int)
            self.assertGreater(value, 0)
        self.assertLessEqual(capacity["max_queued"], capacity["max_records"])

    def test_the_advertised_queue_is_what_the_host_enforces(self):
        # One job runs (held inside its download) and `max_queued` wait; the next is job_limit, with 409 on the wire.
        record = self.bootstrap()
        queued = record["capabilities"]["job_capacity"]["max_queued"]
        release, started = threading.Event(), threading.Event()
        owned = self.app.download

        def held(link, destination):
            started.set()
            release.wait(10)
            return owned(link, destination)
        self.app.download = held
        self.addCleanup(release.set)
        intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2}
        first = self.app.submit(dict(intent, idempotency_key="capacity-held-0000"))
        self.assertTrue(started.wait(5))
        for index in range(1, queued + 1):
            self.app.submit(dict(intent, idempotency_key=f"capacity-held-{index:04}"))
        self.assertEqual(self.mutation("/api/jobs", dict(intent, idempotency_key="capacity-held-9999"))[0], 409)
        # A replay of a request already taken needs no room.
        self.assertEqual(self.app.submit(dict(intent, idempotency_key="capacity-held-0003"))["status"], "queued")
        release.set()
        self.assertEqual(self.await_job(first["id"])["status"], "analyzed")
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline and any(job["status"] != "analyzed" for job in self.app.jobs()):
            time.sleep(.02)
        self.assertEqual({job["status"] for job in self.app.jobs()}, {"analyzed"})
        self.assertEqual(len(self.app.jobs()), queued + 1)
        self.assertEqual(self.mutation("/api/jobs", dict(intent, idempotency_key="capacity-held-9999"))[0], 202)

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
            {"unresolved": [{"code": "unknown_target", "component": "/private/source"},
                {"code": "missing_requirements", "component": "/private/component", "requirements": [
                    {"requirement": {"id": "runtime.shared", "version": ">=1.2", "consumer": "/private/consumer"}}]}]})
        self.app.analyze = mock.Mock(return_value={"analysis": {"status": "analyzed", "report": report,
            "inventory_path": "/private/inventory", "plan_path": "/private/plan"}})
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_analyze", "game": "anothergame",
            "mod_id": 87, "file_id": 95, "idempotency_key": "report-owned-001"})
        self.assertEqual(status, 202)
        job = self.await_job(json.loads(body)["id"])
        self.assertEqual(job["analysis_report"], report)
        self.assertEqual(job["analysis_report"]["blocked_prerequisites"], {"complete": True,
            "items": [{"id": "runtime.shared", "version": ">=1.2"}]})
        self.assertNotIn("/private/", json.dumps(job))
        job["analysis_report"]["reason_codes"].append("missing_backend")
        job["analysis_report"]["blocked_prerequisites"]["items"][0]["id"] = "runtime.changed"
        self.assertEqual(self.app.jobs(job["id"])["analysis_report"], report)
        self.app.close()
        restarted = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account)
        self.addCleanup(restarted.close)
        self.assertEqual(restarted.jobs(job["id"])["analysis_report"], report)
        forged = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        forged["analysis_report"]["blocked_prerequisites"]["items"][0]["id"] = "/private/forged"
        with self.assertRaises(host.HostError):
            self.app._validated_receipt(forged, job["id"])
        nexus.save_private_json(self.app.receipts / (job["id"] + ".json"), forged)
        with self.assertRaises(host.HostError):
            host.BridgeApplication(self.root / "private" / "credentials.json",
                self.root / "quarantine", account=self.account)
        forged = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        forged["analysis_report"] = dict(report)
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

    def metadata_inventory(self, build_version="02.003.000"):
        inventory, reference, _ = self.owned_inventory()
        resolve = inventory.resolve.side_effect
        def with_metadata(ref):
            record = resolve(ref)
            record["build_version"] = {"value": build_version, "status": "observed", "source": "owned"}
            return record
        inventory.resolve.side_effect = with_metadata
        return reference

    def submit_bound(self, reference, key, game="anothergame"):
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_analyze", "game": game,
            "mod_id": 87, "file_id": 95, "idempotency_key": key, "target_ref": reference})
        self.assertEqual(status, 202)
        return self.await_job(json.loads(body)["id"])

    def test_no_profile_gives_explicit_unknown_no_index_and_round_trips(self):
        self.bootstrap()
        reference = self.metadata_inventory()
        self.app.analyze = mock.Mock(return_value={"analysis": {"status": "analyzed"}})
        job = self.submit_bound(reference, "compat-noprofile-01")
        self.assertEqual(job["compatibility"], "unresolved")
        assessment = json.loads(json.dumps(job["compatibility_assessment"]))
        self.assertEqual(assessment["verdict"], "unknown")
        self.assertEqual(assessment["reasons"], [{"code": "no_index", "detail": "no game profile for this title"}])
        self.assertEqual(assessment["console_content_version"], "02.003.000")
        saved = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        self.assertEqual(self.app._validated_receipt(saved, job["id"])["compatibility_assessment"], assessment)
        job["compatibility_assessment"]["reasons"].append({"code": "no_index", "detail": "x"})
        self.assertEqual(len(self.app.jobs(job["id"])["compatibility_assessment"]["reasons"]), 1)
        self.app.close()
        restarted = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account)
        self.addCleanup(restarted.close)
        self.assertEqual(restarted.jobs(job["id"])["compatibility_assessment"], assessment)
        for change in ({"extra": 1}, {"verdict": "working"}, {"reasons": [{"code": "no_index", "detail": "x" * 513}]},
                       {"missing": {"assets": ["a"] * 51, "classes": [], "functions": [], "short_class_names": []}}):
            forged = json.loads(json.dumps(saved))
            forged["compatibility_assessment"].update(change)
            with self.subTest(change=change), self.assertRaises(host.HostError):
                self.app._validated_receipt(forged, job["id"])
        forged = json.loads(json.dumps(saved))
        forged["status"], forged["stage"] = "failed", "downloaded"
        forged["error"] = {"code": "operation_failed", "message": host.MESSAGES["operation_failed"]}
        with self.assertRaises(host.HostError):
            self.app._validated_receipt(forged, job["id"])

    def test_unbound_job_has_unknown_console_version(self):
        self.bootstrap()
        self.app.analyze = mock.Mock(return_value={"analysis": {"status": "analyzed"}})
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_analyze", "game": "anothergame",
            "mod_id": 87, "file_id": 95, "idempotency_key": "compat-unbound-001"})
        job = self.await_job(json.loads(body)["id"])
        self.assertEqual(job["compatibility_assessment"]["verdict"], "unknown")
        self.assertEqual(job["compatibility_assessment"]["reasons"][0]["code"], "unknown_console_version")

    def test_cli_failure_is_generic_assessment_failed(self):
        self.bootstrap()
        reference = self.metadata_inventory()
        profiles = self.root / "profiles"
        profiles.mkdir()
        (profiles / "PPSA98765.json").write_text(json.dumps({"title_id": "PPSA98765", "nexus_domain": "anothergame"}))
        self.app.profiles = profiles
        self.app.analyze = mock.Mock(return_value={"analysis": {"status": "analyzed"}})
        self.app.compat = mock.Mock(side_effect=RuntimeError("stderr /private/path secret"))
        job = self.submit_bound(reference, "compat-clifail-001")
        reasons = job["compatibility_assessment"]["reasons"]
        self.assertEqual(job["compatibility_assessment"]["verdict"], "unknown")
        self.assertEqual([reason["code"] for reason in reasons], ["assessment_failed"])
        self.assertNotIn("secret", json.dumps(job))
        self.assertNotIn("/private/", json.dumps(job))

    def test_profile_loader_accepts_only_small_regular_matching_json_objects(self):
        profiles = self.root / "profiles"
        profiles.mkdir()
        good = {"title_id": "PPSA98765", "nexus_domain": "anothergame"}
        (profiles / "PPSA98765.json").write_text(json.dumps(good))
        self.assertEqual(host.load_game_profile(profiles, "PPSA98765", "anothergame"), good)
        self.assertIsNone(host.load_game_profile(profiles, "PPSA98765", "othergame"))
        self.assertIsNone(host.load_game_profile(None, "PPSA98765"))
        self.assertIsNone(host.load_game_profile(profiles, "ppsa98765"))
        self.assertIsNone(host.load_game_profile(profiles, "../PPSA98765"))
        self.assertIsNone(host.load_game_profile(profiles, "PPSA11111"))
        (profiles / "PPSA11111.json").write_text("[1]")
        self.assertIsNone(host.load_game_profile(profiles, "PPSA11111"))
        (profiles / "PPSA22222.json").write_text(json.dumps(dict(good, title_id="PPSA98765")))
        self.assertIsNone(host.load_game_profile(profiles, "PPSA22222"))
        (profiles / "PPSA33333.json").write_text(json.dumps(dict(good, title_id="PPSA33333", pad="x" * (4 * 1024 * 1024))))
        self.assertIsNone(host.load_game_profile(profiles, "PPSA33333"))
        (profiles / "PPSA44444.json").symlink_to(profiles / "PPSA98765.json")
        self.assertIsNone(host.load_game_profile(profiles, "PPSA44444"))
        (profiles / "PPSA55555.json").write_text("{bad")
        self.assertIsNone(host.load_game_profile(profiles, "PPSA55555"))

    @unittest.skipUnless(Path(os.environ.get("MODBRIDGE_BIN", PROJECT / "build" / "modbridge")).is_file(),
                         "desktop core is not built")
    def test_profile_and_lua_package_produce_a_real_verdict_through_the_core(self):
        import zipfile
        from nexus import audit_download
        self.bootstrap()
        reference = self.metadata_inventory()
        profiles = self.root / "profiles"
        profiles.mkdir()
        profile = {"schema": "modbridge.game-profile/1", "title_id": "PPSA98765", "nexus_domain": "anothergame",
            "engine": {"family": "unreal", "version": "5.6"},
            "builds": [{"console_content_version": "02.003.000", "pc_build": {"label": "1.03", "released_utc": 1758153600}}],
            "content_index": {"console_content_version": "02.003.000",
                "complete": {"assets": True, "classes": False, "functions": False},
                "assets": ["/Game/Townfall/Present"], "classes": [], "functions": []}}
        (profiles / "PPSA98765.json").write_text(json.dumps(profile))
        self.app.profiles = profiles
        self.account.files = lambda game, mod: [{"file_id": 95, "name": "Mod", "file_name": "m.zip",
            "version": "1", "size_kb": 1, "is_primary": True, "category_id": 1, "category_name": "MAIN",
            "uploaded_timestamp": 1758200000}]
        scripts = {"first": 'local a = StaticFindObject("/Game/Townfall/Present")\n',
                   "second": 'local a = StaticFindObject("/Game/Townfall/Present")\nlocal b = StaticFindObject("/Game/Townfall/Gone")\n'}
        current = {}
        def download(link, destination):
            destination.mkdir(mode=0o700, parents=True, exist_ok=True)
            path = destination / "mod.zip"
            with zipfile.ZipFile(path, "w") as package:
                package.writestr("Mods/Owned/Scripts/main.lua", current["script"])
            return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size, link.source)
        self.app.download = download
        self.app.analyze = audit_download
        current["script"] = scripts["first"]
        job = self.submit_bound(reference, "compat-real-first-1")
        assessment = job["compatibility_assessment"]
        self.assertEqual(assessment["verdict"], "compatible", assessment)
        self.assertEqual((assessment["checked"], assessment["present"], assessment["selected_file_id"],
                          assessment["pc_build"], assessment["console_content_version"]),
                         (1, 1, 95, "1.03", "02.003.000"))
        current["script"] = scripts["second"]
        job = self.submit_bound(reference, "compat-real-second-")
        assessment = job["compatibility_assessment"]
        self.assertEqual(assessment["verdict"], "partially_compatible", assessment)
        self.assertEqual(assessment["missing"]["assets"], ["/Game/Townfall/Gone"])
        self.assertEqual((assessment["checked"], assessment["present"]), (2, 1))
        self.assertIn("missing_references", [reason["code"] for reason in assessment["reasons"]])
        self.assertEqual(job["compatibility"], "unresolved")
        self.assertNotIn(str(self.root), json.dumps(job))

    def test_content_index_loader_reads_only_regular_json_objects(self):
        profiles = self.root / "profiles"
        profiles.mkdir()
        index = {"console_content_version": "02.003.000"}
        (profiles / "PPSA98765.content-index.json").write_text(json.dumps(index))
        self.assertEqual(host.load_content_index(profiles, "PPSA98765"), index)
        self.assertIsNone(host.load_content_index(profiles, "PPSA11111"))
        self.assertIsNone(host.load_content_index(None, "PPSA98765"))
        self.assertIsNone(host.load_content_index(profiles, "../PPSA98765"))
        (profiles / "PPSA22222.content-index.json").write_text("[1]")
        self.assertIsNone(host.load_content_index(profiles, "PPSA22222"))
        (profiles / "PPSA33333.content-index.json").write_text("{bad")
        self.assertIsNone(host.load_content_index(profiles, "PPSA33333"))
        (profiles / "PPSA44444.content-index.json").symlink_to(profiles / "PPSA98765.content-index.json")
        self.assertIsNone(host.load_content_index(profiles, "PPSA44444"))

    @unittest.skipUnless(Path(os.environ.get("MODBRIDGE_BIN", PROJECT / "build" / "modbridge")).is_file(),
                         "desktop core is not built")
    def test_content_index_sidecar_turns_unknown_into_a_real_verdict(self):
        import zipfile
        from nexus import audit_download
        self.bootstrap()
        reference = self.metadata_inventory()
        profiles = self.root / "profiles"
        profiles.mkdir()
        profile = {"schema": "modbridge.game-profile/1", "title_id": "PPSA98765", "nexus_domain": "anothergame",
            "engine": {"family": "unreal", "version": "5.6"},
            "builds": [{"console_content_version": "02.003.000", "pc_build": None}]}
        (profiles / "PPSA98765.json").write_text(json.dumps(profile))
        self.app.profiles = profiles
        self.account.files = lambda game, mod: [{"file_id": 95, "name": "Mod", "file_name": "m.zip",
            "version": "1", "size_kb": 1, "is_primary": True, "category_id": 1, "category_name": "MAIN"}]
        def download(link, destination):
            destination.mkdir(mode=0o700, parents=True, exist_ok=True)
            path = destination / "mod.zip"
            with zipfile.ZipFile(path, "w") as package:
                package.writestr("Mods/Owned/Scripts/main.lua", 'local a = StaticFindObject("/Game/Townfall/Present")\n')
            return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size, link.source)
        self.app.download = download
        self.app.analyze = audit_download
        # Profile only, PC patch unknown: no index -> unknown.
        job = self.submit_bound(reference, "compat-sidecar-none-")
        self.assertEqual(job["compatibility_assessment"]["verdict"], "unknown")
        # The content index next to the profile decides, the unknown PC patch stays a caveat.
        index = {"console_content_version": "02.003.000",
                 "complete": {"assets": True, "classes": True, "functions": True},
                 "assets": ["/Game/Townfall/Present"], "classes": [], "functions": []}
        (profiles / "PPSA98765.content-index.json").write_text(json.dumps(index))
        assessment = self.submit_bound(reference, "compat-sidecar-real-")["compatibility_assessment"]
        self.assertEqual(assessment["verdict"], "compatible", assessment)
        self.assertEqual([r["code"] for r in assessment["reasons"]], ["unknown_build"])
        self.assertEqual((assessment["checked"], assessment["present"]), (1, 1))
        # An index for a build the profile does not list fails closed to unknown, and says why.
        index["console_content_version"] = "09.000.000"
        (profiles / "PPSA98765.content-index.json").write_text(json.dumps(index))
        assessment = self.submit_bound(reference, "compat-sidecar-bad-")["compatibility_assessment"]
        self.assertEqual(assessment["verdict"], "unknown")
        self.assertEqual([r["code"] for r in assessment["reasons"]], ["index_for_other_build"])
        # A malformed index for a listed build is the core's to reject: the check could not run.
        (profiles / "PPSA98765.content-index.json").write_text(json.dumps(
            {"console_content_version": "02.003.000", "assets": "not-a-list"}))
        assessment = self.submit_bound(reference, "compat-sidecar-broken-")["compatibility_assessment"]
        self.assertEqual(assessment["verdict"], "unknown")
        self.assertEqual([r["code"] for r in assessment["reasons"]], ["assessment_failed"])

    # ---- PS5 Lua runtime coverage: static usage of the mod's scripts against a runtime manifest ----
    RUNTIME_KEYS = {"status", "reason", "runtime", "console_wired", "missing", "partial",
                    "missing_modules", "partial_modules", "dynamic_require"}
    CORE_BUILT = Path(os.environ.get("MODBRIDGE_BIN", PROJECT / "build" / "modbridge")).is_file()

    def real_core_job(self, key, files):
        """Run a mod made of `files` (archive path -> bytes) through the real core analyzer."""
        import zipfile
        from nexus import audit_download
        self.bootstrap()
        def download(link, destination):
            destination.mkdir(mode=0o700, parents=True, exist_ok=True)
            path = destination / "mod.zip"
            with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as package:
                for name, data in files.items():
                    package.writestr(name, data)
            return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size, link.source)
        self.app.download = download
        self.app.analyze = audit_download
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_analyze", "game": "anothergame",
            "mod_id": 87, "file_id": 95, "idempotency_key": key})
        self.assertEqual(status, 202)
        return self.await_job(json.loads(body)["id"])

    def usage_job(self, key, usage, **hooks):
        """A job whose analysis (mocked) reports `usage` as the package's lua_api union."""
        self.bootstrap()
        analysis = {"status": "analyzed"}
        if usage is not ...:
            analysis["lua_api"] = usage
        self.app.analyze = mock.Mock(return_value={"analysis": analysis})
        for name, value in hooks.items():
            setattr(self.app, name, value)
        status, body, _ = self.mutation("/api/jobs", {"kind": "download_analyze", "game": "anothergame",
            "mod_id": 87, "file_id": 95, "idempotency_key": key})
        self.assertEqual(status, 202)
        return self.await_job(json.loads(body)["id"])

    @unittest.skipUnless(CORE_BUILT, "desktop core is not built")
    def test_lua_mod_is_blocked_by_the_bundled_runtime_through_the_real_core_and_round_trips(self):
        script = (b"ExecuteInGameThread(function() end)\nRegisterKeyBind(Key.F1, function() end)\n"
                  b"RegisterLoadMapPreHook(function() end)\n")
        job = self.real_core_job("runtime-real-0001", {"Mods/Owned/Scripts/main.lua": script, "Mods/Owned/readme.txt": b"hi"})
        coverage = json.loads(json.dumps(job["runtime_coverage"]))
        # The bundled manifest is the PS5 UE4SS port: Key and RegisterKeyBind work, the game-thread
        # family waits on the pump (partial), and LoadMap hooks never fire (missing).
        self.assertEqual(coverage, {"status": "blocked", "reason": None, "runtime": "ps5-ue4ss-port",
            "console_wired": False, "missing": ["RegisterLoadMapPreHook"], "partial": ["ExecuteInGameThread"], "missing_modules": [],
            "partial_modules": [], "dynamic_require": False})
        self.assertEqual(job["compatibility"], "unresolved")
        self.assertNotIn(str(self.root), json.dumps(job))
        self.assertNotIn("/private/", json.dumps(job))
        # Durable, validated on load, an owned copy in the public view, and served on the jobs endpoint.
        saved = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        self.assertEqual(self.app._validated_receipt(saved, job["id"])["runtime_coverage"], coverage)
        job["runtime_coverage"]["missing"].append("Changed")
        self.assertEqual(self.app.jobs(job["id"])["runtime_coverage"], coverage)
        status, body, _ = self.request("GET", "/api/jobs")
        self.assertEqual(json.loads(body)["items"][0]["runtime_coverage"], coverage)
        self.app.close()
        restarted = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account)
        self.addCleanup(restarted.close)
        self.assertEqual(restarted.jobs(job["id"])["runtime_coverage"], coverage)

    @unittest.skipUnless(CORE_BUILT, "desktop core is not built")
    def test_a_hook_called_only_through_pcall_is_optional_not_blocking(self):
        # The Item Spawner pattern: pcall(RegisterLoadMapPreHook, f) keeps the mod running without the hook.
        script = (b"RegisterKeyBind(Key.F7, function() ExecuteInGameThread(f) end)\n"
                  b"pcall(RegisterLoadMapPreHook, function() end)\n")
        job = self.real_core_job("runtime-pcall-001", {"Mods/Owned/Scripts/main.lua": script})
        self.assertEqual(job["runtime_coverage"], {"status": "partial", "reason": None, "runtime": "ps5-ue4ss-port",
            "console_wired": False, "missing": [], "partial": ["ExecuteInGameThread"], "missing_modules": [],
            "partial_modules": [], "dynamic_require": False, "optional_missing": ["RegisterLoadMapPreHook"]})
        saved = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        self.assertEqual(self.app._validated_receipt(saved, job["id"])["runtime_coverage"], job["runtime_coverage"])

    @unittest.skipUnless(CORE_BUILT, "desktop core is not built")
    def test_a_mod_without_lua_has_no_runtime_coverage(self):
        job = self.real_core_job("runtime-nolua-001", {"Data/shared.txt": b"plain data\n", "Mods/Owned/Config/a.ini": b"[a]\n"})
        self.assertEqual(job["status"], "analyzed")
        self.assertNotIn("runtime_coverage", job)
        self.assertIn("compatibility_assessment", job)
        saved = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        self.assertNotIn("runtime_coverage", saved)
        # The union treats "no Lua" as nothing to report, whatever the analyzer left in the key.
        for usage in (..., None):
            with self.subTest(usage=usage):
                job = self.usage_job("runtime-nolua-%03d" % (1 if usage is ... else 2), usage)
                self.assertNotIn("runtime_coverage", job)

    @unittest.skipUnless(CORE_BUILT, "desktop core is not built")
    def test_unreadable_scripts_are_unknown_not_covered_through_the_real_core(self):
        too_large = b" " * (4 * 1024 * 1024 + 1)
        job = self.real_core_job("runtime-large-001", {"Mods/A/Scripts/main.lua": too_large})
        self.assertEqual(job["runtime_coverage"], {"status": "unknown", "reason": "usage_unavailable", "runtime": None,
            "console_wired": None, "missing": [], "partial": [], "missing_modules": [], "partial_modules": [],
            "dynamic_require": False})
        # One readable script does not hide a script that could not be read.
        job = self.real_core_job("runtime-large-002", {"Mods/A/Scripts/main.lua": b"RegisterKeyBind(1, f)\n",
                                                       "Mods/A/Scripts/big.lua": too_large})
        self.assertEqual((job["runtime_coverage"]["status"], job["runtime_coverage"]["reason"]),
                         ("unknown", "usage_unavailable"))

    def test_unavailable_usage_is_unknown_usage_unavailable_without_running_the_check(self):
        check = mock.Mock(side_effect=AssertionError("must not run"))
        job = self.usage_job("runtime-unavail-001", {"unavailable": True}, runtime_check=check)
        self.assertEqual(job["runtime_coverage"], {"status": "unknown", "reason": "usage_unavailable", "runtime": None,
            "console_wired": None, "missing": [], "partial": [], "missing_modules": [], "partial_modules": [],
            "dynamic_require": False})
        check.assert_not_called()
        saved = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        self.assertEqual(self.app._validated_receipt(saved, job["id"])["runtime_coverage"], job["runtime_coverage"])
        job = self.usage_job("runtime-unavail-002", "garbage", runtime_check=check)
        self.assertEqual((job["runtime_coverage"]["status"], job["runtime_coverage"]["reason"]),
                         ("unknown", "usage_unavailable"))

    def test_failed_runtime_check_is_unknown_assessment_failed_and_leaks_nothing(self):
        usage = {"functions": {"RegisterKeyBind": 1}, "modules": [], "shadowed": [], "dynamic_require": False}
        good = {"status": "covered", "runtime": "r", "console_wired": False, "missing": [], "partial": [],
                "missing_modules": [], "partial_modules": [], "dynamic_require": False}
        failures = [
            mock.Mock(side_effect=RuntimeError("stderr /private/path secret-token")),
            mock.Mock(side_effect=host.nexus.NexusError("provider")),
            mock.Mock(return_value="not a record /private/path secret-token"),
            mock.Mock(return_value=dict(good, status="works")),
            mock.Mock(return_value=dict(good, status="unknown")),
            mock.Mock(return_value=dict(good, missing=["secret-token /private/path"])),
            mock.Mock(return_value=dict(good, missing=["ExecuteInGameThread"])),       # covered beside a missing name
            mock.Mock(return_value=dict(good, console_wired="no")),
            mock.Mock(return_value=dict(good, runtime="/private/path")),
        ]
        for number, check in enumerate(failures):
            with self.subTest(number=number):
                job = self.usage_job("runtime-fail-%03d" % number, usage, runtime_check=check)
                self.assertEqual(job["status"], "analyzed")
                self.assertEqual(job["runtime_coverage"], {"status": "unknown", "reason": "assessment_failed",
                    "runtime": None, "console_wired": None, "missing": [], "partial": [], "missing_modules": [],
                    "partial_modules": [], "dynamic_require": False})
                self.assertNotIn("secret", json.dumps(job))
                self.assertNotIn("/private/", json.dumps(job))
                check.assert_called_once_with(usage, self.app.lua_runtime)

    @unittest.skipUnless(CORE_BUILT, "desktop core is not built")
    def test_missing_or_broken_runtime_manifest_is_assessment_failed_through_the_real_core(self):
        usage = {"functions": {"RegisterKeyBind": 1}, "modules": [], "shadowed": [], "dynamic_require": False}
        broken = self.root / "broken-runtime.json"
        broken.write_text("{")
        for number, manifest in enumerate((self.root / "absent-runtime.json", broken)):
            with self.subTest(manifest=manifest.name):
                self.app.lua_runtime = manifest
                job = self.usage_job("runtime-manifest-%02d" % number, usage)
                self.assertEqual((job["runtime_coverage"]["status"], job["runtime_coverage"]["reason"]),
                                 ("unknown", "assessment_failed"))
                self.assertNotIn(str(self.root), json.dumps(job))

    def test_runtime_coverage_defaults_to_the_bundled_manifest_and_takes_an_operator_one(self):
        bundled = PROJECT / "examples" / "runtimes" / "ps5-ue4ss-port.json"
        self.assertTrue(bundled.is_file())
        self.assertEqual(self.app.lua_runtime, bundled)
        self.assertEqual(host.DEFAULT_LUA_RUNTIME, bundled)
        custom = self.root / "custom-runtime.json"
        app = host.BridgeApplication(self.root / "private" / "other.json", self.root / "other-quarantine",
                                     account=self.account, lua_runtime=custom)
        self.addCleanup(app.close)
        self.assertEqual(app.lua_runtime, custom)
        main = (PROJECT / "companion" / "host.py").read_text()
        self.assertIn('"--lua-runtime"', main)
        self.assertIn("lua_runtime=args.lua_runtime", main)

    def test_runtime_coverage_receipts_are_validated_and_forged_ones_refused(self):
        usage = {"functions": {"RegisterKeyBind": 1}, "modules": [], "shadowed": [], "dynamic_require": False}
        blocked = {"status": "blocked", "reason": None, "runtime": "ps5-ue4ss-portable", "console_wired": False,
                   "missing": ["ExecuteInGameThread"], "partial": [], "missing_modules": [], "partial_modules": [],
                   "dynamic_require": False}
        job = self.usage_job("runtime-forge-001", usage, runtime_check=mock.Mock(return_value=dict(blocked)))
        saved = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        self.assertEqual(self.app._validated_receipt(saved, job["id"])["runtime_coverage"], blocked)
        changes = [{"extra": 1}, {"status": "works"}, {"status": "covered"}, {"status": "unknown"},
                   {"reason": "assessment_failed"}, {"reason": "/private/path"}, {"runtime": None}, {"runtime": "Bad Id"},
                   {"console_wired": None}, {"console_wired": "no"}, {"dynamic_require": 1},
                   {"missing": ["a"] * 65}, {"missing": ["bad name"]}, {"missing": ["x" * 129]}, {"missing": "ExecuteInGameThread"},
                   {"missing": [3]}, {"missing_modules": ["bad module!"]}, {"partial_modules": ["x" * 129]}]
        for change in changes:
            forged = json.loads(json.dumps(saved))
            forged["runtime_coverage"].update(change)
            with self.subTest(change=change), self.assertRaises(host.HostError):
                self.app._validated_receipt(forged, job["id"])
        for key in blocked:
            forged = json.loads(json.dumps(saved))
            del forged["runtime_coverage"][key]
            with self.subTest(removed=key), self.assertRaises(host.HostError):
                self.app._validated_receipt(forged, job["id"])
        for value in (None, [], "blocked", [blocked]):
            forged = json.loads(json.dumps(saved))
            forged["runtime_coverage"] = value
            with self.subTest(value=value), self.assertRaises(host.HostError):
                self.app._validated_receipt(forged, job["id"])
        # The field belongs to an analyzed job only.
        forged = json.loads(json.dumps(saved))
        forged["status"], forged["stage"] = "failed", "downloaded"
        forged["error"] = {"code": "operation_failed", "message": host.MESSAGES["operation_failed"]}
        with self.assertRaises(host.HostError):
            self.app._validated_receipt(forged, job["id"])
        # A restart refuses a store holding a forged record instead of showing it.
        forged = json.loads(json.dumps(saved))
        forged["runtime_coverage"]["status"] = "works"
        self.app.close()
        nexus.save_private_json(self.app.receipts / (job["id"] + ".json"), forged)
        with self.assertRaises(host.HostError):
            host.BridgeApplication(self.root / "private" / "credentials.json", self.root / "quarantine", account=self.account)

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

    def test_free_account_can_resume_existing_download_with_ephemeral_nxm(self):
        self.bootstrap()
        wire = FreeNexusWireFixture()
        client = nexus.NexusClient(nexus.Credentials(api_key="owned-fixture-only"), wire)
        self.account.client = lambda: client
        intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1,
                  "file_id": 2, "idempotency_key": "free-resume-0001"}
        status, body, _ = self.mutation("/api/jobs", intent)
        self.assertEqual(status, 202)
        identifier = json.loads(body)["id"]
        self.assertEqual(self.await_job(identifier)["status"], "needs_nexus_authorization")
        self.assertEqual(wire.download_queries, [])
        expiry = int(time.time()) + 3600
        grant = f"nxm://ownedgame/mods/1/files/2?key=website-grant-fixture&expires={expiry}&user_id=42"
        status, body, _ = self.mutation("/api/jobs", dict(intent, nxm=grant))
        self.assertEqual(status, 202)
        self.assertEqual(json.loads(body)["id"], identifier)
        self.assertEqual(self.await_job(identifier)["status"], "analyzed")
        self.assertEqual(wire.download_queries, [{"key": ["website-grant-fixture"], "expires": [str(expiry)]}])
        # Resubmitting a completed intent must not consume the grant again.
        self.assertEqual(self.mutation("/api/jobs", dict(intent, nxm=grant))[0], 202)
        self.assertEqual(len(wire.download_queries), 1)
        serialized = json.dumps(self.app.jobs()) + json.dumps(self.app.records)
        serialized += "".join(path.read_text() for path in self.app.receipts.glob("*.json"))
        self.assertNotIn("website-grant-fixture", serialized)
        self.assertNotIn("nxm://", serialized)
        self.assertNotIn("signed=fixture-only", serialized)
        restarted = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account, download=lambda *_: None, analyze=lambda *_: None)
        try:
            self.assertEqual(restarted.jobs(identifier)["status"], "analyzed")
        finally:
            restarted.close()

    def test_nxm_scope_and_expiry_are_rejected_before_queuing(self):
        self.bootstrap()
        intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1,
                  "file_id": 2, "idempotency_key": "nxm-rejected-0001"}
        expiry = int(time.time()) + 3600
        valid = f"nxm://ownedgame/mods/1/files/2?key=rejected-grant-fixture&expires={expiry}&user_id=42"
        invalid = [None, {}, valid.replace("mods/1/", "mods/3/"),
                   valid.replace("files/2?", "files/4?"),
                   valid.replace("ownedgame/", "othergame/"),
                   valid.replace(f"expires={expiry}", "expires=1")]
        for value in invalid:
            with self.subTest(value_type=type(value).__name__):
                status, body, _ = self.mutation("/api/jobs", dict(intent, nxm=value))
                self.assertEqual(status, 400)
                self.assertNotIn(b"rejected-grant-fixture", body)
        self.assertEqual(self.app.jobs(), [])
        self.assertEqual(self.account.download_count, 0)

    def test_new_nxm_job_uses_live_account_identity_before_obtaining_link(self):
        self.bootstrap()
        wire = FreeNexusWireFixture()
        self.account.client = lambda: nexus.NexusClient(nexus.Credentials(api_key="owned-fixture-only"), wire)
        expiry = int(time.time()) + 3600
        intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1,
                  "file_id": 2, "idempotency_key": "nxm-live-user-0001"}
        grant = f"nxm://ownedgame/mods/1/files/2?key=account-grant-fixture&expires={expiry}&user_id=43"
        status, body, _ = self.mutation("/api/jobs", dict(intent, nxm=grant))
        self.assertEqual(status, 202)
        failed = self.await_job(json.loads(body)["id"])
        self.assertEqual(failed["status"], "needs_nexus_authorization")
        self.assertEqual(wire.download_queries, [])
        # A valid grant can also enter a new intent directly, without a failed
        # attempt first. The worker still obtains the current provider user.
        intent["idempotency_key"] = "nxm-live-user-0002"
        status, body, _ = self.mutation("/api/jobs", dict(intent, nxm=grant.replace("user_id=43", "user_id=42")))
        self.assertEqual(status, 202)
        self.assertEqual(self.await_job(json.loads(body)["id"])["status"], "analyzed")
        self.assertEqual(len(wire.download_queries), 1)
        self.assertNotIn("account-grant-fixture", json.dumps(self.app.jobs()))

    def test_nxm_resume_reuses_record_when_job_history_is_full(self):
        self.bootstrap()
        wire = FreeNexusWireFixture()
        self.account.client = lambda: nexus.NexusClient(nexus.Credentials(api_key="owned-fixture-only"), wire)
        intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1,
                  "file_id": 2, "idempotency_key": "nxm-ceiling-0000"}
        first = self.app.submit(intent)
        self.assertEqual(self.await_job(first["id"])["status"], "needs_nexus_authorization")
        for index in range(1, 100):
            job = self.app.submit(dict(intent, idempotency_key=f"nxm-ceiling-{index:04}"))
            self.assertEqual(self.await_job(job["id"])["status"], "needs_nexus_authorization")
        self.assertEqual(self.mutation("/api/jobs", dict(intent, idempotency_key="nxm-ceiling-0100"))[0], 409)
        expiry = int(time.time()) + 3600
        grant = f"nxm://ownedgame/mods/1/files/2?key=ceiling-grant-fixture&expires={expiry}&user_id=42"
        status, body, _ = self.mutation("/api/jobs", dict(intent, nxm=grant))
        self.assertEqual(status, 202)
        self.assertEqual(json.loads(body)["id"], first["id"])
        self.assertEqual(self.await_job(first["id"])["status"], "analyzed")
        self.assertEqual(len(self.app.jobs()), 100)

    def test_disconnect_revokes_queued_nxm_before_serialized_lookup(self):
        self.bootstrap()
        wire = FreeNexusWireFixture()
        self.account.client = lambda: nexus.NexusClient(nexus.Credentials(api_key="owned-fixture-only"), wire)
        expiry = int(time.time()) + 3600
        intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1,
                  "file_id": 2, "idempotency_key": "nxm-revoke-0001",
                  "nxm": f"nxm://ownedgame/mods/1/files/2?key=revoked-grant-fixture&expires={expiry}&user_id=42"}
        # A separate account operation holds the real serialization lock.
        # Disconnect is reentrant here; the queued worker cannot look up links.
        with self.app.operation_lock:
            job = self.app.submit(intent)
            deadline = time.monotonic() + 1
            while self.app.jobs(job["id"])["status"] == "queued" and time.monotonic() < deadline:
                time.sleep(.001)
            self.assertEqual(self.app.jobs(job["id"])["status"], "downloading")
            self.app.disconnect()
        self.assertEqual(self.await_job(job["id"])["status"], "needs_nexus_authorization")
        self.assertEqual(wire.download_queries, [])

    def waiting_job(self, wire, key, **extra):
        """A job parked in needs_nexus_authorization by a free account; no download link was requested."""
        self.account.client = lambda: nexus.NexusClient(nexus.Credentials(api_key="owned-fixture-only"), wire)
        intent = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2,
                  "idempotency_key": key, **extra}
        job = self.app.submit(intent)
        self.assertEqual(self.await_job(job["id"])["status"], "needs_nexus_authorization")
        self.assertEqual(wire.download_queries, [])
        return job["id"]

    def grant(self, key="resume-grant-fixture", **change):
        values = {"game": "ownedgame", "mod": 1, "file": 2, "expires": int(time.time()) + 3600, "user": 42, **change}
        return (f"nxm://{values['game']}/mods/{values['mod']}/files/{values['file']}"
                f"?key={key}&expires={values['expires']}&user_id={values['user']}")

    def test_resume_route_requeues_a_waiting_job_with_a_pasted_nxm_link(self):
        self.bootstrap()
        wire = FreeNexusWireFixture()
        identifier = self.waiting_job(wire, "resume-route-0001")
        grant = self.grant()
        status, body, _ = self.mutation(f"/api/jobs/{identifier}/resume", {"nxm": grant})
        self.assertEqual(status, 202)
        record = json.loads(body)
        self.assertEqual((record["id"], record["status"], record["game"], record["mod_id"], record["file_id"]),
                         (identifier, "queued", "ownedgame", 1, 2))
        self.assertNotIn("error", record)
        self.assertEqual(self.await_job(identifier)["status"], "analyzed")
        self.assertEqual(len(wire.download_queries), 1)
        self.assertEqual(wire.download_queries[0]["key"], ["resume-grant-fixture"])
        # The grant is a secret: no response, record, receipt or public view keeps it.
        serialized = body.decode() + json.dumps(self.app.jobs()) + json.dumps(self.app.records)
        serialized += "".join(path.read_text() for path in self.app.receipts.glob("*.json"))
        for secret in ("resume-grant-fixture", "nxm://"):
            self.assertNotIn(secret, serialized)
        self.assertNotIn("idempotency_key", body.decode() + json.dumps(self.app.jobs()))
        self.assertEqual(self.app.download_authorizations, {})
        # A second paste for the same, now finished job is refused and consumes nothing.
        status, body, _ = self.mutation(f"/api/jobs/{identifier}/resume", {"nxm": grant})
        self.assertEqual((status, json.loads(body)["error"]["code"]), (409, "auth_refused"))
        self.assertEqual(len(wire.download_queries), 1)

    def test_resume_route_rejects_links_that_do_not_match_the_job(self):
        self.bootstrap()
        wire = FreeNexusWireFixture()
        identifier = self.waiting_job(wire, "resume-route-0002")
        path = f"/api/jobs/{identifier}/resume"
        valid = self.grant("mismatch-grant-fixture")
        wrong = {
            "other game": self.grant("mismatch-grant-fixture", game="othergame"),
            "other mod": self.grant("mismatch-grant-fixture", mod=3),
            "other file": self.grant("mismatch-grant-fixture", file=4),
            "expired": self.grant("mismatch-grant-fixture", expires=1),
            "not nxm": valid.replace("nxm://", "https://"),
            "no query": "nxm://ownedgame/mods/1/files/2",
            "extra parameter": valid + "&extra=1",
            "duplicate parameter": valid + "&key=again",
            "missing user": valid.rsplit("&", 1)[0],
            "fragment": valid + "#frag",
            "whitespace": valid + " ",
            "non-ascii": valid.replace("mismatch", "mismatch\u00e9"),
            "collection link": "nxm://ownedgame/collections/abc/revisions/1",
            "empty": "",
        }
        for name, value in wrong.items():
            with self.subTest(name):
                status, body, _ = self.mutation(path, {"nxm": value})
                self.assertEqual((status, json.loads(body)["error"]["code"]), (400, "download_authorization"))
                self.assertNotIn(b"mismatch-grant-fixture", body)
        for name, value in {"not a string": {"nxm": 5}, "null": {"nxm": None}, "list": {"nxm": [valid]},
                            "missing": {}, "extra field": {"nxm": valid, "mod_id": 9},
                            "other field": {"link": valid}}.items():
            with self.subTest(name):
                status, body, _ = self.mutation(path, value)
                # A string that is not a usable link is a grant problem; a wrong body shape is not.
                expected = "download_authorization" if name in ("not a string", "null", "list") else "invalid_input"
                self.assertEqual((status, json.loads(body)["error"]["code"]), (400, expected))
        self.assertEqual(self.app.jobs(identifier)["status"], "needs_nexus_authorization")
        self.assertEqual(wire.download_queries, [])
        self.assertEqual(self.app.pending.qsize(), 0)
        # After all those refusals the job still resumes with one good link.
        self.assertEqual(self.mutation(path, {"nxm": valid})[0], 202)
        self.assertEqual(self.await_job(identifier)["status"], "analyzed")
        self.assertEqual(len(wire.download_queries), 1)

    def test_resume_route_only_accepts_waiting_jobs_and_known_ids(self):
        self.bootstrap()
        grant = self.grant("state-grant-fixture")
        # An analyzed job, a failed job and an unknown job are never requeued.
        done = self.app.submit({"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2,
                                "idempotency_key": "resume-state-0001"})["id"]
        self.assertEqual(self.await_job(done)["status"], "analyzed")
        with mock.patch.object(self.account, "download_links", side_effect=nexus.NexusError("provider")):
            failed = self.app.submit({"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2,
                                      "idempotency_key": "resume-state-0002"})["id"]
            self.assertEqual(self.await_job(failed)["status"], "failed")
        for identifier, expected in ((done, (409, "auth_refused")), (failed, (409, "auth_refused")),
                                     ("a" * 32, (404, "not_found"))):
            with self.subTest(identifier=identifier[:6]):
                status, body, _ = self.mutation(f"/api/jobs/{identifier}/resume", {"nxm": grant})
                self.assertEqual((status, json.loads(body)["error"]["code"]), expected)
                self.assertNotIn(b"state-grant-fixture", body)
        self.assertEqual(self.app.jobs(failed)["status"], "failed")
        self.assertEqual(self.app.jobs(done)["status"], "analyzed")
        # Malformed ids and extra path or query parts never reach the handler.
        for path in ("/api/jobs/not-an-id/resume", f"/api/jobs/{done}/resume/", f"/api/jobs/{done}/resume?x=1",
                     f"/api/jobs/{'A' * 32}/resume", f"/api/jobs/{done}/other"):
            with self.subTest(path=path):
                self.assertEqual(self.mutation(path, {"nxm": grant})[0], 404)
        self.assertEqual(self.request("GET", f"/api/jobs/{done}/resume")[0], 404)

    def test_resume_route_follows_the_mutation_gate(self):
        self.bootstrap()
        wire = FreeNexusWireFixture()
        identifier = self.waiting_job(wire, "resume-route-0003")
        path, body = f"/api/jobs/{identifier}/resume", {"nxm": self.grant("gate-grant-fixture")}
        saved = self.cookie
        self.cookie = None
        self.assertEqual(self.request("POST", path, body, {"Origin": self.origin, "X-CSRF-Token": self.csrf})[0], 401)
        self.cookie = saved
        self.assertEqual(self.request("POST", path, body)[0], 403)
        self.assertEqual(self.request("POST", path, body, {"Origin": "https://unrelated.example", "X-CSRF-Token": self.csrf})[0], 403)
        self.assertEqual(self.request("POST", path, body, {"Origin": self.origin, "X-CSRF-Token": "wrong"})[0], 403)
        self.assertEqual(self.request("POST", path, body, {"Origin": self.origin, "X-CSRF-Token": self.csrf,
                                                           "Host": "unrelated.example"})[0], 403)
        self.assertEqual(self.request("POST", path, body, {"Origin": self.origin, "X-CSRF-Token": self.csrf,
                                                           "Sec-Fetch-Site": "cross-site"})[0], 403)
        self.assertEqual(self.request("POST", path, body, {"Origin": self.origin, "X-CSRF-Token": self.csrf,
                                                           "Content-Type": "text/plain"})[0], 400)
        self.assertEqual(self.app.jobs(identifier)["status"], "needs_nexus_authorization")
        self.assertEqual(wire.download_queries, [])
        self.assertEqual(self.mutation(path, body)[0], 202)
        self.assertEqual(self.await_job(identifier)["status"], "analyzed")

    def test_resume_route_is_blocked_by_storage_failure_stale_target_and_full_queue(self):
        self.bootstrap()
        wire = FreeNexusWireFixture()
        inventory, reference, _ = self.owned_inventory()
        self.app.analyze = lambda result, game, target=None: {"analysis": {"status": "analyzed"}}
        identifier = self.waiting_job(wire, "resume-route-0004", target_ref=reference)
        self.assertEqual(self.app.jobs(identifier)["target_ref"], reference)
        path, body = f"/api/jobs/{identifier}/resume", {"nxm": self.grant("blocked-grant-fixture")}
        from modbridge_inventory import TitleInventoryError
        inventory.resolve.side_effect = TitleInventoryError("stale_target_ref")
        status, response, _ = self.mutation(path, body)
        self.assertEqual((status, json.loads(response)["error"]["code"]), (409, "stale_target_ref"))
        self.assertEqual(self.app.jobs(identifier)["status"], "needs_nexus_authorization")
        inventory.resolve.side_effect = None
        self.app.storage_failed = True
        status, response, _ = self.mutation(path, body)
        self.assertEqual((status, json.loads(response)["error"]["code"]), (503, "storage_unavailable"))
        self.app.storage_failed = False
        with mock.patch.object(self.app.pending, "full", return_value=True):
            status, response, _ = self.mutation(path, body)
        self.assertEqual((status, json.loads(response)["error"]["code"]), (409, "job_limit"))
        self.assertEqual(self.app.jobs(identifier)["status"], "needs_nexus_authorization")
        self.assertEqual(wire.download_queries, [])
        self.assertEqual(self.mutation(path, body)[0], 202)
        self.assertEqual(self.await_job(identifier)["status"], "analyzed")

    def test_disconnect_revokes_a_resumed_grant_before_the_serialized_lookup(self):
        self.bootstrap()
        wire = FreeNexusWireFixture()
        identifier = self.waiting_job(wire, "resume-route-0005")
        with self.app.operation_lock:
            self.assertEqual(self.mutation(f"/api/jobs/{identifier}/resume", {"nxm": self.grant("revoke-grant-fixture")})[0], 202)
            deadline = time.monotonic() + 1
            while self.app.jobs(identifier)["status"] == "queued" and time.monotonic() < deadline:
                time.sleep(.001)
            self.assertEqual(self.app.jobs(identifier)["status"], "downloading")
            self.app.disconnect()
        self.assertEqual(self.await_job(identifier)["status"], "needs_nexus_authorization")
        self.assertEqual(wire.download_queries, [])

    def test_failed_and_interrupted_jobs_expose_what_a_new_request_needs_and_nothing_secret(self):
        self.bootstrap()
        inventory, reference, _ = self.owned_inventory()
        with mock.patch.object(self.account, "download_links", side_effect=nexus.NexusError("provider")):
            failed = self.app.submit({"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2,
                                      "idempotency_key": "again-public-0001", "target_ref": reference})["id"]
            self.assertEqual(self.await_job(failed)["status"], "failed")
        public = self.app.jobs(failed)
        self.assertEqual((public["game"], public["mod_id"], public["file_id"], public["target_ref"]),
                         ("ownedgame", 1, 2, reference))
        self.assertEqual(public["error"]["code"], "provider")
        for name in ("idempotency_key", "submit_seq", "source_url_secret"):
            self.assertNotIn(name, public)
        self.assertNotIn("again-public-0001", json.dumps(self.app.jobs()))
        # A restart marks in-flight work interrupted; that record is just as repeatable.
        job = self.app.submit({"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2,
                               "idempotency_key": "again-public-0002"})
        self.await_job(job["id"])
        record = dict(self.app.records[job["id"]], status="queued", stage="requested")
        record.pop("sha256"), record.pop("bytes_downloaded")
        for key in ("analysis_report", "compatibility_assessment", "runtime_coverage", "conflicts"):
            record.pop(key, None)
        nexus.save_private_json(self.app.receipts / (job["id"] + ".json"), record)
        restarted = host.BridgeApplication(self.root / "private" / "credentials.json",
            self.root / "quarantine", account=self.account, download=lambda *_: None, analyze=lambda *_: None)
        try:
            interrupted = restarted.jobs(job["id"])
            self.assertEqual((interrupted["status"], interrupted["error"]["code"]), ("interrupted", "interrupted"))
            self.assertEqual((interrupted["game"], interrupted["mod_id"], interrupted["file_id"]), ("ownedgame", 1, 2))
            self.assertNotIn("again-public-0002", json.dumps(restarted.jobs()))
            # Request again is a plain new submission: a fresh key makes a fresh job; the old key replays.
            replay = restarted.submit({"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2,
                                       "idempotency_key": "again-public-0002"})
            self.assertEqual(replay["id"], job["id"])
            self.assertEqual(replay["status"], "interrupted")
            fresh = restarted.submit({"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2,
                                      "idempotency_key": "again-public-0003"})
            self.assertNotEqual(fresh["id"], job["id"])
        finally:
            restarted.close()

    def test_ui_error_catalog_covers_every_code_a_host_can_return(self):
        """Every request or job error code of either host must have catalog text in web/app.js."""
        import re
        source = (PROJECT / "web" / "app.js").read_text()
        block = source[source.index("const errorCatalog = Object.freeze({"):source.index("const signInCodes")]
        catalog = set(re.findall(r"^\s{4}([a-z_]+): ", block, re.M))
        python_codes = set(host.MESSAGES) | set(nexus.errors._MESSAGES)
        native = (PROJECT / "native-host" / "http.c").read_text()
        native_codes = set(re.findall(r'code = "([a-z_]+)"', native)) | set(re.findall(r'auth_error\(out, \d+, "([a-z_]+)"\)', native))
        for name, codes in (("python host", python_codes), ("native host", native_codes)):
            self.assertEqual(sorted(codes - catalog), [], name + " returns codes the UI has no text for")
        # ...and so does every code that section 3.2 of the install-flow spec lists.
        spec = (PROJECT / "docs" / "INSTALL-FLOW-UX.md").read_text()
        table = spec[spec.index("### 3.2"):spec.index("### 3.3")]
        listed = set()
        for line in table.splitlines():
            if line.startswith("|") and not line.startswith(("| Code", "|---")):
                listed |= set(re.findall(r"[a-z_]+", line.split("|")[1]))
        self.assertGreaterEqual(len(listed), 45)
        self.assertEqual(sorted(listed - catalog), [], "section 3.2 codes without UI text")
        self.assertTrue({"auth_refused", "download_authorization", "reconnect_required", "job_limit"} <= catalog)

    # ---- "Newer file available": GET /api/jobs/{id}/update ----

    def update_job(self, file_id=2, mod_id=1, key="update-public-0001"):
        self.bootstrap()
        job = self.app.submit({"kind": "download_analyze", "game": "ownedgame", "mod_id": mod_id,
                               "file_id": file_id, "idempotency_key": key})
        self.assertEqual(self.await_job(job["id"])["status"], "analyzed")
        return job["id"]

    @staticmethod
    def update_data(*, edges=((2, 3),), ids=(2, 3)):
        return {"files": [{"file_id": i, "name": f"File {i}", "file_name": f"f{i}.zip", "version": f"{i}.0",
                           "description": "secret-description", "size_kb": 1, "category_name": "UPDATE" if i > 2 else "MAIN",
                           "uploaded_timestamp": 100 * i, "source_url": "https://www.nexusmods.com/ownedgame/mods/1"} for i in ids],
                "file_updates": [{"old_file_id": a, "new_file_id": b, "old_file_name": "old", "new_file_name": "new",
                                  "uploaded_timestamp": 5, "uploaded_time": "t"} for a, b in edges]}

    def update_get(self, identifier):
        status, body, headers = self.request("GET", f"/api/jobs/{identifier}/update")
        return status, json.loads(body), headers

    def test_update_route_reports_newer_current_and_unknown(self):
        identifier = self.update_job()
        self.account.files_and_updates = mock.Mock(return_value=self.update_data())
        status, record, headers = self.update_get(identifier)
        self.assertEqual(status, 200)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(record, {"status": "newer", "steps": 1, "latest": {"file_id": 3, "name": "File 3", "version": "3.0",
                                  "category_name": "UPDATE", "uploaded_timestamp": 300}})
        self.account.files_and_updates.assert_called_once_with("ownedgame", 1)
        # The job's own file is the end of its chain once nothing links it: only "no newer file linked".
        current = self.update_job(file_id=3, key="update-public-0002")
        status, record, _ = self.update_get(current)
        self.assertEqual((status, record["status"], record["steps"], record["latest"]["file_id"]), (200, "current", 0, 3))
        # A file Nexus no longer lists with no link out of it is never guessed at.
        gone = self.update_job(file_id=9, key="update-public-0003")
        self.assertEqual(self.update_get(gone)[1], {"status": "unknown", "latest": None, "steps": 0})
        # A branch is never guessed at either.
        self.app.update_cache.clear()
        self.account.files_and_updates.return_value = self.update_data(edges=((2, 3), (2, 4)), ids=(2, 3, 4))
        self.assertEqual(self.update_get(identifier)[1]["status"], "unknown")
        self.assertEqual(self.account.download_count, 3)

    def test_update_route_is_read_only_and_leaks_nothing_beyond_the_verdict(self):
        identifier = self.update_job()
        data = self.update_data()
        data["files"][1]["key"] = "must-not-leak-key"
        self.account.files_and_updates = mock.Mock(return_value=data)
        before = json.dumps(self.app.jobs(identifier), sort_keys=True)
        status, body, _ = self.request("GET", f"/api/jobs/{identifier}/update")
        self.assertEqual(status, 200)
        record = json.loads(body)
        self.assertEqual(set(record), {"status", "latest", "steps"})
        self.assertEqual(set(record["latest"]), {"file_id", "name", "version", "category_name", "uploaded_timestamp"})
        for secret in (b"secret-description", b"must-not-leak", b"nexusmods.com", str(self.root).encode(), b"idempotency", b"update-public-0001"):
            self.assertNotIn(secret, body)
        self.assertEqual(json.dumps(self.app.jobs(identifier), sort_keys=True), before)
        self.assertEqual(self.app.jobs(identifier)["status"], "analyzed")
        self.assertEqual(list(self.app.receipts.glob("*.json")), [self.app.receipts / (identifier + ".json")])

    def test_update_route_follows_the_get_job_gates(self):
        identifier = self.update_job()
        self.account.files_and_updates = mock.Mock(return_value=self.update_data())
        path = f"/api/jobs/{identifier}/update"
        saved, self.cookie = self.cookie, None
        self.assertEqual(self.request("GET", path)[0], 401)
        self.assertEqual(self.request("GET", path, headers={"Cookie": "mb_session=forged"})[0], 401)
        self.cookie = saved
        self.assertEqual(self.request("GET", path, headers={"Host": "unrelated.example"})[0], 403)
        self.assertEqual(self.request("GET", path, headers={"Origin": "https://unrelated.example"})[0], 403)
        self.assertEqual(self.request("GET", path, headers={"Sec-Fetch-Site": "cross-site"})[0], 403)
        self.account.files_and_updates.assert_not_called()
        # Only the exact lowercase 32-hex route exists; a query string or other verb is not a route.
        for bad in (path + "?refresh=1", f"/api/jobs/{identifier.upper()}/update", f"/api/jobs/{identifier[:31]}/update",
                    f"/api/jobs/{identifier}/update/", f"/api/jobs/{identifier}/updates", f"/api/jobs/{identifier}/update/x"):
            with self.subTest(path=bad):
                self.assertEqual(self.request("GET", bad)[0], 404)
        self.assertEqual(self.mutation(path, {})[0], 404)
        self.assertEqual(self.request("POST", path, {}, {"Origin": self.origin, "X-CSRF-Token": "wrong"})[0], 403)
        self.assertEqual(self.request("DELETE", path)[0], 501)
        self.account.files_and_updates.assert_not_called()
        self.assertEqual(self.request("GET", path)[0], 200)
        self.account.files_and_updates.assert_called_once_with("ownedgame", 1)

    def test_update_route_unknown_job_is_not_found_without_asking_nexus(self):
        self.bootstrap()
        self.account.files_and_updates = mock.Mock()
        status, record, _ = self.update_get("0" * 32)
        self.assertEqual((status, record["error"]["code"]), (404, "not_found"))
        self.assertEqual(record["error"]["message"], host.MESSAGES["not_found"])
        with self.assertRaises(host.HostError) as caught:
            self.app.file_update("f" * 32)
        self.assertEqual((caught.exception.code, caught.exception.status), ("not_found", 404))
        self.account.files_and_updates.assert_not_called()

    def test_update_route_caches_one_provider_fetch_per_mod_for_ten_minutes(self):
        now = [1000.0]
        self.app.update_clock = lambda: now[0]
        identifier = self.update_job()
        other_file = self.update_job(file_id=3, key="update-public-0002")
        self.account.files_and_updates = mock.Mock(return_value=self.update_data())
        self.assertEqual(self.update_get(identifier)[1]["status"], "newer")
        self.assertEqual(self.account.files_and_updates.call_count, 1)
        # Polling, and another job of the same mod, reuse the fetch.
        now[0] += 599.0
        self.assertEqual(self.update_get(identifier)[1]["status"], "newer")
        self.assertEqual(self.update_get(other_file)[1]["status"], "current")
        self.assertEqual(self.account.files_and_updates.call_count, 1)
        # Another mod is a separate entry.
        other_mod = dict(self.app.records[identifier], id="a" * 32, mod_id=2)
        self.app.records[other_mod["id"]] = other_mod
        self.update_get(other_mod["id"])
        self.assertEqual(self.account.files_and_updates.call_args.args, ("ownedgame", 2))
        self.assertEqual(self.account.files_and_updates.call_count, 2)
        # The entry expires exactly ten minutes after its fetch and is then fetched afresh.
        now[0] = 1000.0 + 600.0
        self.account.files_and_updates.return_value = self.update_data(edges=((2, 3), (3, 4)), ids=(2, 3, 4))
        record = self.update_get(identifier)[1]
        self.assertEqual((record["latest"]["file_id"], record["steps"]), (4, 2))
        self.assertEqual(self.account.files_and_updates.call_count, 3)
        self.update_get(identifier)
        self.assertEqual(self.account.files_and_updates.call_count, 3)

    def test_update_cache_is_bounded_least_recently_used_and_dropped_on_account_change(self):
        identifier = self.update_job()
        self.account.files_and_updates = mock.Mock(return_value=self.update_data())
        self.assertEqual(host.UPDATE_CACHE_TTL, 600)
        self.assertEqual(host.UPDATE_CACHE_MAX, 256)
        for mod in range(10, 14):
            self.app.records[f"{mod:032x}"] = dict(self.app.records[identifier], id=f"{mod:032x}", mod_id=mod)
        with mock.patch.object(host, "UPDATE_CACHE_MAX", 3):
            for mod in (10, 11, 12):
                self.update_get(f"{mod:032x}")
            self.update_get(f"{10:032x}")  # touch: 11 is now the oldest
            self.update_get(f"{13:032x}")
            self.assertEqual(list(self.app.update_cache), [("ownedgame", 12), ("ownedgame", 10), ("ownedgame", 13)])
            calls = self.account.files_and_updates.call_count
            self.update_get(f"{10:032x}")
            self.assertEqual(self.account.files_and_updates.call_count, calls)
            self.update_get(f"{11:032x}")
            self.assertEqual(self.account.files_and_updates.call_count, calls + 1)
            self.assertEqual(len(self.app.update_cache), 3)
        # Cached entries hold only what the verdict needs, not descriptions or URLs.
        self.assertNotIn("secret-description", repr(dict(self.app.update_cache)))
        self.assertNotIn("nexusmods", repr(dict(self.app.update_cache)))
        # Disconnecting (or signing in again) drops everything: another account may see other content.
        self.assertEqual(self.mutation("/api/auth/disconnect", {})[0], 200)
        self.assertEqual(len(self.app.update_cache), 0)
        calls = self.account.files_and_updates.call_count
        self.update_get(identifier)
        self.assertEqual(self.account.files_and_updates.call_count, calls + 1)

    def test_update_route_maps_provider_errors_to_existing_codes_and_never_caches_failures(self):
        identifier = self.update_job()
        self.account.files_and_updates = mock.Mock()
        raw = "raw provider body https://api.nexusmods.com/v1/secret?apikey=sk-live /private/internal/path"
        for code in ("rate_limit", "network", "timeout", "provider", "payload", "size_limit", "content_hidden",
                     "reconnect_required", "credentials"):
            with self.subTest(code=code):
                self.account.files_and_updates.side_effect = nexus.NexusError(code, status=429, retry_after=30)
                status, body, _ = self.request("GET", f"/api/jobs/{identifier}/update")
                record = json.loads(body)
                self.assertEqual(status, 400)
                self.assertEqual(record, {"error": {"code": code, "message": str(nexus.NexusError(code))}})
                self.assertNotIn("retry_after", body.decode())
                self.assertEqual(len(self.app.update_cache), 0)
        # Anything unexpected is the generic code, with no exception text, path or URL.
        self.account.files_and_updates.side_effect = RuntimeError(raw)
        status, body, _ = self.request("GET", f"/api/jobs/{identifier}/update")
        self.assertEqual((status, json.loads(body)["error"]["code"]), (500, "operation_failed"))
        for fragment in (b"raw provider", b"sk-live", b"/private", b"nexusmods"):
            self.assertNotIn(fragment, body)
        self.account.files_and_updates.side_effect = None
        self.account.files_and_updates.return_value = self.update_data()
        calls = self.account.files_and_updates.call_count
        self.assertEqual(self.update_get(identifier)[1]["status"], "newer")
        self.assertEqual(self.account.files_and_updates.call_count, calls + 1)

    def test_update_route_takes_the_account_operation_lock_like_other_provider_calls(self):
        identifier = self.update_job()
        self.account.files_and_updates = mock.Mock(return_value=self.update_data())
        lock = mock.MagicMock()
        lock.acquire.return_value = False  # another account operation holds it past the host's wait
        with mock.patch.object(self.app, "operation_lock", lock):
            status, body, _ = self.request("GET", f"/api/jobs/{identifier}/update")
        self.assertEqual((status, json.loads(body)["error"]["code"]), (409, "busy"))
        lock.acquire.assert_called_once_with(timeout=2)
        self.account.files_and_updates.assert_not_called()
        lock = mock.MagicMock()
        lock.acquire.return_value = True
        with mock.patch.object(self.app, "operation_lock", lock):
            self.assertEqual(self.update_get(identifier)[0], 200)
        lock.acquire.assert_called_once_with(timeout=2)
        lock.release.assert_called_once_with()

    def test_update_route_uses_the_real_client_with_one_provider_fetch_per_ttl(self):
        calls = []
        def request(method, uri, **kwargs):
            calls.append(urlsplit(uri).path)
            if urlsplit(uri).path == "/v1/games/ownedgame/mods/1":
                body = {"mod_id": 1, "game_id": 1, "domain_name": "ownedgame", "name": "Owned mod", "author": "A",
                        "version": "1", "contains_adult_content": False, "available": True, "status": "published"}
            elif urlsplit(uri).path == "/v1/games/ownedgame/mods/1/files":
                body = {"files": [{"file_id": i, "name": f"File {i}", "file_name": f"f{i}.zip", "version": str(i), "size_kb": 1,
                                   "category_name": "UPDATE", "uploaded_timestamp": 10 * i} for i in (2, 3)],
                        "file_updates": [{"old_file_id": 2, "new_file_id": 3, "old_file_name": "File 2",
                                          "new_file_name": "File 3", "uploaded_timestamp": 30, "uploaded_time": "t"}]}
            else:
                raise AssertionError("Unexpected fixture provider endpoint")
            return nexus.Response(200, {}, json.dumps(body).encode())
        transport = mock.Mock()
        transport.request.side_effect = request
        account = mock.Mock()
        account.client.side_effect = lambda: nexus.NexusClient(nexus.Credentials(api_key="fixture-only"), transport)
        identifier = self.update_job()
        self.app.account = account
        now = [50.0]
        self.app.update_clock = lambda: now[0]
        first = self.update_get(identifier)
        self.assertEqual(first[1], {"status": "newer", "steps": 1, "latest": {"file_id": 3, "name": "File 3", "version": "3",
                                    "category_name": "UPDATE", "uploaded_timestamp": 30}})
        self.assertEqual(calls, ["/v1/games/ownedgame/mods/1", "/v1/games/ownedgame/mods/1/files"])
        self.update_get(identifier)
        self.assertEqual(len(calls), 2)
        now[0] += 601
        self.update_get(identifier)
        self.assertEqual(len(calls), 4)

    # ---- Mod requirements: GET /api/games/{game}/mods/{mod}/requirements ----

    @staticmethod
    def requirements_data(game="ownedgame", mod=1, **changes):
        record = {"game": game, "mod_id": mod, "discovery_complete": True,
            "nexus_requirements": [
                {"id": "relation-1", "external_requirement": False, "game": "ownedgame", "mod_id": "5", "name": "Needed mod",
                 "notes": "", "url": "https://www.nexusmods.com/ownedgame/mods/5", "satisfaction": "unresolved"},
                {"id": "relation-2", "external_requirement": True, "game": "External", "mod_id": "v1", "name": "UE4SS",
                 "notes": "Install first", "url": "https://github.com/UE4SS-RE/RE-UE4SS", "satisfaction": "unresolved"}],
            "dlc_requirements": [{"game": "ownedgame", "id": "dlc-1", "name": "Expansion", "notes": "", "satisfaction": "unresolved"}],
            "file_version_constraints_provided": False, "installation_supported": False}
        record.update(changes)
        return record

    def requirements_get(self, game="ownedgame", mod=1):
        status, body, headers = self.request("GET", f"/api/games/{game}/mods/{mod}/requirements")
        return status, json.loads(body), headers

    def test_mod_requirements_capability_is_advertised(self):
        record = self.bootstrap()
        self.assertIs(record["capabilities"]["mod_requirements"], True)
        self.assertIs(self.app.capabilities()["capabilities"]["mod_requirements"], True)

    def test_requirements_route_returns_the_listed_requirements_unresolved(self):
        self.bootstrap()
        self.account.requirements = mock.Mock(return_value=self.requirements_data())
        status, record, headers = self.requirements_get()
        self.assertEqual(status, 200)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(record, self.requirements_data())
        self.account.requirements.assert_called_once_with("ownedgame", 1)
        self.assertEqual({row["satisfaction"] for row in record["nexus_requirements"] + record["dlc_requirements"]}, {"unresolved"})
        self.assertEqual((record["installation_supported"], record["file_version_constraints_provided"]), (False, False))
        # An author who listed nothing is an explicit, complete empty set, not an error.
        self.account.requirements.return_value = self.requirements_data(mod=2, nexus_requirements=[], dlc_requirements=[])
        status, record, _ = self.requirements_get(mod=2)
        self.assertEqual((status, record["discovery_complete"], record["nexus_requirements"], record["dlc_requirements"]),
                         (200, True, [], []))

    def test_requirements_route_is_read_only_and_leaks_nothing_beyond_the_record(self):
        self.bootstrap()
        data = self.requirements_data()
        data["apikey"] = "must-not-leak-key"
        data["nexus_requirements"][0]["fileId"] = "must-not-leak-file"
        data["nexus_requirements"][0]["satisfaction"] = "met"  # the wire form never carries anything but "unresolved"
        data["dlc_requirements"][0]["installed"] = True
        self.account.requirements = mock.Mock(return_value=data)
        status, body, _ = self.request("GET", "/api/games/ownedgame/mods/1/requirements")
        self.assertEqual(status, 200)
        record = json.loads(body)
        self.assertEqual(set(record), {"game", "mod_id", "discovery_complete", "nexus_requirements", "dlc_requirements",
                                       "file_version_constraints_provided", "installation_supported"})
        self.assertEqual(set(record["nexus_requirements"][0]), {"id", "external_requirement", "game", "mod_id", "name",
                                                                  "notes", "url", "satisfaction"})
        self.assertEqual(set(record["dlc_requirements"][0]), {"game", "id", "name", "notes", "satisfaction"})
        self.assertEqual(record["nexus_requirements"][0]["satisfaction"], "unresolved")
        for secret in (b"must-not-leak", b"installed", b'"met"', str(self.root).encode(), b"idempotency", b"csrf"):
            self.assertNotIn(secret, body)
        self.assertEqual(self.app.jobs(), [])
        self.assertEqual(list(self.app.receipts.glob("*.json")), [])
        self.assertEqual(self.account.download_count, 0)

    def test_requirement_links_are_author_text_and_only_https_survives(self):
        self.bootstrap()
        urls = {"https://example.test/a/b?c=d#frag": "https://example.test/a/b?c=d#frag",
                "https://example.test:443/x": "https://example.test:443/x",
                "http://example.test/plain": "", "javascript:alert(1)": "", "data:text/html,x": "", "//example.test/x": "",
                "ftp://example.test/x": "", "nxm://game/mods/1/files/2": "", "https://user:pw@example.test/": "",
                "https://example.test:8443/": "", "https://example.test:99999/": "", "https://exa mple.test/": "",
                "https://example.test/\njavascript": "", "https://example.test\\evil/": "", "https://例え.test/": "",
                "https:///nohost": "", "": "", "relative/path": "", "https://example.test/" + "x" * 8200: ""}
        for url, expected in urls.items():
            with self.subTest(url=url[:40]):
                self.app.requirements_cache.clear()
                data = self.requirements_data()
                data["nexus_requirements"][1]["url"] = url
                self.account.requirements = mock.Mock(return_value=data)
                record = self.requirements_get()[1]
                self.assertEqual(record["nexus_requirements"][1]["url"], expected)
                self.assertEqual(record["nexus_requirements"][0]["url"], "https://www.nexusmods.com/ownedgame/mods/5")

    def test_requirements_route_follows_the_get_gates(self):
        self.bootstrap()
        self.account.requirements = mock.Mock(return_value=self.requirements_data())
        path = "/api/games/ownedgame/mods/1/requirements"
        saved, self.cookie = self.cookie, None
        self.assertEqual(self.request("GET", path)[0], 401)
        self.assertEqual(self.request("GET", path, headers={"Cookie": "mb_session=forged"})[0], 401)
        self.cookie = saved
        self.assertEqual(self.request("GET", path, headers={"Host": "unrelated.example"})[0], 403)
        self.assertEqual(self.request("GET", path, headers={"Origin": "https://unrelated.example"})[0], 403)
        self.assertEqual(self.request("GET", path, headers={"Sec-Fetch-Site": "cross-site"})[0], 403)
        self.account.requirements.assert_not_called()
        # Only the exact route exists; a query string, other verb or other path shape is not a route.
        for bad in (path + "?refresh=1", path + "/", path + "/x", "/api/games/ownedgame/mods/1/requirement",
                    "/api/games/ownedgame/mods/1/Requirements", "/api/games/OWNED/mods/1/requirements",
                    "/api/games/ownedgame/mods/abc/requirements", "/api/games/ownedgame/mods//requirements",
                    "/api/games/ownedgame/mods/1/files/requirements", "/api/games/ownedgame/requirements",
                    "/api/games/ownedgame/mods/12345678901234567890/requirements", "/api/requirements"):
            with self.subTest(path=bad):
                self.assertEqual(self.request("GET", bad)[0], 404)
        self.assertEqual(self.mutation(path, {})[0], 404)
        self.assertEqual(self.request("POST", path, {}, {"Origin": self.origin, "X-CSRF-Token": "wrong"})[0], 403)
        self.assertEqual(self.request("DELETE", path)[0], 501)
        self.account.requirements.assert_not_called()
        self.assertEqual(self.request("GET", path)[0], 200)
        self.account.requirements.assert_called_once_with("ownedgame", 1)

    def test_requirements_route_refuses_ids_the_graphql_query_cannot_carry_without_asking_nexus(self):
        self.bootstrap()
        self.account.requirements = mock.Mock(return_value=self.requirements_data())
        for mod in (0, 2147483648, 9223372036854775807, 9223372036854775808):
            with self.subTest(mod=mod):
                status, record, _ = self.requirements_get(mod=mod)
                self.assertEqual((status, record), (400, {"error": {"code": "invalid_input", "message": str(nexus.NexusError("invalid_input"))}}))
        self.account.requirements.assert_not_called()
        self.assertEqual(self.requirements_get(mod=2147483647)[0], 200)
        self.account.requirements.assert_called_once_with("ownedgame", 2147483647)
        self.assertEqual(len(self.app.requirements_cache), 1)

    def test_requirements_cache_is_one_provider_fetch_per_mod_for_ten_minutes(self):
        self.bootstrap()
        now = [1000.0]
        self.app.update_clock = lambda: now[0]
        self.account.requirements = mock.Mock(side_effect=lambda game, mod: self.requirements_data(game=game, mod=mod))
        self.assertEqual((host.REQUIREMENTS_CACHE_TTL, host.REQUIREMENTS_CACHE_MAX), (600, 256))
        first = self.requirements_get()[1]
        self.assertEqual(self.account.requirements.call_count, 1)
        now[0] += 599.0
        self.assertEqual(self.requirements_get()[1], first)
        self.assertEqual(self.account.requirements.call_count, 1)
        # Another mod, and another game with the same mod id, are separate entries.
        self.requirements_get(mod=2)
        self.assertEqual(self.account.requirements.call_args.args, ("ownedgame", 2))
        self.requirements_get(game="othergame", mod=1)
        self.assertEqual(self.account.requirements.call_args.args, ("othergame", 1))
        self.assertEqual(self.account.requirements.call_count, 3)
        # Expiry is exactly ten minutes after the fetch, then it is fetched afresh.
        now[0] = 1000.0 + 600.0
        self.account.requirements.side_effect = lambda game, mod: self.requirements_data(game=game, mod=mod, nexus_requirements=[])
        self.assertEqual(self.requirements_get()[1]["nexus_requirements"], [])
        self.assertEqual(self.account.requirements.call_count, 4)
        self.requirements_get()
        self.assertEqual(self.account.requirements.call_count, 4)
        # A clock that runs backwards never serves an entry from the future.
        now[0] = 10.0
        self.requirements_get()
        self.assertEqual(self.account.requirements.call_count, 5)
        # The newer-file cache is a different cache: neither fills nor reads the other.
        self.assertEqual(len(self.app.update_cache), 0)

    def test_requirements_results_are_copies_of_the_cached_entry(self):
        self.account.requirements = mock.Mock(return_value=self.requirements_data())
        first = self.app.mod_requirements("ownedgame", 1)
        first["nexus_requirements"].clear()
        first["installation_supported"] = True
        again = self.app.mod_requirements("ownedgame", 1)
        self.assertEqual(self.account.requirements.call_count, 1)
        self.assertEqual(len(again["nexus_requirements"]), 2)
        self.assertIs(again["installation_supported"], False)
        self.assertIsNot(again, self.app.mod_requirements("ownedgame", 1))
        with self.assertRaises(nexus.NexusError) as caught:
            self.app.mod_requirements("ownedgame", 0)
        self.assertEqual(caught.exception.code, "invalid_input")

    def test_requirements_cache_is_bounded_least_recently_used_and_dropped_on_account_change(self):
        self.bootstrap()
        self.account.requirements = mock.Mock(side_effect=lambda game, mod: self.requirements_data(game=game, mod=mod))
        with mock.patch.object(host, "REQUIREMENTS_CACHE_MAX", 3):
            for mod in (10, 11, 12):
                self.requirements_get(mod=mod)
            self.requirements_get(mod=10)  # touch: 11 is now the oldest
            self.requirements_get(mod=13)
            self.assertEqual(list(self.app.requirements_cache), [("ownedgame", 12), ("ownedgame", 10), ("ownedgame", 13)])
            calls = self.account.requirements.call_count
            self.requirements_get(mod=10)
            self.assertEqual(self.account.requirements.call_count, calls)
            self.requirements_get(mod=11)
            self.assertEqual(self.account.requirements.call_count, calls + 1)
            self.assertEqual(len(self.app.requirements_cache), 3)
        # Entries hold only the wire form.
        self.assertNotIn("must-not-leak", repr(dict(self.app.requirements_cache)))
        # Disconnecting drops both provider caches: another account may see other content.
        self.app.update_cache[("ownedgame", 1)] = (0, {})
        self.assertEqual(self.mutation("/api/auth/disconnect", {})[0], 200)
        self.assertEqual((len(self.app.requirements_cache), len(self.app.update_cache)), (0, 0))
        calls = self.account.requirements.call_count
        self.requirements_get(mod=10)
        self.assertEqual(self.account.requirements.call_count, calls + 1)

    def test_requirements_cache_is_dropped_on_sign_in(self):
        self.app.oauth_config = nexus.OAuthConfig("owned-client", self.origin + "/auth/callback")
        self.bootstrap()
        self.account.requirements = mock.Mock(return_value=self.requirements_data())
        self.requirements_get()
        self.app.update_cache[("ownedgame", 1)] = (0, {})
        self.assertEqual(len(self.app.requirements_cache), 1)
        params = parse_qs(urlsplit(json.loads(self.mutation("/api/auth/start", {})[1])["authorization_url"]).query)
        callback = "/auth/callback?" + "code=owned-private-code&state=" + params["state"][0]
        self.assertEqual(self.request("GET", callback, headers={"Sec-Fetch-Site": "cross-site"})[0], 303)
        self.assertEqual((len(self.app.requirements_cache), len(self.app.update_cache)), (0, 0))

    def test_requirements_route_maps_provider_errors_to_existing_codes_and_never_caches_failures(self):
        self.bootstrap()
        self.account.requirements = mock.Mock()
        raw = "raw provider body https://api.nexusmods.com/v2/graphql?apikey=sk-live /private/internal/path"
        for code in ("rate_limit", "network", "timeout", "provider", "payload", "size_limit", "content_hidden",
                     "reconnect_required", "credentials", "redirect"):
            with self.subTest(code=code):
                self.account.requirements.side_effect = nexus.NexusError(code, status=429, retry_after=30)
                status, body, _ = self.request("GET", "/api/games/ownedgame/mods/1/requirements")
                self.assertEqual(status, 400)
                self.assertEqual(json.loads(body), {"error": {"code": code, "message": str(nexus.NexusError(code))}})
                self.assertNotIn("retry_after", body.decode())
                self.assertEqual(len(self.app.requirements_cache), 0)
        self.account.requirements.side_effect = RuntimeError(raw)
        status, body, _ = self.request("GET", "/api/games/ownedgame/mods/1/requirements")
        self.assertEqual((status, json.loads(body)["error"]["code"]), (500, "operation_failed"))
        for fragment in (b"raw provider", b"sk-live", b"/private", b"nexusmods", b"graphql"):
            self.assertNotIn(fragment, body)
        self.account.requirements.side_effect = None
        self.account.requirements.return_value = self.requirements_data()
        calls = self.account.requirements.call_count
        self.assertEqual(self.requirements_get()[0], 200)
        self.assertEqual(self.account.requirements.call_count, calls + 1)
        # A failure after a success leaves the earlier entry alone and caches nothing new.
        self.account.requirements.side_effect = nexus.NexusError("network")
        self.assertEqual(self.requirements_get(mod=2)[0], 400)
        self.assertEqual(list(self.app.requirements_cache), [("ownedgame", 1)])

    def test_requirements_route_takes_the_account_operation_lock_like_other_provider_calls(self):
        self.bootstrap()
        self.account.requirements = mock.Mock(return_value=self.requirements_data())
        lock = mock.MagicMock()
        lock.acquire.return_value = False
        with mock.patch.object(self.app, "operation_lock", lock):
            status, record, _ = self.requirements_get()
        self.assertEqual((status, record["error"]["code"]), (409, "busy"))
        lock.acquire.assert_called_once_with(timeout=2)
        self.account.requirements.assert_not_called()
        lock = mock.MagicMock()
        lock.acquire.return_value = True
        with mock.patch.object(self.app, "operation_lock", lock):
            self.assertEqual(self.requirements_get()[0], 200)
        lock.acquire.assert_called_once_with(timeout=2)
        lock.release.assert_called_once_with()

    def test_requirements_route_uses_the_real_client_and_decoder_with_one_fetch_per_ttl(self):
        self.bootstrap()
        calls = []
        graphql = {"data": {"modRequirements": {
            "nexusRequirements": {"nodes": [
                {"id": "r1", "externalRequirement": True, "gameId": "", "modId": "", "modName": "UE4SS", "notes": None,
                 "url": "javascript:alert(1)"},
                {"id": "r2", "externalRequirement": False, "gameId": "ownedgame", "modId": "5", "modName": "Needed mod",
                 "notes": "first", "url": "https://www.nexusmods.com/ownedgame/mods/5", "fileId": "must-not-leak"}],
                "nodesCount": 2, "totalCount": 2},
            "dlcRequirements": [{"gameExpansion": {"gameId": "ownedgame", "id": "dlc-1", "name": "Expansion"}, "notes": None}]}}}
        def request(method, uri, **kwargs):
            calls.append((method, urlsplit(uri).path, kwargs))
            if (method, urlsplit(uri).path) == ("GET", "/v1/games/ownedgame/mods/1"):
                body = {"mod_id": 1, "game_id": 1, "domain_name": "ownedgame", "name": "Owned mod", "author": "A",
                        "version": "1", "contains_adult_content": False, "available": True, "status": "published"}
            elif (method, urlsplit(uri).path) == ("POST", "/v2/graphql"):
                body = graphql
            else:
                raise AssertionError("Unexpected fixture provider endpoint")
            return nexus.Response(200, {}, json.dumps(body).encode())
        transport = mock.Mock()
        transport.request.side_effect = request
        account = mock.Mock()
        account.client.side_effect = lambda: nexus.NexusClient(nexus.Credentials(api_key="fixture-only"), transport)
        self.app.account = account
        now = [50.0]
        self.app.update_clock = lambda: now[0]
        status, record, _ = self.requirements_get()
        self.assertEqual(status, 200)
        self.assertEqual([call[:2] for call in calls], [("GET", "/v1/games/ownedgame/mods/1"), ("POST", "/v2/graphql")])
        self.assertEqual(json.loads(calls[1][2]["body"])["variables"], {"gameDomainName": "ownedgame", "modId": 1})
        self.assertEqual(calls[1][2]["headers"]["apikey"], "fixture-only")
        self.assertEqual([(row["id"], row["external_requirement"], row["notes"], row["url"]) for row in record["nexus_requirements"]],
                         [("r1", True, "", ""), ("r2", False, "first", "https://www.nexusmods.com/ownedgame/mods/5")])
        self.assertEqual(record["dlc_requirements"], [{"game": "ownedgame", "id": "dlc-1", "name": "Expansion", "notes": "",
                                                       "satisfaction": "unresolved"}])
        self.assertNotIn("must-not-leak", json.dumps(record))
        self.requirements_get()
        self.assertEqual(len(calls), 2)
        now[0] += 601
        self.requirements_get()
        self.assertEqual(len(calls), 4)
        # A truncated page is a payload error with the existing code, and is not cached.
        self.app.requirements_cache.clear()
        graphql["data"]["modRequirements"]["nexusRequirements"]["totalCount"] = 3
        status, record, _ = self.requirements_get()
        self.assertEqual((status, record["error"]["code"]), (400, "payload"))
        self.assertEqual(len(self.app.requirements_cache), 0)

    # ---- Mod search: GET /api/games/{game}/mods/search?q=...&page=N ----

    @staticmethod
    def search_data(page=1, total=1, **changes):
        record = {"items": [{"mod_id": 1, "game_id": 1, "game_domain": "ownedgame", "name": "Owned mod", "author": "Owned author",
                             "summary": "A fixture", "description": "", "version": "1", "contains_adult_content": False,
                             "source_url": "https://www.nexusmods.com/ownedgame/mods/1"}],
                  "total": total, "page": page, "pages": 1}
        record.update(changes)
        return record

    def search_get(self, query="q=owned", game="ownedgame"):
        status, body, headers = self.request("GET", f"/api/games/{game}/mods/search" + ("?" + query if query is not None else ""))
        return status, json.loads(body), headers

    def search_fake(self, **kwargs):
        self.account.search_mods = mock.Mock(side_effect=lambda game, text, page: self.search_data(page=page), **kwargs)
        return self.account.search_mods

    def test_mod_search_capability_is_advertised(self):
        record = self.bootstrap()
        self.assertIs(record["capabilities"]["mod_search"], True)
        self.assertIs(self.app.capabilities()["capabilities"]["mod_search"], True)

    def test_search_route_returns_the_page_in_the_documented_shape(self):
        self.bootstrap()
        fake = self.search_fake()
        status, record, headers = self.search_get("q=owned%20mod")
        self.assertEqual(status, 200)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(record, self.search_data())
        self.assertEqual(set(record), {"items", "total", "page", "pages"})
        fake.assert_called_once_with("ownedgame", "owned mod", 1)
        self.assertEqual(self.account.requests, [])  # not the latest-mods listing
        self.assertEqual(self.account.download_count, 0)
        self.assertEqual(self.app.jobs(), [])
        # Paging, and the spellings of a space; the text reaches the client trimmed and with one space between words.
        for query, expected in (("q=owned+mod&page=3", ("ownedgame", "owned mod", 3)), ("page=50&q=ab", ("ownedgame", "ab", 50)),
                                ("q=%20%20owned%20%20%20mod%20", ("ownedgame", "owned mod", 1)), ("q=ab&page=007", ("ownedgame", "ab", 7)),
                                ("q=%E5%90%8D%E5%89%8D&page=2", ("ownedgame", "名前", 2)),
                                ("q=a%26b%3Dc%23d%25e%22", ("ownedgame", 'a&b=c#d%e"', 1)), ("q=%2A%3F", ("ownedgame", "*?", 1))):
            with self.subTest(query=query):
                self.app.search_cache.clear()
                status, record, _ = self.search_get(query)
                self.assertEqual(status, 200)
                self.assertEqual(fake.call_args.args, expected)
                self.assertEqual(record["page"], expected[2])
        # A zero-result search is a complete answer, not an error.
        self.account.search_mods = mock.Mock(return_value={"items": [], "total": 0, "page": 1, "pages": 0})
        self.assertEqual(self.search_get("q=nothing")[1], {"items": [], "total": 0, "page": 1, "pages": 0})

    def test_search_route_leaks_nothing_beyond_the_four_keys(self):
        self.bootstrap()
        data = self.search_data()
        data["apikey"] = "must-not-leak-key"
        data["query"] = "must-not-leak-query"
        self.account.search_mods = mock.Mock(return_value=data)
        status, body, _ = self.request("GET", "/api/games/ownedgame/mods/search?q=owned")
        self.assertEqual(status, 200)
        self.assertEqual(set(json.loads(body)), {"items", "total", "page", "pages"})
        for secret in (b"must-not-leak", str(self.root).encode(), b"csrf", b"idempotency"):
            self.assertNotIn(secret, body)

    def test_search_route_follows_the_get_gates(self):
        self.bootstrap()
        fake = self.search_fake()
        path = "/api/games/ownedgame/mods/search?q=owned"
        saved, self.cookie = self.cookie, None
        self.assertEqual(self.request("GET", path)[0], 401)
        self.assertEqual(self.request("GET", path, headers={"Cookie": "mb_session=forged"})[0], 401)
        self.cookie = saved
        self.assertEqual(self.request("GET", path, headers={"Host": "unrelated.example"})[0], 403)
        self.assertEqual(self.request("GET", path, headers={"Origin": "https://unrelated.example"})[0], 403)
        self.assertEqual(self.request("GET", path, headers={"Sec-Fetch-Site": "cross-site"})[0], 403)
        fake.assert_not_called()
        # Only the exact route exists; the other verbs and other path shapes are not routes.
        for bad in ("/api/games/ownedgame/mods/search/?q=owned", "/api/games/ownedgame/mods/Search?q=owned",
                    "/api/games/ownedgame/mods/search/x?q=owned", "/api/games/OWNED/mods/search?q=owned",
                    "/api/games/ownedgame/search?q=owned", "/api/games/ownedgame/mods/1/search?q=owned",
                    "/api/games/ownedgame/mods/search/files?q=owned", "/api/mods/search?q=owned", "/api/search?q=owned"):
            with self.subTest(path=bad):
                self.assertEqual(self.request("GET", bad)[0], 404)
        self.assertEqual(self.mutation(path.split("?")[0], {})[0], 404)
        self.assertEqual(self.request("POST", path, {}, {"Origin": self.origin, "X-CSRF-Token": "wrong"})[0], 403)
        self.assertEqual(self.request("DELETE", path)[0], 501)
        fake.assert_not_called()
        # The query string is allowed for this route only.
        for other in ("/api/games/ownedgame/mods?q=owned", "/api/games?q=owned", "/api/account?q=owned", "/api/jobs?page=2",
                      "/api/games/ownedgame/mods/1?q=owned", "/api/games/ownedgame/mods/1/requirements?q=owned"):
            with self.subTest(path=other):
                self.assertEqual(self.request("GET", other)[0], 404)
        self.assertEqual(self.account.requests, [])
        self.assertEqual(self.request("GET", path)[0], 200)
        fake.assert_called_once_with("ownedgame", "owned", 1)

    def test_search_route_refuses_invalid_queries_without_asking_nexus(self):
        self.bootstrap()
        fake = self.search_fake()
        refused = {"error": {"code": "invalid_input", "message": "The request is invalid."}}
        queries = [None, "", "q", "q=", "q=a", "q=%20a%20", "q=%20%20%20", "q=" + "x" * 101, "q=%00ab", "q=a%00", "q=a%0Ab",
                   "q=a%09b", "q=ab%0D", "q=%7Fab", "q=a%C2%85b", "q=a%E2%80%A8b", "q=%FF%FE", "q=%C3", "q=%ED%A0%80ab",
                   "q=ab&q=cd", "q=ab&page=0", "q=ab&page=51", "q=ab&page=abc", "q=ab&page=-1", "q=ab&page=1.5", "q=ab&page=1e1",
                   "q=ab&page=", "q=ab&page=1&page=2", "q=ab&page=%201", "q=ab&page=99999", "q=ab&page=00000", "q=ab&sort=name",
                   "q=ab&Q=cd", "Q=ab", "page=2", "q=ab&q", "&q=ab", "q=ab&", "q=ab&&page=1", "q=ab&page=1&x=1&y=2", "q=ab#x",
                   "q=" + "%41" * 1000, "q=ab&" + "p" * 2100 + "=1", "query=ab", "name=ab"]
        for query in queries:
            with self.subTest(query=(query or "")[:40]):
                status, record, _ = self.search_get(query)
                self.assertEqual((status, record), (400, refused))
        # The game domain is judged as everywhere else.
        for game in ("_x", "-x"):
            with self.subTest(game=game):
                status, record, _ = self.search_get("q=ab", game=game)
                self.assertEqual((status, record), (400, refused))
        fake.assert_not_called()
        self.assertEqual(len(self.app.search_cache), 0)
        # 100 characters and the first and last page are the limits that are still fine.
        self.assertEqual(self.search_get("q=" + "x" * 100 + "&page=50")[0], 200)
        self.assertEqual(fake.call_args.args, ("ownedgame", "x" * 100, 50))
        self.assertEqual(self.search_get("q=ab&page=1")[0], 200)
        # The same checks guard the application method itself.
        for args in (("ownedgame", "a", 1), ("ownedgame", "ab", 0), ("ownedgame", "ab", 51), ("ownedgame", "ab", True),
                     ("ownedgame", "ab", "1"), ("ownedgame", None, 1), ("ownedgame", "a\x00b", 1), ("../x", "ab", 1)):
            with self.subTest(args=args):
                with self.assertRaises(host.HostError) as caught:
                    self.app.mod_search(*args)
                self.assertEqual((caught.exception.code, caught.exception.status), ("invalid_input", 400))
        self.assertEqual(fake.call_count, 2)
        for query in ("", "a", "q=" + "x" * 3000, "q=\u540d", "q=ab&q=cd", "q=ab&page=x"):
            with self.subTest(parse=query[:20]):
                with self.assertRaises(host.HostError) as caught:
                    host.parse_search_query(query)
                self.assertEqual(caught.exception.code, "invalid_input")
        self.assertEqual(host.parse_search_query("q=ab"), ("ab", 1))
        self.assertEqual(host.parse_search_query("page=0&q=ab"), ("ab", 0))  # the range is judged with the text, by mod_search

    def test_search_cache_is_one_provider_fetch_per_game_text_and_page_for_two_minutes(self):
        self.bootstrap()
        now = [1000.0]
        self.app.update_clock = lambda: now[0]
        fake = self.search_fake()
        self.assertEqual((host.SEARCH_CACHE_TTL, host.SEARCH_CACHE_MAX), (120, 128))
        first = self.search_get("q=owned")[1]
        self.assertEqual(fake.call_count, 1)
        now[0] += 119.0
        self.assertEqual(self.search_get("q=owned")[1], first)
        self.assertEqual(fake.call_count, 1)
        # The key is the normalized text: spacing is not a different search; case is (Nexus may tell them apart).
        self.search_get("q=%20owned%20")
        self.assertEqual(fake.call_count, 1)
        self.search_get("q=a+++b")
        self.search_get("q=a+b")
        self.assertEqual(fake.call_count, 2)
        self.search_get("q=Owned")
        self.assertEqual(fake.call_args.args, ("ownedgame", "Owned", 1))
        # Another page, another text and another game are separate entries.
        self.search_get("q=owned&page=2")
        self.assertEqual(fake.call_args.args, ("ownedgame", "owned", 2))
        self.search_get("q=owned&page=1")
        self.search_get("q=other")
        self.search_get("q=owned", game="othergame")
        self.assertEqual(fake.call_args.args, ("othergame", "owned", 1))
        self.assertEqual(fake.call_count, 6)
        self.assertEqual(list(self.app.search_cache), [("ownedgame", "a b", 1), ("ownedgame", "Owned", 1), ("ownedgame", "owned", 2),
                         ("ownedgame", "owned", 1), ("ownedgame", "other", 1), ("othergame", "owned", 1)])
        # An entry expires exactly two minutes after its fetch; it is fetched afresh and the new fetch restarts the clock.
        now[0] = 1000.0 + 120.0
        fake.side_effect = lambda game, text, page: self.search_data(page=page, total=9, pages=1)
        self.assertEqual(self.search_get("q=owned")[1]["total"], 9)
        self.assertEqual(fake.call_count, 7)
        self.search_get("q=owned")
        self.assertEqual(fake.call_count, 7)
        # A clock that runs backwards never serves an entry from the future.
        now[0] = 10.0
        self.search_get("q=owned")
        self.assertEqual(fake.call_count, 8)
        # No other cache is filled or read.
        self.assertEqual((len(self.app.update_cache), len(self.app.requirements_cache)), (0, 0))

    def test_search_results_are_copies_of_the_cached_entry(self):
        fake = self.search_fake()
        first = self.app.mod_search("ownedgame", "owned")
        first["items"].clear()
        first["total"] = 99
        again = self.app.mod_search("ownedgame", "owned", 1)
        self.assertEqual(fake.call_count, 1)
        self.assertEqual((len(again["items"]), again["total"]), (1, 1))
        self.assertIsNot(again, self.app.mod_search("ownedgame", "owned"))
        again["items"][0]["name"] = "changed"
        self.assertEqual(self.app.mod_search("ownedgame", "owned")["items"][0]["name"], "Owned mod")

    def test_search_cache_is_bounded_least_recently_used_and_dropped_on_account_change(self):
        self.bootstrap()
        fake = self.search_fake()
        with mock.patch.object(host, "SEARCH_CACHE_MAX", 3):
            for text in ("aa", "bb", "cc"):
                self.search_get("q=" + text)
            self.search_get("q=aa")  # touch: bb is now the oldest
            self.search_get("q=dd")
            self.assertEqual(list(self.app.search_cache), [("ownedgame", "cc", 1), ("ownedgame", "aa", 1), ("ownedgame", "dd", 1)])
            calls = fake.call_count
            self.search_get("q=aa")
            self.assertEqual(fake.call_count, calls)
            self.search_get("q=bb")
            self.assertEqual(fake.call_count, calls + 1)
            self.assertEqual(len(self.app.search_cache), 3)
        # With the real bound, a hundred and thirty distinct searches keep the newest 128.
        self.app.search_cache.clear()
        for number in range(130):
            self.search_get("q=text%03d" % number)
        self.assertEqual(len(self.app.search_cache), 128)
        self.assertNotIn(("ownedgame", "text000", 1), self.app.search_cache)
        self.assertIn(("ownedgame", "text129", 1), self.app.search_cache)
        # Disconnecting drops every provider cache: another account may see other content.
        self.app.update_cache[("ownedgame", 1)] = (0, {})
        self.app.requirements_cache[("ownedgame", 1)] = (0, {})
        self.assertEqual(self.mutation("/api/auth/disconnect", {})[0], 200)
        self.assertEqual((len(self.app.search_cache), len(self.app.update_cache), len(self.app.requirements_cache)), (0, 0, 0))
        calls = fake.call_count
        self.search_get("q=text129")
        self.assertEqual(fake.call_count, calls + 1)

    def test_search_cache_is_dropped_on_sign_in(self):
        self.app.oauth_config = nexus.OAuthConfig("owned-client", self.origin + "/auth/callback")
        self.bootstrap()
        self.search_fake()
        self.search_get("q=owned")
        self.assertEqual(len(self.app.search_cache), 1)
        params = parse_qs(urlsplit(json.loads(self.mutation("/api/auth/start", {})[1])["authorization_url"]).query)
        callback = "/auth/callback?" + "code=owned-private-code&state=" + params["state"][0]
        self.assertEqual(self.request("GET", callback, headers={"Sec-Fetch-Site": "cross-site"})[0], 303)
        self.assertEqual(len(self.app.search_cache), 0)

    def test_search_route_maps_provider_errors_to_existing_codes_and_never_caches_failures(self):
        self.bootstrap()
        self.account.search_mods = mock.Mock()
        raw = "raw provider body https://api.nexusmods.com/v2/graphql?apikey=sk-live /private/internal/path"
        for code in ("rate_limit", "network", "timeout", "provider", "payload", "size_limit", "content_hidden",
                     "reconnect_required", "credentials", "redirect", "invalid_input"):
            with self.subTest(code=code):
                self.account.search_mods.side_effect = nexus.NexusError(code, status=429, retry_after=30)
                status, body, _ = self.request("GET", "/api/games/ownedgame/mods/search?q=owned")
                self.assertEqual(status, 400)
                self.assertEqual(json.loads(body), {"error": {"code": code, "message": str(nexus.NexusError(code))}})
                self.assertNotIn("retry_after", body.decode())
                self.assertEqual(len(self.app.search_cache), 0)
        self.account.search_mods.side_effect = RuntimeError(raw)
        status, body, _ = self.request("GET", "/api/games/ownedgame/mods/search?q=owned")
        self.assertEqual((status, json.loads(body)["error"]["code"]), (500, "operation_failed"))
        for fragment in (b"raw provider", b"sk-live", b"/private", b"nexusmods", b"graphql"):
            self.assertNotIn(fragment, body)
        self.account.search_mods.side_effect = None
        self.account.search_mods.return_value = self.search_data()
        calls = self.account.search_mods.call_count
        self.assertEqual(self.search_get("q=owned")[0], 200)
        self.assertEqual(self.account.search_mods.call_count, calls + 1)
        # A failure after a success leaves the earlier entry alone and caches nothing new.
        self.account.search_mods.side_effect = nexus.NexusError("network")
        self.assertEqual(self.search_get("q=another")[0], 400)
        self.assertEqual(list(self.app.search_cache), [("ownedgame", "owned", 1)])
        # And the failed search is asked again, not remembered.
        self.account.search_mods.side_effect = None
        calls = self.account.search_mods.call_count
        self.assertEqual(self.search_get("q=another")[0], 200)
        self.assertEqual(self.account.search_mods.call_count, calls + 1)

    def test_search_route_takes_the_account_operation_lock_like_other_provider_calls(self):
        self.bootstrap()
        fake = self.search_fake()
        lock = mock.MagicMock()
        lock.acquire.return_value = False
        with mock.patch.object(self.app, "operation_lock", lock):
            status, record, _ = self.search_get()
        self.assertEqual((status, record["error"]["code"]), (409, "busy"))
        lock.acquire.assert_called_once_with(timeout=2)
        fake.assert_not_called()
        lock = mock.MagicMock()
        lock.acquire.return_value = True
        with mock.patch.object(self.app, "operation_lock", lock):
            self.assertEqual(self.search_get()[0], 200)
        lock.acquire.assert_called_once_with(timeout=2)
        lock.release.assert_called_once_with()

    def test_search_route_uses_the_real_client_and_decoder_with_one_fetch_per_ttl(self):
        self.bootstrap()
        calls = []

        def hit(mod_id, **changes):
            value = {"modId": mod_id, "name": "Owned mod %d" % mod_id, "summary": "Found", "version": "1", "author": "Owned author",
                     "adultContent": False, "status": "published", "uploader": {"name": "Uploader"},
                     "game": {"id": 1, "domainName": "ownedgame"}}
            value.update(changes)
            return value
        graphql = {"data": {"mods": {"nodes": [hit(5), hit(6, adultContent=True), hit(7, status="hidden"), hit(8, downloads=3)],
                                     "nodesCount": 4, "totalCount": 4}}}

        def request(method, uri, **kwargs):
            calls.append((method, urlsplit(uri).path, kwargs))
            if (method, urlsplit(uri).path) != ("POST", "/v2/graphql"):
                raise AssertionError("Unexpected fixture provider endpoint")
            return nexus.Response(200, {}, json.dumps(graphql).encode())
        transport = mock.Mock()
        transport.request.side_effect = request
        account = mock.Mock()
        account.client.side_effect = lambda: nexus.NexusClient(nexus.Credentials(api_key="fixture-only"), transport)
        self.app.account = account
        now = [50.0]
        self.app.update_clock = lambda: now[0]
        status, record, _ = self.search_get("q=owned+mod")
        self.assertEqual(status, 200)
        self.assertEqual([call[:2] for call in calls], [("POST", "/v2/graphql")])
        self.assertEqual(json.loads(calls[0][2]["body"])["variables"], {"gameDomainName": "ownedgame", "name": "owned mod", "count": 20, "offset": 0})
        self.assertEqual(calls[0][2]["headers"]["apikey"], "fixture-only")
        # Hidden and adult mods are left out of the list but still counted by Nexus; records are the latest-mods records.
        self.assertEqual([item["mod_id"] for item in record["items"]], [5, 8])
        self.assertEqual((record["total"], record["page"], record["pages"]), (4, 1, 1))
        self.assertEqual(set(record["items"][0]), {"mod_id", "game_id", "game_domain", "name", "author", "summary", "description",
                                                    "version", "contains_adult_content", "source_url"})
        self.assertEqual(record["items"][0]["source_url"], "https://www.nexusmods.com/ownedgame/mods/5")
        self.assertNotIn("downloads", json.dumps(record))
        self.search_get("q=owned+mod")
        self.search_get("q=%20owned%20%20mod%20")
        self.assertEqual(len(calls), 1)
        now[0] += 121
        self.search_get("q=owned+mod")
        self.assertEqual(len(calls), 2)
        # GraphQL errors and a truncated page are payload errors with the existing code, and are not cached.
        self.app.search_cache.clear()
        graphql["errors"] = [{"message": "SecretMarker"}]
        status, record, _ = self.search_get("q=owned+mod")
        self.assertEqual((status, record["error"]["code"]), (400, "payload"))
        self.assertNotIn("SecretMarker", json.dumps(record))
        del graphql["errors"]
        graphql["data"]["mods"]["totalCount"] = 90
        status, record, _ = self.search_get("q=owned+mod")
        self.assertEqual((status, record["error"]["code"]), (400, "payload"))
        self.assertEqual(len(self.app.search_cache), 0)

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

    PROGRESS_INTENT = {"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2}

    def start_progress_job(self, key, behavior):
        """Submit a job whose fake download reports through the host callback, then blocks until released.

        `behavior(progress)` runs inside the download thread before it blocks. Returns
        (job id, release event, the callback the host gave the download).
        """
        self.bootstrap()
        reported, release, given = threading.Event(), threading.Event(), []
        def download(link, destination, progress=None):
            given.append(progress)
            behavior(progress)
            reported.set()
            self.assertTrue(release.wait(5))
            if self.fail_download:
                raise nexus.NexusError("network")
            destination.mkdir(mode=0o700, parents=True, exist_ok=True)
            path = destination / "owned.zip"
            path.write_bytes(b"owned archive")
            return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(),
                                        path.stat().st_size, link.source)
        self.app.download = download
        self.addCleanup(release.set)
        status, body, _ = self.mutation("/api/jobs", dict(self.PROGRESS_INTENT, idempotency_key=key))
        self.assertEqual(status, 202)
        job = json.loads(body)
        self.assertTrue(reported.wait(5))
        return job["id"], release, given[0]

    fail_download = False

    def wait_for_status(self, identifier, *statuses):
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            job = self.app.jobs(identifier)
            if job["status"] in statuses:
                return job
            time.sleep(.01)
        self.fail("job did not reach " + "/".join(statuses))

    def api_job(self, identifier):
        status, body, _ = self.request("GET", "/api/jobs/" + identifier)
        self.assertEqual(status, 200)
        listed = [item for item in json.loads(self.request("GET", "/api/jobs")[1])["items"] if item["id"] == identifier]
        self.assertEqual(listed, [json.loads(body)])
        return json.loads(body)

    def saved_receipt(self, identifier):
        return json.loads((self.app.receipts / (identifier + ".json")).read_text())

    def test_live_download_progress_is_in_the_job_while_downloading_and_never_in_the_receipt(self):
        identifier, release, _ = self.start_progress_job("progress-live-0001", lambda progress: (
            progress(0, 1000), progress(400, 1000)))
        job = self.api_job(identifier)
        self.assertEqual(job["status"], "downloading")
        self.assertEqual(job["progress"], {"unit": "bytes", "done": 400, "total": 1000})
        self.assertEqual(list(job["progress"]), ["unit", "done", "total"])
        # Memory only: the durable receipt is exactly what it was before any progress.
        receipt = self.saved_receipt(identifier)
        self.assertEqual(receipt["status"], "downloading")
        self.assertNotIn("progress", receipt)
        self.assertNotIn("progress", self.app.records[identifier])
        release.set()
        job = self.wait_for_status(identifier, "analyzed")
        self.assertNotIn("progress", job)
        self.assertNotIn("progress", self.api_job(identifier))
        self.assertNotIn("progress", self.saved_receipt(identifier))
        self.assertEqual(self.app.progress, {})

    def test_unknown_total_is_reported_as_null_and_a_zero_total_is_unknown(self):
        identifier, release, progress = self.start_progress_job("progress-null-0001", lambda progress: progress(7, None))
        self.assertEqual(self.api_job(identifier)["progress"], {"unit": "bytes", "done": 7, "total": None})
        progress(0, 0)
        self.assertEqual(self.api_job(identifier)["progress"], {"unit": "bytes", "done": 0, "total": None})
        release.set()
        self.assertNotIn("progress", self.wait_for_status(identifier, "analyzed"))

    def test_job_without_a_report_yet_has_no_progress_field(self):
        identifier, release, _ = self.start_progress_job("progress-none-0001", lambda progress: None)
        job = self.api_job(identifier)
        self.assertEqual(job["status"], "downloading")
        self.assertNotIn("progress", job)
        release.set()
        self.wait_for_status(identifier, "analyzed")

    def test_failed_download_drops_its_progress(self):
        self.fail_download = True
        identifier, release, _ = self.start_progress_job("progress-fail-0001", lambda progress: progress(10, 20))
        self.assertEqual(self.api_job(identifier)["progress"], {"unit": "bytes", "done": 10, "total": 20})
        release.set()
        job = self.wait_for_status(identifier, "failed")
        self.assertEqual(job["error"]["code"], "network")
        self.assertNotIn("progress", job)
        self.assertNotIn("progress", self.api_job(identifier))
        self.assertNotIn("progress", self.saved_receipt(identifier))
        self.assertEqual(self.app.progress, {})

    def test_failed_receipt_save_still_drops_progress(self):
        self.fail_download = True
        identifier, release, _ = self.start_progress_job("progress-save-0001", lambda progress: progress(10, 20))
        with mock.patch.object(self.app, "_save", side_effect=nexus.NexusError("storage")):
            release.set()
            job = self.wait_for_status(identifier, "failed")
        self.assertEqual(job["error"]["code"], "storage_unavailable")
        self.assertNotIn("progress", job)
        self.assertEqual(self.app.progress, {})

    def test_late_or_nonsense_reports_cannot_show_progress(self):
        identifier, release, progress = self.start_progress_job("progress-late-0001", lambda progress: progress(1, 10))
        for done, total in ((True, 10), (-1, 10), (1.5, 10), ("1", 10), (1, "10"), (1, -5), (1, True), (2 ** 53, None), (1, 2 ** 53)):
            progress(done, total)
            self.assertEqual(self.api_job(identifier)["progress"], {"unit": "bytes", "done": 1, "total": 10}, (done, total))
        progress(50, 10)  # more than the total is clamped, never shown
        self.assertEqual(self.api_job(identifier)["progress"], {"unit": "bytes", "done": 10, "total": 10})
        release.set()
        self.wait_for_status(identifier, "analyzed")
        progress(5, 10)  # a report after the job left "downloading" is dropped
        self.assertNotIn("progress", self.api_job(identifier))
        self.assertEqual(self.app.progress, {})

    def test_progress_updates_never_write_a_receipt(self):
        identifier, release, progress = self.start_progress_job("progress-save-0002", lambda progress: progress(0, 5000))
        with mock.patch.object(self.app, "_save", wraps=self.app._save) as save:
            for done in range(0, 5001, 5):
                progress(done, 5000)
            self.assertEqual(self.api_job(identifier)["progress"]["done"], 5000)
            self.assertEqual(save.call_count, 0)
        release.set()
        self.wait_for_status(identifier, "analyzed")

    def test_progress_updates_from_many_threads_are_always_well_formed(self):
        identifier, release, progress = self.start_progress_job("progress-race-0001", lambda progress: progress(0, 4000))
        def writer(offset):
            for _ in range(20):
                for done in range(offset, 4001, 4):
                    progress(done, 4000)
        threads = [threading.Thread(target=writer, args=(offset,)) for offset in range(4)]
        for thread in threads:
            thread.start()
        seen = 0
        while True:
            running = any(thread.is_alive() for thread in threads)
            counted = self.app.jobs(identifier).get("progress")
            self.assertIsNotNone(counted)
            self.assertEqual(set(counted), {"unit", "done", "total"})
            self.assertTrue(counted["unit"] == "bytes" and counted["total"] == 4000 and 0 <= counted["done"] <= 4000)
            seen += 1
            if not running:
                break
        for thread in threads:
            thread.join(5)
        self.assertGreater(seen, 1)
        release.set()
        self.wait_for_status(identifier, "analyzed")

    def test_a_download_without_a_progress_parameter_still_runs_and_has_no_progress(self):
        self.bootstrap()
        release = threading.Event()
        self.addCleanup(release.set)
        def plain(link, destination):
            self.assertTrue(release.wait(5))
            destination.mkdir(mode=0o700, parents=True, exist_ok=True)
            path = destination / "owned.zip"
            path.write_bytes(b"owned archive")
            return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size, link.source)
        self.app.download = plain
        job = json.loads(self.mutation("/api/jobs", dict(self.PROGRESS_INTENT, idempotency_key="progress-plain-0001"))[1])
        self.assertNotIn("progress", self.wait_for_status(job["id"], "downloading"))
        release.set()
        self.assertNotIn("progress", self.wait_for_status(job["id"], "analyzed"))

    def test_progress_aware_downloads_are_detected_by_signature(self):
        accepts = host._accepts_progress
        self.assertTrue(accepts(lambda link, destination, progress=None: None))
        self.assertTrue(accepts(lambda link, destination, *, progress: None))
        self.assertTrue(accepts(lambda *args, **kwargs: None))
        self.assertFalse(accepts(lambda link, destination: None))
        self.assertFalse(accepts(lambda *_: None))
        self.assertTrue(accepts(lambda link, destination, **_: None))
        self.assertTrue(accepts(nexus.download_to_quarantine))

    def test_receipt_that_carries_progress_is_refused_and_restart_marks_downloading_jobs_interrupted(self):
        record = {"id": "b" * 32, "kind": "download_analyze", "idempotency_key": "progress-load-0001",
                  "game": "ownedgame", "mod_id": 1, "file_id": 2, "label": "Mod 1 \u00b7 file 2", "status": "downloading",
                  "stage": "requested", "compatibility": "unresolved",
                  "source_url": nexus.DownloadSource("ownedgame", 1, 2).source_url}
        path = self.app.receipts / (record["id"] + ".json")
        nexus.save_private_json(path, dict(record, progress={"unit": "bytes", "done": 1, "total": 2}))
        with self.assertRaises(host.HostError):
            host.BridgeApplication(self.root / "credentials.json", self.root / "quarantine", account=self.account)
        nexus.save_private_json(path, record)
        restarted = host.BridgeApplication(self.root / "credentials.json", self.root / "quarantine",
                                           account=self.account, download=lambda *_: None, analyze=lambda *_: None)
        self.addCleanup(restarted.close)
        job = restarted.jobs(record["id"])
        self.assertEqual(job["status"], "interrupted")
        self.assertNotIn("progress", job)
        self.assertNotIn("progress", json.loads(path.read_text()))

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

    # ---- Browse all of a game's mods: GET /api/games/{game}/mods/browse?sort=<key>&page=<n> ----

    def browse_data(self, sort="downloads", page=1, total=1, **changes):
        record = dict(self.search_data(page=page, total=total), sort=sort)
        record.update(changes)
        return record

    def browse_get(self, query=None, game="ownedgame"):
        status, body, headers = self.request("GET", f"/api/games/{game}/mods/browse" + ("?" + query if query is not None else ""))
        return status, json.loads(body), headers

    def browse_fake(self, **kwargs):
        self.account.browse_mods = mock.Mock(side_effect=lambda game, sort, page: self.browse_data(sort=sort, page=page), **kwargs)
        return self.account.browse_mods

    def test_browse_and_key_sign_in_capabilities_are_advertised(self):
        record = self.bootstrap()
        for name in ("mod_browse", "nexus_key_sign_in"):
            self.assertIs(record["capabilities"][name], True, name)
            self.assertIs(self.app.capabilities()["capabilities"][name], True, name)
        self.assertIs(record["capabilities"]["nexus_auth_start"], False)  # key sign-in does not need OAuth

    def test_browse_route_returns_the_page_in_the_documented_shape_and_defaults(self):
        self.bootstrap()
        fake = self.browse_fake()
        status, record, headers = self.browse_get("sort=endorsements&page=2")
        self.assertEqual(status, 200)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(record, self.browse_data(sort="endorsements", page=2))
        self.assertEqual(set(record), {"items", "total", "page", "pages", "sort"})
        fake.assert_called_once_with("ownedgame", "endorsements", 2)
        self.assertEqual(self.account.requests, [])  # not the latest-mods listing
        self.assertEqual(self.account.download_count, 0)
        self.assertEqual(self.app.jobs(), [])
        # Both fields are optional (downloads, page 1), in either order, and a page is a canonical decimal.
        for query, expected in ((None, ("ownedgame", "downloads", 1)), ("", ("ownedgame", "downloads", 1)),
                                ("sort=name", ("ownedgame", "name", 1)), ("page=3", ("ownedgame", "downloads", 3)),
                                ("page=50&sort=updated", ("ownedgame", "updated", 50)), ("sort=downloads&page=007", ("ownedgame", "downloads", 7))):
            with self.subTest(query=query):
                self.app.browse_cache.clear()
                status, record, _ = self.browse_get(query)
                self.assertEqual(status, 200)
                self.assertEqual(fake.call_args.args, expected)
                self.assertEqual((record["sort"], record["page"]), expected[1:])
        # A zero-result listing is a complete answer, not an error.
        self.account.browse_mods = mock.Mock(return_value={"items": [], "total": 0, "page": 1, "pages": 0, "sort": "name"})
        self.assertEqual(self.browse_get("sort=name")[1], {"items": [], "total": 0, "page": 1, "pages": 0, "sort": "name"})

    def test_browse_route_leaks_nothing_beyond_the_five_keys(self):
        self.bootstrap()
        data = self.browse_data()
        data["apikey"] = "must-not-leak-key"
        data["query"] = "must-not-leak-query"
        self.account.browse_mods = mock.Mock(return_value=data)
        status, body, _ = self.request("GET", "/api/games/ownedgame/mods/browse")
        self.assertEqual(status, 200)
        self.assertEqual(set(json.loads(body)), {"items", "total", "page", "pages", "sort"})
        for secret in (b"must-not-leak", str(self.root).encode(), b"csrf", b"idempotency"):
            self.assertNotIn(secret, body)

    def test_browse_route_follows_the_get_gates(self):
        self.bootstrap()
        fake = self.browse_fake()
        path = "/api/games/ownedgame/mods/browse?sort=name"
        saved, self.cookie = self.cookie, None
        self.assertEqual(self.request("GET", path)[0], 401)
        self.assertEqual(self.request("GET", path, headers={"Cookie": "mb_session=forged"})[0], 401)
        self.cookie = saved
        self.assertEqual(self.request("GET", path, headers={"Host": "unrelated.example"})[0], 403)
        self.assertEqual(self.request("GET", path, headers={"Origin": "https://unrelated.example"})[0], 403)
        self.assertEqual(self.request("GET", path, headers={"Sec-Fetch-Site": "cross-site"})[0], 403)
        fake.assert_not_called()
        for bad in ("/api/games/ownedgame/mods/browse/?sort=name", "/api/games/ownedgame/mods/Browse?sort=name",
                    "/api/games/ownedgame/mods/browse/x?sort=name", "/api/games/OWNED/mods/browse?sort=name",
                    "/api/games/ownedgame/browse?sort=name", "/api/games/ownedgame/mods/1/browse?sort=name",
                    "/api/games/ownedgame/mods/browse/files?sort=name", "/api/mods/browse?sort=name", "/api/browse?sort=name"):
            with self.subTest(path=bad):
                self.assertEqual(self.request("GET", bad)[0], 404)
        self.assertEqual(self.mutation(path.split("?")[0], {})[0], 404)
        self.assertEqual(self.request("POST", path, {}, {"Origin": self.origin, "X-CSRF-Token": "wrong"})[0], 403)
        self.assertEqual(self.request("DELETE", path)[0], 501)
        fake.assert_not_called()
        self.assertEqual(self.request("GET", path)[0], 200)
        fake.assert_called_once_with("ownedgame", "name", 1)
        # Search keeps its own route, and a query string is still refused on the routes that take none.
        self.assertEqual(self.request("GET", "/api/games/ownedgame/mods/search?sort=name")[0], 400)
        self.assertEqual(self.request("GET", "/api/games/ownedgame/mods?sort=name")[0], 404)

    def test_browse_route_refuses_invalid_queries_without_asking_nexus(self):
        self.bootstrap()
        fake = self.browse_fake()
        refused = {"error": {"code": "invalid_input", "message": "The request is invalid."}}
        queries = ["sort", "sort=", "sort=Downloads", "sort=downloads%20", "sort=%20name", "sort=relevance", "sort=createdAt",
                   "sort=updatedAt", "sort=random", "sort=name%2Cdownloads", "sort=name&sort=name", "sort=name&page=0", "sort=name&page=51",
                   "page=0", "page=51", "page=abc", "page=-1", "page=1.5", "page=1e1", "page=", "page=1&page=2", "page=%201", "page=99999",
                   "q=ab", "sort=name&q=ab", "sort=name&x=1", "x=1", "&sort=name", "sort=name&", "sort=name&&page=1", "sort=a%00",
                   "sort=%FF%FE", "sort=%C3", "Sort=name", "sort=name&Page=1", "sort=name#x", "sort=" + "x" * 2100, "sort=name&page=1&x=1"]
        for query in queries:
            with self.subTest(query=query[:40]):
                status, record, _ = self.browse_get(query)
                self.assertEqual((status, record), (400, refused))
        for game in ("_x", "-x"):
            with self.subTest(game=game):
                self.assertEqual(self.browse_get("sort=name", game=game)[0], 400)
        fake.assert_not_called()
        self.assertEqual(len(self.app.browse_cache), 0)
        self.assertEqual(self.browse_get("sort=name&page=50")[0], 200)
        # The same checks guard the application method itself.
        for args in (("ownedgame", "relevance", 1), ("ownedgame", "downloads", 0), ("ownedgame", "downloads", 51), ("ownedgame", "downloads", True),
                     ("ownedgame", "downloads", "1"), ("ownedgame", None, 1), ("ownedgame", ["name"], 1), ("../x", "name", 1)):
            with self.subTest(args=args):
                with self.assertRaises(host.HostError) as caught:
                    self.app.mod_browse(*args)
                self.assertEqual((caught.exception.code, caught.exception.status), ("invalid_input", 400))
        self.assertEqual(fake.call_count, 1)
        self.assertEqual(host.parse_browse_query(""), ("downloads", 1))
        self.assertEqual(host.parse_browse_query("page=0&sort=name"), ("name", 0))  # the range is judged by mod_browse
        for query in ("sort=name&sort=name", "sort=名", "sort=name&q=x", "x", "sort=name&page=x"):
            with self.subTest(parse=query):
                with self.assertRaises(host.HostError) as caught:
                    host.parse_browse_query(query)
                self.assertEqual(caught.exception.code, "invalid_input")

    def test_browse_cache_is_one_provider_fetch_per_game_sort_and_page_for_two_minutes(self):
        self.bootstrap()
        now = [1000.0]
        self.app.update_clock = lambda: now[0]
        fake = self.browse_fake()
        self.assertEqual((host.BROWSE_CACHE_TTL, host.BROWSE_CACHE_MAX), (120, 128))
        first = self.browse_get()[1]
        self.assertEqual(fake.call_count, 1)
        now[0] += 119.0
        self.assertEqual(self.browse_get()[1], first)
        # The default and the spelled-out default are one entry.
        self.browse_get("sort=downloads&page=1")
        self.browse_get("page=1")
        self.assertEqual(fake.call_count, 1)
        # Another sort, page and game are separate entries.
        self.browse_get("sort=name")
        self.browse_get("page=2")
        self.browse_get("sort=name&page=2")
        self.browse_get(game="othergame")
        self.assertEqual(fake.call_count, 5)
        self.assertEqual(list(self.app.browse_cache), [("ownedgame", "downloads", 1), ("ownedgame", "name", 1), ("ownedgame", "downloads", 2),
                                                       ("ownedgame", "name", 2), ("othergame", "downloads", 1)])
        # An entry expires exactly two minutes after its fetch; a clock that runs backwards never serves one from the future.
        now[0] = 1000.0 + 120.0
        fake.side_effect = lambda game, sort, page: self.browse_data(sort=sort, page=page, total=9)
        self.assertEqual(self.browse_get()[1]["total"], 9)
        self.assertEqual(fake.call_count, 6)
        self.browse_get()
        self.assertEqual(fake.call_count, 6)
        now[0] = 10.0
        self.browse_get()
        self.assertEqual(fake.call_count, 7)
        # No other cache is filled or read.
        self.assertEqual((len(self.app.update_cache), len(self.app.requirements_cache), len(self.app.search_cache)), (0, 0, 0))

    def test_browse_results_are_copies_and_the_cache_is_bounded_and_dropped_on_account_change(self):
        self.bootstrap()
        fake = self.browse_fake()
        first = self.app.mod_browse("ownedgame")
        first["items"].clear()
        first["total"] = 99
        again = self.app.mod_browse("ownedgame", "downloads", 1)
        self.assertEqual(fake.call_count, 1)
        self.assertEqual((len(again["items"]), again["total"]), (1, 1))
        self.assertIsNot(again, self.app.mod_browse("ownedgame"))
        with mock.patch.object(host, "BROWSE_CACHE_MAX", 3):
            self.app.browse_cache.clear()
            for page in (1, 2, 3):
                self.browse_get(f"page={page}")
            self.browse_get("page=1")  # touch: page 2 is now the oldest
            self.browse_get("page=4")
            self.assertEqual(list(self.app.browse_cache), [("ownedgame", "downloads", 3), ("ownedgame", "downloads", 1), ("ownedgame", "downloads", 4)])
        self.app.browse_cache.clear()
        for sort in ("downloads", "endorsements", "updated", "name"):
            for page in range(1, 41):
                self.browse_get(f"sort={sort}&page={page}")
        self.assertEqual(len(self.app.browse_cache), 128)
        # Disconnecting drops every provider cache: another account may see other content.
        self.assertEqual(self.mutation("/api/auth/disconnect", {})[0], 200)
        self.assertEqual(len(self.app.browse_cache), 0)

    def test_browse_cache_is_dropped_on_oauth_sign_in(self):
        self.app.oauth_config = nexus.OAuthConfig("owned-client", self.origin + "/auth/callback")
        self.bootstrap()
        self.browse_fake()
        self.browse_get()
        self.assertEqual(len(self.app.browse_cache), 1)
        params = parse_qs(urlsplit(json.loads(self.mutation("/api/auth/start", {})[1])["authorization_url"]).query)
        callback = "/auth/callback?" + "code=owned-private-code&state=" + params["state"][0]
        self.assertEqual(self.request("GET", callback, headers={"Sec-Fetch-Site": "cross-site"})[0], 303)
        self.assertEqual(len(self.app.browse_cache), 0)

    def test_browse_route_maps_provider_errors_to_existing_codes_and_never_caches_failures(self):
        self.bootstrap()
        self.account.browse_mods = mock.Mock()
        raw = "raw provider body https://api.nexusmods.com/v2/graphql?apikey=sk-live /private/internal/path"
        for code in ("rate_limit", "network", "timeout", "provider", "payload", "size_limit", "content_hidden", "reconnect_required",
                     "credentials", "redirect", "invalid_input"):
            with self.subTest(code=code):
                self.account.browse_mods.side_effect = nexus.NexusError(code, status=429, retry_after=30)
                status, body, _ = self.request("GET", "/api/games/ownedgame/mods/browse")
                self.assertEqual(status, 400)
                self.assertEqual(json.loads(body), {"error": {"code": code, "message": str(nexus.NexusError(code))}})
                self.assertNotIn("retry_after", body.decode())
                self.assertEqual(len(self.app.browse_cache), 0)
        self.account.browse_mods.side_effect = RuntimeError(raw)
        status, body, _ = self.request("GET", "/api/games/ownedgame/mods/browse")
        self.assertEqual((status, json.loads(body)["error"]["code"]), (500, "operation_failed"))
        for fragment in (b"raw provider", b"sk-live", b"/private", b"nexusmods", b"graphql"):
            self.assertNotIn(fragment, body)
        self.account.browse_mods.side_effect = None
        self.account.browse_mods.return_value = self.browse_data()
        calls = self.account.browse_mods.call_count
        self.assertEqual(self.browse_get()[0], 200)
        self.assertEqual(self.account.browse_mods.call_count, calls + 1)
        self.account.browse_mods.side_effect = nexus.NexusError("network")
        self.assertEqual(self.browse_get("sort=name")[0], 400)
        self.assertEqual(list(self.app.browse_cache), [("ownedgame", "downloads", 1)])

    def test_browse_route_takes_the_account_operation_lock_like_other_provider_calls(self):
        self.bootstrap()
        fake = self.browse_fake()
        lock = mock.MagicMock()
        lock.acquire.return_value = False
        with mock.patch.object(self.app, "operation_lock", lock):
            status, record, _ = self.browse_get()
        self.assertEqual((status, record["error"]["code"]), (409, "busy"))
        lock.acquire.assert_called_once_with(timeout=2)
        fake.assert_not_called()

    def test_browse_through_the_real_client_sends_the_sorts_query_and_applies_the_content_filter(self):
        """The host and the real NexusClient together, the provider boundary faked: what goes on the wire, what comes back."""
        self.bootstrap()
        graphql = _GraphqlWire([
            {"modId": 11, "name": "Listed", "summary": "S", "version": "1", "author": "A", "adultContent": False, "status": "published",
             "uploader": {"name": "U"}, "game": {"id": 7, "domainName": "ownedgame"}},
            {"modId": 12, "name": "Adult", "summary": "S", "version": "1", "author": "A", "adultContent": True, "status": "published",
             "uploader": {"name": "U"}, "game": {"id": 7, "domainName": "ownedgame"}},
            {"modId": 13, "name": "Unlisted", "summary": "S", "version": "1", "author": "A", "adultContent": False, "status": "hidden",
             "uploader": {"name": "U"}, "game": {"id": 7, "domainName": "ownedgame"}}], total=3)
        self.account.client = lambda: nexus.NexusClient(nexus.Credentials(api_key="fixture-key"), graphql)
        status, record, _ = self.browse_get("sort=updated&page=1")
        self.assertEqual(status, 200)
        self.assertEqual([item["mod_id"] for item in record["items"]], [11])  # adult (API key: no preference) and unpublished stay out
        self.assertEqual((record["total"], record["page"], record["pages"], record["sort"]), (3, 1, 1, "updated"))
        self.assertEqual(set(record["items"][0]), {"mod_id", "game_id", "game_domain", "name", "author", "summary", "description",
                                                   "version", "contains_adult_content", "source_url"})
        self.assertEqual(len(graphql.requests), 1)
        method, url, kwargs = graphql.requests[0]
        self.assertEqual((method, url), ("POST", "https://api.nexusmods.com/v2/graphql"))
        from modbridge_nexus import search as graphql_search
        self.assertEqual(kwargs["body"], graphql_search.browse_body("ownedgame", "updated", 1))
        self.assertIn(b"updatedAt: {direction: DESC}", kwargs["body"])
        self.assertEqual(kwargs["headers"]["apikey"], "fixture-key")
        # The same page again is the cache's (no second request), and every sort sends its own query.
        self.browse_get("sort=updated&page=1")
        self.assertEqual(len(graphql.requests), 1)
        for sort, fragment in (("downloads", b"downloads: {direction: DESC}"), ("endorsements", b"endorsements: {direction: DESC}"),
                               ("name", b"name: {direction: ASC}")):
            with self.subTest(sort=sort):
                self.assertEqual(self.browse_get(f"sort={sort}")[0], 200)
                self.assertIn(fragment, graphql.requests[-1][2]["body"])
        self.assertEqual(len(graphql.requests), 4)

    # ---- Sign in with a personal API key: POST /api/auth/key ----

    def real_account(self):
        """Swap in the real AccountManager (and the credential store the host already owns) over a faked provider boundary."""
        wire = _KeyWire()
        self.app.account = nexus.AccountManager(self.app.store, transport=wire)
        return wire

    def key_post(self, body, headers=None):
        status, raw, response_headers = self.request("POST", "/api/auth/key", body,
            dict({"Origin": self.origin, "X-CSRF-Token": self.csrf}, **(headers or {})))
        return status, json.loads(raw), raw, response_headers

    def test_key_sign_in_validates_live_then_saves_privately_and_returns_the_account_status(self):
        self.bootstrap()
        wire = self.real_account()
        store = self.app.store.path
        self.assertFalse(store.exists())
        self.assertEqual(json.loads(self.request("GET", "/api/account")[1])["status"], "disconnected")
        status, record, raw, headers = self.key_post({"api_key": _KeyWire.GOOD})
        self.assertEqual(status, 200)
        self.assertEqual(headers["Cache-Control"], "no-store")
        self.assertEqual(record, {"credentials_present": True, "live_service": "verified", "status": "connected",
                                  "account": {"user_id": 42, "display_name": "Key owner", "is_premium": False, "is_supporter": True,
                                              "adult_content": None},
                                  "console_translation": "unverified"})
        # Nexus was asked once about this key, by the client's own identity call, before anything was written.
        keyed = [request for request in wire.requests if request[2].get("apikey") == _KeyWire.GOOD]
        self.assertEqual([(request[0], request[1]) for request in keyed[:1]], [("GET", "https://api.nexusmods.com/v1/users/validate")])
        self.assertEqual(len(wire.requests), 1)
        self.assertTrue(store.exists())
        self.assertEqual(store.stat().st_mode & 0o777, 0o600)
        self.assertEqual(nexus.CredentialStore(store).load_account(), (nexus.Credentials(api_key=_KeyWire.GOOD), None))
        # The status route now answers from the saved key, as after a restart.
        status, body, _ = self.request("GET", "/api/account")
        self.assertEqual((status, json.loads(body)["status"], json.loads(body)["account"]["display_name"]), (200, "connected", "Key owner"))
        self.assertEqual(wire.requests[-1][2]["apikey"], _KeyWire.GOOD)
        restarted = nexus.AccountManager(nexus.CredentialStore(store), transport=_KeyWire())
        self.assertTrue(restarted.status()["connected"])

    def test_key_sign_in_refuses_a_bad_body_or_key_before_asking_nexus(self):
        self.bootstrap()
        wire = self.real_account()
        refused = {"error": {"code": "invalid_input", "message": "The request is invalid."}}
        long_key = "k" * 512
        secret = "very-secret-value-0001"
        bodies = [{}, {"api_key": ""}, {"api_key": None}, {"api_key": 5}, {"api_key": True}, {"api_key": ["a"]}, {"api_key": {"k": secret}},
                  {"api_key": "has space " + secret}, {"api_key": " " + secret}, {"api_key": secret + " "}, {"api_key": secret + "\n"},
                  {"api_key": secret + "\t"}, {"api_key": "café" + secret}, {"api_key": secret + "\x7f"}, {"api_key": secret + "\x00"},
                  {"api_key": "k" * 513}, {"api_key": secret, "extra": 1}, {"apikey": secret}, {"api_key": secret, "api_key2": secret},
                  {"key": secret}, {"api_key": long_key + "́"}]
        for body in bodies:
            with self.subTest(body=json.dumps(body)[:50]):
                status, record, raw, _ = self.key_post(body)
                self.assertEqual((status, record), (400, refused))
                self.assertNotIn(secret.encode(), raw)
        # A body that is not an object, not JSON, with a repeated key or the wrong content type is refused as well.
        for raw_body, headers in ((b'["x"]', None), (b'"x"', None), (b"not json", None), (b'{"api_key":"a","api_key":"b"}', None),
                                  (b'{"api_key":"abcdef"}', {"Content-Type": "text/plain"})):
            with self.subTest(raw=raw_body[:30]):
                wire_request = http.client.HTTPConnection("127.0.0.1", self.port, timeout=2)
                merged = {"Cookie": self.cookie, "Origin": self.origin, "X-CSRF-Token": self.csrf, "Content-Type": "application/json"}
                merged.update(headers or {})
                wire_request.request("POST", "/api/auth/key", body=raw_body, headers=merged)
                response = wire_request.getresponse()
                self.assertEqual(response.status, 400)
                self.assertEqual(json.loads(response.read())["error"]["code"], "invalid_input")
                wire_request.close()
        self.assertEqual(wire.requests, [], "Nexus is never asked about a malformed key")
        self.assertFalse(self.app.store.path.exists())
        self.assertEqual(self.app.key_failures, [], "a malformed request is not a failed attempt")
        # The longest allowed key is fine.
        self.assertEqual(self.key_post({"api_key": long_key})[0], 400)  # well formed, Nexus refuses it
        self.assertEqual(len(wire.requests), 1)

    def test_key_sign_in_follows_the_mutation_gates(self):
        self.bootstrap()
        wire = self.real_account()
        body = {"api_key": _KeyWire.GOOD}
        saved, self.cookie = self.cookie, None
        self.assertEqual(self.request("POST", "/api/auth/key", body, {"Origin": self.origin, "X-CSRF-Token": self.csrf})[0], 401)
        self.cookie = saved
        self.assertEqual(self.request("POST", "/api/auth/key", body)[0], 403)  # no Origin, no token
        self.assertEqual(self.request("POST", "/api/auth/key", body, {"X-CSRF-Token": self.csrf})[0], 403)
        self.assertEqual(self.request("POST", "/api/auth/key", body, {"Origin": self.origin})[0], 403)
        self.assertEqual(self.request("POST", "/api/auth/key", body, {"Origin": self.origin, "X-CSRF-Token": "wrong"})[0], 403)
        self.assertEqual(self.request("POST", "/api/auth/key", body, {"Origin": "https://unrelated.example", "X-CSRF-Token": self.csrf})[0], 403)
        self.assertEqual(self.request("POST", "/api/auth/key", body, {"Origin": self.origin, "X-CSRF-Token": self.csrf, "Host": "unrelated.example"})[0], 403)
        self.assertEqual(self.request("POST", "/api/auth/key", body, {"Origin": self.origin, "X-CSRF-Token": self.csrf, "Sec-Fetch-Site": "cross-site"})[0], 403)
        # Only the exact path and verb exist.
        self.assertEqual(self.request("GET", "/api/auth/key")[0], 404)
        self.assertEqual(self.request("PUT", "/api/auth/key", body, {"Origin": self.origin, "X-CSRF-Token": self.csrf})[0], 501)
        for path in ("/api/auth/key/", "/api/auth/keys", "/api/auth/Key", "/api/auth/key?x=1", "/api/auth"):
            with self.subTest(path=path):
                self.assertEqual(self.request("POST", path, body, {"Origin": self.origin, "X-CSRF-Token": self.csrf})[0], 404)
        self.assertEqual(wire.requests, [])
        self.assertFalse(self.app.store.path.exists())
        self.assertEqual(self.key_post(body)[0], 200)

    def test_a_rejected_key_is_key_rejected_and_writes_nothing(self):
        self.bootstrap()
        wire = self.real_account()
        store = self.app.store
        for wrong in ("wrong-key-0001", "zz-unique-wrong-9"):
            status, record, raw, _ = self.key_post({"api_key": wrong})
            self.assertEqual((status, record), (400, {"error": {"code": "key_rejected", "message": "Nexus did not accept this API key."}}))
            self.assertNotIn(wrong.encode(), raw)
        self.assertFalse(store.path.exists())
        # An earlier sign-in survives a wrong key byte for byte.
        store.save(nexus.Credentials(api_key="older-saved-key"))
        before = store.path.read_bytes()
        self.assertEqual(self.key_post({"api_key": "wrong-key-0002"})[0], 400)
        self.assertEqual(store.path.read_bytes(), before)
        # A forbidden key is refused the same way; the right key replaces the old one.
        wire.status = 403
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[1]["error"]["code"], "key_rejected")
        wire.status = None
        self.assertEqual(store.path.read_bytes(), before)
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[0], 200)
        self.assertEqual(store.load().api_key, _KeyWire.GOOD)

    def test_failed_keys_are_rate_limited_five_per_ten_minutes(self):
        self.bootstrap()
        wire = self.real_account()
        now = [5000.0]
        self.app.update_clock = lambda: now[0]
        self.assertEqual((host.KEY_ATTEMPT_LIMIT, host.KEY_ATTEMPT_WINDOW), (5, 600))
        for number in range(5):
            now[0] += 1
            self.assertEqual(self.key_post({"api_key": f"wrong-key-{number:04d}"})[1]["error"]["code"], "key_rejected")
        asked = len(wire.requests)
        self.assertEqual(asked, 5)
        # The sixth attempt, even with the right key, is refused with 429 and Nexus is not asked.
        status, record, raw, _ = self.key_post({"api_key": _KeyWire.GOOD})
        self.assertEqual((status, record), (429, {"error": {"code": "too_many_attempts",
                         "message": "Too many failed sign-in attempts. Wait ten minutes and try again."}}))
        self.assertEqual(len(wire.requests), asked)
        self.assertFalse(self.app.store.path.exists())
        self.assertNotIn(_KeyWire.GOOD.encode(), raw)
        # A malformed request is judged first (nothing counts, nothing is asked), and the lock holds until the oldest failure ages out.
        self.assertEqual(self.key_post({"api_key": "has space"})[0], 400)
        now[0] += 595  # 599 seconds after the first failure
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[0], 429)
        now[0] += 1  # exactly ten minutes after the first failure: it ages out, the other four still count
        self.assertEqual(self.key_post({"api_key": "wrong-key-0009"})[1]["error"]["code"], "key_rejected")
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[0], 429)
        # Once they have all aged out, the right key signs in and clears the record.
        now[0] += 700
        status, record, _, _ = self.key_post({"api_key": _KeyWire.GOOD})
        self.assertEqual((status, record["status"]), (200, "connected"))
        self.assertEqual(self.app.key_failures, [])
        # A success resets the count: four fresh failures do not lock.
        for number in range(4):
            self.assertEqual(self.key_post({"api_key": f"wrong-key-{number:04d}"})[1]["error"]["code"], "key_rejected")
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[0], 200)

    def test_failures_that_are_not_a_refusal_do_not_count_and_keep_their_own_codes(self):
        self.bootstrap()
        wire = self.real_account()
        for code in ("network", "timeout", "rate_limit", "provider"):
            wire.error = nexus.NexusError(code)
            for _ in range(7):
                status, record, _, _ = self.key_post({"api_key": _KeyWire.GOOD})
                self.assertEqual((status, record["error"]["code"]), (400, code))
        wire.error = None
        wire.status = 500
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[1]["error"]["code"], "provider")
        wire.status = None
        self.assertEqual(self.app.key_failures, [])
        self.assertFalse(self.app.store.path.exists())
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[0], 200)

    def test_the_key_is_never_echoed_logged_or_written_outside_the_private_store(self):
        import contextlib
        import io
        import logging
        self.bootstrap()
        # A finished job leaves a receipt, a manifest and a quarantined archive: the key must reach none of them.
        job = self.app.submit({"kind": "download_analyze", "game": "ownedgame", "mod_id": 1, "file_id": 2, "idempotency_key": "key-privacy-0001"})
        self.assertEqual(self.await_job(job["id"])["status"], "analyzed")
        self.real_account()
        out, err = io.StringIO(), io.StringIO()
        records = []

        class Capture(logging.Handler):
            def emit(self, record):
                records.append(record.getMessage())
        capture = Capture()
        logging.getLogger().addHandler(capture)
        logging.getLogger().setLevel(logging.DEBUG)
        try:
            with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
                def plain(path):
                    status, body, headers = self.request("GET", path)
                    return status, None, body, headers
                results = [self.key_post({"api_key": "wrong-secret-key-0001"}), self.key_post({"api_key": "bad key"}),
                           self.key_post({"api_key": _KeyWire.GOOD}), self.key_post({"api_key": _KeyWire.GOOD}),
                           plain("/api/account"), plain("/api/capabilities"), plain("/api/jobs")]
        finally:
            logging.getLogger().removeHandler(capture)
        for key in (b"wrong-secret-key-0001", _KeyWire.GOOD.encode(), b"bad key"):
            for result in results:
                self.assertNotIn(key, result[2], "response body")
                self.assertNotIn(key, json.dumps(result[3]).encode(), "response headers")
            self.assertNotIn(key.decode(), out.getvalue() + err.getvalue() + "\n".join(records))
            self.assertNotIn(key.decode(), repr(self.app.account) + repr(self.app.jobs()) + repr(self.app.capabilities()))
        # On disk the key is in the one private credential file and nowhere else (no receipt, no temporary file, no quarantine file).
        holders = [path for path in self.root.rglob("*") if path.is_file() and _KeyWire.GOOD.encode() in path.read_bytes()]
        self.assertEqual(holders, [self.app.store.path])
        self.assertTrue(any(path.suffix == ".json" for path in (self.root / "quarantine" / ".jobs").iterdir()), "the receipts were scanned too")
        self.assertNotIn(_KeyWire.GOOD, json.dumps(self.app.jobs()))
        self.assertEqual([path for path in self.root.rglob("*") if path.is_file() and b"wrong-secret-key-0001" in path.read_bytes()], [])
        # Not even a leftover temporary file from the atomic write.
        self.assertEqual([path.name for path in self.app.store.path.parent.iterdir()], ["credentials.json"])

    def test_key_sign_in_drops_provider_caches_and_website_grants(self):
        self.bootstrap()
        self.real_account()
        now = [1000.0]
        self.app.update_clock = lambda: now[0]
        for cache in (self.app.update_cache, self.app.requirements_cache, self.app.search_cache, self.app.browse_cache):
            cache[("ownedgame", 1)] = (now[0], {})
        self.app.download_authorizations["x" * 32] = object()
        self.assertEqual(self.key_post({"api_key": "wrong-key-0001"})[0], 400)
        self.assertEqual(len(self.app.browse_cache), 1, "a refused key changes nothing")
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[0], 200)
        self.assertEqual([len(cache) for cache in (self.app.update_cache, self.app.requirements_cache, self.app.search_cache,
                                                   self.app.browse_cache)], [0, 0, 0, 0])
        self.assertEqual(self.app.download_authorizations, {})

    def test_disconnect_clears_a_key_sign_in(self):
        self.bootstrap()
        self.real_account()
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[0], 200)
        self.assertTrue(self.app.store.path.exists())
        self.assertEqual(self.mutation("/api/auth/disconnect", {})[0], 200)
        self.assertFalse(self.app.store.path.exists())
        self.assertEqual(json.loads(self.request("GET", "/api/account")[1])["status"], "disconnected")
        # The failure record is not the account's: disconnecting does not forgive failed attempts.
        for _ in range(5):
            self.key_post({"api_key": "wrong-key-0001"})
        self.assertEqual(self.mutation("/api/auth/disconnect", {})[0], 200)
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[0], 429)

    def test_key_sign_in_replaces_an_oauth_sign_in(self):
        self.bootstrap()
        self.real_account()
        config = nexus.OAuthConfig("owned-client", "http://127.0.0.1:8181/callback")
        self.app.store.save_account(nexus.Credentials(access_token="old-access", refresh_token="old-refresh", expires_at=2000000000), config)
        self.assertEqual(self.key_post({"api_key": _KeyWire.GOOD})[0], 200)
        self.assertEqual(self.app.store.load_account(), (nexus.Credentials(api_key=_KeyWire.GOOD), None))

    def test_key_sign_in_takes_the_account_operation_lock(self):
        self.bootstrap()
        wire = self.real_account()
        lock = mock.MagicMock()
        lock.acquire.return_value = False
        with mock.patch.object(self.app, "operation_lock", lock):
            status, record, _, _ = self.key_post({"api_key": _KeyWire.GOOD})
        self.assertEqual((status, record["error"]["code"]), (409, "busy"))
        self.assertEqual(wire.requests, [])
        self.assertFalse(self.app.store.path.exists())

    def test_key_sign_in_error_codes_have_ui_text(self):
        self.assertEqual({"key_rejected", "too_many_attempts"} - set(host.MESSAGES), set())


# ---- Mod-to-mod file conflicts (companion/modbridge_conflicts.py) ----
import modbridge_conflicts as conflicts
import random
import subprocess
import zipfile

CORE = Path(os.environ.get("MODBRIDGE_BIN", PROJECT / "build" / "modbridge"))


def _sha(char):
    return char * 64


def _file(path, char="a"):
    return {"path": path, "sha256": _sha(char)}


def _mod(identifier, rank, *files):
    return {"id": identifier, "name": "Name of " + identifier, "rank": rank, "files": list(files)}


def _policy(case="ascii_insensitive", winner="last_wins"):
    return {"case": case, "winner": winner}


def _fixture_tree():
    """Deterministic multi-mod fixture with overwrites, file/directory clashes and siblings."""
    chance = random.Random(5)
    names = ["a", "A", "a-b", "a.b", "a b", "b", "ab", "x", "X", "y!"]
    pool = sorted({"/".join(chance.choice(names) for _ in range(chance.randint(1, 4))) for _ in range(150)})
    mods = []
    for index in range(9):
        taken, files = [], []
        for path in chance.sample(pool, 25):
            key = path.lower()
            if any(key == other or key.startswith(other + "/") or other.startswith(key + "/") for other in taken):
                continue
            taken.append(key)
            files.append(_file(path.replace("/", chance.choice("/\\")), chance.choice("abc")))
        mods.append(_mod("tree-%d" % index, 10 + index * 7, *files))
    return mods


CONFLICT_FIXTURES = [
    ("overwrite", [_mod("A", 1, _file("Data/X.pak", "a"), _file("Data/only_a.pak")),
                   _mod("B", 2, _file("data\\x.pak", "b"), _file("only_b.pak"))], _policy()),
    ("harmless", [_mod("A", 1, _file("lib/shared.dll", "c"), _file("x")), _mod("B", 2, _file("LIB/Shared.DLL", "c"))], _policy()),
    ("file_vs_directory", [_mod("A", 1, _file("mods/x")), _mod("B", 2, _file("mods/x/y.lua", "b"), _file("mods/x/deep/z.lua", "b"))], _policy()),
    ("kinds_share_a_path", [_mod("A", 1, _file("x")), _mod("B", 2, _file("x", "b")), _mod("C", 3, _file("x/y", "c"))], _policy("byte_exact", "first_wins")),
    ("siblings", [_mod("A", 1, _file("a")), _mod("B", 2, _file("a-b"), _file("a.txt"), _file("a b")), _mod("C", 3, _file("a/x"))], _policy()),
    ("byte_exact_case", [_mod("A", 1, _file("Data/Mod.PAK")), _mod("B", 2, _file("data/mod.pak", "b"))], _policy("byte_exact")),
    ("non_ascii_exact", [_mod("A", 1, _file("caf\u00e9/x")), _mod("B", 2, _file("caf\u00e9/x", "b"))], _policy("byte_exact")),
    ("rank_order", [_mod("high", 100, _file("p")), _mod("low", 5, _file("p", "b")), _mod("mid", 7, _file("p", "c"))], _policy("ascii_insensitive", "first_wins")),
    ("empty", [], _policy()),
    ("tree", _fixture_tree(), _policy()),
    ("tree_exact", _fixture_tree(), _policy("byte_exact", "first_wins")),
    # Inputs both implementations must refuse.
    ("tied_ranks", [_mod("A", 3, _file("p")), _mod("B", 3, _file("p", "b"))], _policy()),
    ("traversal", [_mod("A", 1, _file("../x"))], _policy()),
    ("non_ascii_folded", [_mod("A", 1, _file("caf\u00e9"))], _policy()),
    ("repeat_in_one_mod", [_mod("A", 1, _file("A.txt"), _file("a.TXT"))], _policy()),
    ("file_and_directory_in_one_mod", [_mod("A", 1, _file("x"), _file("x/y"))], _policy("byte_exact")),
    ("uppercase_sha", [_mod("A", 1, {"path": "p", "sha256": "A" * 64})], _policy()),
]


class ConflictAlgorithmTests(unittest.TestCase):
    def detect(self, mods, policy=None):
        return conflicts.detect_conflicts(mods, _policy() if policy is None else policy)

    def refusal(self, mods, policy=None):
        with self.assertRaises(conflicts.ConflictError) as error:
            self.detect(mods, policy)
        self.assertTrue(str(error.exception).startswith("conflicts: "))
        return str(error.exception)

    def test_overwrite_names_both_mods_and_the_later_one_wins(self):
        self.assertEqual(self.detect(CONFLICT_FIXTURES[0][1]), {
            "conflicts": [{"identical": False, "kind": "overwrite", "mods": ["A", "B"], "path": "data/x.pak", "winner": "B"}],
            "pairs": [{"a": "A", "b": "B", "paths": 1, "winner": "B"}],
            "summary": {"conflicting_paths": 1, "harmless_duplicates": 0, "mods_involved": 2}})

    def test_identical_duplicates_are_harmless_and_form_no_pair(self):
        self.assertEqual(self.detect(CONFLICT_FIXTURES[1][1]), {
            "conflicts": [{"identical": True, "kind": "overwrite", "mods": ["A", "B"], "path": "lib/shared.dll", "winner": "B"}],
            "pairs": [], "summary": {"conflicting_paths": 0, "harmless_duplicates": 1, "mods_involved": 0}})

    def test_file_versus_directory_is_its_own_kind_and_can_share_a_path_with_an_overwrite(self):
        self.assertEqual(self.detect(CONFLICT_FIXTURES[2][1])["conflicts"], [
            {"identical": False, "kind": "file_vs_directory", "mods": ["A", "B"], "path": "mods/x", "winner": "B"}])
        both = self.detect([_mod("A", 1, _file("x")), _mod("B", 2, _file("x", "b")), _mod("C", 3, _file("x/y", "c"))])
        self.assertEqual(both["conflicts"], [
            {"identical": False, "kind": "file_vs_directory", "mods": ["A", "B", "C"], "path": "x", "winner": "C"},
            {"identical": False, "kind": "overwrite", "mods": ["A", "B"], "path": "x", "winner": "B"}])
        self.assertEqual(both["summary"], {"conflicting_paths": 2, "harmless_duplicates": 0, "mods_involved": 3})

    def test_directory_prefix_ignores_lexically_adjacent_siblings(self):
        result = self.detect(CONFLICT_FIXTURES[4][1])
        self.assertEqual([(c["path"], c["mods"]) for c in result["conflicts"]], [("a", ["A", "C"])])
        self.assertEqual(self.detect([_mod("A", 1, _file("ab")), _mod("B", 2, _file("a/b"))])["conflicts"], [])

    def test_nested_chain_reports_every_ancestor_once(self):
        mods = [_mod("m%d" % (1000 + i), i, _file("/".join(["a"] * (i + 1)))) for i in range(300)]
        result = self.detect(mods)
        self.assertEqual(len(result["conflicts"]), 299)
        self.assertEqual(result["conflicts"][0]["path"], "a")
        self.assertEqual(len(result["conflicts"][0]["mods"]), 300)
        self.assertEqual(result["pairs"][-1], {"a": "m1298", "b": "m1299", "paths": 299, "winner": "m1299"})

    def test_case_and_winner_policies(self):
        mods = [_mod("A", 1, _file("Data/Mod.PAK")), _mod("B", 2, _file("data/mod.pak", "b"))]
        self.assertEqual(self.detect(mods, _policy("byte_exact"))["conflicts"], [])
        self.assertEqual(self.detect(mods)["conflicts"][0]["path"], "data/mod.pak")
        mods = [_mod("A", 1, _file("p"), _file("x")), _mod("B", 5, _file("p", "b")), _mod("C", 9, _file("p", "c"), _file("x/y", "c"))]
        last, first = self.detect(mods), self.detect(mods, _policy("ascii_insensitive", "first_wins"))
        self.assertEqual([c["winner"] for c in last["conflicts"]], ["C", "C"])
        self.assertEqual([c["winner"] for c in first["conflicts"]], ["A", "A"])
        self.assertEqual(last["pairs"][0]["winner"], "B")
        self.assertEqual(first["pairs"][0]["winner"], "A")
        self.assertEqual(first["summary"], last["summary"])

    def test_non_ascii_is_exact_or_refused(self):
        mods = [_mod("A", 1, _file("caf\u00e9/x")), _mod("B", 2, _file("CAF\u00e9/x", "b"))]
        self.assertIn("non-ASCII", self.refusal(mods))
        self.assertEqual(self.detect(mods, _policy("byte_exact"))["conflicts"], [])

    def test_ranks_are_load_order_ties_are_refused(self):
        result = self.detect(CONFLICT_FIXTURES[7][1], _policy())
        self.assertEqual(result["conflicts"][0]["mods"], ["low", "mid", "high"])
        self.assertEqual(result["conflicts"][0]["winner"], "high")
        tied = [_mod("A", 3, _file("p")), _mod("B", 3, _file("p", "b"))]
        message = self.refusal(tied)
        self.assertIn("share rank 3", message)
        self.assertIn('"A"', message)
        self.assertIn("ranks must be unique", self.refusal(tied, _policy("byte_exact", "first_wins")))
        self.assertIn("share rank", self.refusal([_mod("A", 3), _mod("B", 3)]))
        self.assertEqual(self.refusal(tied), self.refusal(tied[::-1]))
        self.assertIn("duplicate mod id", self.refusal([_mod("A", 1), _mod("A", 2)]))
        for bad in (-1, 1.5, 2.0, "1", True, None, 2 ** 64, [1]):
            with self.subTest(rank=bad):
                self.assertIn("rank must be an unsigned integer", self.refusal([dict(_mod("A", 1), rank=bad)]))
        self.assertEqual(self.detect([_mod("zero", 0, _file("p")), _mod("max", 2 ** 64 - 1, _file("p", "b"))])["conflicts"][0]["winner"], "max")

    def test_invalid_paths_are_refused_with_the_reason(self):
        bad = [("../x", "parent traversal"), ("a/../b", "parent traversal"), ("a/..", "parent traversal"),
               ("..\\x", "parent traversal"), ("/abs", "absolute or empty path"), ("\\abs", "absolute or empty path"),
               ("", "absolute or empty path"), (".", "empty logical destination"), ("./", "empty logical destination"),
               ("//", "absolute or empty path"), ("a:b", "unsafe path character"), ("C:\\x", "unsafe path character"),
               ("a\x00b", "unsafe path character"), ("a\tb", "unsafe path character"), ("a\x7f", "unsafe path character"),
               ("trailing.", "ambiguous destination spelling"), ("dir /x", "ambiguous destination spelling"),
               ("x/trail ", "ambiguous destination spelling"), ("...", "ambiguous destination spelling"),
               ("a" * 4097, "exceeds 4096 bytes"), ("\udcff", "not valid UTF-8")]
        for path, reason in bad:
            for case in ("byte_exact", "ascii_insensitive"):
                with self.subTest(path=path[:12], case=case):
                    message = self.refusal([_mod("a", 1, _file(path))], _policy(case))
                    self.assertTrue(message.startswith('conflicts: mod "a" path '))
                    self.assertIn(reason, message)
        for path, key in (("a" * 4096, "a" * 4096), ("a/./b//c/", "a/b/c"), ("a\\b\\c", "a/b/c"),
                          (".hidden/.x", ".hidden/.x"), ("a b/c d", "a b/c d"), ("a/ b", "a/ b")):
            result = self.detect([_mod("a", 1, _file(path)), _mod("b", 2, _file(path, "b"))], _policy("byte_exact"))
            self.assertEqual(result["conflicts"][0]["path"], key)
        message = self.refusal([_mod("a", 1, _file("bad\x01name/../x\"\\"))])
        self.assertNotIn("\x01", message)
        self.assertIn("bad?name", message)

    def test_repeats_and_prefixes_inside_one_mod_are_refused(self):
        def one(files, case):
            try:
                self.detect([_mod("A", 1, *files)], _policy(case))
            except conflicts.ConflictError as error:
                return str(error)
            return ""
        self.assertIn("more than once", one([_file("x"), _file("./x")], "byte_exact"))
        self.assertIn("more than once", one([_file("A.txt"), _file("a.TXT")], "ascii_insensitive"))
        self.assertEqual(one([_file("A.txt"), _file("a.TXT")], "byte_exact"), "")
        self.assertIn('has both file "x"', one([_file("x"), _file("x/y")], "byte_exact"))
        self.assertIn('has both file "x/y"', one([_file("x/y"), _file("x/w"), _file("x/y/z")], "byte_exact"))
        self.assertIn("has both file", one([_file("X"), _file("x/y")], "ascii_insensitive"))
        self.assertEqual(one([_file("x/y"), _file("x/z"), _file("xy")], "byte_exact"), "")

    def test_schema_and_policy_are_strict(self):
        good = _mod("A", 1, _file("p"))
        self.assertIn("mods must be an array", self.refusal({}))
        for field in ("id", "name", "rank", "files"):
            self.assertIn("missing field " + field, self.refusal([{k: v for k, v in good.items() if k != field}]))
        self.assertIn("unknown fields", self.refusal([dict(good, note="x")]))
        for field in ("id", "name"):
            for bad in ("", 7, None, []):
                self.assertIn(field, self.refusal([dict(good, **{field: bad})]))
        self.assertIn("field id", self.refusal([dict(good, id="i" * 257)]))
        self.assertEqual(self.detect([dict(good, id="i" * 256)])["conflicts"], [])
        self.assertIn("field name", self.refusal([dict(good, name="n" * 1025)]))
        self.assertIn("not valid UTF-8", self.refusal([dict(good, id="bad\udcff")]))
        self.assertIn("files must be an array", self.refusal([dict(good, files={})]))
        for bad in ({"path": "p"}, {"sha256": _sha("a")}, {"path": "p", "sha256": _sha("a"), "size": 1},
                    {"path": 7, "sha256": _sha("a")}, "p", None):
            self.refusal([dict(good, files=[bad])])
        for bad in ("a" * 63, "a" * 65, "A" * 64, "g" * 64, 7, None, ""):
            self.assertIn("sha256 must be 64 lowercase hex digits", self.refusal([dict(good, files=[{"path": "p", "sha256": bad}])]))
        for bad, text in (([], "policy"), ({"winner": "last_wins"}, "missing field case"), ({"case": "byte_exact"}, "missing field winner"),
                          ({"case": "byte_exact", "winner": "last_wins", "extra": 1}, "unknown fields"),
                          (_policy("Byte_Exact"), "case must be"), (_policy("byte_exact", "highest"), "winner must be"),
                          ({"case": 1, "winner": "last_wins"}, "must be strings")):
            self.assertIn(text, self.refusal([], bad))

    def test_limits(self):
        many = [_mod("m%d" % i, i, _file("shared.bin", "abcdef"[i % 6])) for i in range(512)]
        result = self.detect(many)
        self.assertEqual(len(result["conflicts"][0]["mods"]), 512)
        self.assertEqual(result["conflicts"][0]["winner"], "m511")
        self.assertEqual(len(result["pairs"]), 512 * 511 // 2)
        self.assertEqual(result["summary"]["mods_involved"], 512)
        self.assertIn("too many mods", self.refusal(many + [_mod("overflow", 9999)]))
        big = [_file("d/%d" % i) for i in range(100000)]
        self.assertEqual(self.detect([_mod("A", 1, *big)])["conflicts"], [])
        self.assertIn("too many files", self.refusal([_mod("A", 1, *big), _mod("B", 2, _file("one-too-many"))]))
        first, second = [_file("a/%d" % i) for i in range(60000)], [_file("b/%d" % i) for i in range(40001)]
        self.assertIn("too many files", self.refusal([_mod("A", 1, *first), _mod("B", 2, *second)]))
        self.assertEqual(self.detect([_mod("A", 1, *first), _mod("B", 2, *second[:-1])])["conflicts"], [])
        heavy = [_file("a" * 4000 + str(i)) for i in range(8400)]
        self.assertIn("path bytes exceed", self.refusal([_mod("A", 1, *heavy)]))

    def test_results_do_not_depend_on_input_order(self):
        mods = CONFLICT_FIXTURES[9][1]
        baseline = self.detect(mods)
        self.assertGreater(len(baseline["conflicts"]), 5)
        self.assertTrue(any(c["kind"] == "file_vs_directory" for c in baseline["conflicts"]))
        chance = random.Random(99)
        for _ in range(25):
            shuffled = [dict(mod, files=chance.sample(mod["files"], len(mod["files"]))) for mod in mods]
            chance.shuffle(shuffled)
            self.assertEqual(json.dumps(self.detect(shuffled), sort_keys=True), json.dumps(baseline, sort_keys=True))
        paths = [(c["path"].encode(), c["kind"]) for c in baseline["conflicts"]]
        self.assertEqual(paths, sorted(paths))
        text = json.dumps(baseline)
        self.assertNotIn("Name of", text)
        self.assertNotIn(_sha("a"), text)

    def test_core_agrees_with_python_when_it_has_the_conflicts_command(self):
        if not CORE.is_file():
            self.skipTest("desktop core is not built")
        folder = tempfile.TemporaryDirectory()
        self.addCleanup(folder.cleanup)
        for name, mods, policy in CONFLICT_FIXTURES:
            inputs = Path(folder.name)
            (inputs / "mods.json").write_text(json.dumps(mods))
            (inputs / "policy.json").write_text(json.dumps(policy))
            completed = subprocess.run([str(CORE), "conflicts", "--mods", str(inputs / "mods.json"),
                "--policy", str(inputs / "policy.json")], capture_output=True, timeout=60)
            if completed.returncode != 0 and b"Unknown command" in completed.stderr:
                self.skipTest("this core build has no conflicts command yet")
            with self.subTest(fixture=name):
                try:
                    expected = self.detect(mods, policy)
                except conflicts.ConflictError:
                    self.assertNotEqual(completed.returncode, 0)
                    continue
                self.assertEqual(completed.returncode, 0, completed.stderr)
                self.assertEqual(json.loads(completed.stdout), expected)


JOB_A, JOB_B, JOB_C = "a" * 32, "b" * 32, "c" * 32


class ConflictViewTests(unittest.TestCase):
    def job(self, identifier, rank, files, label=None):
        return {"id": identifier, "label": label or "Job " + identifier[:2], "rank": rank, "files": files}

    def test_summary_is_relative_to_each_job_and_only_real_conflicts_pair(self):
        views = conflicts.conflict_views([
            self.job(JOB_A, 1, [_file("Data/x.pak", "a"), _file("lib/shared.dll", "c"), _file("only/a")]),
            self.job(JOB_B, 2, [_file("data/X.PAK", "b"), _file("lib/shared.dll", "c")]),
            self.job(JOB_C, 3, [_file("lib/shared.dll", "c"), _file("unrelated", "c")])])
        self.assertEqual(views[JOB_A], {"with": [{"job_id": JOB_B, "label": "Job bb", "paths": 1, "winner": "other"}],
                                        "paths_sample": ["data/x.pak"], "harmless_duplicates": 1})
        self.assertEqual(views[JOB_B], {"with": [{"job_id": JOB_A, "label": "Job aa", "paths": 1, "winner": "this"}],
                                        "paths_sample": ["data/x.pak"], "harmless_duplicates": 1})
        self.assertEqual(views[JOB_C], {"with": [], "paths_sample": [], "harmless_duplicates": 1})

    def test_file_versus_directory_counts_for_every_party(self):
        views = conflicts.conflict_views([self.job(JOB_A, 1, [_file("mods/x")]), self.job(JOB_B, 2, [_file("mods/x/y")])])
        self.assertEqual(views[JOB_A]["with"][0]["winner"], "other")
        self.assertEqual(views[JOB_B]["with"][0]["winner"], "this")
        self.assertEqual(views[JOB_A]["paths_sample"], ["mods/x"])

    def test_later_submission_wins_and_a_lone_job_has_an_empty_summary(self):
        empty = {"with": [], "paths_sample": [], "harmless_duplicates": 0}
        self.assertEqual(conflicts.conflict_views([self.job(JOB_A, 5, [_file("p")])]), {JOB_A: empty})
        self.assertEqual(conflicts.conflict_views([]), {})
        later_first = conflicts.conflict_views([self.job(JOB_B, 9, [_file("p", "b")]), self.job(JOB_A, 2, [_file("p")])])
        self.assertEqual(later_first[JOB_B]["with"][0]["winner"], "this")
        # Equal ranks never raise here: ties break by job id.
        tied = conflicts.conflict_views([self.job(JOB_B, 1, [_file("p", "b")]), self.job(JOB_A, 1, [_file("p")])])
        self.assertEqual(tied[JOB_B]["with"][0]["winner"], "this")

    def test_summary_is_capped_and_deterministic(self):
        identifiers = ["%032x" % (index + 1) for index in range(40)]
        views = conflicts.conflict_views([self.job(identifier, index, [_file("shared.bin", "abcdef"[index % 6])])
                                          for index, identifier in enumerate(identifiers)])
        newest = views[identifiers[-1]]
        self.assertEqual(len(newest["with"]), conflicts.WITH_LIMIT)
        self.assertEqual([item["job_id"] for item in newest["with"]], sorted(item["job_id"] for item in newest["with"]))
        self.assertTrue(all(item["winner"] == "this" for item in newest["with"]))
        long = conflicts.conflict_views([self.job(JOB_A, 1, [_file("x" * 300), _file("k")]),
                                         self.job(JOB_B, 2, [_file("x" * 300, "b"), _file("k", "b")])])
        self.assertEqual(long[JOB_B]["paths_sample"], ["k"])
        self.assertEqual(long[JOB_B]["with"][0]["paths"], 2)
        many = conflicts.conflict_views([self.job(JOB_A, 1, [_file("d/%02d" % i) for i in range(30)]),
                                         self.job(JOB_B, 2, [_file("d/%02d" % i, "b") for i in range(30)])])
        self.assertEqual(many[JOB_B]["paths_sample"], ["d/%02d" % i for i in range(20)])
        self.assertEqual(many[JOB_B]["with"][0]["paths"], 30)
        heavy = conflicts.conflict_views([self.job("%032x" % (index + 1), index, [_file("%03d" % path + "x" * 253, "abc"[index % 3]) for path in range(25)],
                                                   label="L" * 256) for index in range(40)])
        self.assertLess(max(len(json.dumps(view)) for view in heavy.values()), 32000)
        for view in list(views.values()) + list(many.values()) + list(heavy.values()):
            conflicts.validate_view(view)

    def test_a_job_that_cannot_be_compared_is_left_out_not_reported_clean(self):
        views = conflicts.conflict_views([
            self.job(JOB_A, 1, [_file("p")]), self.job(JOB_B, 2, [_file("p", "b")]),
            self.job(JOB_C, 3, [_file("caf\u00e9")])])
        self.assertEqual(set(views), {JOB_A, JOB_B})
        self.assertEqual(views[JOB_A]["with"][0]["job_id"], JOB_B)
        self.assertEqual(conflicts.conflict_views([self.job(JOB_A, 1, [_file("x"), _file("X")])]), {})
        big = [self.job("%032x" % index, index, [_file("d/%d" % i) for i in range(1000)]) for index in range(101)]
        self.assertEqual(conflicts.conflict_views(big), {})

    def test_manifests_are_bounded_and_collision_free(self):
        files = [_file("a/b"), _file("C")]
        self.assertEqual(conflicts.validate_manifest(files), files)
        self.assertIsNot(conflicts.validate_manifest(files)[0], files[0])
        for bad in (None, {}, [], [_file("a"), _file("A")], [_file("../x")], [_file("caf\u00e9")], [{"path": "a"}],
                    [_file("p%d" % i) for i in range(conflicts.MAX_MANIFEST_FILES + 1)]):
            with self.subTest(bad=str(bad)[:30]), self.assertRaises(ValueError):
                conflicts.validate_manifest(bad)
        inventory = {"entries": [{"path": "A/b.lua", "sha256": _sha("a"), "kind": "lua", "size": 3}, {"path": "c", "sha256": _sha("b")}]}
        self.assertEqual(conflicts.inventory_files(inventory), [_file("A/b.lua"), _file("c", "b")])
        for bad in (None, {}, {"entries": "x"}, {"entries": [{"path": "a"}]}, {"entries": [{"path": 1, "sha256": _sha("a")}]},
                    {"entries": [{"path": "a", "sha256": "xyz"}]}):
            with self.subTest(bad=str(bad)[:30]), self.assertRaises(ValueError):
                conflicts.inventory_files(bad)

    def test_validate_view_refuses_every_forgery(self):
        good = {"with": [{"job_id": JOB_A, "label": "Mod 1 \u00b7 file 2", "paths": 2, "winner": "this"}],
                "paths_sample": ["a/b", "c"], "harmless_duplicates": 0}
        self.assertEqual(conflicts.validate_view(good), good)
        self.assertIsNot(conflicts.validate_view(good), good)
        entry = good["with"][0]
        for change in ({"extra": 1}, {"with": [dict(entry, extra=1)]}, {"with": [dict(entry, winner="draw")]},
                       {"with": [dict(entry, job_id="A" * 32)]}, {"with": [dict(entry, job_id="/private/x")]},
                       {"with": [dict(entry, paths=0)]}, {"with": [dict(entry, paths=True)]}, {"with": [dict(entry, label="")]},
                       {"with": [dict(entry, label="x" * 257)]}, {"with": [entry, entry]},
                       {"with": [dict(entry, job_id="%032x" % i) for i in range(33)]},
                       {"paths_sample": ["../x"]}, {"paths_sample": ["/abs"]}, {"paths_sample": ["A/B"]},
                       {"paths_sample": ["b", "a"]}, {"paths_sample": ["a", "a"]}, {"paths_sample": [7]},
                       {"paths_sample": ["p%02d" % i for i in range(21)]}, {"paths_sample": ["p" * 257]},
                       {"harmless_duplicates": -1},
                       {"harmless_duplicates": 1.0}, {"harmless_duplicates": True}, {"with": {}}, {"paths_sample": "a"}):
            with self.subTest(change=str(change)[:40]), self.assertRaises(ValueError):
                conflicts.validate_view(dict(good, **change))
        with self.assertRaises(ValueError):
            conflicts.validate_view({key: value for key, value in good.items() if key != "with"})
        with self.assertRaises(ValueError):
            conflicts.validate_view([])


class HostConflictTests(unittest.TestCase):
    """Real application, private storage and worker; only the Nexus provider boundary is replaced."""

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.manifests = {}
        self.counter = 0
        self.app = self.start()

    def start(self, analyze=None):
        def download(link, destination):
            destination.mkdir(mode=0o700, parents=True, exist_ok=True)
            path = destination / "owned.zip"
            path.write_bytes(b"owned archive")
            return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size, link.source)

        def default_analyze(result, game):
            analysis = {"status": "analyzed", "compatibility": "unresolved"}
            files = self.manifests.get((game, result.source.mod_id))
            if files is not None:
                analysis["files"] = files
            return {"analysis": analysis}
        app = host.BridgeApplication(self.root / "private" / "credentials.json", self.root / "quarantine",
            account=FakeAccount(), download=download, analyze=analyze or default_analyze)
        self.addCleanup(app.close)
        return app

    def analyzed(self, game, mod, files=None, app=None):
        app = app or self.app
        if files is not None:
            self.manifests[(game, mod)] = files
        self.counter += 1
        job = app.submit({"kind": "download_analyze", "game": game, "mod_id": mod, "file_id": 2,
                          "idempotency_key": "conflict-job-%04d" % self.counter})
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            current = app.jobs(job["id"])
            if current["status"] in ("analyzed", "failed", "interrupted"):
                self.assertEqual(current["status"], "analyzed", current)
                return current
            time.sleep(.01)
        self.fail("Owned job did not settle")

    def test_later_job_wins_and_both_jobs_learn_about_each_other(self):
        first = self.analyzed("ownedgame", 1, [_file("Data/Shared.pak", "a"), _file("keep/me", "a"), _file("lib/common.dll", "c")])
        self.assertEqual(first["conflicts"], {"with": [], "paths_sample": [], "harmless_duplicates": 0})
        second = self.analyzed("ownedgame", 2, [_file("data/shared.PAK", "b"), _file("lib/common.dll", "c"), _file("own/file", "b")])
        self.assertEqual(second["conflicts"], {"with": [{"job_id": first["id"], "label": "Mod 1 \u00b7 file 2", "paths": 1, "winner": "this"}],
                                                "paths_sample": ["data/shared.pak"], "harmless_duplicates": 1})
        refreshed = self.app.jobs(first["id"])
        self.assertEqual(refreshed["conflicts"], {"with": [{"job_id": second["id"], "label": "Mod 2 \u00b7 file 2", "paths": 1, "winner": "other"}],
                                                   "paths_sample": ["data/shared.pak"], "harmless_duplicates": 1})
        self.assertEqual(self.app.jobs(second["id"]), second)
        self.assertEqual(set(job["id"] for job in self.app.jobs() if "conflicts" in job), {first["id"], second["id"]})
        self.assertNotIn(str(self.root), json.dumps(self.app.jobs()))
        self.assertNotIn("sha256" + _sha("a"), json.dumps(self.app.jobs()))
        # A mutated reply never reaches the stored job.
        second["conflicts"]["paths_sample"].append("forged")
        self.assertEqual(self.app.jobs(second["id"])["conflicts"]["paths_sample"], ["data/shared.pak"])

    def test_only_jobs_for_the_same_game_are_compared(self):
        self.analyzed("ownedgame", 1, [_file("same/path", "a")])
        other = self.analyzed("othergame", 2, [_file("same/path", "b")])
        self.assertEqual(other["conflicts"], {"with": [], "paths_sample": [], "harmless_duplicates": 0})

    def test_jobs_without_a_file_list_carry_no_summary_and_never_block_others(self):
        plain = self.analyzed("ownedgame", 1)
        self.assertNotIn("conflicts", plain)
        listed = self.analyzed("ownedgame", 2, [_file("p")])
        self.assertEqual(listed["conflicts"]["with"], [])
        self.assertNotIn("conflicts", self.app.jobs(plain["id"]))
        # A file list the comparison cannot trust is dropped, not half-used.
        bad = self.analyzed("ownedgame", 3, [{"path": "../escape", "sha256": _sha("a")}])
        self.assertNotIn("conflicts", bad)
        unicode_job = self.analyzed("ownedgame", 4, [_file("caf\u00e9")])
        self.assertNotIn("conflicts", unicode_job)

    def test_summary_and_submission_order_survive_restart_and_later_jobs_still_compare(self):
        first = self.analyzed("ownedgame", 1, [_file("p", "a"), _file("q", "a")])
        second = self.analyzed("ownedgame", 2, [_file("p", "b")])
        self.app.close()
        receipt = nexus.load_private_json(self.app.receipts / (first["id"] + ".json"))
        self.assertEqual(self.app._validated_receipt(receipt, first["id"])["conflicts"], self.app.jobs(first["id"])["conflicts"])
        self.assertEqual((receipt["submit_seq"], nexus.load_private_json(self.app.receipts / (second["id"] + ".json"))["submit_seq"]), (1, 2))
        restarted = self.start()
        self.assertEqual(restarted.jobs(first["id"]), self.app.jobs(first["id"]))
        self.assertEqual(restarted.jobs(second["id"]), self.app.jobs(second["id"]))
        third = self.analyzed("ownedgame", 3, [_file("p", "c"), _file("q", "c")], app=restarted)
        self.assertEqual([(item["job_id"], item["paths"], item["winner"]) for item in third["conflicts"]["with"]],
            sorted([(first["id"], 2, "this"), (second["id"], 1, "this")], key=lambda item: (-item[1], item[0])))
        self.assertEqual(restarted.jobs(first["id"])["conflicts"]["with"][0]["winner"], "other")
        manifests = sorted(path.name for path in restarted.receipts.glob("*.manifest"))
        self.assertEqual(manifests, sorted(job + ".manifest" for job in (first["id"], second["id"], third["id"])))
        for path in restarted.receipts.glob("*.manifest"):
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)

    def test_manifest_files_do_not_count_against_the_job_limit_or_receipt_scan(self):
        self.analyzed("ownedgame", 1, [_file("p")])
        self.app.close()
        self.assertTrue(list(self.app.receipts.glob("*.manifest")))
        restarted = self.start()
        self.assertEqual(len(restarted.jobs()), 1)

    def test_an_untrusted_manifest_file_is_ignored(self):
        first = self.analyzed("ownedgame", 1, [_file("p", "a")])
        self.app.close()
        manifest = self.app.receipts / (first["id"] + ".manifest")
        saved = json.loads(manifest.read_text())
        for damage in (lambda value: value.update(package_sha256="f" * 64), lambda value: value.update(id="9" * 32),
                       lambda value: value.update(files=[{"path": "../x", "sha256": _sha("a")}]),
                       lambda value: value.update(extra=1), lambda value: value.update(version=2)):
            forged = json.loads(json.dumps(saved))
            damage(forged)
            manifest.write_text(json.dumps(forged))
            os.chmod(manifest, 0o600)
            restarted = self.start()
            second = self.analyzed("ownedgame", 5, [_file("p", "b")], app=restarted)
            self.assertEqual(second["conflicts"]["with"], [])
            self.assertEqual(restarted.jobs(first["id"])["conflicts"], {"with": [], "paths_sample": [], "harmless_duplicates": 0})
            restarted.close()
            for path in restarted.receipts.glob("*"):
                if path.name.startswith(second["id"]):
                    path.unlink()
            self.manifests.pop(("ownedgame", 5))
            restarted.records.clear()
        manifest.write_text("{not json")
        os.chmod(manifest, 0o600)
        self.assertIsNone(host._read_manifest(manifest, first["id"], saved["package_sha256"]))
        manifest.write_text(json.dumps(saved))
        os.chmod(manifest, 0o644)
        self.assertIsNone(host._read_manifest(manifest, first["id"], saved["package_sha256"]))
        os.chmod(manifest, 0o600)
        self.assertEqual(host._read_manifest(manifest, first["id"], saved["package_sha256"]), [_file("p", "a")])
        link = self.app.receipts / ("link-" + first["id"])
        link.symlink_to(manifest)
        self.assertIsNone(host._read_manifest(link, first["id"], saved["package_sha256"]))

    def test_receipt_validation_accepts_old_jobs_and_refuses_forged_summaries(self):
        job = self.analyzed("ownedgame", 1, [_file("p")])
        saved = nexus.load_private_json(self.app.receipts / (job["id"] + ".json"))
        old = {key: value for key, value in saved.items() if key not in ("conflicts", "submit_seq")}
        self.assertEqual(self.app._validated_receipt(old, job["id"]), old)
        self.assertEqual(self.app._validated_receipt(saved, job["id"]), saved)
        view = saved["conflicts"]
        entry = {"job_id": "a" * 32, "label": "Mod 1", "paths": 1, "winner": "this"}
        for change in ({"conflicts": dict(view, extra=1)}, {"conflicts": dict(view, **{"with": [dict(entry, winner="x")]})},
                       {"conflicts": dict(view, paths_sample=["../x"])}, {"conflicts": dict(view, harmless_duplicates=-1)},
                       {"conflicts": "none"}, {"submit_seq": 0}, {"submit_seq": True}, {"submit_seq": "1"},
                       {"status": "failed", "stage": "downloaded"}):
            forged = dict(saved, **change)
            if change.get("status") == "failed":
                forged["error"] = {"code": "operation_failed", "message": host.MESSAGES["operation_failed"]}
            with self.subTest(change=str(change)[:40]), self.assertRaises(host.HostError):
                self.app._validated_receipt(forged, job["id"])
        # A conflict summary on a job that never finished analysis is invalid.
        queued = dict(old, conflicts=view, status="queued", stage="requested")
        queued.pop("sha256"), queued.pop("bytes_downloaded")
        for key in ("analysis_report", "compatibility_assessment", "runtime_coverage"):
            queued.pop(key, None)
        with self.assertRaises(host.HostError):
            self.app._validated_receipt(queued, job["id"])

    def test_the_summary_is_exposed_on_the_jobs_endpoint(self):
        self.analyzed("ownedgame", 1, [_file("p", "a")])
        self.analyzed("ownedgame", 2, [_file("p", "b")])
        server = host.create_server(self.app)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        self.addCleanup(thread.join, 2)
        self.addCleanup(server.server_close)
        self.addCleanup(server.shutdown)
        wire = http.client.HTTPConnection("127.0.0.1", server.server_address[1], timeout=2)
        wire.request("GET", "/api/capabilities")
        response = wire.getresponse()
        cookie = response.getheader("Set-Cookie").split(";", 1)[0]
        response.read()
        wire.request("GET", "/api/jobs", headers={"Cookie": cookie})
        items = json.loads(wire.getresponse().read())["items"]
        wire.close()
        self.assertEqual([item["conflicts"]["with"][0]["winner"] for item in items], ["other", "this"])
        self.assertEqual([item["conflicts"]["paths_sample"] for item in items], [["p"], ["p"]])

    @unittest.skipUnless(CORE.is_file(), "desktop core is not built")
    def test_real_archives_through_the_core_report_overwrites_and_identical_files(self):
        from nexus import audit_download
        contents = {1: {"Mods/Owned/Scripts/main.lua": b"print('one')\n", "Data/shared.txt": b"shared\n"},
                    2: {"mods/owned/scripts/MAIN.lua": b"print('two')\n", "Data/shared.txt": b"shared\n", "Data/own.txt": b"own\n"}}
        current = {}

        def download(link, destination):
            destination.mkdir(mode=0o700, parents=True, exist_ok=True)
            path = destination / "mod.zip"
            with zipfile.ZipFile(path, "w") as package:
                for name, data in contents[current["mod"]].items():
                    package.writestr(name, data)
            return nexus.DownloadResult(path, hashlib.sha256(path.read_bytes()).hexdigest(), path.stat().st_size, link.source)
        app = self.start(analyze=audit_download)
        app.download = download
        current["mod"] = 1
        first = self.analyzed("anothergame", 1, app=app)
        self.assertEqual(first["conflicts"], {"with": [], "paths_sample": [], "harmless_duplicates": 0})
        current["mod"] = 2
        second = self.analyzed("anothergame", 2, app=app)
        self.assertEqual(second["conflicts"], {"with": [{"job_id": first["id"], "label": "Mod 1 \u00b7 file 2", "paths": 1, "winner": "this"}],
                                                "paths_sample": ["mods/owned/scripts/main.lua"], "harmless_duplicates": 1})
        self.assertEqual(app.jobs(first["id"])["conflicts"]["with"][0]["winner"], "other")
        self.assertNotIn(str(self.root), json.dumps(app.jobs()))


if __name__ == "__main__":
    unittest.main(verbosity=2)
