"""Real companion entry-point checks; never contacts Nexus or opens a browser."""
import json
import hashlib
import importlib.util
import http.client
import os
from pathlib import Path
import stat
import socket
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from types import SimpleNamespace
from unittest import mock
import zipfile
from urllib.parse import parse_qs, urlencode, urlsplit

PROJECT = Path(__file__).resolve().parents[1]
COMMAND = PROJECT / "companion" / "nexus.py"


class CompanionCommands(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="modbridge-companion-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.store = self.root / "credentials.json"

    def invoke(self, *args, expected=0):
        result = subprocess.run([sys.executable, str(COMMAND), *map(str, args)],
                                capture_output=True, text=True, timeout=15)
        self.assertEqual(result.returncode, expected, result.stdout + result.stderr)
        return result

    def test_help_describes_available_commands(self):
        result = self.invoke("--help")
        for command in ("status", "connect-key", "login", "games", "mods", "files", "download", "disconnect"):
            self.assertIn(command, result.stdout)

    def test_status_does_not_create_credentials_or_claim_live_access(self):
        status = json.loads(self.invoke("status", "--store", self.store).stdout)
        self.assertFalse(status["credentials_present"])
        self.assertEqual(status["live_service"], "not_checked")
        self.assertEqual(status["console_translation"], "unverified")
        self.assertFalse(self.store.exists())

    def load_entry(self):
        sys.path.insert(0, str(PROJECT / "companion"))
        self.addCleanup(lambda: sys.path.remove(str(PROJECT / "companion")))
        import modbridge_nexus as actual
        spec = importlib.util.spec_from_file_location("nexus_entry", COMMAND)
        entry = importlib.util.module_from_spec(spec); spec.loader.exec_module(entry)
        return entry, actual

    def test_personal_key_is_validated_before_replacing_private_account(self):
        entry, actual = self.load_entry()
        key = self.root / "test-key.txt"; secret = "owned-offline-test-key-12345"
        key.write_text(secret); os.chmod(key, 0o600)
        args = SimpleNamespace(command="connect-key", key_file=key, store=self.store)
        actual.CredentialStore(self.store).save(actual.Credentials(api_key="owned-original"))
        output = []
        with mock.patch.object(actual.HTTPSTransport, "request", return_value=actual.Response(401, {}, b'{}')), mock.patch.object(entry, "emit", side_effect=output.append):
            with self.assertRaises(actual.NexusError): entry.execute(args, actual)
        self.assertEqual(actual.CredentialStore(self.store).load().api_key, "owned-original")
        body = b'{"user_id":42,"name":"Owned user","is_premium":false,"is_supporter":false}'
        with mock.patch.object(actual.HTTPSTransport, "request", return_value=actual.Response(200, {}, body)), mock.patch.object(entry, "emit", side_effect=output.append):
            entry.execute(args, actual)
        self.assertNotIn(secret, json.dumps(output)); self.assertTrue(output[-1]["connected"])
        self.assertEqual(stat.S_IMODE(self.store.stat().st_mode), 0o600)
        with mock.patch.object(actual.HTTPSTransport, "request", return_value=actual.Response(401, {}, b'{}')), mock.patch.object(entry, "emit", side_effect=output.append):
            entry.execute(SimpleNamespace(command="status", store=self.store), actual)
        self.assertFalse(output[-1]["connected"])
        self.invoke("disconnect", "--store", self.store); self.assertFalse(self.store.exists())

    def test_missing_credentials_prevent_api_request(self):
        result = self.invoke("games", "--store", self.store, expected=2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "credentials_required")
        self.assertFalse(self.store.exists())

    def test_login_requires_project_configuration(self):
        self.invoke("login", "--store", self.store, expected=2)
        self.assertFalse(self.store.exists())
        result = self.invoke("login", "--client-id", "owned-client",
                             "--redirect-uri", "https://example.com/callback",
                             "--store", self.store, expected=2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "invalid_callback")
        self.assertFalse(self.store.exists())

    def test_key_symlink_is_not_imported(self):
        key = self.root / "test-key.txt"
        key.write_text("owned-test-key")
        alias = self.root / "key-alias"
        alias.symlink_to(key)
        result = self.invoke("connect-key", "--key-file", alias, "--store", self.store, expected=2)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "invalid_input")
        self.assertFalse(self.store.exists())

    def test_argument_errors_do_not_echo_accidental_secret_values(self):
        secret = "owned-accidental-test-secret"
        result = self.invoke("status", "--store", self.store, "--api-key", secret, expected=2)
        self.assertNotIn(secret, result.stdout + result.stderr)
        self.assertEqual(json.loads(result.stderr)["error"]["code"], "invalid_argument")

    def execute_owned_download(self, changed=False):
        sys.path.insert(0, str(PROJECT / "companion"))
        self.addCleanup(lambda: sys.path.remove(str(PROJECT / "companion")))
        spec = importlib.util.spec_from_file_location("nexus_entry", COMMAND)
        entry = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(entry)
        archive = self.root / "owned-test.zip"
        with zipfile.ZipFile(archive, "w") as package:
            package.writestr("logic.lua", "RegisterHook('/Script/Owned.Event', function() end)")
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if changed:
            with archive.open("ab") as stream:
                stream.write(b"changed after fixture download")
        source = SimpleNamespace(game_domain="ownedgame", mod_id=1, file_id=2,
                                 source_url="https://www.nexusmods.com/ownedgame/mods/1")
        result = SimpleNamespace(path=archive, sha256=digest, bytes_downloaded=archive.stat().st_size,
                                 source=source, stage="downloaded")
        provider = SimpleNamespace(
            CredentialStore=lambda _: SimpleNamespace(load=lambda: object()),
            AccountManager=lambda _: SimpleNamespace(client=lambda: SimpleNamespace(download_links=lambda *_, **__: [object()])),
            download_to_quarantine=lambda *_, **__: result)
        args = SimpleNamespace(command="download", store=self.store, game="ownedgame", mod=1,
                               file=2, nxm_file=None, destination=self.root)
        output = []
        with mock.patch.object(entry, "emit", side_effect=output.append):
            entry.execute(args, provider)
        return output[0]

    def test_download_pipeline_uses_real_core_and_keeps_unknown_target_unresolved(self):
        record = self.execute_owned_download()
        self.assertEqual(record["stage"], "downloaded")
        self.assertEqual(record["source"]["url"], "https://www.nexusmods.com/ownedgame/mods/1")
        analysis = record["analysis"]
        self.assertEqual(analysis["status"], "analyzed")
        inventory = json.loads(Path(analysis["inventory_path"]).read_text())
        self.assertEqual(inventory["package_sha256"], record["sha256"])
        plan = json.loads(Path(analysis["plan_path"]).read_text())
        self.assertEqual(plan["status"], "unresolved")
        self.assertEqual(plan["eligible_steps"], [])
        self.assertEqual(analysis["report"]["compatibility_state"], "unverified")
        self.assertIn("unknown_target", analysis["report"]["reason_codes"])
        self.assertEqual(analysis["report"]["install_authority"]["state"], "unsupported")
        self.assertNotIn("game_working", record)

    def test_changed_download_cannot_keep_old_provenance(self):
        with self.assertRaises(Exception) as error:
            self.execute_owned_download(changed=True)
        self.assertEqual(error.exception.code, "source_changed")

    def test_measured_target_hash_is_only_identity_and_never_runtime_evidence(self):
        entry, actual = self.load_entry()
        archive = self.root / "different-title.zip"
        with zipfile.ZipFile(archive, "w") as package:
            package.writestr("data/owned.txt", "Different generic title fixture")
        source = actual.DownloadSource("unrelatedgame", 72, 19)
        result = actual.DownloadResult(archive, hashlib.sha256(archive.read_bytes()).hexdigest(),
            archive.stat().st_size, source)
        record = entry.audit_download(result, "unrelatedgame", {"fingerprints": {"build_sha256": "c" * 64}})
        analysis = record["analysis"]
        plan = json.loads(Path(analysis["plan_path"]).read_text())
        target = json.loads((Path(analysis["plan_path"]).parent / "target.json").read_text())
        self.assertEqual(target["fingerprints"], {"build_sha256": "c" * 64})
        self.assertEqual(target["capabilities"], [])
        self.assertNotIn("unknown_target", analysis["report"]["reason_codes"])
        self.assertEqual(analysis["report"]["compatibility_state"], "unverified")
        self.assertEqual(analysis["report"]["install_authority"]["state"], "unsupported")
        self.assertEqual(plan["eligible_steps"], [])
        with self.assertRaises(entry.CommandError):
            entry.audit_download(result, "unrelatedgame", {"fingerprints": {"nexus_game": "unrelatedgame"}})

    def test_real_core_distinguishes_windows_managed_and_filename_only_mods(self):
        entry, actual = self.load_entry()
        fixtures = self.root / "structural-fixtures"
        subprocess.run([sys.executable, str(PROJECT / "tests" / "make_fixtures.py"), str(fixtures)],
            check=True, capture_output=True, timeout=10)
        for filename, reason in (("native.dll", "windows_runtime_unavailable"),
                ("managed.dll", "managed_runtime_unavailable"), ("fake.dll", None)):
            with self.subTest(filename=filename):
                archive = self.root / (filename + ".zip")
                with zipfile.ZipFile(archive, "w") as package:
                    package.writestr(filename, (fixtures / filename).read_bytes())
                result = actual.DownloadResult(archive, hashlib.sha256(archive.read_bytes()).hexdigest(),
                    archive.stat().st_size, actual.DownloadSource("othergame2080", 23, 45))
                report = entry.audit_download(result, "othergame2080")["analysis"]["report"]
                self.assertEqual(report["compatibility_state"], "unsupported" if reason else "unverified")
                if reason:
                    self.assertIn(reason, report["reason_codes"])
                else:
                    self.assertNotIn("windows_runtime_unavailable", report["reason_codes"])
                self.assertEqual(report["install_authority"]["state"], "unsupported")

    def owned_callback(self, fail=False, identity_fail=False):
        sys.path.insert(0, str(PROJECT / "companion"))
        self.addCleanup(lambda: sys.path.remove(str(PROJECT / "companion")))
        import modbridge_nexus as actual
        spec = importlib.util.spec_from_file_location("nexus_login_entry", COMMAND)
        entry = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(entry)
        store = actual.CredentialStore(self.store)
        store.save(actual.Credentials(api_key="owned-original-key"))
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0))
            port = reservation.getsockname()[1]
        redirect = "http://127.0.0.1:%d/callback" % port
        args = SimpleNamespace(client_id="owned-client", redirect_uri=redirect, scope="")
        output, responses, threads = [], [], []

        def exchange(session, callback, *_, **__):
            session.consume_callback(callback)
            if fail:
                raise actual.NexusError("provider")
            return actual.Credentials(access_token="owned-access-token", refresh_token="owned-refresh-token", expires_at=__import__("time").time()+300)

        provider = actual
        import modbridge_nexus.account as account_module
        profile_body = b'{"sub":"42","name":"Owned user","membership_roles":[],"preferences":{}}'
        profile_response = actual.Response(401 if identity_fail else 200, {}, b'{}' if identity_fail else profile_body)

        def open_owned_browser(url):
            state = parse_qs(urlsplit(url).query)["state"][0]
            def request():
                connection = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
                try:
                    connection.request("GET", "/callback?" + urlencode({"state": state, "code": "owned-code"}))
                    response = connection.getresponse()
                    responses.append(response.status)
                    response.read()
                finally:
                    connection.close()
            thread = threading.Thread(target=request)
            threads.append(thread)
            thread.start()
            return True

        with mock.patch.object(entry.webbrowser, "open", side_effect=open_owned_browser), \
                mock.patch.object(entry, "emit", side_effect=output.append), \
                mock.patch.object(account_module, "exchange_code", side_effect=exchange), \
                mock.patch.object(actual, "exchange_code", side_effect=exchange), \
                mock.patch.object(actual.HTTPSTransport, "request", return_value=profile_response):
            if fail or identity_fail:
                with self.assertRaises(entry.CommandError) as error:
                    entry.login(args, store, provider)
                self.assertEqual(error.exception.code, "authorization_incomplete")
            else:
                entry.login(args, store, provider)
        for thread in threads:
            thread.join(5)
            self.assertFalse(thread.is_alive())
        return store.load(), output, responses

    def test_owned_loopback_callback_stores_tokens_without_echoing_them(self):
        credentials, output, responses = self.owned_callback()
        self.assertEqual(responses, [200])
        self.assertEqual(credentials.access_token, "owned-access-token")
        self.assertEqual(output[-1]["status"], "authorized")
        self.assertNotIn("owned-access-token", json.dumps(output))
        self.assertNotIn("owned-refresh-token", json.dumps(output))
        self.assertNotIn("owned-code", json.dumps(output))

    def test_failed_live_identity_preserves_previous_credentials(self):
        credentials, output, responses = self.owned_callback(identity_fail=True)
        self.assertEqual(responses, [400])
        self.assertEqual(credentials.api_key, "owned-original-key")

    def test_partial_callback_and_late_final_newline_are_bounded(self):
        entry, actual = self.load_entry()
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0)); port = reservation.getsockname()[1]
        args = SimpleNamespace(client_id="owned-client", redirect_uri="http://127.0.0.1:%d/callback" % port, scope="")
        threads = []
        def open_browser(url):
            def request():
                with socket.create_connection(("127.0.0.1", port), timeout=2) as wire:
                    wire.sendall(b"GET /callback HTTP/1.1\r\nHost: localhost\r\n")
                    threading.Event().wait(0.3)
                    try: wire.sendall(b"\r\n")
                    except OSError: pass
            thread = threading.Thread(target=request); threads.append(thread); thread.start()
        started = __import__('time').monotonic()
        with mock.patch.object(entry.webbrowser, "open", side_effect=open_browser), mock.patch.object(entry, "emit"):
            with self.assertRaises(entry.CommandError):
                entry.login(args, actual.CredentialStore(self.store), actual, timeout=0.12)
        self.assertLess(__import__('time').monotonic() - started, 0.25)
        self.assertFalse(self.store.exists())
        for thread in threads: thread.join(2)

    def test_unrelated_callback_does_not_restart_the_accept_deadline(self):
        entry, actual = self.load_entry()
        with socket.socket() as reservation:
            reservation.bind(("127.0.0.1", 0)); port = reservation.getsockname()[1]
        args = SimpleNamespace(client_id="owned-client", redirect_uri="http://127.0.0.1:%d/callback" % port, scope="")
        threads = []
        def open_browser(_):
            def request():
                time.sleep(0.07)
                connection = http.client.HTTPConnection("127.0.0.1", port, timeout=2)
                try:
                    connection.request("GET", "/unrelated")
                    response = connection.getresponse(); response.read()
                finally: connection.close()
            thread = threading.Thread(target=request); threads.append(thread); thread.start()
        started = time.monotonic()
        with mock.patch.object(entry.webbrowser, "open", side_effect=open_browser), mock.patch.object(entry, "emit"):
            with self.assertRaises(entry.CommandError): entry.login(args, actual.CredentialStore(self.store), actual, timeout=0.12)
        self.assertLess(time.monotonic() - started, 0.17)
        for thread in threads: thread.join(2)

    def test_failed_exchange_preserves_previous_credentials(self):
        credentials, output, responses = self.owned_callback(fail=True)
        self.assertEqual(responses, [400])
        self.assertEqual(credentials.api_key, "owned-original-key")
        self.assertNotIn("owned-original-key", json.dumps(output))


if __name__ == "__main__":
    unittest.main(verbosity=2)
