"""Runtime profiles on the Python host: status rules, generation, gating. Owned fixtures only; the generator is a stand-in
(tests/fake_profilegen.py) except in the one test that runs the real tools/profilegen when it exists and the real executable is at hand.

Rules under test are section 5 of docs/profiles/PROFILE-CONTRACT-PROPOSAL.md as set out in docs/profiles/APP-INTEGRATION.md.
"""
import hashlib
import http.client
import io
import json
import os
from pathlib import Path
import shutil
import stat
import subprocess
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / "companion"))
sys.path.insert(0, str(PROJECT / "tests"))
import modbridge_host as host
import modbridge_profiles as profiles
from fake_profilegen import build_profile, tiny_elf, with_verification, write_profile
from host_tests import FakeAccount
from titles_root_tests import Tree, param

FAKE = str(PROJECT / "tests" / "fake_profilegen.py")
TITLE, VERSION = "PPSA33286", "02.003.000"
KEYS = {"status", "profile_id", "generated_utc", "confidence", "unresolved", "reason"}
SELF_MAGIC = b"\x4f\x15\x3d\x1d"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def bound_sha(document):
    """The executable sha256 a profile is bound to, plain or as an envelope."""
    value = document["binding"]["executable"]["sha256"]
    return value["value"] if isinstance(value, dict) else value


def fake(mode="ok", *extra):
    return [sys.executable, FAKE, "--mode", mode, *extra]


class Base(unittest.TestCase):
    """A console-shaped tree with a Townfall dump, a host over it and an HTTP client."""

    generator = None
    timeout = 20

    def setUp(self):
        self.tree = Tree(self)
        private = tempfile.TemporaryDirectory()
        self.addCleanup(private.cleanup)
        self.root = Path(private.name).resolve()
        self.profiles_dir = self.root / "profiles"
        self.exe = tiny_elf(b"decrypted-townfall")
        self.dump = self.add_dump("Silent Hill Townfall PPSA33286", TITLE, VERSION, "SILENT HILL: Townfall", self.exe)
        self.start_host(self.generator or fake())

    def add_dump(self, folder, title, version, name, executable=None, where="decrypted"):
        directory = self.tree.homebrew / folder
        self.tree.dump(folder, param(title, version, name))
        if executable is not None:
            target = directory / "decrypted" / "eboot.bin" if where == "decrypted" else directory / "eboot.bin"
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(executable)
        return directory

    def start_host(self, command, timeout=None):
        if getattr(self, "app", None) is not None:
            self.stop_host()
        self.app = host.BridgeApplication(self.root / "private" / "credentials.json", self.root / "quarantine", account=FakeAccount(),
                                          titles_root=self.tree.root, runtime_profiles=self.profiles_dir, generator_command=command,
                                          generator_timeout=timeout or self.timeout)
        self.server = host.create_server(self.app)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.port, self.origin = self.server.server_address[1], self.app.origin
        self.cookie = self.csrf = None
        self.addCleanup(self.stop_host)

    def stop_host(self):
        if getattr(self, "app", None) is None:
            return
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)
        self.app.close()
        self.app = None

    # HTTP

    def request(self, method, path, body=None, headers=None):
        wire = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
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
        wire.close()
        return response.status, content

    def bootstrap(self):
        status, body = self.request("GET", "/api/capabilities")
        self.assertEqual(status, 200)
        self.csrf = json.loads(body)["csrf_token"]
        return json.loads(body)

    def get(self, title=TITLE, version=VERSION):
        status, body = self.request("GET", "/api/titles/%s/profile?content_version=%s" % (title, version))
        return status, json.loads(body)

    def post(self, title=TITLE, body=None):
        status, content = self.request("POST", "/api/titles/%s/profile/generate" % title, {} if body is None else body,
                                       {"Origin": self.origin, "X-CSRF-Token": self.csrf})
        return status, json.loads(content)

    def await_status(self, predicate, title=TITLE, version=VERSION, timeout=15):
        deadline = time.monotonic() + timeout
        while True:
            record = self.app.runtime_profile(title, version)
            if predicate(record["status"]) or time.monotonic() > deadline:
                return record
            time.sleep(0.02)

    # files

    def profile_path(self, title=TITLE, version=VERSION):
        return self.profiles_dir / title / version / "runtime-profile.json"

    def plant(self, document, title=TITLE, version=VERSION):
        write_profile(self.profile_path(title, version), document)

    def document(self, **options):
        return build_profile(TITLE, VERSION, sha(self.exe), **options)

    def status(self):
        return self.app.runtime_profile(TITLE, VERSION)


class StatusRuleTests(Base):
    def test_no_file_is_none_and_the_record_has_exactly_six_keys(self):
        self.assertEqual(self.status(), {"status": "none", "profile_id": None, "generated_utc": None, "confidence": None,
                                         "unresolved": None, "reason": None})
        self.assertFalse(self.profiles_dir.exists(), "reading status creates nothing")

    def test_unverified_profile_is_generated_with_counts_and_no_reason(self):
        self.plant(self.document(high=10, medium=3, low=2, unresolved=4))
        record = self.status()
        self.assertEqual(set(record), KEYS)
        self.assertEqual(record, {"status": "generated", "profile_id": "%s/%s/%s" % (TITLE, VERSION, sha(self.exe)[:8]),
                                  "generated_utc": "2026-10-07T12:00:00Z", "confidence": {"high": 10, "medium": 3, "low": 2},
                                  "unresolved": 4, "reason": None})

    def test_gaps_and_rejected_alternatives_are_not_counted(self):
        # The fixture holds a null-valued low envelope and two alternatives next to its counted values.
        self.plant(self.document(high=8, medium=0, low=0, unresolved=0))
        self.assertEqual(self.status()["confidence"], {"high": 8, "medium": 0, "low": 0})
        self.assertEqual(self.status()["unresolved"], 0)

    def test_console_verified_needs_a_matching_digest_and_the_visible_executable(self):
        self.plant(with_verification(self.document()))
        record = self.status()
        self.assertEqual((record["status"], record["reason"]), ("console-verified", None))
        self.assertEqual(record["confidence"], {"high": 10, "medium": 3, "low": 2})

    def test_every_stale_and_verified_combination(self):
        document = self.document()
        bad = "f" * 64
        other_exe = build_profile(TITLE, VERSION, bad)  # a profile made from some other executable
        cases = [
            # (name, file, expected status, reason)
            ("digest and executable match", with_verification(document), "console-verified", None),
            ("digest differs", with_verification(document, digest=bad), "stale", "digest_mismatch"),
            ("executable differs", with_verification(other_exe), "stale", "executable_mismatch"),
            ("both differ", with_verification(other_exe, digest=bad), "stale", "digest_mismatch"),
            ("file still says generated, verification matches", with_verification(document, status="generated"), "console-verified", None),
            ("file says verified, digest differs", with_verification(document, digest=bad, status="console-verified"), "stale", "digest_mismatch"),
            ("file says generated, digest differs", with_verification(document, digest=bad, status="generated"), "stale", "digest_mismatch"),
            ("the test did not pass", with_verification(document, result="fail"), "generated", None),
            ("no verification", document, "generated", None),
        ]
        for name, content, status, reason in cases:
            with self.subTest(name):
                self.plant(content)
                record = self.status()
                self.assertEqual((record["status"], record["reason"]), (status, reason))
                self.assertEqual(record["profile_id"], content["profile_id"])

    def test_a_verification_cannot_be_checked_without_a_readable_executable(self):
        self.plant(with_verification(self.document()))
        self.assertEqual(self.status()["status"], "console-verified")
        (self.dump / "decrypted" / "eboot.bin").unlink()
        record = self.status()
        self.assertEqual((record["status"], record["reason"]), ("stale", "executable_unavailable"))
        # An unverified profile needs no executable to be shown.
        self.plant(self.document())
        self.assertEqual(self.status()["status"], "generated")

    def test_the_executable_is_hashed_again_when_it_changes(self):
        self.plant(with_verification(self.document()))
        self.assertEqual(self.status()["status"], "console-verified")
        path = self.dump / "decrypted" / "eboot.bin"
        path.write_bytes(self.exe + b"patched")
        self.assertEqual(self.status()["reason"], "executable_mismatch")
        path.write_bytes(self.exe)
        self.assertEqual(self.status()["status"], "console-verified")

    def test_a_title_that_is_not_a_dump_has_no_executable_to_match(self):
        self.tree.appmeta(self.tree.user, "PPSA00010", param("PPSA00010", "1.0", "Cached only"))
        self.app.detected_titles(refresh=True)
        document = build_profile("PPSA00010", "1.0", "a" * 64)
        self.plant(with_verification(document), "PPSA00010", "1.0")
        record = self.app.runtime_profile("PPSA00010", "1.0")
        self.assertEqual((record["status"], record["reason"]), ("stale", "executable_unavailable"))

    def test_a_forged_claim_is_never_console_verified(self):
        document = self.document()
        # Claims the status without any verification block.
        claim = dict(document, status="console-verified")
        # Verification blocks that are not blocks.
        forged = [("a string", "pass"), ("a list", ["pass"]), ("empty", {}), ("missing keys", {"profile_digest": document["profile_digest"], "result": "pass"})]
        forged.append(("wrong types", dict(with_verification(document)["verification"], verified="yes")))
        forged.append(("digest not text", dict(with_verification(document)["verification"], profile_digest=7)))
        forged.append(("runtime build not an object", dict(with_verification(document)["verification"], runtime_build="x")))
        self.plant(claim)
        record = self.status()
        self.assertEqual((record["status"], record["reason"]), ("failed", "status_inconsistent"))
        for name, verification in forged:
            with self.subTest(name):
                self.plant(dict(document, status="console-verified", verification=verification))
                record = self.status()
                self.assertEqual((record["status"], record["reason"]), ("failed", "profile_invalid"))
                self.assertIsNone(record["profile_id"])

    def test_malformed_files_are_failed_with_a_reason_and_never_generated(self):
        document = self.document()
        path = self.profile_path()
        path.parent.mkdir(parents=True)
        without = lambda key: {name: value for name, value in document.items() if name != key}
        text = json.dumps(document)
        cases = [
            ("not json", b"{nope", "profile_invalid"),
            ("empty", b"", "profile_invalid"),
            ("not an object", b"[]", "profile_invalid"),
            ("not utf-8", b"\xff\xfe", "profile_invalid"),
            ("duplicate key", text.replace('"status": "generated"', '"status": "generated", "status": "generated"').encode(), "profile_invalid"),
            ("nan", text.replace('"unresolved": [', '"x": NaN, "unresolved": [').encode(), "profile_invalid"),
            ("another schema", json.dumps(dict(document, schema="modbridge.runtime-profile/2")).encode(), "profile_invalid"),
            ("no schema", json.dumps(without("schema")).encode(), "profile_invalid"),
            ("no profile id", json.dumps(without("profile_id")).encode(), "profile_invalid"),
            ("profile id with a slash escape", json.dumps(dict(document, profile_id="../x")).encode(), "profile_invalid"),
            ("short digest", json.dumps(dict(document, profile_digest="abc")).encode(), "profile_invalid"),
            ("uppercase digest", json.dumps(dict(document, profile_digest="A" * 64)).encode(), "profile_invalid"),
            ("another status", json.dumps(dict(document, status="verified")).encode(), "profile_invalid"),
            ("status stale", json.dumps(dict(document, status="stale")).encode(), "profile_invalid"),
            ("no generation time", json.dumps(without("generated")).encode(), "profile_invalid"),
            ("time not a time", json.dumps(dict(document, generated={"utc": "yesterday"})).encode(), "profile_invalid"),
            ("no binding", json.dumps(without("binding")).encode(), "profile_invalid"),
            ("binding without a executable hash", json.dumps(dict(document, binding=dict(document["binding"], executable={"name": "x"}))).encode(), "profile_invalid"),
            ("unresolved not a list", json.dumps(dict(document, unresolved=3)).encode(), "profile_invalid"),
            ("unresolved rows not objects", json.dumps(dict(document, unresolved=[1])).encode(), "profile_invalid"),
            ("a section that is not an object", json.dumps(dict(document, symbols=[])).encode(), "profile_invalid"),
            ("another title", json.dumps(build_profile("PPSA00001", VERSION, sha(self.exe))).encode(), "profile_mismatch"),
            ("another version", json.dumps(build_profile(TITLE, "01.000.000", sha(self.exe))).encode(), "profile_mismatch"),
        ]
        bad_level = json.loads(text)
        bad_level["symbols"]["high_0"]["label"]["confidence"] = "certain"
        cases.append(("an unknown confidence", json.dumps(bad_level).encode(), "profile_invalid"))
        for name, content, reason in cases:
            with self.subTest(name):
                path.write_bytes(content)
                record = self.status()
                self.assertEqual(set(record), KEYS)
                self.assertEqual((record["status"], record["reason"]), ("failed", reason))
                for key in ("profile_id", "generated_utc", "confidence", "unresolved"):
                    self.assertIsNone(record[key])

    def test_a_file_that_is_not_a_plain_readable_file_is_unreadable(self):
        path = self.profile_path()
        path.parent.mkdir(parents=True)
        target = self.root / "elsewhere.json"
        write_profile(target, self.document())
        os.symlink(target, path)
        self.assertEqual((self.status()["status"], self.status()["reason"]), ("failed", "profile_unreadable"))
        path.unlink()
        path.mkdir()
        self.assertEqual((self.status()["status"], self.status()["reason"]), ("failed", "profile_unreadable"))
        path.rmdir()
        write_profile(path, self.document())
        os.chmod(path, 0)
        try:
            if os.access(path, os.R_OK):
                self.skipTest("the test runs with rights to read anything")
            self.assertEqual((self.status()["status"], self.status()["reason"]), ("failed", "profile_unreadable"))
        finally:
            os.chmod(path, 0o600)
        # A folder that is a link is not followed either.
        shutil_target = self.root / "other-dir"
        shutil_target.mkdir()
        write_profile(shutil_target / "runtime-profile.json", self.document())
        self.profile_path().unlink()
        self.profile_path().parent.rmdir()
        os.symlink(shutil_target, self.profile_path().parent)
        self.assertEqual((self.status()["status"], self.status()["reason"]), ("failed", "profile_unreadable"))

    def test_a_file_over_the_size_limit_is_unreadable(self):
        path = self.profile_path()
        path.parent.mkdir(parents=True)
        with open(path, "wb") as stream:
            stream.truncate(profiles.PROFILE_LIMIT + 1)
        self.assertEqual((self.status()["status"], self.status()["reason"]), ("failed", "profile_unreadable"))

    def test_reading_never_changes_the_file(self):
        self.plant(with_verification(self.document()))
        path = self.profile_path()
        before = (path.read_bytes(), path.stat().st_mtime_ns, sorted(os.listdir(path.parent)))
        for _ in range(3):
            self.status()
            self.bootstrap()
            self.get()
        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns, sorted(os.listdir(path.parent))), before)

    def test_the_source_has_no_writer_of_a_verification_or_a_verified_status(self):
        # The host's modules read profile files; the only writer is the generator process. A source-level guard: nothing assigns or
        # builds a verification, the profile module has no way to write or remove a file, and the verified status is named in exactly
        # the places that judge it.
        profile_source = (PROJECT / "companion" / "modbridge_profiles.py").read_text()
        host_source = (PROJECT / "companion" / "modbridge_host.py").read_text()
        for name, source in (("modbridge_profiles.py", profile_source), ("modbridge_host.py", host_source)):
            for forbidden in ('["verification"] =', "['verification'] =", '"verification":', "'verification':", "verification="):
                self.assertFalse(forbidden in source, "%s holds %r" % (name, forbidden))
        for forbidden in (".write_text(", ".write_bytes(", "os.rename", "os.replace(", "os.unlink", "os.remove", "os.write(", "shutil", "O_WRONLY",
                          "O_RDWR", "O_CREAT", "O_TRUNC", "O_APPEND"):
            self.assertFalse(forbidden in profile_source, "modbridge_profiles.py holds %r" % forbidden)
        self.assertFalse('"console-verified"' in host_source, "the host module names the verified status")
        lines = [line.strip() for line in profile_source.splitlines() if '"console-verified"' in line]
        self.assertEqual(lines, ['or claimed not in ("generated", "console-verified")', 'elif claimed == "console-verified":',
                                 'return record("console-verified", profile=profile)'])


class LocateTests(Base):
    def generate(self, title=TITLE):
        self.bootstrap()
        status, body = self.post(title)
        self.assertIn(status, (200, 202))
        return status, body

    def test_decrypted_folder_is_preferred_over_the_dump_root(self):
        (self.dump / "eboot.bin").write_bytes(tiny_elf(b"root-copy"))
        self.generate()
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")
        document = json.loads(self.profile_path().read_text())
        self.assertEqual(bound_sha(document), sha(self.exe))

    def test_the_dump_root_is_used_when_there_is_no_decrypted_folder(self):
        (self.dump / "decrypted" / "eboot.bin").unlink()
        (self.dump / "decrypted").rmdir()
        root_copy = tiny_elf(b"root-copy")
        (self.dump / "eboot.bin").write_bytes(root_copy)
        self.generate()
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")
        self.assertEqual(bound_sha(json.loads(self.profile_path().read_text())), sha(root_copy))

    def test_an_encrypted_executable_is_refused_with_its_own_reason_and_nothing_runs(self):
        recorder = self.root / "ran"
        self.start_host([sys.executable, "-c", "import sys; open(%r, 'w').write('ran')" % str(recorder)])
        for body in (SELF_MAGIC + bytes(60), b"", b"MZ" + bytes(60)):
            (self.dump / "decrypted" / "eboot.bin").write_bytes(body)
            self.bootstrap()
            status, record = self.post()
            self.assertEqual((status, record["status"], record["reason"]), (200, "failed", "executable_encrypted"))
            self.assertEqual(self.app.runtime_profile(TITLE, VERSION)["reason"], "executable_encrypted")
        self.assertFalse(recorder.exists())
        self.assertFalse(self.profiles_dir.exists())

    def test_a_decrypted_executable_that_is_not_an_elf_falls_back_to_the_root_file_which_may_be(self):
        (self.dump / "decrypted" / "eboot.bin").write_bytes(SELF_MAGIC + bytes(60))
        (self.dump / "eboot.bin").write_bytes(tiny_elf(b"root-copy"))
        self.bootstrap()
        self.assertEqual(self.post()[1]["status"], "generating")
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")

    def test_a_dump_without_an_executable_is_no_executable(self):
        (self.dump / "decrypted" / "eboot.bin").unlink()
        self.bootstrap()
        status, record = self.post()
        self.assertEqual((status, record["status"], record["reason"]), (200, "failed", "no_executable"))
        self.assertFalse(self.profiles_dir.exists())

    def test_folders_are_not_followed_through_links_but_the_executable_file_may_be_one(self):
        decrypted = self.dump / "decrypted"
        link_target = self.root / "real-decrypted"
        decrypted.rename(link_target)
        os.symlink(link_target, decrypted)
        self.bootstrap()
        self.assertEqual(self.post()[1]["reason"], "no_executable", "a linked decrypted folder is not entered")
        os.unlink(decrypted)
        decrypted.mkdir()
        os.symlink(link_target / "eboot.bin", decrypted / "eboot.bin")
        status, record = self.post()
        self.assertEqual((status, record["status"]), (202, "generating"))
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")
        # A link to something that is not a regular file is no executable.
        self.profile_path().unlink()
        os.unlink(decrypted / "eboot.bin")
        os.symlink(self.root, decrypted / "eboot.bin")
        self.assertEqual(self.post()[1]["reason"], "no_executable")

    def test_only_a_dump_can_be_generated(self):
        self.tree.appmeta(self.tree.user, "PPSA00010", param("PPSA00010", "1.0", "Cached only"))
        self.app.detected_titles(refresh=True)
        self.bootstrap()
        status, record = self.post("PPSA00010")
        self.assertEqual((status, record["status"], record["reason"]), (200, "failed", "no_executable"))
        # A dump that shares its id with cached app data is a dump (the dump wins the inventory).
        self.tree.appmeta(self.tree.user, TITLE, param(TITLE, "01.000.000", "Appmeta Townfall"))
        self.app.detected_titles(refresh=True)
        self.assertEqual(self.post()[1]["status"], "generating")
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")
        self.assertTrue(self.profile_path().is_file(), "filed under the dump's version, not the cache's")

    def test_a_title_the_host_does_not_list_is_not_found(self):
        self.bootstrap()
        for title in ("PPSA99999", "NOPE"):
            self.assertEqual(self.post(title)[0], 404)
            self.assertEqual(self.get(title)[0], 404)

    def test_a_dump_without_a_usable_version_is_no_version(self):
        self.add_dump("No version PPSA00031", "PPSA00031", None, "No version", tiny_elf())
        self.add_dump("Odd version PPSA00032", "PPSA00032", "1 0/x", "Odd version", tiny_elf())
        self.app.detected_titles(refresh=True)
        self.bootstrap()
        for title in ("PPSA00031", "PPSA00032"):
            status, record = self.post(title)
            self.assertEqual((status, record["status"], record["reason"]), (200, "failed", "no_version"), title)
        self.assertFalse(self.profiles_dir.exists())

    def test_the_version_comes_from_the_inventory_never_from_the_caller(self):
        self.bootstrap()
        self.assertEqual(self.post(body={"content_version": "../../etc"})[0], 400)
        self.assertEqual(self.post()[1]["status"], "generating")
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")
        self.assertEqual(sorted(p.name for p in self.profiles_dir.iterdir()), [TITLE])
        self.assertEqual(sorted(p.name for p in (self.profiles_dir / TITLE).iterdir()), [VERSION])


class GenerateTests(Base):
    def test_success_runs_the_generator_with_the_exact_arguments_and_files_the_profile(self):
        recorder = self.root / "argv.json"
        script = ("import json, sys; json.dump(sys.argv[1:], open(%r, 'w')); import runpy; "
                  "sys.argv = [%r] + sys.argv[1:]; runpy.run_path(%r, run_name='__main__')" % (str(recorder), FAKE, FAKE))
        self.start_host([sys.executable, "-c", script])
        self.assertTrue(self.bootstrap()["capabilities"]["profile_generate"])
        status, record = self.post()
        self.assertEqual((status, record["status"]), (202, "generating"))
        self.assertEqual(set(record), KEYS)
        self.assertEqual(self.get()[1]["status"] in ("generating", "generated"), True)
        done = self.await_status(lambda value: value != "generating")
        self.assertEqual((done["status"], done["reason"]), ("generated", None))
        argv = json.loads(recorder.read_text())
        self.assertEqual(argv[:1], ["--eboot"])
        self.assertEqual(argv, ["--eboot", str(self.dump / "decrypted" / "eboot.bin"), "--param", str(self.dump / "sce_sys" / "param.json"),
                                "--out", str(self.profiles_dir / TITLE / VERSION)])
        # Private folders, a file the status reads back, and a profile that is generated and unverified.
        for folder in (self.profiles_dir, self.profiles_dir / TITLE, self.profiles_dir / TITLE / VERSION):
            self.assertEqual(stat.S_IMODE(folder.stat().st_mode), 0o700, folder)
        document = json.loads(self.profile_path().read_text())
        self.assertEqual((document["status"], document["verification"]), ("generated", None))
        self.assertEqual(self.get()[1], done)
        self.assertEqual(self.post()[0], 202, "regenerating a finished profile starts again")
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")

    def test_the_generator_runs_from_the_project_root_without_the_hosts_secrets(self):
        probe = self.root / "probe.json"
        script = ("import json, os, sys; json.dump({'cwd': os.getcwd(), 'env': sorted(os.environ)}, open(%r, 'w')); sys.exit(2)" % str(probe))
        with mock.patch.dict(os.environ, {"NEXUS_API_KEY": "do-not-leak", "PYTHONPATH": "/nowhere"}):
            self.start_host([sys.executable, "-c", script])
            self.bootstrap()
            self.post()
            self.await_status(lambda value: value != "generating")
        seen = json.loads(probe.read_text())
        self.assertEqual(os.path.realpath(seen["cwd"]), os.path.realpath(PROJECT))
        self.assertNotIn("NEXUS_API_KEY", seen["env"])
        self.assertNotIn("PYTHONPATH", seen["env"])

    def test_default_command_is_the_isolated_module_run_with_the_project_on_the_path(self):
        command = profiles.default_generator_command()
        self.assertEqual(command[:3], [sys.executable, "-I", "-B"])
        self.assertEqual(command[3], "-c")
        self.assertIn("tools.profilegen", command[4])
        self.assertIn(repr(str(PROJECT)), command[4])
        # The bootstrap reaches the module (a package with no __main__ yet says so; once the generator lands it runs).
        process = subprocess.run(command + ["--help"], cwd=str(PROJECT), capture_output=True, text=True, timeout=60, stdin=subprocess.DEVNULL)
        self.assertNotIn("No module named 'tools'", process.stderr)
        self.assertNotIn("Error while finding module specification", process.stderr)

    def test_a_failing_generator_is_failed_with_a_code_and_nothing_it_said_reaches_the_wire(self):
        self.start_host(fake("fail"))
        self.bootstrap()
        replies = [self.post()]
        done = self.await_status(lambda value: value != "generating")
        self.assertEqual((done["status"], done["reason"]), ("failed", "generator_failed"))
        replies += [self.get(), self.post()]
        replies.append(self.get())
        wire = json.dumps(replies)
        for secret in ("fixture generator failure", str(self.tree.root), str(self.profiles_dir), "eboot.bin", "Traceback"):
            self.assertNotIn(secret, wire)
        self.assertEqual(replies[0][1]["status"], "generating")

    def test_a_failure_shows_until_the_next_attempt_which_may_succeed(self):
        mark = self.root / "mark"
        script = ("import os, sys, runpy\nif not os.path.exists(%r):\n    open(%r, 'w').close()\n    sys.exit(2)\n"                  "sys.argv = [%r] + sys.argv[1:]\nrunpy.run_path(%r, run_name='__main__')" % (str(mark), str(mark), FAKE, FAKE))
        self.start_host([sys.executable, "-c", script])
        self.bootstrap()
        self.post()
        self.assertEqual(self.await_status(lambda value: value != "generating")["reason"], "generator_failed")
        self.assertEqual(self.status()["status"], "failed", "the failure stays until something else happens")
        self.assertEqual(self.status()["status"], "failed")
        self.assertEqual(self.post()[1]["status"], "generating", "a new attempt clears it")
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")
        self.stop_host()
        self.start_host(fake("fail"))
        self.bootstrap()
        self.post()
        self.assertEqual(self.await_status(lambda value: value != "generating")["reason"], "generator_failed")
        self.stop_host()
        self.start_host(fake("ok"))  # a restart forgets the failure and the profile on disk shows again
        self.assertEqual(self.status()["status"], "generated")

    def test_a_failed_regeneration_leaves_the_existing_file_alone(self):
        self.plant(self.document())
        path = self.profile_path()
        before = (path.read_bytes(), path.stat().st_mtime_ns)
        self.start_host(fake("fail"))
        self.bootstrap()
        self.post()
        self.assertEqual(self.await_status(lambda value: value != "generating")["reason"], "generator_failed")
        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), before)

    def test_timeout_stops_the_generator_and_says_so(self):
        pid_file = self.root / "pid"
        self.start_host([sys.executable, "-c", "import os, time; open(%r, 'w').write(str(os.getpid())); time.sleep(60)" % str(pid_file)],
                        timeout=1.0)
        self.bootstrap()
        started = time.monotonic()
        self.assertEqual(self.post()[1]["status"], "generating")
        done = self.await_status(lambda value: value != "generating")
        self.assertEqual((done["status"], done["reason"]), ("failed", "generator_timeout"))
        self.assertLess(time.monotonic() - started, 15)
        pid = int(pid_file.read_text())
        with self.assertRaises(ProcessLookupError):
            os.kill(pid, 0)

    def test_garbage_output_and_no_output_are_failures_not_profiles(self):
        for mode, reason in (("garbage", "profile_invalid"), ("silent", "generator_failed")):
            with self.subTest(mode):
                shutil.rmtree(self.profiles_dir, ignore_errors=True)
                self.start_host(fake(mode))
                self.bootstrap()
                self.post()
                done = self.await_status(lambda value: value != "generating")
                self.assertEqual((done["status"], done["reason"]), ("failed", reason))
                self.assertNotEqual(done["status"], "generated")

    def test_a_generator_that_cannot_start_is_generator_unavailable(self):
        self.start_host([str(self.root / "no-such-generator")])
        self.bootstrap()
        self.post()
        done = self.await_status(lambda value: value != "generating")
        self.assertEqual((done["status"], done["reason"]), ("failed", "generator_unavailable"))

    def test_the_profiles_folder_that_cannot_be_prepared_is_storage_unavailable(self):
        self.profiles_dir.write_text("a file where the folder should be")
        self.bootstrap()
        status, record = self.post()
        self.assertEqual((status, record["status"], record["reason"]), (200, "failed", "storage_unavailable"))
        self.profiles_dir.unlink()
        self.profiles_dir.mkdir()
        os.symlink(self.root, self.profiles_dir / TITLE)
        self.assertEqual(self.post()[1]["reason"], "storage_unavailable", "a linked title folder is refused")

    def test_one_generation_at_a_time(self):
        other = self.add_dump("Other PPSA00020", "PPSA00020", "1.00", "Other game", tiny_elf(b"other"))
        release = self.root / "go"
        self.start_host(fake("hold", "--release", str(release)))
        self.bootstrap()
        self.assertEqual(self.post()[1]["status"], "generating")
        self.assertEqual(self.get()[1]["status"], "generating")
        self.assertEqual(self.post()[0], 202, "the same title again is the same generation")
        status, body = self.post("PPSA00020")
        self.assertEqual((status, body["error"]["code"]), (409, "busy"))
        self.assertEqual(self.get("PPSA00020", "1.00")[1]["status"], "none", "the other game is not marked busy")
        release.write_bytes(b"")
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")
        self.assertEqual(self.post("PPSA00020")[1]["status"], "generating")
        self.assertEqual(self.await_status(lambda value: value != "generating", "PPSA00020", "1.00")["status"], "generated")
        self.assertTrue(other.exists())

    def test_closing_the_host_stops_a_running_generator(self):
        pid_file = self.root / "pid"
        self.start_host([sys.executable, "-c", "import os, time; open(%r, 'w').write(str(os.getpid())); time.sleep(60)" % str(pid_file)])
        self.bootstrap()
        self.post()
        deadline = time.monotonic() + 10
        while not pid_file.exists() and time.monotonic() < deadline:
            time.sleep(0.02)
        pid = int(pid_file.read_text())
        started = time.monotonic()
        self.stop_host()
        self.assertLess(time.monotonic() - started, 10)
        with self.assertRaises(ProcessLookupError):
            os.kill(pid, 0)

    def test_the_host_never_creates_a_verification_or_a_verified_status(self):
        self.bootstrap()
        self.post()
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")
        for _ in range(2):
            document = json.loads(self.profile_path().read_text())
            self.assertEqual((document["status"], document["verification"]), ("generated", None))
            self.assertEqual(self.status()["status"], "generated")
            self.post()
            self.await_status(lambda value: value != "generating")
        # A verification planted by the receipt ingester: Regenerate is refused (the generator would refuse with exit 3, and the host
        # never forces it), nothing runs, nothing is remembered, and the file and its status stand.
        self.plant(with_verification(json.loads(self.profile_path().read_text())))
        path = self.profile_path()
        before = (path.read_bytes(), path.stat().st_mtime_ns)
        status, record = self.post()
        self.assertEqual((status, record["status"], record["reason"]), (200, "failed", "verified_profile_kept"))
        self.assertEqual(self.status()["status"], "console-verified")
        self.assertEqual((path.read_bytes(), path.stat().st_mtime_ns), before)

    def test_a_stale_verification_is_replaced_by_a_regenerated_profile(self):
        # A verification for another digest no longer applies (contract section 5): Regenerate replaces the file with a fresh
        # "generated" profile and no verification, passing --force; the host still writes no verification itself.
        recorder = self.root / "argv.json"
        script = ("import json, sys; json.dump(sys.argv[1:], open(%r, 'w')); import runpy; "
                  "sys.argv = [%r] + sys.argv[1:]; runpy.run_path(%r, run_name='__main__')" % (str(recorder), FAKE, FAKE))
        self.start_host([sys.executable, "-c", script])
        self.plant(with_verification(self.document(), digest="f" * 64))
        self.assertEqual(self.status()["status"], "stale")
        self.bootstrap()
        status, record = self.post()
        self.assertEqual(record["status"], "generating")
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")
        self.assertEqual(json.loads(recorder.read_text())[-1], "--force")
        document = json.loads(self.profile_path().read_text())
        self.assertEqual((document["status"], document["verification"]), ("generated", None))
    def test_generating_over_a_file_that_is_not_a_profile_is_allowed(self):
        # A file that is not a valid profile carries no verification the host can see: generating over it is allowed.
        self.plant(self.document())
        self.profile_path().write_text("garbage")
        self.start_host(fake("ok"))
        self.bootstrap()
        self.assertEqual(self.post()[1]["status"], "generating")
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")

    def test_the_generators_own_refusal_is_the_same_reason_and_leaves_the_file_to_speak(self):
        # Exit 3 is the real generator's "would overwrite a verified profile" (a race with the ingester, or a verification the host
        # could not read): the reason is not remembered, so the card goes back to what the file says.
        self.start_host([sys.executable, "-c", "import sys; sys.exit(3)"])
        self.bootstrap()
        self.assertEqual(self.post()[1]["status"], "generating")
        done = self.await_status(lambda value: value != "generating")
        self.assertEqual(done["status"], "none")
        self.plant(self.document())
        self.assertEqual(self.post()[1]["status"], "generating")
        self.assertEqual(self.await_status(lambda value: value != "generating")["status"], "generated")

    def test_a_status_for_another_version_is_its_own(self):
        self.bootstrap()
        self.post()
        self.await_status(lambda value: value != "generating")
        self.assertEqual(self.get(TITLE, "01.000.000")[1]["status"], "none")
        self.assertEqual(self.get(TITLE, VERSION)[1]["status"], "generated")


class GateTests(Base):
    def test_capabilities_advertise_the_two_flags(self):
        flags = self.bootstrap()["capabilities"]
        self.assertIs(flags["runtime_profiles"], True)
        self.assertIs(flags["profile_generate"], True)
        self.assertIs(self.app.capabilities()["capabilities"]["runtime_profiles"], True)

    def test_a_session_is_needed_to_read_and_the_mutation_gate_to_generate(self):
        url = "/api/titles/%s/profile?content_version=%s" % (TITLE, VERSION)
        self.assertEqual(self.request("GET", url)[0], 401)
        self.assertEqual(self.request("POST", "/api/titles/%s/profile/generate" % TITLE, {})[0], 401)
        self.bootstrap()
        self.assertEqual(self.request("GET", url)[0], 200)
        path = "/api/titles/%s/profile/generate" % TITLE
        self.assertEqual(self.request("POST", path, {})[0], 403, "no origin, no token")
        self.assertEqual(self.request("POST", path, {}, {"Origin": self.origin})[0], 403, "no token")
        self.assertEqual(self.request("POST", path, {}, {"Origin": self.origin, "X-CSRF-Token": "wrong"})[0], 403)
        self.assertEqual(self.request("POST", path, {}, {"Origin": "https://unrelated.example", "X-CSRF-Token": self.csrf})[0], 403)
        self.assertEqual(self.request("GET", url, headers={"Host": "unrelated.example"})[0], 403)
        self.assertFalse(self.profiles_dir.exists(), "no refused request started anything")
        self.assertEqual(self.request("POST", path, {}, {"Origin": self.origin, "X-CSRF-Token": self.csrf})[0], 202)

    def test_only_the_exact_body_and_paths_are_accepted(self):
        self.bootstrap()
        for body in ({"x": 1}, {"content_version": VERSION}, [], "x"):
            self.assertEqual(self.post(body=body)[0], 400, body)
        headers = {"Origin": self.origin, "X-CSRF-Token": self.csrf}
        self.assertEqual(self.request("POST", "/api/titles/%s/profile/generate?x=1" % TITLE, {}, headers)[0], 404)
        self.assertEqual(self.request("POST", "/api/titles/%s/profile/generate/" % TITLE, {}, headers)[0], 404)
        self.assertEqual(self.request("POST", "/api/titles/%s/profile" % TITLE, {}, headers)[0], 404)
        self.assertEqual(self.request("POST", "/api/titles/%s/profile/generate" % ("A" * 65), {}, headers)[0], 404)
        self.assertEqual(self.request("POST", "/api/titles/../profile/generate", {}, headers)[0], 404)
        self.assertEqual(self.request("GET", "/api/titles/%s/profile/generate" % TITLE)[0], 404)
        self.assertFalse(self.profiles_dir.exists())

    def test_the_status_query_is_exactly_one_content_version(self):
        self.bootstrap()
        base = "/api/titles/%s/profile" % TITLE
        for query in ("", "?", "?content_version=", "?content_version=a&content_version=b", "?content_version=%s&x=1" % VERSION,
                      "?x=1", "?content_version=..", "?content_version=a%2Fb", "?content_version=a/b", "?content_version=%20", "?CONTENT_VERSION=1",
                      "?content_version=" + "9" * 65, "?content_version=%ff"):
            status, body = self.request("GET", base + query)
            self.assertEqual(status, 400, query)
            self.assertEqual(json.loads(body)["error"]["code"], "invalid_input", query)
        self.assertEqual(self.request("GET", base + "?content_version=" + VERSION)[0], 200)
        self.assertEqual(self.request("GET", "/api/titles/%s/profile?content_version=%s#x" % (TITLE, VERSION))[0], 400, "no fragment")
        self.assertEqual(self.request("GET", "/api/titles/a%2Fb/profile?content_version=1")[0], 404)
        self.assertEqual(self.request("GET", "/api/titles/%s/profile/extra?content_version=1" % TITLE)[0], 404)

    def test_other_routes_still_refuse_a_query_string(self):
        self.bootstrap()
        self.assertEqual(self.request("GET", "/api/account?content_version=1")[0], 404)
        self.assertEqual(self.request("GET", "/api/jobs?content_version=1")[0], 404)

    def test_a_title_with_a_dot_or_dash_is_routable_and_judged_by_the_inventory(self):
        self.add_dump("Dotted", "a.b_c-d", "1.0", "Dotted", tiny_elf())
        self.app.detected_titles(refresh=True)
        self.bootstrap()
        self.assertEqual(self.get("a.b_c-d", "1.0")[1]["status"], "none")

    def test_no_wire_reply_carries_a_path_or_generator_text(self):
        self.bootstrap()
        wire = json.dumps([self.post(), self.get(), self.get("PPSA99999"), self.post("PPSA99999")])
        self.await_status(lambda value: value != "generating")
        wire += json.dumps([self.get(), self.post()])
        for secret in (str(self.root), str(self.tree.root), "eboot", "/Users"):
            self.assertNotIn(secret, wire)


class OptionTests(unittest.TestCase):
    def test_runtime_profiles_option_and_default(self):
        private = tempfile.TemporaryDirectory()
        self.addCleanup(private.cleanup)
        root = Path(private.name).resolve()
        app = host.BridgeApplication(root / "a" / "c.json", root / "a" / "q", runtime_profiles=root / "profiles-here")
        self.addCleanup(app.close)
        self.assertEqual(app.profile_store.root, root / "profiles-here")
        self.assertFalse((root / "profiles-here").exists(), "created on first write, not at start")
        plain = host.BridgeApplication(root / "b" / "c.json", root / "b" / "q")
        self.addCleanup(plain.close)
        self.assertEqual(plain.profile_store.root, PROJECT / "profiles")
        with self.assertRaises(ValueError):
            host.BridgeApplication(root / "c" / "c.json", root / "c" / "q", generator_command=[])
        with self.assertRaises(ValueError):
            host.BridgeApplication(root / "d" / "c.json", root / "d" / "q", generator_timeout=0)

    def test_cli_passes_the_option_to_the_application(self):
        private = tempfile.TemporaryDirectory()
        self.addCleanup(private.cleanup)
        root = Path(private.name).resolve()
        import importlib
        cli = importlib.import_module("host")
        seen = []

        class Quiet:
            def serve_forever(self):
                raise KeyboardInterrupt()

            def server_close(self):
                pass

        def fake_server(application, port=0):
            seen.append(application.profile_store.root)
            return Quiet()

        for number, (arguments, expected) in enumerate(((["--runtime-profiles", str(root / "mine")], root / "mine"), ([], PROJECT / "profiles"))):
            argv = ["host.py", "--store", str(root / ("c%d.json" % number)), "--quarantine", str(root / ("q%d" % number)), "--port", "0", *arguments]
            with mock.patch.object(cli, "create_server", fake_server), mock.patch.object(sys, "argv", argv), \
                    mock.patch.object(sys, "stdout", io.StringIO()):
                self.assertEqual(cli.main(), 0)
            self.assertEqual(seen[-1], expected)
        self.assertFalse((root / "mine").exists(), "created on first write")


class ProviderLocateTests(unittest.TestCase):
    """TitlesRootProvider.locate_dump: where a dump title's folder is, by the rules that built the inventory (never on the wire)."""

    def test_the_winning_dump_folder_of_a_title_and_nothing_for_any_other_source(self):
        tree = Tree(self)
        tree.dump("A folder PPSA00001", param("PPSA00001", "1.0", "A"))
        tree.dump("Older PPSA00002", param("PPSA00002", "1.0", "B"))
        tree.dump("Newer PPSA00002", param("PPSA00002", "2.0", "B"))
        tree.appmeta(tree.user, "PPSA00003", param("PPSA00003", "1.0", "Cached only"))
        tree.appmeta(tree.system, "PPSA00001", param("PPSA00001", "9.9", "Cached too"))
        real = tree.root / "elsewhere"
        tree.put(real / "sce_sys" / "param.json", param("PPSA00004", "1.0", "Linked"))
        os.symlink(real, tree.homebrew / "Linked PPSA00004")
        provider = tree.provider()
        self.assertEqual(provider.locate_dump("PPSA00001"), (str(tree.homebrew / "A folder PPSA00001"), "1.0"))
        self.assertEqual(provider.locate_dump("PPSA00002"), (str(tree.homebrew / "Newer PPSA00002"), "2.0"), "the inventory's winner")
        for title in ("PPSA00003", "PPSA00004", "PPSA99999", "ppsa00001"):
            self.assertIsNone(provider.locate_dump(title), title)
        self.assertIsNone(Tree(self).provider().locate_dump("PPSA00001"), "a root with no dump folder")


class ValidatorTests(unittest.TestCase):
    """The pure validator: the shapes a profile file may have."""

    def test_counting_follows_the_envelopes_not_the_layout(self):
        document = build_profile(TITLE, VERSION, "a" * 64, high=12, medium=1, low=4)
        facts = profiles.validate_profile(json.dumps(document).encode(), TITLE, VERSION)
        self.assertEqual(facts["confidence"], {"high": 12, "medium": 1, "low": 4})
        self.assertEqual(facts["unresolved"], 2)
        self.assertEqual(facts["exe_sha256"], "a" * 64)

    def test_the_binding_may_be_plain_as_in_the_contract_or_envelopes_as_the_generator_writes_it(self):
        for plain in (True, False):
            document = build_profile(TITLE, VERSION, "b" * 64, high=9, medium=0, low=0, plain_binding=plain)
            facts = profiles.validate_profile(json.dumps(document).encode(), TITLE, VERSION)
            self.assertEqual((facts["exe_sha256"], facts["confidence"]["high"]), ("b" * 64, 9), plain)
        document = build_profile(TITLE, VERSION, "b" * 64, plain_binding=True)
        document["binding"]["executable"]["sha256"] = 5
        with self.assertRaises(profiles.ProfileError):
            profiles.validate_profile(json.dumps(document).encode(), TITLE, VERSION)
        document["binding"]["executable"]["sha256"] = {"value": "short", "confidence": "high", "method": "x", "evidence": []}
        with self.assertRaises(profiles.ProfileError):
            profiles.validate_profile(json.dumps(document).encode(), TITLE, VERSION)

    def test_an_envelope_is_counted_whole_and_not_looked_into(self):
        document = build_profile(TITLE, VERSION, "a" * 64, high=8, medium=0, low=0)
        document["symbols"]["nested"] = {"label": {"value": {"confidence": "high", "value": "1"}, "confidence": "low", "method": "x", "evidence": []}}
        facts = profiles.validate_profile(json.dumps(document).encode(), TITLE, VERSION)
        self.assertEqual(facts["confidence"], {"high": 8, "medium": 0, "low": 1})

    def test_deeply_nested_input_is_refused_not_a_crash(self):
        text = '{"schema":"x","a":' + "[" * 100000 + "]" * 100000 + "}"
        with self.assertRaises(profiles.ProfileError) as caught:
            profiles.validate_profile(text.encode(), TITLE, VERSION)
        self.assertEqual(caught.exception.reason, "profile_invalid")
        document = build_profile(TITLE, VERSION, "a" * 64)
        nest = []
        for _ in range(200):
            nest = [nest]
        document["symbols"]["deep"] = {"x": nest}
        with self.assertRaises(profiles.ProfileError) as caught:
            profiles.validate_profile(json.dumps(document).encode(), TITLE, VERSION)
        self.assertEqual(caught.exception.reason, "profile_invalid")

    def test_every_reason_code_is_in_the_documented_set(self):
        self.assertEqual(sorted(profiles.FAILURE_REASONS), sorted([
            "no_executable", "executable_encrypted", "no_version", "storage_unavailable", "generator_unavailable", "generator_failed",
            "generator_timeout", "profile_unreadable", "profile_invalid", "profile_mismatch", "status_inconsistent",
            "verified_profile_kept"]))
        self.assertEqual(sorted(profiles.STALE_REASONS), ["digest_mismatch", "executable_mismatch", "executable_unavailable"])


@unittest.skipUnless((PROJECT / "tools" / "profilegen" / "__main__.py").is_file() or (PROJECT / "tools" / "profilegen" / "cli.py").is_file(),
                     "the real generator (tools/profilegen) is not in the tree yet")
class RealGeneratorTests(unittest.TestCase):
    """The real generator on the real Townfall executable, read only. Skipped unless it exists and the executable is at hand."""

    EXECUTABLE = os.environ.get("MODBRIDGE_TOWNFALL_EBOOT") or (
        "/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-observations/"
        "townfall-static-executable-2026-10-02/decrypted-eboot.bin")
    PARAM = "/WORKSPACE/ps5-claude/outputs/2026-10-04-zcode-team/worker-game-version/param.json"

    def test_generate_then_read_through_the_host(self):
        if not (os.path.isfile(self.EXECUTABLE) and os.path.isfile(self.PARAM)):
            self.skipTest("the Townfall executable or its param.json is not on this machine")
        tree = Tree(self)
        private = tempfile.TemporaryDirectory()
        self.addCleanup(private.cleanup)
        root = Path(private.name).resolve()
        dump = tree.homebrew / "Silent Hill Townfall PPSA33286"
        (dump / "decrypted").mkdir(parents=True)
        # param.json is copied (a few KB; the inventory refuses a linked one); the executable is only linked, never copied.
        tree.put(dump / "sce_sys" / "param.json", Path(self.PARAM).read_bytes())
        os.symlink(self.EXECUTABLE, dump / "decrypted" / "eboot.bin")
        app = host.BridgeApplication(root / "private" / "credentials.json", root / "quarantine", account=FakeAccount(),
                                     titles_root=tree.root, runtime_profiles=root / "profiles")
        self.addCleanup(app.close)
        app.detected_titles()
        version = json.loads(Path(self.PARAM).read_text())["contentVersion"]
        self.assertEqual(app.runtime_profile("PPSA33286", version)["status"], "none")
        self.assertEqual(app.generate_profile("PPSA33286")["status"], "generating")
        deadline = time.monotonic() + 590
        record = app.runtime_profile("PPSA33286", version)
        while record["status"] == "generating" and time.monotonic() < deadline:
            time.sleep(0.5)
            record = app.runtime_profile("PPSA33286", version)
        self.assertEqual((record["status"], record["reason"]), ("generated", None), record)
        self.assertEqual(set(record["confidence"]), {"high", "medium", "low"})
        self.assertGreater(sum(record["confidence"].values()), 0)
        document = json.loads((root / "profiles" / "PPSA33286" / version / "runtime-profile.json").read_text())
        self.assertEqual(record["unresolved"], len(document["unresolved"]))
        self.assertEqual(record["profile_id"], document["profile_id"])
        try:
            sys.path.append(str(PROJECT))
            from tools.profilegen import profile as generator_profile
        except ImportError:
            generator_profile = None
        if generator_profile is not None:  # the generator's own count of its values is the host's count
            self.assertEqual(record["confidence"], generator_profile.confidence_counts(document))
        self.assertIsNone(document["verification"])
        self.assertEqual(document["status"], "generated")
        self.assertEqual(bound_sha(document), hashlib.sha256(Path(self.EXECUTABLE).read_bytes()).hexdigest())


if __name__ == "__main__":
    unittest.main()
