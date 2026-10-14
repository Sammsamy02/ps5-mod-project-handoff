"""Focused loopback proof for native embedded app routes and unavailable services."""
import hashlib
import json
import pathlib
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import unittest

from build_fixture import ROOT, compile_fixture

sys.path.insert(0, str(ROOT))
try:
    import web_assets
except ImportError:
    web_assets = None


def asset_fixture(directory):
    directory.mkdir(parents=True, exist_ok=True)
    index = (b'<!doctype html><html><head><link rel="stylesheet" href="/style.css">'
             b'</head><body><main id="app"></main><script src="/app.js"></script></body></html>')
    script = b'/* deterministic owned browser fixture: ' + b'x' * 12000 + b' */\n'
    style = b'body { color: #123456; }\n'
    for name, data in (('index.html', index), ('app.js', script), ('style.css', style)):
        (directory / name).write_bytes(data)
    return {'index.html': index, 'app.js': script, 'style.css': style}


class NativeAppAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix='native-app-assets-')
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.root = pathlib.Path(cls.temporary.name)
        cls.asset_dir = cls.root / 'source-web'
        cls.assets = asset_fixture(cls.asset_dir)
        if web_assets is None:
            raise AssertionError('web_assets.py is missing')
        c_source = cls.root / 'web_assets_generated.c'
        web_assets.write_c_assets(web_assets.inspect_assets(cls.asset_dir), c_source)
        build_id = hashlib.sha256(b'owned-native-app-assets-loopback-fixture').hexdigest()
        cls.binary = compile_fixture(cls.root / 'host',
            [ROOT / 'host.c', ROOT / 'http.c', ROOT / 'entropy.c', ROOT / 'drbg.c',
             ROOT / 'tests/failed_bridge.c', c_source],
            defines=['-DBUILD_ID="' + build_id + '"'])
        cls.caller = compile_fixture(cls.root / 'http-caller',
            [ROOT / 'http.c', ROOT / 'tests/app_http_caller.c', c_source])
        # The executable contains all response bytes; requests must not depend on this tree.
        shutil.rmtree(cls.asset_dir)

    def setUp(self):
        with socket.socket() as reserve:
            reserve.bind(('127.0.0.1', 0))
            self.port = reserve.getsockname()[1]
        self.authority = f'127.0.0.1:{self.port}'
        self.server = subprocess.Popen([str(self.binary), '--port', str(self.port), '--lifetime', '20'],
                                       stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            if self.server.poll() is not None:
                self.fail(f'fixture host failed to start: {self.server.communicate()}')
            try:
                with socket.create_connection(('127.0.0.1', self.port), .1):
                    break
            except OSError:
                time.sleep(.01)
        else:
            self.fail('fixture host did not listen on loopback')

    def tearDown(self):
        if self.server and self.server.poll() is None:
            self.server.terminate()
        if self.server:
            self.server.communicate(timeout=4)

    def request(self, target, method='GET'):
        raw = f'{method} {target} HTTP/1.1\r\nHost: {self.authority}\r\n\r\n'.encode('ascii')
        with socket.create_connection(('127.0.0.1', self.port), 3) as peer:
            peer.settimeout(4)
            peer.sendall(raw)
            result = bytearray()
            while True:
                part = peer.recv(16384)
                if not part:
                    break
                result.extend(part)
        header, body = bytes(result).split(b'\r\n\r\n', 1)
        lines = header.decode('ascii').split('\r\n')
        fields = dict(line.split(': ', 1) for line in lines[1:])
        self.assertEqual(int(fields['Content-Length']), len(body))
        return int(lines[0].split()[1]), body, fields

    def test_static_spans_above_json_cap_are_exact_and_need_no_runtime_files(self):
        self.assertGreater(len(self.assets['app.js']), 8192)
        self.assertEqual(subprocess.run([str(self.caller)], check=False).returncode, 0)
        cases = [('/app', 'text/html; charset=utf-8', 'index.html'),
                 ('/app.js', 'text/javascript; charset=utf-8', 'app.js'),
                 ('/style.css', 'text/css; charset=utf-8', 'style.css')]
        for route, content_type, name in cases:
            with self.subTest(route=route):
                status, body, headers = self.request(route)
                self.assertEqual(status, 200)
                self.assertEqual(headers['Content-Type'], content_type)
                self.assertEqual(body, self.assets[name])
                self.assertEqual(headers['Content-Security-Policy'],
                    "default-src 'none'; connect-src 'self'; script-src 'self'; style-src 'self'; "
                    "img-src 'none'; object-src 'none'; base-uri 'none'; form-action 'none'; frame-ancestors 'none'")

    def test_native_bootstrap_services_are_truthfully_unavailable(self):
        status, body, _ = self.request('/api/capabilities')
        self.assertEqual(status, 200)
        capabilities = json.loads(body)
        self.assertEqual(capabilities['mode'], 'console_preview')
        self.assertEqual(capabilities['console_execution'], 'unverified')
        self.assertEqual(set(capabilities['capabilities'].values()), {False})
        self.assertNotIn('csrf_token', capabilities)

        status, body, _ = self.request('/api/account')
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body), {
            'status': 'disconnected', 'live_service': 'unavailable',
            'console_translation': 'unverified'})

        status, body, _ = self.request('/api/detected-titles')
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body), {
            'schema_version': 1, 'status': 'unavailable', 'complete': False,
            'snapshot_id': None, 'source_scope': 'none', 'items': [],
            'reason': 'platform_provider_unavailable', 'issues': []})

    def test_diagnostics_remain_and_static_routes_reject_nonexact_targets(self):
        status, body, fields = self.request('/')
        self.assertEqual(status, 200)
        self.assertIn(b'<title>Modbridge native status</title>', body)
        diagnostic_csp = fields['Content-Security-Policy']
        self.assertIn("style-src 'unsafe-inline'", diagnostic_csp)
        self.assertIn("592HA5tyxWCjT8/6wd/Ih6t8yGFKrOGOaE4XqZLW6ro=", diagnostic_csp)
        for target in ('/app?x=1', '/%61pp.js', '/%2e%2e/app.js', '/app/../app.js', '/style.css?x=1'):
            with self.subTest(target=target):
                status, body, _ = self.request(target)
                self.assertEqual(status, 404)
                self.assertIn(b'Not Found', body)
        self.assertEqual(self.request('/app', method='POST')[0], 405)


if __name__ == '__main__':
    unittest.main()
