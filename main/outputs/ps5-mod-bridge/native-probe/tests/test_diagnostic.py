"""Owned development fixtures; the ELF is data and is never executed."""
import copy
import hashlib
import json
import pathlib
import socket
import struct
import sys
import tempfile
import threading
import time
import unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
try:
    import diagnostic
except ModuleNotFoundError:
    diagnostic = None

ARTIFACT = ROOT.parents[1] / "native-probe-artifacts/native-probe.elf"
ARTIFACT_HASH = "33fba54b4e18d9467a3b295aca080a411729758e12ca91f6015c9dd74dd3be3f"


def owned_report():
    return {
        "schema_version": 1, "record_kind": "native_probe_report",
        "evidence_origin": "native_entry_unverified",
        "build": {"compiler": "owned fixture", "target": "x86_64-sie-ps5",
                  "sizeof_pointer": 8, "sizeof_long": 8, "sizeof_int": 4,
                  "byte_order": "little"},
        "candidate_entry_abi": "sdk-payload-args-v0.43",
        "environment": {"firmware_verified": None, "loader": None,
                        "loader_version": None, "game": None, "game_update": None},
        "observations": {
            "getpid": {"status": "observed", "value": 42, "resolve_code": 0},
            "getuid": {"status": "unavailable", "value": None, "resolve_code": 77},
            "geteuid": {"status": "observed", "value": 0, "resolve_code": 0},
            "getpagesize": {"status": "unavailable", "value": -1, "resolve_code": 0}},
        "output_resolve_code": 0,
        "capabilities": [
            {"id": name, "context": context, "status": "unknown", "evidence": None}
            for name, context in (
                ("host.payload", "standalone"), ("filesystem.scratch", "standalone"),
                ("network.tls", "standalone"), ("ui.controller_display", "standalone"),
                ("host.native_module", "game_process"), ("runtime.lua", "game_process"),
                ("runtime.ue4ss", "game_process"), ("runtime.windows", "game_process"),
                ("translation.unreal_pak", "preparation"),
                ("translation.unreal_asset", "preparation"))]}


def encode_report(record):
    return json.dumps(record, separators=(",", ":")).encode() + b"\n"


class GateTests(unittest.TestCase):
    def setUp(self):
        self.assertIsNotNone(diagnostic, "diagnostic gates are not implemented")

    def inspect(self, path, data):
        return diagnostic.inspect_artifact(
            path, expected_sha256=hashlib.sha256(data).hexdigest(), expected_size=len(data))

    def test_saved_artifact_is_checked_and_snapshot_survives_later_change(self):
        artifact = diagnostic.inspect_artifact(
            ARTIFACT, expected_sha256=ARTIFACT_HASH, expected_size=50848)
        self.assertEqual((artifact.size, artifact.extent), (50848, 50848))
        self.assertEqual(artifact.sha256, ARTIFACT_HASH)
        self.assertEqual(artifact.prefix_sha256, ARTIFACT_HASH)
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "candidate.elf"
            path.write_bytes(artifact.data)
            snapshot = self.inspect(path, artifact.data)
            path.write_bytes(b"changed")
            self.assertEqual(snapshot.data, artifact.data)

    def test_trailer_is_excluded_from_loader_prefix(self):
        original = ARTIFACT.read_bytes()
        data = original + b"THIS MUST NOT BECOME STDIN"
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "candidate.elf"
            path.write_bytes(data)
            artifact = self.inspect(path, data)
            self.assertEqual(artifact.extent, 50848)
            self.assertEqual(artifact.prefix_sha256, ARTIFACT_HASH)
            self.assertNotEqual(artifact.sha256, artifact.prefix_sha256)

    def test_size_hash_symlink_and_nonregular_inputs_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "candidate.elf"
            path.write_bytes(ARTIFACT.read_bytes())
            link = pathlib.Path(directory) / "link.elf"
            link.symlink_to(path)
            for candidate, digest, size in (
                    (path, "0" * 64, 50848), (path, ARTIFACT_HASH, 50847),
                    (path, ARTIFACT_HASH.upper(), 50848), (path, ARTIFACT_HASH, True),
                    (link, ARTIFACT_HASH, 50848),
                    (pathlib.Path(directory), ARTIFACT_HASH, 50848)):
                with self.subTest(candidate=candidate, digest=digest, size=size):
                    with self.assertRaises((ValueError, OSError)):
                        diagnostic.inspect_artifact(candidate, expected_sha256=digest,
                                                    expected_size=size)

    def test_invalid_elf_and_over_bound_artifacts_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "candidate.elf"
            for data in (b"not an ELF", b"x" * (1048576 + 1)):
                path.write_bytes(data)
                with self.assertRaises(ValueError):
                    self.inspect(path, data)

    def test_load_segment_outside_section_derived_extent_is_rejected(self):
        data = bytearray(ARTIFACT.read_bytes())
        # The third LOAD starts at 49152. Extend it past the table ending at 50848.
        header = struct.unpack_from("<16sHHIQQQIHHHHHH", data)
        phoff, phsize, phnum = header[5], header[9], header[10]
        for i in range(phnum):
            at = phoff + phsize * i
            kind, flags, offset = struct.unpack_from("<IIQ", data, at)
            if kind == 1 and offset == 49152:
                struct.pack_into("<QQ", data, at + 32, 2048, 2048)
                break
        else:
            self.fail("owned fixture's third LOAD missing")
        data.extend(b"\0" * 1024)
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "candidate.elf"
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                self.inspect(path, bytes(data))

    def test_native_schema_preserves_unavailable_values_without_capability_claims(self):
        record = diagnostic.parse_report(encode_report(owned_report()))
        self.assertEqual(record["observations"]["getpid"]["value"], 42)
        self.assertEqual(record["observations"]["getpagesize"]["value"], -1)
        self.assertIsNone(record["environment"]["firmware_verified"])
        self.assertTrue(all(item["status"] == "unknown" for item in record["capabilities"]))

    def test_report_rejects_encoding_framing_duplicates_and_nonfinite_values(self):
        valid = encode_report(owned_report())
        invalid = (b"\xff\n", valid[:-1], valid + b"{}\n", b"x" * 4097,
                   valid.replace(b'"schema_version":1', b'"schema_version":1,"schema_version":1'),
                   valid.replace(b'"output_resolve_code":0', b'"output_resolve_code":NaN'),
                   b"[" * 1500 + b"0" + b"]" * 1500 + b"\n")
        for data in invalid:
            with self.subTest(data=data[:80]):
                with self.assertRaises(ValueError):
                    diagnostic.parse_report(data)

    def test_report_rejects_unearned_identity_types_and_capabilities(self):
        changes = (
            (("evidence_origin",), "desktop_fixture"), (("schema_version",), True),
            (("schema_version",), 2), (("extra",), 0),
            (("environment", "firmware_verified"), "13.60"),
            (("build", "target"), "desktop_test"),
            (("build", "sizeof_int"), True), (("build", "compiler"), "x\n"),
            (("build", "compiler"), "x" * 257),
            (("observations", "getpid", "value"), True),
            (("observations", "getpid", "value"), 0),
            (("observations", "getpid", "resolve_code"), 77),
            (("observations", "geteuid", "value"), -1),
            (("observations", "getuid", "value"), 5),
            (("observations", "getpagesize", "value"), -(1 << 63) - 1),
            (("output_resolve_code",), 1 << 31),
            (("candidate_entry_abi",), "other"),
            (("capabilities", 0, "status"), "supported"),
            (("capabilities", 0, "evidence"), "fixture"),
            (("capabilities", 0, "id"), "network.tls"),
            (("capabilities", 0, "context"), "game_process"))
        for keys, value in changes:
            record = copy.deepcopy(owned_report())
            target = record
            for key in keys[:-1]:
                target = target[key]
            target[keys[-1]] = value
            with self.subTest(keys=keys):
                with self.assertRaises(ValueError):
                    diagnostic.parse_report(encode_report(record))
        record = owned_report()
        record["observations"]["getpid"] = {
            "status": "unmeasured", "value": 42, "resolve_code": 0}
        with self.assertRaises(ValueError):
            diagnostic.parse_report(encode_report(record))


class OwnedPeer:
    """Test-only socket lifecycle; no ELF loading or process spawning."""
    def __init__(self, output=b"", *, hold=False, delay=0):
        self.output, self.hold, self.delay = output, hold, delay
        self.received = bytearray()
        self.half_closed = False
        self.stopped = threading.Event()
        self.server = socket.socket()
        self.server.bind(("127.0.0.1", 0))
        self.server.listen(1)
        self.server.settimeout(1)
        self.port = self.server.getsockname()[1]
        self.thread = threading.Thread(target=self.run, daemon=True)

    def run(self):
        try:
            connection, _ = self.server.accept()
            with connection:
                connection.settimeout(1)
                while True:
                    part = connection.recv(65536)
                    if not part:
                        self.half_closed = True
                        break
                    self.received.extend(part)
                for at in range(0, len(self.output), 127):
                    if self.stopped.wait(self.delay):
                        return
                    connection.sendall(self.output[at:at + 127])
                if self.hold:
                    self.stopped.wait(1)
        except (OSError, TimeoutError):
            pass  # Closing/timing out is an intentional fixture outcome.

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *_args):
        self.stopped.set()
        self.server.close()
        self.thread.join(2)


class TransportTests(unittest.TestCase):
    def setUp(self):
        self.assertTrue(callable(getattr(diagnostic, "send_loopback_diagnostic", None)),
                        "send_loopback_diagnostic is not implemented")

    def send(self, port, **kwargs):
        return diagnostic.send_loopback_diagnostic(
            "127.0.0.1", port, ARTIFACT, expected_sha256=ARTIFACT_HASH,
            expected_size=50848, **kwargs)

    def test_chunked_report_after_exact_prefix_and_half_close_stays_unverified(self):
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "candidate.elf"
            data = ARTIFACT.read_bytes() + b"not stdin"
            path.write_bytes(data)
            with OwnedPeer(encode_report(owned_report())) as peer:
                result = diagnostic.send_loopback_diagnostic(
                    "127.0.0.1", peer.port, path, expected_sha256=hashlib.sha256(data).hexdigest(),
                    expected_size=len(data), timeout_seconds=1)
        self.assertEqual(result.status, "report_received_unverified")
        self.assertTrue(result.send_complete)
        self.assertEqual(result.bytes_sent, 50848)
        self.assertEqual(result.prefix_size, 50848)
        self.assertEqual(result.prefix_sha256, ARTIFACT_HASH)
        self.assertEqual(result.artifact_size, 50857)
        self.assertEqual(bytes(peer.received), ARTIFACT.read_bytes())
        self.assertTrue(peer.half_closed)
        self.assertEqual(result.bytes_received, len(encode_report(owned_report())))
        self.assertIsNone(result.report["environment"]["loader"])

    def test_empty_malformed_and_overflow_responses_have_distinct_safe_results(self):
        for output, status in ((b"", "peer_closed_empty"), (b"hostile peer text\n", "response_invalid"),
                               (b"x" * 4097, "response_overflow")):
            with self.subTest(status=status):
                with OwnedPeer(output) as peer:
                    result = self.send(peer.port, timeout_seconds=1)
                self.assertEqual(result.status, status)
                self.assertTrue(result.send_complete)
                self.assertIsNone(result.report)
                self.assertLessEqual(result.bytes_received, 4097)

    def test_complete_line_without_eof_does_not_become_a_received_report(self):
        with OwnedPeer(encode_report(owned_report()), hold=True) as peer:
            result = self.send(peer.port, timeout_seconds=0.15)
        self.assertEqual(result.status, "response_timeout")
        self.assertTrue(result.send_complete)
        self.assertEqual(result.bytes_received, len(encode_report(owned_report())))
        self.assertIsNone(result.report)

    def test_progressing_peer_cannot_restart_response_deadline(self):
        start = time.monotonic()
        with OwnedPeer(encode_report(owned_report()), delay=0.04) as peer:
            result = self.send(peer.port, timeout_seconds=0.15)
        self.assertEqual(result.status, "response_timeout")
        self.assertGreater(result.bytes_received, 0)
        self.assertLess(time.monotonic() - start, 0.75)

    def test_closed_listener_is_not_execution_success(self):
        server = socket.socket()
        server.bind(("127.0.0.1", 0))
        port = server.getsockname()[1]
        server.close()
        result = self.send(port, timeout_seconds=0.2)
        self.assertEqual(result.status, "connect_failed")
        self.assertFalse(result.send_complete)
        self.assertEqual(result.bytes_sent, 0)

    def test_configuration_and_preflight_fail_before_socket_creation(self):
        cases = (("ps5-host-not-configured.invalid", 9021, 1), ("localhost", 9021, 1),
                 ("127.0.0.1", True, 1), ("127.0.0.1", 65536, 1),
                 ("127.0.0.1", 9021, True), ("127.0.0.1", 9021, float("nan")),
                 ("127.0.0.1", 9021, 61), ("127.0.0.1", 9021, 0))
        with mock.patch.object(diagnostic.socket, "socket", side_effect=AssertionError("socket opened")):
            for host, port, timeout in cases:
                with self.subTest(host=host, port=port, timeout=timeout):
                    result = diagnostic.send_loopback_diagnostic(
                        host, port, ARTIFACT, expected_sha256=ARTIFACT_HASH,
                        expected_size=50848, timeout_seconds=timeout)
                    self.assertEqual(result.status, "preflight_rejected")
                    self.assertIsNone(result.artifact_sha256)
            result = diagnostic.send_loopback_diagnostic(
                "127.0.0.1", 9021, ARTIFACT, expected_sha256="0" * 64, expected_size=50848)
            self.assertEqual(result.status, "preflight_rejected")

    def test_short_sends_use_snapshot_and_shrinking_budget(self):
        clock = [10.0]
        output = encode_report(owned_report())

        class ShortSocket:
            def __init__(self, *, fail=None, mutate=None):
                self.sent, self.timeouts = bytearray(), []
                self.closed, self.half_closed = False, False
                self.fail, self.mutate = fail, mutate
                self.receives = 0

            def settimeout(self, timeout):
                self.timeouts.append(timeout)

            def connect(self, _address):
                clock[0] += 0.1
                if self.mutate:
                    self.mutate()
                if self.fail == "connect":
                    raise socket.timeout()

            def send(self, data):
                clock[0] += 0.02
                if self.fail == "send":
                    return 0
                part = bytes(data[:8192])
                self.sent.extend(part)
                return len(part)

            def shutdown(self, how):
                self.half_closed = how == socket.SHUT_WR

            def recv(self, _count):
                clock[0] += 0.03
                if self.fail == "response":
                    raise ConnectionResetError()
                self.receives += 1
                return output if self.receives == 1 else b""

            def close(self):
                self.closed = True

        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "candidate.elf"
            path.write_bytes(ARTIFACT.read_bytes())
            stream = ShortSocket(mutate=lambda: path.write_bytes(b"changed"))
            with mock.patch.object(diagnostic.socket, "socket", return_value=stream), \
                    mock.patch.object(diagnostic.time, "monotonic", side_effect=lambda: clock[0]):
                result = diagnostic.send_loopback_diagnostic(
                    "127.0.0.1", 9021, path, expected_sha256=ARTIFACT_HASH,
                    expected_size=50848, timeout_seconds=1)
        self.assertEqual(result.status, "report_received_unverified")
        self.assertEqual(bytes(stream.sent), ARTIFACT.read_bytes())
        self.assertTrue(stream.closed and stream.half_closed)
        self.assertTrue(all(b <= a for a, b in zip(stream.timeouts, stream.timeouts[1:])))
        self.assertLess(stream.timeouts[-1], stream.timeouts[0])
        for phase, status in (("connect", "connect_failed"), ("send", "send_failed"),
                              ("response", "response_failed")):
            stream = ShortSocket(fail=phase)
            with mock.patch.object(diagnostic.socket, "socket", return_value=stream) as factory, \
                    mock.patch.object(diagnostic.time, "monotonic", side_effect=lambda: clock[0]):
                result = self.send(9021, timeout_seconds=1)
            self.assertEqual(result.status, status)
            self.assertEqual(factory.call_count, 1, "an ambiguous transfer must never retry")
            self.assertTrue(stream.closed)

    def test_eof_observed_after_deadline_cannot_accept_an_already_complete_report(self):
        clock = [0.0]
        output = encode_report(owned_report())

        class LateEofSocket:
            reads = 0
            def settimeout(self, _timeout):
                pass
            def connect(self, _address):
                pass
            def send(self, data):
                return len(data)
            def shutdown(self, _how):
                pass
            def recv(self, _count):
                self.reads += 1
                if self.reads == 1:
                    clock[0] = 0.05
                    return output
                clock[0] = 0.11
                return b""
            def close(self):
                pass

        with mock.patch.object(diagnostic.socket, "socket", return_value=LateEofSocket()), \
                mock.patch.object(diagnostic.time, "monotonic", side_effect=lambda: clock[0]):
            result = self.send(9021, timeout_seconds=0.1)
        self.assertEqual(result.status, "response_timeout")
        self.assertEqual(result.bytes_received, len(output))
        self.assertTrue(result.send_complete)
        self.assertIsNone(result.report)


if __name__ == "__main__":
    unittest.main()
