"""Exercise the real C server, including its owned subprocess lifecycle."""
import hashlib
import json
import pathlib
import socket
import subprocess
import tempfile
import time
import unittest
from build_fixture import compile_fixture, core_sources

ROOT = pathlib.Path(__file__).resolve().parents[1]

class HostTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='native-host-diagnostics-')
        cls.addClassCleanup(cls.temporary.cleanup)
        sources = [ROOT / 'host.c', ROOT / 'http.c', ROOT / 'entropy.c', ROOT / 'drbg.c',
                   ROOT / 'tests/web_assets_stub.c']
        cls.available = all(p.exists() for p in sources)
        if cls.available:
            cls.binary = compile_fixture(pathlib.Path(cls.temporary.name) / 'desktop-host', sources, core_sources(),
                            ['-DBUILD_ID="' + hashlib.sha256(b''.join(p.read_bytes() for p in sources)).hexdigest() + '"'])

    def setUp(self):
        self.assertTrue(self.available, 'native host fixed HTTP implementation is missing')
        self.proc = None
        self.start()

    def start(self, port=None, lifetime=20):
        if port is None:
            with socket.socket() as reserve:
                reserve.bind(('127.0.0.1', 0)); port = reserve.getsockname()[1]
        self.port = port
        self.authority = f'127.0.0.1:{port}'
        self.proc = subprocess.Popen([str(self.binary), '--port', str(port), '--lifetime', str(lifetime)],
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if self.proc.poll() is not None:
                self.fail(f'host stopped before readiness: {self.proc.communicate()}')
            try:
                with socket.create_connection(('127.0.0.1', port), .1): return
            except OSError: time.sleep(.01)
        self.fail('host did not listen')

    def tearDown(self):
        if self.proc:
            if self.proc.poll() is None:
                self.proc.terminate()
            self.proc.communicate(timeout=4)

    def exchange(self, raw, fragments=None):
        with socket.create_connection(('127.0.0.1', self.port), 3) as peer:
            peer.settimeout(4)
            if fragments:
                for part in fragments:
                    peer.sendall(part); time.sleep(.02)
            else: peer.sendall(raw)
            data = b''
            while True:
                part = peer.recv(16384)
                if not part: break
                data += part
            header, body = data.split(b'\r\n\r\n', 1)
            lines = header.decode('ascii').split('\r\n')
            fields = dict(line.split(': ', 1) for line in lines[1:])
            self.assertEqual(int(fields['Content-Length']), len(body))
            self.assertEqual(fields['Connection'], 'close')
            self.assertEqual(fields['Cache-Control'], 'no-store')
            self.assertEqual(fields['X-Content-Type-Options'], 'nosniff')
            self.assertNotIn('Access-Control-Allow-Origin', fields)
            return int(lines[0].split()[1]), body, fields

    def req(self, method='GET', target='/report', extras=''):
        return f'{method} {target} HTTP/1.1\r\nHost: {self.authority}\r\n{extras}\r\n'.encode()

    def assert_rejected(self, raw, status=400):
        code, body, _ = self.exchange(raw)
        self.assertEqual(code, status)
        self.assertIn('error', json.loads(body))
        self.assertIsNone(self.proc.poll())
        self.assertEqual(self.exchange(self.req())[0], 200)

    def test_report_is_desktop_observation_and_install_is_unavailable(self):
        code, body, _ = self.exchange(self.req())
        self.assertEqual(code, 200)
        report = json.loads(body)
        self.assertEqual(report['schema_version'], 1)
        self.assertEqual(report['origin'], 'desktop_fixture')
        self.assertEqual(report['phase'], 'main_http_ready')
        self.assertFalse(report['installation_supported'])
        self.assertEqual(report['observations']['getpid']['value'], self.proc.pid)
        self.assertGreater(report['observations']['page_size']['value'], 0)
        self.assertEqual(report['firmware']['status'], 'unavailable')
        self.assertEqual(len(report['build_id']), 64)

    def test_cached_core_route_equals_additive_report_object(self):
        code, body, _ = self.exchange(self.req(target='/core'))
        self.assertEqual(code, 200)
        core = json.loads(body)
        self.assertEqual(core, json.loads(self.exchange(self.req())[1])['core'])
        self.assertEqual(core['status'], 'passed')
        self.assertFalse(core['installation_supported'])
        self.assertEqual(body, self.exchange(self.req(target='/core'))[1])

    def test_entropy_is_cached_metadata_only_and_unavailable_on_desktop(self):
        report = json.loads(self.exchange(self.req())[1])
        self.assertIn('entropy', report['observations'])
        entropy = report['observations']['entropy']
        self.assertEqual(entropy, {
            'status': 'unavailable', 'reason': 'desktop_fixture',
            'api': 'sysctl(KERN_ARND)', 'scope': 'api_result_only',
            'requested_bytes': 32, 'call_count': 0,
            'return_code': None, 'errno': None, 'returned_length_bytes': None,
            'elapsed_ms': None, 'hard_deadline_enforced': False,
        })
        self.assertEqual(entropy, json.loads(self.exchange(self.req())[1])['observations']['entropy'])
        self.assertFalse(report['installation_supported'])

    def test_drbg_is_cached_metadata_and_normal_desktop_runs_no_library_operations(self):
        first = self.exchange(self.req())[1]
        self.assertEqual(first, self.exchange(self.req())[1])
        self.assertLess(len(first), 8192)
        report = json.loads(first)
        self.assertEqual(report['observations']['drbg'], {
            'status': 'unavailable', 'reason': 'desktop_fixture',
            'scope': 'owned_startup_checkpoint', 'mbedtls_version': None,
            'config_id': None, 'requested_generation_bytes': 32,
            'add_source_rc': None, 'seed_rc': None, 'generate_rc': None,
            'completed_length_bytes': None, 'source_callback_count': 0,
            'source_os_call_count': 0, 'total_api_call_count': 0,
            'api_call_limit': 4, 'elapsed_ms': None, 'hard_deadline_enforced': False,
            'source_failure': None,
        })
        self.assertEqual(set(report['core']['checks']), {
            'sha256_known_vector', 'allocator_roundtrip', 'exception_raii', 'thread_local_single_thread',
            'dds_legacy_roundtrip', 'dds_cube_mip_roundtrip', 'dds_truncation_detected'})
        self.assertEqual(report['firmware']['reason'], 'not_queried')
        self.assertEqual(report['game']['reason'], 'not_queried')

    def test_failed_bridge_leaves_host_responsive_with_unavailable_noninstallable_core(self):
        import tempfile
        with tempfile.TemporaryDirectory() as temporary:
            failure = compile_fixture(pathlib.Path(temporary) / 'failed-host',
                                      [ROOT / 'host.c', ROOT / 'http.c', ROOT / 'entropy.c', ROOT / 'drbg.c',
                                       ROOT / 'tests/web_assets_stub.c', ROOT / 'tests/failed_bridge.c'],
                                      defines=['-DBUILD_ID="' + 'f' * 64 + '"'])
            self.proc.terminate(); self.proc.communicate()
            self.binary = failure
            self.start()
            code, body, _ = self.exchange(self.req(target='/core'))
            self.assertEqual(code, 200)
            core = json.loads(body)
            self.assertEqual(core['status'], 'unavailable')
            self.assertFalse(core['installation_supported'])
            report = json.loads(self.exchange(self.req())[1])
            self.assertEqual(core, report['core'])
            self.assertFalse(report['installation_supported'])
            self.assertEqual(self.exchange(self.req('POST', '/shutdown', 'Content-Length: 0\r\nX-Modbridge-Shutdown: 1\r\n'))[0], 200)
            self.assertEqual(self.proc.wait(timeout=3), 0)

    def test_version_matches_report(self):
        version = json.loads(self.exchange(self.req(target='/version'))[1])
        report = json.loads(self.exchange(self.req())[1])
        for key in ('version', 'build_id', 'profile', 'origin'):
            self.assertEqual(version[key], report[key])
        self.assertEqual(version['version'], '0.1.0')

    def test_page_has_accessible_owned_shutdown_and_restrictive_csp(self):
        code, body, fields = self.exchange(self.req(target='/'))
        self.assertEqual(code, 200)
        self.assertIn(b'<button', body)
        self.assertIn(b'X-Modbridge-Shutdown', body)
        self.assertIn("default-src 'none'", fields['Content-Security-Policy'])
        self.assertIn("frame-ancestors 'none'", fields['Content-Security-Policy'])

    def test_fragmented_header(self):
        raw = self.req()
        self.assertEqual(self.exchange(None, [raw[:6], raw[6:35], raw[35:]])[0], 200)

    def test_host_and_framing_rejections(self):
        bad = [self.req().replace(self.authority.encode(), b'example.org'),
               b'GET / HTTP/1.1\r\n\r\n',
               self.req(extras=f'Host: {self.authority}\r\n'),
               self.req(extras='Content-Length: 0\r\nContent-Length: 0\r\n'),
               self.req(extras='Content-Length: -1\r\n'),
               self.req(extras='Content-Length: 1\r\n'),
               self.req(extras='Transfer-Encoding: chunked\r\n'),
               self.req(extras='Bad Header: value\r\n'),
               self.req(extras=' Folded: value\r\n'),
               self.req(extras='X-Test: a\x00b\r\n'),
               self.req() + b'x',
               self.req() + self.req(),
               self.req().replace(b'HTTP/1.1', b'HTTP/2.0'),
               self.req().replace(b'\r\n', b'\n') + b'\r\n\r\n']
        for raw in bad:
            with self.subTest(raw=raw): self.assert_rejected(raw)

    def test_wrong_route_and_method(self):
        self.assert_rejected(self.req(target='/report?x=1'), 404)
        self.assert_rejected(self.req(method='PUT'), 405)
        self.assert_rejected(self.req(target='/shutdown'), 405)

    def test_shutdown_gates_preserve_process(self):
        cases = [self.req('POST', '/shutdown'),
                 self.req('POST', '/shutdown', 'Content-Length: 0\r\n'),
                 self.req('POST', '/shutdown', 'Content-Length: 0\r\nX-Modbridge-Shutdown: 0\r\n'),
                 self.req('POST', '/shutdown', 'Content-Length: 0\r\nX-Modbridge-Shutdown: 1\r\nOrigin: http://evil.example\r\n'),
                 self.req('POST', '/shutdown', f'Content-Length: 0\r\nX-Modbridge-Shutdown: 1\r\nOrigin: http://{self.authority}\r\nOrigin: http://{self.authority}\r\n')]
        for raw in cases:
            with self.subTest(raw=raw):
                code, body, _ = self.exchange(raw)
                self.assertIn(code, (400, 403))
                self.assertIsNone(self.proc.poll())

    def test_shutdown_exits_and_reuses_owned_port(self):
        raw = self.req('POST', '/shutdown', f'Content-Length: 0\r\nX-Modbridge-Shutdown: 1\r\nOrigin: http://{self.authority}\r\n')
        self.assertEqual(self.exchange(raw)[0], 200)
        self.assertEqual(self.proc.wait(timeout=3), 0)
        self.proc.communicate()
        self.start(self.port)
        self.assertEqual(self.exchange(self.req())[0], 200)

    def test_incomplete_and_oversized_peers_do_not_stall_server(self):
        began = time.monotonic()
        with socket.create_connection(('127.0.0.1', self.port), 3) as peer:
            peer.sendall(b'GET / HTTP/1.1\r\nHost: ')
            peer.settimeout(3)
            self.assertTrue(peer.recv(4096).startswith(b'HTTP/1.1 408'))
        self.assertLess(time.monotonic() - began, 2.8)
        code, _, _ = self.exchange(b'GET / HTTP/1.1\r\nX: ' + b'a' * 4096)
        self.assertEqual(code, 431)
        self.assertEqual(self.exchange(self.req())[0], 200)

    def test_abandoned_peer_and_broken_receiver(self):
        with socket.create_connection(('127.0.0.1', self.port), 3) as peer:
            peer.sendall(b'GET / HTTP/1.1\r\n')
        with socket.create_connection(('127.0.0.1', self.port), 3) as peer:
            peer.sendall(self.req(target='/'))
        self.assertEqual(self.exchange(self.req())[0], 200)

    def test_lifetime_expires_with_exit_status(self):
        self.proc.terminate(); self.proc.communicate()
        self.start(self.port, lifetime=1)
        self.assertEqual(self.proc.wait(timeout=3), 0)

    def test_bind_failure_preserves_original_listener(self):
        other = subprocess.run([str(self.binary), '--port', str(self.port), '--lifetime', '1'],
                               capture_output=True, timeout=3)
        self.assertEqual(other.returncode, 3)
        self.assertIsNone(self.proc.poll())
        self.assertEqual(self.exchange(self.req())[0], 200)

if __name__ == '__main__': unittest.main()
