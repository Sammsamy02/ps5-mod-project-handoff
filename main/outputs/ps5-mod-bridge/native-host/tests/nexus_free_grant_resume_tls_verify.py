#!/usr/bin/env python3
"""Owned TLS proof for one-use free-account NXM job resume.

Only owned fixture grants/accounts are used. The real archive stays unchanged;
there is no production Nexus, console or game/system publication.
"""
import argparse
import base64
from collections import Counter
import hashlib
import json
from pathlib import Path
import socket
import re
import ssl
import subprocess
import time
import threading
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qsl, quote, urlsplit

import nexus_auth_verify as auth
import nexus_jobs_verify as jobs
import nexus_acquisition_queue_verify as previous
import nexus_iostore_generations_verify as generations
import nexus_requirements_verify as requirements

ROOT = auth.ROOT
SPIKE = ROOT / '.superpowers/native-dependency-spike/native-nexus-free-grant-resume-1'
CONFIG = SPIKE / 'fixture-config-1/oauth-config.json'
RED_HEAD = '9679483575871c87b43f4950666079f5874c7f1c'
RED_PROFILE = previous.SPIKE / 'configured-host-1/desktop_fixture-download'
RED_RECEIPT = previous.SPIKE / 'actual-tls-green-1/receipt.json'
RED_CONFIG = previous.CONFIG
RED_ARTIFACT_SHA = '15b29a802473207c2f7ed1e7a066d8c1d1273990f3f39a55c01c1bc5a76b14e0'
RED_BINARY_SHA = '731191248ae545c430d158fc4b939be78142edbf3656137f292f1e07aa4fd528'
FIXTURE_KEY = 'owned/+%free-grant'
WRONG_USER_KEY = 'owned/+%wrong-user-grant'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def nxm(expires, user_id=12345, file_id=21, mod_id=7, game='silenthilltownfall', key=FIXTURE_KEY):
    # Canonical uppercase percent encoding exercises decoded printable bytes.
    return f'nxm://{game}/mods/{mod_id}/files/{file_id}?key={quote(key, safe="-._~")}&expires={expires}&user_id={user_id}'


def resume_request(port, identifier, headers, body, timeout=20):
    """Send one complete bounded frame; never infer status from a reset."""
    wire = [f'POST /api/jobs/{identifier}/resume HTTP/1.1', f'Host: 127.0.0.1:{port}',
            'Connection: close', *[f'{name}: {value}' for name, value in headers.items()],
            f'Content-Length: {len(body)}']
    packed = ('\r\n'.join(wire) + '\r\n\r\n').encode('ascii') + body
    with socket.create_connection(('127.0.0.1', port), timeout=timeout) as stream:
        stream.settimeout(timeout); stream.sendall(packed)
        received = bytearray()
        while len(received) <= 16384:
            try:
                block = stream.recv(4096)
            except ConnectionResetError:
                break
            if not block:
                break
            received.extend(block)
    if len(received) > 16384 or b'\r\n\r\n' not in received:
        raise RuntimeError('resume response has no complete bounded HTTP header')
    head, payload = bytes(received).split(b'\r\n\r\n', 1)
    lines = head.decode('latin1').split('\r\n')
    status = lines[0].split(' ')
    response_headers = {}
    for line in lines[1:]:
        name, value = line.split(':', 1)
        response_headers.setdefault(name.lower(), []).append(value.strip())
    lengths = response_headers.get('content-length', [])
    if len(status) < 2 or not status[1].isdigit() or len(lengths) != 1 or not lengths[0].isdigit() or int(lengths[0]) != len(payload):
        raise RuntimeError('resume response status/body framing is incomplete')
    return int(status[1]), response_headers, payload


def historical_binary(output):
    manifest_path = RED_PROFILE / 'manifest.json'
    manifest = json.loads(manifest_path.read_bytes())
    prior = json.loads(RED_RECEIPT.read_bytes()); identity = prior['app_identity']
    binary = Path(identity['private_directory']) / 'modbridge-native-auth-fixture'
    if (prior.get('all_passed') is not True or sha(RED_PROFILE / manifest['artifact']) != RED_ARTIFACT_SHA
            or identity.get('fixture_binary_sha256') != RED_BINARY_SHA or sha(binary) != RED_BINARY_SHA):
        raise RuntimeError('retained historical app identity differs')
    destination = output / 'source-snapshot'; destination.mkdir(mode=0o700)
    tracked = {}
    for path, expected in manifest['source_sha256'].items():
        result = subprocess.run(['git', 'show', RED_HEAD + ':' + path], cwd=ROOT, capture_output=True, timeout=15)
        if result.returncode or hashlib.sha256(result.stdout).hexdigest() != expected:
            raise RuntimeError('historical source identity differs from commit')
        target = destination / path; target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(result.stdout); tracked[path] = expected
    if len(tracked) != 136:
        raise RuntimeError('historical source count differs')
    (output / 'historical-manifest.json').write_bytes(manifest_path.read_bytes())
    (output / 'historical-config.h').write_bytes((RED_PROFILE / 'config.h').read_bytes())
    return binary, {'historical_head': RED_HEAD, 'tracked_source_checks': len(tracked),
        'source_snapshot_sha256': tracked, 'manifest_sha256': sha(manifest_path),
        'artifact_sha256': RED_ARTIFACT_SHA, 'fixture_binary_sha256': RED_BINARY_SHA,
        'test_only_socket_redirect': True}


def verify_red(output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    binary, identity = historical_binary(output)
    config = json.loads(RED_CONFIG.read_bytes()); scratch = requirements.profile_scratch(RED_PROFILE)
    state = {'config': config, 'premium': False, 'records': [],
             'archives': {21: generations.MOVEMENT.read_bytes()}}
    port, close_tls = previous.fixture_server(RED_CONFIG.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; process = None; completed = False; status = None
    try:
        process, initial = generations.start_app(binary, app_port, port)
        headers = generations.sign_in(app_port, config, state)
        checks['actual_owned_free_pkce_login'] = initial[0] == 200 and any(
            r['route'] == 'userinfo' and r['premium'] is False and r['contract_ok'] for r in state['records'])
        submitted = auth.local_request(app_port, 'POST', '/api/jobs', headers, jobs.encoded({
            'kind': 'download_analyze', 'game': 'silenthilltownfall', 'mod_id': 7,
            'file_id': 21, 'idempotency_key': 'owned_historical_free_resume_001'}), timeout=20)
        identifier = generations.extract_json(submitted).get('id', '')
        if submitted[0] != 202 or len(identifier) != 32:
            raise RuntimeError('historical free job was not admitted')
        before = auth.local_request(app_port, 'GET', '/api/jobs/' + identifier, headers, timeout=20)
        checks['historical_free_job_needs_authorization'] = before[0] == 200 and (
            generations.extract_json(before).get('status') == 'needs_nexus_authorization')
        provider_count = len(state['records'])
        resumed = resume_request(app_port, identifier, headers,
            jobs.encoded({'nxm': nxm(int(time.time()) + 600)}), timeout=20)
        status = resumed[0]
        checks['intended_same_id_resume_available'] = resumed[0] == 202 and (
            generations.extract_json(resumed).get('id') == identifier)
        after = auth.local_request(app_port, 'GET', '/api/jobs/' + identifier, headers, timeout=20)
        checks['historical_resume_refuses_before_provider'] = resumed[0] == 400 and len(state['records']) == provider_count
        checks['refusal_preserves_same_id_authorization_required'] = after[0] == 200 and (
            generations.extract_json(after).get('id') == identifier and
            generations.extract_json(after).get('status') == 'needs_nexus_authorization')
        checks['no_link_archive_or_private_file'] = not list(scratch.iterdir()) and all(
            r['route'] not in ('download_link', 'archive') for r in state['records'])
        shutdown = auth.local_request(app_port, 'POST', '/shutdown',
            {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=10)
        _, stderr = process.communicate(timeout=10)
        checks['clean_owned_shutdown'] = shutdown[0] == 200 and process.returncode == 0 and not stderr and not list(scratch.iterdir())
        checks['only_valid_owned_tls'] = bool(state['records']) and all(r['contract_ok'] for r in state['records'])
        completed = True
    finally:
        if process and process.poll() is None:
            process.terminate(); process.communicate(timeout=5)
        close_tls()
        expected_red = completed and checks.get('intended_same_id_resume_available') is False and all(
            value for name, value in checks.items() if name != 'intended_same_id_resume_available')
        receipt = {'phase': 'red', 'checks': checks, 'expected_red_observed': expected_red,
            'all_passed': completed and bool(checks) and all(checks.values()),
            'actual_http_status': status, 'intended_http_status': 202, 'app_identity': identity,
            'tls_records': state['records'], 'verifier_sha256': sha(Path(__file__)),
            'grants_or_keys_recorded': False, 'production_account_contacted': False,
            'console_executed': False, 'archive_transfer': False, 'game_files_changed': False}
        (output / 'receipt.json').write_bytes(jobs.encoded(receipt))
    return receipt


def fixture_server(directory, state):
    """Narrow owned OAuth/profile/mod/link/archive fixture; no grant logging."""
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(directory / 'fixture-leaf.pem', directory / 'fixture-leaf.key')
    context.set_servername_callback(lambda stream, name, _: setattr(stream, 'fixture_sni', name))

    class Handler(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'
        def log_message(self, *_):
            pass

        def handle_request(self, method):
            parsed = urlsplit(self.path)
            record = {'route': 'unrecognized', 'method': method, 'contract_ok': False, 'response_sent': False}
            forbidden = any(name in self.headers for name in ('apikey', 'Cookie', 'Proxy-Authorization'))
            bearer = self.headers.get('Authorization') == 'Bearer owned-jobs-access-token'
            no_account = not forbidden and 'Authorization' not in self.headers
            sni = getattr(self.connection, 'fixture_sni', '')
            users = self.headers.get('Host') == 'users.nexusmods.com' and sni == 'users.nexusmods.com'
            api = self.headers.get('Host') == 'api.nexusmods.com' and sni == 'api.nexusmods.com'
            length = int(self.headers.get('Content-Length', '0'))
            incoming = self.rfile.read(length) if length else b''
            status, body = 404, {}
            if method == 'POST' and parsed.path == '/oauth/token':
                record['route'] = 'token'
                pairs = parse_qsl(incoming.decode('ascii'), keep_blank_values=True); form = dict(pairs)
                challenge = base64.urlsafe_b64encode(hashlib.sha256(
                    form.get('code_verifier', '').encode()).digest()).rstrip(b'=').decode()
                record['contract_ok'] = (users and no_account and not parsed.query and len(pairs) == len(form) == 5
                    and set(form) == {'grant_type', 'redirect_uri', 'client_id', 'code', 'code_verifier'}
                    and form.get('grant_type') == 'authorization_code'
                    and form.get('redirect_uri') == state['config']['redirect_uri']
                    and form.get('client_id') == state['config']['client_id']
                    and form.get('code') == 'fixture-code-initial' and challenge == state.get('expected_challenge'))
                status, body = 200, {'access_token': 'owned-jobs-access-token',
                    'refresh_token': 'owned-jobs-refresh-token', 'token_type': 'Bearer', 'expires_in': 3600}
            elif method == 'GET' and parsed.path == '/oauth/userinfo':
                record['route'] = 'userinfo'
                record['live_user_matches_grant_fixture'] = state['live_user_id'] == 12345
                record['contract_ok'] = users and bearer and not forbidden and not parsed.query and not incoming
                status, body = 200, {'sub': str(state['live_user_id']), 'name': 'Owned free fixture',
                    'membership_roles': [], 'preferences': {'adult': False}}
            elif method == 'GET' and parsed.path == '/v1/games/silenthilltownfall/mods/7':
                record['route'] = 'mod'
                record['contract_ok'] = api and bearer and not forbidden and not parsed.query and not incoming
                status, body = 200, requirements.source_mod('silenthilltownfall', 7, False)
            elif method == 'GET' and parsed.path == '/v1/games/silenthilltownfall/mods/7/files/21/download_link':
                record['route'] = 'download_link'
                expected = 'key=' + quote(FIXTURE_KEY, safe='-._~') + '&expires=' + str(state['expected_expires'])
                record['canonical_key_expires_exact'] = parsed.query == expected
                record['user_id_not_in_link_query'] = 'user_id' not in parsed.query
                record['contract_ok'] = api and bearer and not forbidden and not incoming and parsed.query == expected
                status, body = 200, [{'name': 'Owned mirror', 'short_name': 'owned',
                    'URI': 'https://api.nexusmods.com/archive/21?fixture_signature=private'}]
            elif method == 'GET' and parsed.path == '/archive/21':
                record['route'] = 'archive'; record['no_account_headers'] = no_account
                record['grant_not_in_archive_query'] = parsed.query == 'fixture_signature=private'
                record['contract_ok'] = api and no_account and not incoming and parsed.query == 'fixture_signature=private'
                status, body = 200, state['archive']
            record['status'] = status
            body = body if isinstance(body, bytes) else jobs.encoded(body)
            try:
                self.send_response(status)
                self.send_header('Content-Type', 'application/octet-stream' if record['route'] == 'archive' else 'application/json')
                self.send_header('Content-Length', str(len(body))); self.send_header('Connection', 'close'); self.end_headers()
                self.wfile.write(body); self.wfile.flush(); record['response_sent'] = True
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                record['peer_closed'] = True
            finally:
                state['records'].append(record); self.close_connection = True

        def do_GET(self):
            self.handle_request('GET')

        def do_POST(self):
            self.handle_request('POST')

    server = HTTPServer(('127.0.0.1', 0), Handler)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': .1}, daemon=True); thread.start()

    def close():
        server.shutdown(); server.server_close(); thread.join(timeout=3)

    return server.server_port, close


def verify_green(profile, config_path, output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    profile = profile.resolve(strict=True); config_path = config_path.resolve(strict=True)
    import nexus_iostore_jobs_verify as iostore
    config = iostore.assert_fixture_config(config_path)
    scratch = requirements.profile_scratch(profile)
    if scratch != (config_path.parent / 'scratch').resolve(strict=True):
        raise RuntimeError('compiled scratch differs from owned fresh fixture')
    expected_archive = generations.EXPECTED['movement']
    archive = generations.MOVEMENT.read_bytes()
    if (len(archive), hashlib.sha256(archive).hexdigest()) != expected_archive:
        raise RuntimeError('retained unchanged Movement archive differs')
    source_before = generations.identity(generations.MOVEMENT)
    scratch_before = (scratch.stat().st_dev, scratch.stat().st_ino)
    auth.MATRIX_DIR = output / 'relink'
    binary, app_identity = auth.relink_with_socket_shim(profile)
    state = {'config': config, 'live_user_id': 12345, 'expected_expires': 0,
             'archive': archive, 'records': []}
    port, close_tls = fixture_server(config_path.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; public = []; summary = {}; process = None; completed = False

    def check(name, passed):
        checks[name] = bool(passed)

    def call(method, target, headers=None, body=b'', timeout=20):
        response = auth.local_request(app_port, method, target, headers, body, timeout=timeout)
        public.append(response[2]); return response

    def resume(identifier, headers, uri):
        response = resume_request(app_port, identifier, headers, jobs.encoded({'nxm': uri}))
        public.append(response[2]); return response

    def get_job(identifier, headers):
        return call('GET', '/api/jobs/' + identifier, headers)

    try:
        process, initial = generations.start_app(binary, app_port, port)
        headers = generations.sign_in(app_port, config, state)
        check('actual_owned_free_account_pkce', initial[0] == 200 and any(
            r['route'] == 'userinfo' and r['live_user_matches_grant_fixture'] and r['contract_ok'] for r in state['records']))
        intent = {'kind': 'download_analyze', 'game': 'silenthilltownfall', 'mod_id': 7,
            'file_id': 21, 'idempotency_key': 'owned_free_movement_resume_001'}
        submitted = call('POST', '/api/jobs', headers, jobs.encoded(intent))
        identifier = generations.extract_json(submitted).get('id', '')
        check('one_original_source_admitted', submitted[0] == 202 and re.fullmatch('[a-f0-9]{32}', identifier) is not None)
        if not identifier:
            raise RuntimeError('source admission has no job ID')
        original = generations.extract_json(get_job(identifier, headers))
        check('initial_free_source_requires_nexus_authorization', original.get('status') == 'needs_nexus_authorization' and
            original.get('error', {}).get('code') == 'download_authorization')
        check('initial_free_refusal_no_link_archive_file', not list(scratch.iterdir()) and all(
            r['route'] not in ('download_link', 'archive') for r in state['records']))
        future = int(time.time()) + 600
        for label, uri in [('wrong_game', nxm(future, game='skyrim')),
                           ('wrong_mod', nxm(future, mod_id=8)), ('wrong_file', nxm(future, file_id=22)),
                           ('expired', nxm(int(time.time()) - 1))]:
            before = len(state['records'])
            refused = resume(identifier, headers, uri)
            row = generations.extract_json(get_job(identifier, headers))
            check(label + '_admission_refused_without_provider_or_mutation', refused[0] == 403 and
                row.get('id') == identifier and row.get('status') == 'needs_nexus_authorization' and
                len(state['records']) == before and not list(scratch.iterdir()))
        owner_b = generations.headers_for_new_session(app_port, config)
        before = len(state['records'])
        foreign = resume(identifier, owner_b, nxm(future))
        check('other_cookie_cannot_resume_owned_job', foreign[0] == 404 and len(state['records']) == before)
        wrong_csrf = resume(identifier, {**headers, 'X-CSRF-Token': 'x' * 43}, nxm(future))
        check('wrong_csrf_refuses_before_provider', wrong_csrf[0] == 403 and len(state['records']) == before)

        # Admission binds the grant's declared source/user; the worker must
        # independently refresh live identity before any download-link read.
        state['live_user_id'] = 54321
        before = len(state['records'])
        wrong_user = resume(identifier, headers, nxm(future, key=WRONG_USER_KEY))
        queued = generations.extract_json(wrong_user)
        check('wrong_live_user_grant_admits_same_id_for_fresh_worker_check', wrong_user[0] == 202 and
            queued.get('id') == identifier and queued.get('status') == 'queued')
        wrong_done = generations.extract_json(get_job(identifier, headers))
        wrong_records = state['records'][before:]
        check('worker_live_user_mismatch_returns_authorization_required', wrong_done.get('id') == identifier and
            wrong_done.get('status') == 'needs_nexus_authorization' and any(
                r['route'] == 'userinfo' and not r['live_user_matches_grant_fixture'] for r in wrong_records))
        check('wrong_live_user_gets_no_link_archive_or_original', not list(scratch.iterdir()) and all(
            r['route'] not in ('download_link', 'archive') for r in wrong_records))

        state['live_user_id'] = 12345
        state['expected_expires'] = int(time.time()) + 600
        before = len(state['records'])
        corrected = resume(identifier, headers, nxm(state['expected_expires']))
        corrected_row = generations.extract_json(corrected)
        check('fresh_corrected_grant_requeues_same_id_source_slot', corrected[0] == 202 and
            corrected_row.get('id') == identifier and corrected_row.get('kind') == intent['kind'] and
            corrected_row.get('game') == intent['game'] and corrected_row.get('mod_id') == 7 and
            corrected_row.get('file_id') == 21 and corrected_row.get('status') == 'queued')
        done = get_job(identifier, headers); analyzed = generations.extract_json(done)
        current_records = state['records'][before:]
        check('corrected_grant_uses_fresh_free_live_identity', sum(r['route'] == 'userinfo' for r in current_records) == 1 and any(
            r['route'] == 'userinfo' and r['live_user_matches_grant_fixture'] for r in current_records))
        link_rows = [r for r in current_records if r['route'] == 'download_link']
        check('only_canonical_decoded_key_expires_fixed_link_query', len(link_rows) == 1 and
            link_rows[0]['canonical_key_expires_exact'] and link_rows[0]['user_id_not_in_link_query'])
        archive_rows = [r for r in current_records if r['route'] == 'archive']
        check('one_credential_free_archive_without_nxm_grant_query', len(archive_rows) == 1 and
            archive_rows[0]['no_account_headers'] and archive_rows[0]['grant_not_in_archive_query'])
        leaf = scratch / ('mb-job-' + identifier + '.archive')
        check('same_id_original_movement_sink_and_real_analysis', done[0] == 200 and analyzed.get('id') == identifier and
            analyzed.get('status') == 'analyzed' and (analyzed.get('bytes_downloaded'), analyzed.get('sha256')) == expected_archive and
            leaf.is_file() and not leaf.is_symlink() and (leaf.stat().st_size, generations.sha256_file(leaf)) == expected_archive and
            analyzed.get('analysis_report', {}).get('operation_state') == 'supported')
        check('resume_confers_no_install_or_runtime_authority', analyzed.get('runtime_supported') is False and
            analyzed.get('installation_supported') is False and 'preparation' not in analyzed)
        before = len(state['records'])
        for label, uri in [('repeated', nxm(state['expected_expires'])), ('terminal_fresh', nxm(int(time.time()) + 700))]:
            denied = resume(identifier, headers, uri)
            check(label + '_resume_refused_without_redownload', denied[0] == 403 and len(state['records']) == before)
        replay = call('POST', '/api/jobs', headers, jobs.encoded(intent))
        check('original_idempotency_binding_still_same_terminal_job', replay[0] == 202 and
            generations.extract_json(replay).get('id') == identifier and len(state['records']) == before)
        listed = generations.extract_json(call('GET', '/api/jobs', headers)).get('items', [])
        check('resume_preserves_one_global_source_slot', len(listed) == 1 and listed[0].get('id') == identifier)
        check('no_automatic_generation', generations.extract_json(call('GET', '/api/generations', headers)).get('items') == [])
        check('exactly_one_link_and_archive_total', Counter(r['route'] for r in state['records'])['download_link'] == 1 and
            Counter(r['route'] for r in state['records'])['archive'] == 1)
        secret_markers = (FIXTURE_KEY.encode(), WRONG_USER_KEY.encode(), quote(FIXTURE_KEY, safe='-._~').encode(),
            quote(WRONG_USER_KEY, safe='-._~').encode(), b'nxm://', b'owned-jobs-access-token', b'owned-jobs-refresh-token',
            b'fixture_signature', b'https://api.nexusmods.com/archive/', str(scratch).encode(), str(config_path).encode())
        check('all_public_responses_wipe_secret_grant_material', all(
            len(body) <= 98304 and not any(marker in body for marker in secret_markers) for body in public))
        check('all_owned_tls_contracts_valid', bool(state['records']) and all(r['contract_ok'] and r['response_sent'] for r in state['records']))
        shutdown = call('POST', '/shutdown', {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'})
        _, stderr = process.communicate(timeout=20)
        check('shutdown_cleans_owned_original_without_logs', shutdown[0] == 200 and process.returncode == 0 and not stderr and not list(scratch.iterdir()))
        check('source_and_private_directory_identity_preserved', source_before == generations.identity(generations.MOVEMENT) and
            scratch_before == (scratch.stat().st_dev, scratch.stat().st_ino))
        summary.update(job_id=identifier, same_id_preserved=True, terminal_state=analyzed.get('status'),
            original_archive_bytes=expected_archive[0], original_archive_sha256=expected_archive[1])
        completed = True
    finally:
        if process and process.poll() is None:
            process.terminate(); process.communicate(timeout=5)
        close_tls()
        receipt = {'phase': 'green', 'scope': 'actual_free_account_same_id_typed_grant_resume',
            'checks': checks, 'all_passed': completed and bool(checks) and all(checks.values()),
            'app_identity': app_identity, 'summary': summary, 'tls_records': state['records'],
            'verifier_sha256': sha(Path(__file__)), 'grants_or_keys_recorded': False,
            'worker_expiry_evidence': 'not exercised here; separate component controls cover this boundary',
            'production_account_contacted': False, 'console_executed': False,
            'game_files_changed': False, 'runtime_or_installation_authority': False}
        (output / 'receipt.json').write_bytes(jobs.encoded(receipt))
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('red', 'green'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--app-profile', type=Path)
    parser.add_argument('--oauth-config', type=Path, default=CONFIG)
    args = parser.parse_args()
    if args.phase == 'green' and not args.app_profile:
        parser.error('GREEN requires --app-profile')
    receipt = verify_red(args.output.resolve()) if args.phase == 'red' else verify_green(
        args.app_profile, args.oauth_config, args.output.resolve())
    print(json.dumps({'phase': args.phase, 'checks': len(receipt['checks']),
        'all_passed': receipt['all_passed'], 'expected_red_observed': receipt.get('expected_red_observed'),
        'failed_checks': [name for name, value in receipt['checks'].items() if not value]}))
    return 0 if receipt.get('expected_red_observed') or receipt['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
