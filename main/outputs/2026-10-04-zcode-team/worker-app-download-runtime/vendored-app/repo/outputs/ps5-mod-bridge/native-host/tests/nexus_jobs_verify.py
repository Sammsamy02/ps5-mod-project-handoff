#!/usr/bin/env python3
"""Owned actual-app TLS download/reader proof; never contacts a console/account."""
import argparse
import base64
import hashlib
import io
import json
import os
from pathlib import Path
import re
import ssl
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qsl, urlsplit
import zipfile
import nexus_auth_verify as auth

ROOT = auth.ROOT


def encoded(value):
    return json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode('ascii')


def prepare(directory):
    directory.mkdir(parents=True, exist_ok=False, mode=0o700)
    certs = directory / 'certificates'; certs.mkdir(mode=0o700)
    ca, _, _ = auth.create_certificates(certs)
    scratch = directory / 'scratch'; scratch.mkdir(mode=0o700)
    config = {'client_id': 'owned_native_jobs_client', 'scope': 'fixture_scope',
              'redirect_uri': f'http://127.0.0.1:{auth.free_port()}/auth/callback',
              'ca_pem_path': str(ca.resolve()),
              'users_resolve': 'users.nexusmods.com:443:127.0.0.1',
              'api_resolve': 'api.nexusmods.com:443:127.0.0.1',
              'timeout_ms': 5000, 'connect_timeout_ms': 1000}
    (directory / 'oauth-config.json').write_bytes(encoded(config))
    native = {**config, 'redirect_uri': 'http://127.0.0.1:8085/auth/callback'}
    (directory / 'oauth-config-native-compile.json').write_bytes(encoded(native))
    result = {'oauth_config': str(directory / 'oauth-config.json'),
              'native_oauth_config': str(directory / 'oauth-config-native-compile.json'),
              'job_directory': str(scratch.resolve()), 'production_credentials': False}
    (directory / 'config-receipt.json').write_bytes(encoded(result))
    return result


def fixture_server(directory, state):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(directory / 'fixture-leaf.pem', directory / 'fixture-leaf.key')
    context.set_servername_callback(lambda stream, name, _: setattr(stream, 'fixture_sni', name))
    real = ROOT / '.superpowers/native-observations/nexus-real-mods-2026-10-01/minimap-v3.1.zip'
    archives = {8: real.read_bytes()}
    with io.BytesIO() as storage:
        with zipfile.ZipFile(storage, 'w', compression=zipfile.ZIP_STORED) as archive:
            archive.writestr('Config/Engine.ini', b'[SystemSettings]\nr.Fog=0\n')
            archive.writestr('Mods/Fixture/Scripts/main.lua', b'-- owned nonexecuting fixture\n')
            archive.writestr('payload.bin', bytes((i * 17 + 31) % 256 for i in range(200003)))
        archives[9] = storage.getvalue()
    with io.BytesIO() as storage:
        with zipfile.ZipFile(storage, 'w') as archive:
            archive.writestr('../escape.lua', b'-- refused path\n')
        archives[10] = storage.getvalue()
    archives.update(state.get('archives', {}))

    class Handler(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'
        def log_message(self, *_): pass
        def handle_request(self, method):
            parsed = urlsplit(self.path)
            record = {'method': method, 'route': 'unrecognized', 'tls': True,
                      'contract_ok': False, 'response_sent': False}
            status, body = 404, encoded({})
            host = self.headers.get('Host')
            bearer = self.headers.get('Authorization') == 'Bearer owned-jobs-access-token'
            forbidden = any(name in self.headers for name in ('apikey', 'Cookie', 'Proxy-Authorization'))
            no_account_headers = not forbidden and 'Authorization' not in self.headers
            users = host == 'users.nexusmods.com' and getattr(self.connection, 'fixture_sni', '') == 'users.nexusmods.com'
            api = host == 'api.nexusmods.com' and getattr(self.connection, 'fixture_sni', '') == 'api.nexusmods.com'
            if method == 'POST' and parsed.path == '/oauth/token':
                record['route'] = 'token'
                length = int(self.headers.get('Content-Length', '0'))
                pairs = parse_qsl(self.rfile.read(length).decode('ascii'), keep_blank_values=True)
                form = dict(pairs)
                verifier = form.get('code_verifier', '')
                challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b'=').decode()
                record['contract_ok'] = (users and no_account_headers and len(pairs) == len(form) == 5
                    and set(form) == {'grant_type', 'redirect_uri', 'client_id', 'code', 'code_verifier'}
                    and form.get('grant_type') == 'authorization_code'
                    and form.get('redirect_uri') == state['config']['redirect_uri']
                    and form.get('client_id') == state['config']['client_id']
                    and form.get('code') == 'fixture-code-initial'
                    and challenge == state.get('expected_challenge'))
                status, body = 200, encoded({'access_token': 'owned-jobs-access-token',
                    'refresh_token': 'owned-jobs-refresh-token', 'token_type': 'Bearer', 'expires_in': 3600})
            elif method == 'GET' and parsed.path == '/oauth/userinfo':
                record['route'] = 'userinfo'
                record['contract_ok'] = users and bearer and not forbidden and not parsed.query
                status, body = 200, encoded({'sub': '12345', 'name': 'Owned Jobs Fixture',
                    'membership_roles': ['premium'] if state['premium'] else [],
                    'preferences': {'adult': False}})
            elif method == 'GET' and parsed.path.startswith('/archive/'):
                record['route'] = 'archive'
                record['no_account_headers'] = no_account_headers
                number = int(parsed.path.rsplit('/', 1)[1])
                record['contract_ok'] = api and no_account_headers and parsed.query == 'fixture_signature=private'
                status, body = 200, archives[number]
            elif method == 'GET' and parsed.path.startswith('/v1/games'):
                record['route'] = 'metadata'
                record['contract_ok'] = api and bearer and not forbidden and not parsed.query
                games = [{'id': 7, 'domain_name': domain, 'name': domain.title()}
                         for domain in ('silenthilltownfall', 'skyrim')]
                match = re.fullmatch(r'/v1/games/([a-z0-9_-]+)/mods(?:/(latest_added|7)(?:/files(?:/([0-9]+)/download_link)?)?)?', parsed.path)
                if parsed.path == '/v1/games':
                    status, body = 200, encoded(games)
                elif match:
                    domain = match[1]
                    mod = {'mod_id': 7, 'game_id': 7, 'domain_name': domain,
                        'name': 'Owned Archive Fixture', 'author': 'Fixture', 'summary': '',
                        'description': '', 'version': '1.0', 'contains_adult_content': False,
                        'available': True, 'status': 'published'}
                    if match[3]:
                        record['route'] = 'download_link'
                        status, body = 200, encoded([{'name': 'Owned mirror', 'short_name': 'owned',
                            'URI': 'https://api.nexusmods.com/archive/' + match[3] + '?fixture_signature=private'}])
                    elif parsed.path.endswith('/files'):
                        status, body = 200, encoded({'files': [{'file_id': number,
                            'name': 'Owned archive', 'file_name': f'owned-{number}.zip', 'version': '1.0',
                            'description': '', 'size_kb': len(data) // 1024}
                            for number, data in archives.items()]})
                    else:
                        status, body = 200, encoded([mod] if match[2] == 'latest_added' else mod)
            record['status'] = status
            try:
                self.send_response(status)
                self.send_header('Content-Type', 'application/octet-stream' if record['route'] == 'archive' else 'application/json')
                self.send_header('Content-Length', str(len(body)))
                self.send_header('Connection', 'close'); self.end_headers()
                for offset in range(0, len(body), 16384): self.wfile.write(body[offset:offset + 16384])
                self.wfile.flush(); record['response_sent'] = True
            finally:
                state['records'].append(record)
                self.close_connection = True
        def do_GET(self): self.handle_request('GET')
        def do_POST(self): self.handle_request('POST')

    server = HTTPServer(('127.0.0.1', 0), Handler)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': .1}, daemon=True)
    thread.start()
    def close():
        server.shutdown(); server.server_close(); thread.join(timeout=3)
    return server.server_port, close, archives


def verify(profile, config_path, output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    config_path = config_path.resolve(); config = json.loads(config_path.read_bytes())
    config_dir = config_path.parent; scratch = config_dir / 'scratch'
    auth.MATRIX_DIR = output / 'relink'
    binary, identity = auth.relink_with_socket_shim(profile)
    state = {'config': config, 'premium': True, 'records': []}
    port, close_tls, archives = fixture_server(config_dir / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; observations = {}; proc = None; completed = False
    def check(name, condition): checks[name] = bool(condition)
    def call(method, target, body=None, headers=None):
        return auth.local_request(app_port, method, target, headers, b'' if body is None else encoded(body), timeout=12)
    try:
        proc = subprocess.Popen([str(binary), '--port', str(app_port), '--lifetime', '120'],
            env={'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'MB_NEXUS_AUTH_FIXTURE_PORT': str(port)},
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        for _ in range(100):
            try: initial = call('GET', '/api/capabilities'); break
            except OSError:
                if proc.poll() is not None: raise RuntimeError('app exited before readiness')
                time.sleep(.02)
        else: raise RuntimeError('app did not become ready')
        cookie, csrf, good = auth.get_cookie_and_csrf(initial[1], json.loads(initial[2]))
        caps = json.loads(initial[2])['capabilities']
        check('catalog_jobs_enabled_install_disabled', good and all(caps.get(k) for k in ('nexus_catalog', 'jobs', 'download_analyze')) and not caps.get('deploy'))
        headers = {'Cookie': 'mb_session=' + cookie, 'X-CSRF-Token': csrf,
            'Origin': config['redirect_uri'].rsplit('/auth/callback', 1)[0], 'Content-Type': 'application/json'}
        start = call('POST', '/api/auth/start', {}, headers)
        authorization = auth.parse_authorization_url(start[2], config)
        if not authorization: raise RuntimeError('PKCE authorization URL refused')
        state['expected_challenge'] = authorization['challenge']
        callback = auth.callback_request(app_port, cookie, authorization['state'])
        check('actual_pkce_account_login', start[0] == 200 and callback[0] == 303)
        for path, shape in [('/api/games', 'items'),
            ('/api/games/silenthilltownfall/mods', 'items'),
            ('/api/games/silenthilltownfall/mods/7', 'item'),
            ('/api/games/silenthilltownfall/mods/7/files', 'items'),
            ('/api/games/skyrim/mods/7', 'item')]:
            response = call('GET', path, headers=headers)
            check('catalog_' + path.replace('/', '_'), response[0] == 200 and shape in json.loads(response[2]))
        intent = {'kind': 'download_analyze', 'game': 'silenthilltownfall', 'mod_id': 7,
                  'file_id': 8, 'idempotency_key': 'owned_real_archive_1234'}
        queued = call('POST', '/api/jobs', intent, headers); job = json.loads(queued[2]); identifier = job.get('id')
        check('actual_job_queued', queued[0] == 202 and job.get('status') == 'queued' and bool(re.fullmatch('[a-f0-9]{32}', identifier or '')))
        done = call('GET', '/api/jobs/' + (identifier or ''), headers=headers)
        complete = json.loads(done[2]); analysis = complete.get('analysis_report', {})
        check('real_download_reader_completed', done[0] == 200 and complete.get('status') == 'analyzed' and analysis.get('operation_state') == 'supported')
        check('unchanged_real_archive_hash', complete.get('sha256') == hashlib.sha256(archives[8]).hexdigest() and complete.get('bytes_downloaded') == len(archives[8]))
        check('analysis_never_claims_install_runtime', analysis.get('installation_supported') is False and analysis.get('runtime_supported') is False)
        before = len(state['records'])
        duplicate = call('POST', '/api/jobs', intent, headers)
        check('idempotent_no_second_transfer', duplicate[0] == 202 and json.loads(duplicate[2]).get('id') == identifier and len(state['records']) == before)
        check('conflicting_intent_refused', call('POST', '/api/jobs', {**intent, 'file_id': 9}, headers)[0] == 409)
        check('wrong_csrf_refused', call('POST', '/api/jobs', intent, {**headers, 'X-CSRF-Token': 'x' * 43})[0] == 403)
        check('wrong_cookie_refused', call('POST', '/api/jobs', intent, {**headers, 'Cookie': 'mb_session=' + 'x' * 43})[0] == 401)
        check('wrong_origin_refused', call('POST', '/api/jobs', intent, {**headers, 'Origin': 'http://evil.test'})[0] == 403)
        second = call('GET', '/api/capabilities')
        other_cookie, _, _ = auth.get_cookie_and_csrf(second[1], json.loads(second[2]))
        other = {**headers, 'Cookie': 'mb_session=' + other_cookie}
        check('other_session_cannot_read_job', call('GET', '/api/jobs/' + identifier, headers=other)[0] == 404)
        check('other_session_list_empty', json.loads(call('GET', '/api/jobs', headers=other)[2]).get('items') == [])
        synthetic = call('POST', '/api/jobs', {**intent, 'game': 'skyrim', 'file_id': 9, 'idempotency_key': 'owned_generic_archive_1234'}, headers)
        synthetic_id = json.loads(synthetic[2]).get('id', '')
        synthetic_done = json.loads(call('GET', '/api/jobs/' + synthetic_id, headers=headers)[2])
        check('second_game_multi_chunk_archive', synthetic[0] == 202 and synthetic_done.get('status') == 'analyzed' and synthetic_done.get('sha256') == hashlib.sha256(archives[9]).hexdigest())
        failed = call('POST', '/api/jobs', {**intent, 'file_id': 10, 'idempotency_key': 'owned_refused_archive_1234'}, headers)
        failed_id = json.loads(failed[2]).get('id', '')
        failed_done = json.loads(call('GET', '/api/jobs/' + failed_id, headers=headers)[2])
        check('invalid_archive_terminal_failure', failed[0] == 202 and failed_done.get('status') == 'failed')
        check('failed_archive_removed', len(list(scratch.iterdir())) == 2)
        state['premium'] = False
        free = call('POST', '/api/jobs', {**intent, 'file_id': 11, 'idempotency_key': 'owned_free_account_1234'}, headers)
        free_id = json.loads(free[2]).get('id', '')
        free_done = json.loads(call('GET', '/api/jobs/' + free_id, headers=headers)[2])
        check('free_user_requires_separate_grant', free[0] == 202 and free_done.get('status') == 'needs_nexus_authorization' and len(list(scratch.iterdir())) == 2)
        public = call('GET', '/api/jobs', headers=headers)[2]
        check('public_jobs_no_credentials_or_uri', all(word not in public for word in (b'owned-jobs-access-token', b'owned-jobs-refresh-token', b'fixture_signature', b'https://api.nexusmods.com/archive')))
        shutdown = auth.local_request(app_port, 'POST', '/shutdown', {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'})
        stdout, stderr = proc.communicate(timeout=5)
        check('shutdown_clean_private_scratch', shutdown[0] == 200 and proc.returncode == 0 and not list(scratch.iterdir()) and not stderr)
        check('all_actual_tls_requests_valid', bool(state['records']) and all(row['contract_ok'] and row['response_sent'] for row in state['records']))
        archive_rows = [row for row in state['records'] if row['route'] == 'archive']
        check('archive_requests_no_account_headers', len(archive_rows) == 3 and all(row.get('no_account_headers') for row in archive_rows))
        observations = {'real_source_sha256': hashlib.sha256(archives[8]).hexdigest(),
                        'real_source_bytes': len(archives[8]), 'actual_tls_transfers': len(state['records']),
                        'successful_jobs': 2, 'failed_jobs': 1, 'free_authorization_jobs': 1}
        completed = True
    finally:
        if proc and proc.poll() is None: proc.terminate(); proc.communicate(timeout=4)
        close_tls()
        receipt = {'scope': 'actual_desktop_app_owned_tls_download_scan', 'checks': checks,
                   'all_passed': completed and bool(checks) and all(checks.values()), 'completed': completed, 'observations': observations,
                   'tls_records': state['records'], 'app_identity': identity,
                   'console_executed': False, 'live_account': False, 'installation_supported': False}
        (output / 'receipt.json').write_bytes(encoded(receipt))
    return receipt


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('prepare-config', 'full'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--app-profile', type=Path)
    parser.add_argument('--oauth-config', type=Path)
    args = parser.parse_args()
    if args.mode == 'prepare-config': print(json.dumps(prepare(args.output.resolve())))
    else:
        if not args.app_profile or not args.oauth_config: parser.error('full requires app profile/config')
        result = verify(args.app_profile, args.oauth_config, args.output.resolve())
        print(json.dumps({'checks': len(result['checks']), 'all_passed': result['all_passed'], 'observations': result['observations']}))
        raise SystemExit(0 if result['all_passed'] else 1)
