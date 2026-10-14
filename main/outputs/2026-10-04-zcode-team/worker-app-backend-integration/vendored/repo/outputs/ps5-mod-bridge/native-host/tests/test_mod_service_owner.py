"""Offline owner lifecycle checks against the backend's exact C ABI."""
import pathlib
import os
import socket
import subprocess
import tempfile
import time
import unittest
from build_fixture import compile_fixture

ROOT = pathlib.Path(__file__).resolve().parents[1]


class OwnerTests(unittest.TestCase):
    def test_owner_validates_before_publishing_and_cleans_up(self):
        with tempfile.TemporaryDirectory(prefix='mod-service-owner-') as tmp:
            binary = pathlib.Path(tmp) / 'owner-tests'
            subprocess.run([
                'c++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                '-I', str(ROOT), '-I', str(ROOT.parent / 'third_party'),
                str(ROOT / 'mod_service/owner.cpp'),
                str(ROOT / 'tests/mod_service_owner_tests.cpp'), '-o', str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)


class HostOwnerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='mod-service-host-')
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.output = pathlib.Path(cls.temporary.name)
        sources = [ROOT / name for name in ('host.c', 'http.c', 'entropy.c', 'drbg.c',
            'tests/web_assets_stub.c', 'tests/failed_bridge.c', 'tests/mod_service_host_backend.c')]
        common = ['-DBUILD_ID="owned-service-test"',
                  '-DMB_HOST_MOD_SERVICE_CONFIG_JSON="{\\"fixture\\":true}"']
        cls.enabled = compile_fixture(cls.output / 'enabled', sources,
            [ROOT / 'mod_service/owner.cpp'], [*common, '-DMB_HOST_MOD_SERVICE=1'])
        cls.disabled = compile_fixture(cls.output / 'disabled', sources,
            [ROOT / 'mod_service/owner.cpp'], common)

    def setUp(self):
        self.trace = self.output / (self.id().rsplit('.', 1)[-1] + '.trace')
        self.env = {**os.environ, 'MB_TEST_SERVICE_TRACE': str(self.trace), 'MB_TEST_SERVICE_MODE': 'ok'}

    def events(self):
        return self.trace.read_text().splitlines() if self.trace.exists() else []

    def command(self, binary, port, lifetime=5):
        return [str(binary), '--port', str(port), '--lifetime', str(lifetime)]

    def port(self):
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            return reservation.getsockname()[1]

    def start(self, binary):
        port = self.port()
        process = subprocess.Popen(self.command(binary, port), env=self.env,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        def stop():
            if process.poll() is None:
                process.terminate()
            process.communicate(timeout=3)
        self.addCleanup(stop)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            self.assertIsNone(process.poll(), 'host exited before listening')
            try:
                with socket.create_connection(('127.0.0.1', port), .1):
                    return process, port
            except OSError:
                time.sleep(.01)
        self.fail('host failed to listen')

    def shutdown(self, port):
        with socket.create_connection(('127.0.0.1', port), 3) as peer:
            peer.settimeout(3)
            peer.sendall((f'POST /shutdown HTTP/1.1\r\nHost: 127.0.0.1:{port}\r\n'
                          'Content-Length: 0\r\nX-Modbridge-Shutdown: 1\r\n\r\n').encode())
            response = b''
            while True:
                received = peer.recv(4096)
                if not received:
                    break
                response += received
            self.assertTrue(response.startswith(b'HTTP/1.1 200 '), response)

    def test_bridge_owned_until_http_shutdown(self):
        process, port = self.start(self.enabled)
        self.assertEqual(self.events(), ['open', 'describe', 'release_reply'])
        self.shutdown(port)
        self.assertEqual(process.wait(timeout=3), 0)
        self.assertEqual(self.events(), ['open', 'describe', 'release_reply', 'close'])

    def test_default_build_never_opens_backend(self):
        process, port = self.start(self.disabled)
        self.assertEqual(self.events(), [])
        self.shutdown(port)
        self.assertEqual(process.wait(timeout=3), 0)
        self.assertEqual(self.events(), [])

    def test_configured_startup_failures_close_before_exit(self):
        for mode, events, message in [
            ('open_fail', ['open', 'release_error', 'close'], b'backend open failed'),
            ('bad_contract', ['open', 'describe', 'release_reply', 'close'], b'unsupported contract'),
        ]:
            with self.subTest(mode=mode):
                if self.trace.exists():
                    self.trace.unlink()
                self.env['MB_TEST_SERVICE_MODE'] = mode
                port = self.port()
                result = subprocess.run(self.command(self.enabled, port), env=self.env,
                    capture_output=True, timeout=3)
                self.assertEqual(result.returncode, 5)
                self.assertIn(message, result.stderr)
                self.assertEqual(self.events(), events)
                with socket.socket() as reservation:
                    reservation.bind(('127.0.0.1', port))

    def test_listener_bind_failure_closes_started_backend(self):
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            reservation.listen(1)
            result = subprocess.run(self.command(self.enabled, reservation.getsockname()[1]),
                env=self.env, capture_output=True, timeout=3)
        self.assertEqual(result.returncode, 3)
        self.assertEqual(self.events(), ['open', 'describe', 'release_reply', 'close'])


if __name__ == '__main__':
    unittest.main()
