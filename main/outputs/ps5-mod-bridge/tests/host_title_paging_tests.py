"""Owned loopback paging controls; no service, account or console access."""
import http.client
import json
from pathlib import Path
import sys
import tempfile
import threading
import unittest

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / 'companion'))
import modbridge_host as host
from modbridge_inventory import TitleInventory


def captured(count=32):
    def metadata(value):
        return {'value': value, 'status': 'observed', 'source': 'owned_fixture'}
    return {'status': 'observed', 'complete': True, 'source_scope': 'metadata_candidates',
            'reason': None, 'issues': [], 'items': [
                {'title_id': metadata('GENERIC_%03d' % i),
                 'display_name': metadata('Owned title %d — 星' % i),
                 'build_version': metadata('02.03.%03d' % i), 'build_fingerprint': None}
                for i in range(count)]}


class HostTitlePagingTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.calls = 0
        self.capture = captured()
        def provider():
            self.calls += 1
            return self.capture
        self.inventory = TitleInventory(provider)
        root = Path(self.tmp.name).resolve()
        self.app = host.BridgeApplication(root / 'private' / 'credentials.json',
            root / 'quarantine', inventory=self.inventory)
        self.server = host.create_server(self.app)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.addCleanup(self.shutdown)
        self.port = self.server.server_address[1]
        self.origin = self.app.origin
        self.cookie = None
        self.csrf = None

    def shutdown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(2)
        self.app.close()

    def request(self, method, path, body=None, headers=None):
        wire = http.client.HTTPConnection('127.0.0.1', self.port, timeout=3)
        merged = dict(headers or {})
        if self.cookie:
            merged.setdefault('Cookie', self.cookie)
        if body is not None:
            merged.setdefault('Content-Type', 'application/json')
        wire.request(method, path, body=body, headers=merged)
        response = wire.getresponse()
        content = response.read()
        cookie = response.getheader('Set-Cookie')
        if cookie:
            self.cookie = cookie.split(';', 1)[0]
        result = response.status, content, dict(response.getheaders())
        wire.close()
        return result

    def bootstrap(self):
        status, body, _ = self.request('GET', '/api/capabilities')
        self.assertEqual(status, 200)
        record = json.loads(body)
        self.csrf = record['csrf_token']
        return record

    def refresh(self, body=b'{}', headers=None, path='/api/detected-titles/pages/refresh'):
        values = {'Origin': self.origin, 'X-CSRF-Token': self.csrf}
        values.update(headers or {})
        return self.request('POST', path, body, values)

    def assert_error(self, response, status, code):
        actual, body, headers = response
        self.assertEqual(actual, status)
        value = json.loads(body)
        self.assertEqual(set(value), {'code', 'error'})
        self.assertEqual(value['code'], code)
        self.assertIs(type(value['error']), str)
        self.assertEqual(body, json.dumps(value, sort_keys=True, separators=(',', ':'), ensure_ascii=True).encode('ascii'))
        self.assertLessEqual(len(body), 8191)
        self.assertEqual(headers['Cache-Control'], 'no-store')

    def test_transport_capability_does_not_claim_console_or_provider(self):
        value = self.bootstrap()
        self.assertTrue(value['capabilities']['title_inventory_paging'])
        self.assertFalse(value['capabilities']['detected_titles'])
        self.assertFalse(value['capabilities']['deploy'])
        self.assertEqual(self.calls, 0)

    def test_exact_bytes_and_complete_generic_capture_resolve_past_legacy_limit(self):
        self.bootstrap()
        status, wire, headers = self.request('GET', '/api/detected-titles/pages')
        self.assertEqual(status, 200)
        self.assertEqual(wire, self.inventory.page())
        self.assertEqual(headers['Content-Length'], str(len(wire)))
        first = json.loads(wire)
        items = list(first['items'])
        cursor = first['page']['next_cursor']
        while cursor:
            status, body, _ = self.request('GET', '/api/detected-titles/pages/' + cursor)
            self.assertEqual(status, 200)
            self.assertEqual(body, self.inventory.page(cursor))
            self.assertLessEqual(len(body), 8191)
            page = json.loads(body)
            items.extend(page['items'])
            cursor = page['page']['next_cursor']
        self.assertEqual(len(items), 32)
        self.assertEqual(self.calls, 1)
        reference = {'snapshot_id': first['snapshot']['snapshot_id'], 'title_key': items[-1]['title_key']}
        self.assertEqual(self.inventory.resolve(reference)['title_id']['value'], 'GENERIC_031')
        status, legacy, _ = self.request('GET', '/api/detected-titles')
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(legacy)['status'], 'error')
        self.assertEqual(self.inventory.resolve(reference)['title_id']['value'], 'GENERIC_031')
        status, body, _ = self.request('GET', '/api/detected-titles/pages/current/' + reference['snapshot_id'])
        self.assertEqual(status, 200)
        self.assertEqual(body, self.inventory.current(reference['snapshot_id']))
        self.assertTrue(json.loads(body)['current'])
        self.assertEqual(self.calls, 1)

    def test_paging_session_errors_have_constant_profile(self):
        self.assert_error(self.request('GET', '/api/detected-titles/pages'), 401, 'session_required')
        self.assertEqual(self.calls, 0)

    def test_continuations_and_confirmation_never_trigger_initial_capture(self):
        self.bootstrap()
        self.assert_error(self.request('GET', '/api/detected-titles/pages/' + 'a' * 32), 409, 'inventory_page_stale')
        self.assert_error(self.request('GET', '/api/detected-titles/pages/current/' + 'a' * 32), 409, 'inventory_page_stale')
        self.assertEqual(self.calls, 0)

    def test_refresh_requires_origin_csrf_and_small_empty_object(self):
        self.bootstrap()
        self.assert_error(self.request('POST', '/api/detected-titles/pages/refresh', b'{}'), 403, 'origin_refused')
        self.assert_error(self.refresh(headers={'X-CSRF-Token': 'wrong'}), 403, 'origin_refused')
        for body in (b'[]', b'{"x":1}', b'{"x":1,"x":2}', b' ' * 15 + b'{}', b''):
            with self.subTest(body=body):
                self.assert_error(self.refresh(body), 400, 'invalid_input')
        self.assertEqual(self.calls, 0)
        status, body, _ = self.refresh(b' ' * 14 + b'{}')
        self.assertEqual(status, 200)
        self.assertEqual(json.loads(body)['snapshot']['status'], 'observed')
        self.assertEqual(self.calls, 1)

    def test_canonical_targets_only_no_queries_or_encoded_tokens(self):
        self.bootstrap()
        paths = ['/api/detected-titles/pages?', '/api/detected-titles/pages?x=1',
                 '/api/detected-titles/pages/' + 'A' * 32,
                 '/api/detected-titles/pages/' + '%61' * 32,
                 '/api/detected-titles/pages/current/' + 'a' * 31,
                 '/api/detected-titles/pages/' + 'a' * 200,
                 '/api/detected-titles/pages/refresh', '/api/detected-titles/pages/current']
        for path in paths:
            with self.subTest(path=path):
                self.assert_error(self.request('GET', path), 400, 'invalid_input')
        self.assertEqual(self.calls, 0)
        self.assert_error(self.refresh(path='/api/detected-titles/pages/refresh?x=1'), 400, 'invalid_input')
        self.assertEqual(self.calls, 0)

    def test_failed_refresh_retires_all_tokens_and_never_leaks_paths(self):
        self.bootstrap()
        _, body, _ = self.request('GET', '/api/detected-titles/pages')
        first = json.loads(body)
        cursor = first['page']['next_cursor']
        snapshot = first['snapshot']['snapshot_id']
        self.capture = {'private': '/secret/game/path'}
        status, wire, _ = self.refresh()
        self.assertEqual(status, 200)
        terminal = json.loads(wire)
        self.assertEqual(terminal['snapshot']['status'], 'error')
        self.assertIsNone(terminal['snapshot']['snapshot_id'])
        self.assertEqual(terminal['items'], [])
        self.assertNotIn(b'/secret/', wire)
        self.assert_error(self.request('GET', '/api/detected-titles/pages/' + cursor), 409, 'inventory_page_stale')
        self.assert_error(self.request('GET', '/api/detected-titles/pages/current/' + snapshot), 409, 'inventory_page_stale')
        self.assertEqual(self.calls, 2)

    def test_inflight_capture_gates_reads_and_refresh(self):
        self.bootstrap()
        entered, release = threading.Event(), threading.Event()
        def provider():
            entered.set()
            self.assertTrue(release.wait(3))
            return captured(1)
        self.inventory.provider = provider
        result = []
        thread = threading.Thread(target=lambda: result.append(self.refresh()))
        thread.start()
        try:
            self.assertTrue(entered.wait(2))
            self.assert_error(self.request('GET', '/api/detected-titles/pages'), 503, 'inventory_capture_in_progress')
            self.assert_error(self.refresh(), 409, 'inventory_refresh_busy')
        finally:
            release.set()
            thread.join(3)
        self.assertEqual(result[0][0], 200)


if __name__ == '__main__':
    unittest.main()
