#!/usr/bin/env python3
"""Actual acquisition queue proof using owned TLS and retained original archives.

Metadata relations and account responses are fixture data. Outputs remain
private preparation; no production account, console or game publication occurs.
"""
import argparse
import base64
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import ssl
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qsl, urlsplit

import nexus_auth_verify as auth
import nexus_jobs_verify as jobs
import nexus_acquisition_plan_verify as plans
import nexus_iostore_generations_verify as generations
import nexus_requirements_verify as requirements

ROOT = auth.ROOT
SPIKE = ROOT / '.superpowers/native-dependency-spike/native-nexus-acquisition-queue-1'
CONFIG = SPIKE / 'fixture-config-1/oauth-config.json'
RED_HEAD = '19e007515e276a70cc9b32ff443c63cd262578a3'
RED_PROFILE = plans.SPIKE / 'configured-host-1/desktop_fixture-download'
RED_RECEIPT = plans.SPIKE / 'green-2/receipt.json'
RED_CONFIG = plans.CONFIG
RED_ARTIFACT_SHA = 'a56486fd9b861f47b49ace574f8ddf30041c100bfaf54cac959226cab31146eb'
RED_BINARY_SHA = '3b7f34798265ca133a77acd68be1f76c7b99e9c720c3e17f0f6a56d7697c2045'
SOURCES = ((6, 20, 'leon', generations.LEON), (7, 21, 'movement', generations.MOVEMENT))


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def acquisition_intent(key='owned_acquisition_premium_001'):
    return {'game': 'silenthilltownfall', 'mod_ids': [7], 'file_choices': [],
            'file_policy': 'unique_current_primary', 'idempotency_key': key}


def historical_binary(output):
    manifest_path = RED_PROFILE / 'manifest.json'
    manifest = json.loads(manifest_path.read_bytes())
    prior = json.loads(RED_RECEIPT.read_bytes())
    identity = prior['app_identity']
    binary = Path(identity['private_directory']) / 'modbridge-native-auth-fixture'
    if (prior.get('all_passed') is not True
            or sha(RED_PROFILE / manifest['artifact']) != RED_ARTIFACT_SHA
            or identity.get('fixture_binary_sha256') != RED_BINARY_SHA
            or sha(binary) != RED_BINARY_SHA):
        raise RuntimeError('historical app/profile/receipt identity differs')
    snapshots = output / 'source-snapshot'
    snapshots.mkdir(mode=0o700)
    tracked = {}
    for path, digest in manifest['source_sha256'].items():
        original = subprocess.run(['git', 'show', RED_HEAD + ':' + path],
                                  cwd=ROOT, capture_output=True, timeout=15)
        if original.returncode or hashlib.sha256(original.stdout).hexdigest() != digest:
            raise RuntimeError('historical source closure differs from commit')
        destination = snapshots / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(original.stdout)
        tracked[path] = digest
    if len(tracked) != 136:
        raise RuntimeError('historical source count differs')
    (output / 'historical-manifest.json').write_bytes(manifest_path.read_bytes())
    (output / 'historical-config.h').write_bytes((RED_PROFILE / 'config.h').read_bytes())
    return binary, {'historical_head': RED_HEAD, 'tracked_source_checks': len(tracked),
                    'source_snapshot_sha256': tracked,
                    'profile_manifest_sha256': sha(manifest_path),
                    'profile_artifact_sha256': RED_ARTIFACT_SHA,
                    'fixture_binary_sha256': RED_BINARY_SHA,
                    'test_only_socket_redirect': True}


def verify_red(output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    binary, identity = historical_binary(output)
    config = json.loads(RED_CONFIG.read_bytes())
    scratch = requirements.profile_scratch(RED_PROFILE)
    if list(scratch.iterdir()):
        raise RuntimeError('historical scratch is not empty')
    state = {'config': config, 'scenario': 'complete', 'records': []}
    port, close_tls = plans.fixture_server(RED_CONFIG.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; process = None; completed = False; status = None
    try:
        process, initial = generations.start_app(binary, app_port, port)
        headers = generations.sign_in(app_port, config, state)
        checks['actual_owned_pkce_login'] = initial[0] == 200 and any(
            row['route'] == 'token' and row['contract_ok'] for row in state['records'])
        before = len(state['records'])
        response = auth.local_request(app_port, 'POST', '/api/acquisitions', headers,
                                      jobs.encoded(acquisition_intent()), timeout=12)
        status = response[0]
        checks['intended_atomic_acquisition_available'] = response[0] == 202 and (
            json.loads(response[2]).get('acquisition', {}).get('state') == 'accepted')
        checks['historical_route_refuses_before_discovery'] = response[0] == 400 and len(state['records']) == before
        shutdown = auth.local_request(app_port, 'POST', '/shutdown',
            {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=5)
        _, stderr = process.communicate(timeout=10)
        checks['clean_shutdown_no_private_files'] = (shutdown[0] == 200
            and process.returncode == 0 and not stderr and not list(scratch.iterdir()))
        checks['only_valid_owned_sign_in_requests'] = bool(state['records']) and all(
            row['contract_ok'] and row['route'] in ('token', 'userinfo') for row in state['records'])
        completed = True
    finally:
        if process and process.poll() is None:
            process.terminate(); process.communicate(timeout=5)
        close_tls()
        expected_red = (completed and checks.get('intended_atomic_acquisition_available') is False
            and all(value for name, value in checks.items() if name != 'intended_atomic_acquisition_available'))
        receipt = {'phase': 'red', 'scope': 'historical actual app missing atomic acquisition admission',
            'checks': checks, 'all_passed': completed and bool(checks) and all(checks.values()),
            'expected_red_observed': expected_red, 'actual_http_status': status,
            'intended_http_status': 202, 'app_identity': identity,
            'tls_records': state['records'], 'verifier_sha256': sha(Path(__file__)),
            'console_contact': False, 'production_account_contact': False,
            'game_system_writes': False, 'archive_transfers': False,
            'runtime_or_installation_authority': False}
        (output / 'receipt.json').write_bytes(jobs.encoded(receipt))
    return receipt


def fixed_inputs():
    paths = {label: path for _, _, label, path in SOURCES}
    paths.update(base_toc=generations.BASE_TOC, base_header=generations.BASE_HEADER)
    values = {name: generations.identity(path) for name, path in paths.items()}
    for name, value in values.items():
        expected = generations.EXPECTED[name]
        if (value['bytes'], value['sha256']) != expected:
            raise RuntimeError('retained original input identity differs: ' + name)
    return values


def pair_oracles():
    oracle = json.loads(generations.ORACLE.read_bytes())
    archive = json.loads(generations.ARCHIVE_ORACLE.read_bytes())
    for path, expected in ((generations.ORACLE, generations.ORACLE_SHA256),
                           (generations.ARCHIVE_ORACLE, generations.ARCHIVE_ORACLE_SHA256)):
        if sha(path) != expected:
            raise RuntimeError('retained pair oracle identity differs')
    labels = ['Leon 2K original', 'Movement original']
    if any(value.get('result') != 'pass' or value.get('batch_conflict_free') is not True
           or [row.get('label') for row in value.get('sources', [])] != labels
           for value in (oracle, archive)):
        raise RuntimeError('retained pair oracle describes another source set')
    generations.oracle_artifacts(oracle, archive)
    return oracle, archive


def fixture_server(directory, state):
    """One serialized owned server; record only nonsensitive route facts."""
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(directory / 'fixture-leaf.pem', directory / 'fixture-leaf.key')
    context.set_servername_callback(lambda stream, name, _: setattr(stream, 'fixture_sni', name))
    condition = state['condition'] = threading.Condition()
    by_mod = {mod: (file_id, label) for mod, file_id, label, _ in SOURCES}

    class Handler(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'

        def log_message(self, *_):
            pass

        def handle_request(self, method):
            parsed = urlsplit(self.path)
            record = {'method': method, 'route': 'unrecognized', 'contract_ok': False,
                      'response_sent': False, 'premium': state['premium']}
            sni = getattr(self.connection, 'fixture_sni', '')
            users = self.headers.get('Host') == 'users.nexusmods.com' and sni == 'users.nexusmods.com'
            api = self.headers.get('Host') == 'api.nexusmods.com' and sni == 'api.nexusmods.com'
            forbidden = any(name in self.headers for name in ('apikey', 'Cookie', 'Proxy-Authorization'))
            bearer = self.headers.get('Authorization') == 'Bearer owned-jobs-access-token'
            no_account = not forbidden and 'Authorization' not in self.headers
            length = int(self.headers.get('Content-Length', '0'))
            incoming = self.rfile.read(length) if length else b''
            status, body = 404, {}
            if method == 'POST' and parsed.path == '/oauth/token':
                record['route'] = 'token'
                pairs = parse_qsl(incoming.decode('ascii'), keep_blank_values=True)
                form = dict(pairs)
                challenge = base64.urlsafe_b64encode(hashlib.sha256(
                    form.get('code_verifier', '').encode()).digest()).rstrip(b'=').decode()
                record['contract_ok'] = (users and no_account and not parsed.query
                    and len(pairs) == len(form) == 5
                    and set(form) == {'grant_type', 'redirect_uri', 'client_id', 'code', 'code_verifier'}
                    and form.get('grant_type') == 'authorization_code'
                    and form.get('redirect_uri') == state['config']['redirect_uri']
                    and form.get('client_id') == state['config']['client_id']
                    and form.get('code') == 'fixture-code-initial'
                    and challenge == state.get('expected_challenge'))
                status, body = 200, {'access_token': 'owned-jobs-access-token',
                    'refresh_token': 'owned-jobs-refresh-token', 'token_type': 'Bearer', 'expires_in': 3600}
            elif method == 'GET' and parsed.path == '/oauth/userinfo':
                record['route'] = 'userinfo'
                record['contract_ok'] = users and bearer and not forbidden and not parsed.query and not incoming
                status, body = 200, {'sub': '12345', 'name': 'Owned acquisition fixture',
                    'membership_roles': ['premium'] if state['premium'] else [],
                    'preferences': {'adult': False}}
            elif method == 'GET' and re.fullmatch(r'/archive/(20|21)', parsed.path):
                file_id = int(parsed.path.rsplit('/', 1)[1])
                record.update(route='archive', file_id=file_id, no_account_headers=no_account)
                record['contract_ok'] = api and no_account and not incoming and parsed.query == 'fixture_signature=private'
                status, body = 200, state['archives'][file_id]
                record['archive_bytes'] = len(body)
            elif method == 'GET':
                match = re.fullmatch(r'/v1/games/(silenthilltownfall)/mods/(6|7)(?:/files(?:/(20|21)/download_link)?)?', parsed.path)
                if match:
                    game, mod_id = match[1], int(match[2])
                    file_id, _ = by_mod[mod_id]
                    record.update(source_game=game, source_mod_id=mod_id)
                    record['contract_ok'] = api and bearer and not forbidden and not parsed.query and not incoming
                    if match[3]:
                        record.update(route='download_link', file_id=int(match[3]))
                        record['contract_ok'] = record['contract_ok'] and int(match[3]) == file_id
                        status, body = 200, [{'name': 'Owned mirror', 'short_name': 'owned',
                            'URI': 'https://api.nexusmods.com/archive/' + str(file_id) + '?fixture_signature=private'}]
                    elif parsed.path.endswith('/files'):
                        record['route'] = 'files'
                        selected_id = file_id + 100 if state.get('primary_changed') else file_id
                        file = plans.file_record(selected_id)
                        file['size_kb'] = len(state['archives'][file_id]) // 1024
                        status, body = 200, {'files': [file]}
                    else:
                        record['route'] = 'mod'
                        status, body = 200, requirements.source_mod(game, mod_id, False)
            elif method == 'POST' and parsed.path == '/v2/graphql':
                record['route'] = 'requirements'
                request = json.loads(incoming)
                variables = request.get('variables', {})
                game, mod_id = variables.get('gameDomainName'), variables.get('modId')
                expected = jobs.encoded({'query': requirements.QUERY,
                    'variables': {'gameDomainName': game, 'modId': mod_id}})
                record['contract_ok'] = (api and bearer and not forbidden and not parsed.query
                    and self.headers.get('Content-Type') == 'application/json'
                    and incoming == expected and game == 'silenthilltownfall' and type(mod_id) is int and mod_id in by_mod)
                record.update(source_game=game, source_mod_id=mod_id)
                nodes = [plans.requirement_record(7, 6, game)] if mod_id == 7 else []
                status, body = 200, {'data': {'modRequirements': {'nexusRequirements': {
                    'nodes': nodes, 'nodesCount': len(nodes), 'totalCount': len(nodes)}, 'dlcRequirements': []}}}
            record['status'] = status
            body = body if isinstance(body, bytes) else jobs.encoded(body)
            try:
                self.send_response(status)
                self.send_header('Content-Type', 'application/octet-stream' if record['route'] == 'archive' else 'application/json')
                self.send_header('Content-Length', str(len(body)))
                self.send_header('Connection', 'close'); self.end_headers()
                for offset in range(0, len(body), 16384):
                    self.wfile.write(body[offset:offset + 16384])
                self.wfile.flush(); record['response_sent'] = True
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                record['peer_closed'] = True
            finally:
                with condition:
                    state['records'].append(record)
                    condition.notify_all()
                self.close_connection = True

        def do_GET(self):
            self.handle_request('GET')

        def do_POST(self):
            self.handle_request('POST')

    server = HTTPServer(('127.0.0.1', 0), Handler)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': .1}, daemon=True)
    thread.start()

    def close():
        server.shutdown(); server.server_close(); thread.join(timeout=3)

    return server.server_port, close


def wait_for_sources(state, premium, timeout=45):
    def ready():
        if premium:
            return sum(row['route'] == 'archive' and row['response_sent'] for row in state['records']) >= 2
        return sum(row['route'] == 'userinfo' and row['response_sent'] for row in state['records']) >= 3
    with state['condition']:
        return state['condition'].wait_for(ready, timeout=timeout)


def run_account(binary, config_path, scratch, output, archives, premium, oracle, archive_oracle):
    config = json.loads(config_path.read_bytes())
    before_scratch = (scratch.stat().st_dev, scratch.stat().st_ino)
    if list(scratch.iterdir()):
        raise RuntimeError('each account run requires fresh empty scratch')
    state = {'config': config, 'premium': premium, 'records': [], 'archives': archives}
    port, close_tls = fixture_server(config_path.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; public = []; calls = []; process = None; completed = False; summary = {}

    def check(name, passed):
        checks[name] = bool(passed)

    def call(method, target, headers=None, payload=b'', timeout=180):
        calls.append({'method': method, 'route': target.split('?')[0]})
        response = auth.local_request(app_port, method, target, headers, payload, timeout=timeout)
        public.append(response[2])
        return response

    try:
        process, initial = generations.start_app(binary, app_port, port)
        headers = generations.sign_in(app_port, config, state)
        check('actual_owned_pkce_account', initial[0] == 200 and any(
            row['route'] == 'userinfo' and row['contract_ok'] for row in state['records']))
        intent = acquisition_intent('owned_acquisition_' + ('premium' if premium else 'free') + '_001')
        queued = call('POST', '/api/acquisitions', headers, jobs.encoded(intent), timeout=20)
        acquisition = generations.extract_json(queued).get('acquisition', {})
        source_ids = acquisition.get('source_job_ids', [])
        selected = acquisition.get('selected_sources', [])
        expected_sources = [(6, 20), (7, 21)]
        canonical_ids = (len(source_ids) == 2 and len(set(source_ids)) == 2 and all(
            isinstance(value, str) and re.fullmatch('[a-f0-9]{32}', value) for value in source_ids))
        check('atomic_two_source_prerequisite_first_receipt', queued[0] == 202 and
            acquisition.get('state') == 'accepted' and acquisition.get('game') == 'silenthilltownfall' and
            acquisition.get('idempotency_key') == intent['idempotency_key'] and canonical_ids and
            len(selected) == 2 and [(row.get('mod_id'), row.get('file_id')) for row in selected] == expected_sources and
            [row.get('job_id') for row in selected] == source_ids and all(
                set(row) == {'game', 'mod_id', 'file_id', 'job_id'} and row['game'] == 'silenthilltownfall' for row in selected))
        authority = {'compatibility': 'unresolved', 'version_satisfaction': 'unresolved',
            'download_authorized': False, 'runtime_supported': False, 'installation_supported': False,
            'acquisition_order_is_game_load_order': False}
        check('accepted_receipt_grants_no_download_game_or_install_authority', all(
            acquisition.get(key) == value for key, value in authority.items()))
        if not canonical_ids:
            raise RuntimeError('accepted group did not contain exactly two valid child IDs')

        # There are deliberately no app HTTP calls inside this interval. The
        # first child may start after the POST response; the second needs idle
        # listener progress. Provider events alone release the wait.
        calls_before_idle = len(calls)
        idle_start = time.monotonic()
        idle_ready = wait_for_sources(state, premium)
        idle_seconds = time.monotonic() - idle_start
        calls_after_idle = len(calls)
        check('both_children_progress_without_any_http_poll', idle_ready and calls_before_idle == calls_after_idle)
        summary['idle_window'] = {'driver_requests_before': calls_before_idle,
            'driver_requests_after': calls_after_idle, 'seconds': idle_seconds,
            'provider_condition_met': idle_ready}
        rows = []
        for identifier in source_ids:
            response = call('GET', '/api/jobs/' + identifier, headers)
            row = generations.extract_json(response)
            check('child_' + str(len(rows)) + '_owner_readable', response[0] == 200 and row.get('id') == identifier)
            rows.append(row)
        listing = generations.extract_json(call('GET', '/api/jobs', headers))
        check('group_has_exactly_two_global_sources_for_owner', len(listing.get('items', [])) == 2 and
            {row.get('id') for row in listing.get('items', [])} == set(source_ids))
        gen_list = generations.extract_json(call('GET', '/api/generations', headers))
        check('acquisition_does_not_implicitly_create_generation', gen_list.get('items') == [])

        before_replay = len(state['records'])
        state['primary_changed'] = True
        replay = call('POST', '/api/acquisitions', headers, jobs.encoded(intent), timeout=20)
        check('replay_binds_original_ids_without_rediscovery_or_redownload', replay[0] == 202 and
            generations.extract_json(replay).get('acquisition') == acquisition and len(state['records']) == before_replay)
        changed = call('POST', '/api/acquisitions', headers, jobs.encoded({**intent, 'mod_ids': [6]}), timeout=20)
        check('changed_same_key_conflicts_without_provider_work', changed[0] == 409 and len(state['records']) == before_replay)
        untrusted = call('POST', '/api/acquisitions', headers, jobs.encoded({**intent, 'plan': {'acquisition_ready': True}}), timeout=20)
        check('client_plan_is_refused_before_provider', untrusted[0] == 400 and len(state['records']) == before_replay)
        wrong_csrf = call('POST', '/api/acquisitions', {**headers, 'X-CSRF-Token': 'x' * 43}, jobs.encoded(intent), timeout=20)
        check('wrong_csrf_is_refused_before_replay', wrong_csrf[0] == 403 and len(state['records']) == before_replay)
        check('acquisition_get_is_method_refused', call('GET', '/api/acquisitions', headers, timeout=20)[0] == 405)
        other = generations.headers_for_new_session(app_port, config)
        check('other_local_cookie_cannot_read_group_child',
            call('GET', '/api/jobs/' + source_ids[0], other, timeout=20)[0] == 404)

        archive_rows = [row for row in state['records'] if row['route'] == 'archive']
        link_rows = [row for row in state['records'] if row['route'] == 'download_link']
        user_rows = [row for row in state['records'] if row['route'] == 'userinfo']
        check('each_child_independently_rechecks_live_account', len(user_rows) == 3 and all(
            row['premium'] is premium and row['response_sent'] for row in user_rows))
        if premium:
            check('exactly_two_credential_free_original_archive_downloads',
                [row.get('file_id') for row in archive_rows] == [20, 21] and
                [row.get('file_id') for row in link_rows] == [20, 21] and all(
                    row.get('no_account_headers') is True and row['contract_ok'] and row['response_sent'] for row in archive_rows))
            counts = Counter((row['route'], row.get('source_mod_id')) for row in state['records'])
            check('each_worker_rechecks_visibility_after_plan', all(
                counts[('mod', mod)] == 2 and counts[('files', mod)] == 1 and counts[('requirements', mod)] == 1
                for mod, _, _, _ in SOURCES))
            for index, (_, _, label, _) in enumerate(SOURCES):
                row = rows[index]
                expected = generations.EXPECTED[label]
                leaf = scratch / ('mb-job-' + source_ids[index] + '.archive')
                check(label + '_unchanged_original_download_and_real_analysis',
                    row.get('kind') == 'download_analyze' and row.get('status') == 'analyzed' and
                    (row.get('bytes_downloaded'), row.get('sha256')) == expected and leaf.is_file() and
                    not leaf.is_symlink() and (leaf.stat().st_size, generations.sha256_file(leaf)) == expected and
                    row.get('analysis_report', {}).get('operation_state') == 'supported' and
                    row.get('runtime_supported') is False and row.get('installation_supported') is False)
                check(label + '_no_single_source_preparation', 'preparation' not in row)
            # Only these exact accepted IDs feed the existing explicit route.
            generation = call('POST', '/api/generations', headers, jobs.encoded({
                'kind': 'prepare_iostore_generation', 'source_job_ids': source_ids,
                'idempotency_key': 'owned_acquisition_exact_generation_001'}))
            admitted = generations.extract_json(generation)
            generation_id = admitted.get('id', '')
            check('one_explicit_generation_from_exact_receipt_ids', generation[0] == 202 and
                admitted.get('source_job_ids') == source_ids and admitted.get('status') == 'queued' and
                bool(re.fullmatch('[a-f0-9]{32}', generation_id)))
            done = call('GET', '/api/generations/' + generation_id, headers)
            prepared = generations.extract_json(done)
            directory = generations.find_generation_output(scratch)
            matched, artifacts = generations.check_prepared_generation(prepared, directory, oracle, archive_oracle)
            check('one_private_generation_full_readback_matches_retained_pair_oracle',
                done[0] == 200 and prepared.get('source_job_ids') == source_ids and matched)
            final_list = generations.extract_json(call('GET', '/api/generations', headers))
            check('exactly_one_generation_and_no_extra_archive_transfer',
                len(final_list.get('items', [])) == 1 and final_list['items'][0].get('id') == generation_id and
                sum(row['route'] == 'archive' for row in state['records']) == 2)
            summary.update(generation_id=generation_id, generation_artifacts=artifacts)
        else:
            check('fresh_free_account_blocks_both_sources_without_grant', all(
                row.get('status') == 'needs_nexus_authorization' and row.get('error', {}).get('code') == 'download_authorization'
                and 'sha256' not in row and 'bytes_downloaded' not in row and 'preparation' not in row for row in rows))
            check('free_group_has_zero_link_archive_or_private_subset', not archive_rows and not link_rows and not list(scratch.iterdir()))
            denied = call('POST', '/api/generations', headers, jobs.encoded({
                'kind': 'prepare_iostore_generation', 'source_job_ids': source_ids,
                'idempotency_key': 'owned_acquisition_free_generation_refused'}), timeout=20)
            check('free_unacquired_group_cannot_prepare_generation', denied[0] == 403 and
                generations.extract_json(call('GET', '/api/generations', headers)).get('items') == [] and not list(scratch.iterdir()))

        forbidden_public = (b'owned-jobs-access-token', b'owned-jobs-refresh-token', b'fixture_signature',
            b'https://api.nexusmods.com/archive/', b'fixture-sensitive-plan-notes', b'private-provider-hint',
            str(scratch).encode(), str(config_path).encode(), str(generations.LEON).encode(), str(generations.MOVEMENT).encode())
        check('all_public_receipts_and_reports_are_bounded_and_private', all(
            len(body) <= 98304 and not any(value in body for value in forbidden_public) for body in public))
        check('only_exact_owned_authenticated_tls_routes', bool(state['records']) and all(
            row['contract_ok'] and row['response_sent'] for row in state['records']))
        shutdown = call('POST', '/shutdown', {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=20)
        _, stderr = process.communicate(timeout=20)
        check('shutdown_removes_only_owned_originals_and_generation', shutdown[0] == 200 and process.returncode == 0 and
            not stderr and not list(scratch.iterdir()))
        check('private_directory_identity_preserved', before_scratch == (scratch.stat().st_dev, scratch.stat().st_ino))
        summary.update(source_job_ids=source_ids, selected_sources=selected,
            child_states=[row.get('status') for row in rows], acquisition_state=acquisition.get('state'))
        completed = True
    finally:
        if process and process.poll() is None:
            process.terminate(); process.communicate(timeout=8)
        close_tls()
        receipt = {'scope': 'premium' if premium else 'fresh_free_account_no_grant',
            'checks': checks, 'all_passed': completed and bool(checks) and all(checks.values()),
            'summary': summary, 'tls_records': state['records'], 'driver_requests': calls,
            'production_account_contacted': False, 'console_executed': False,
            'game_system_writes': False, 'runtime_or_installation_authority': False}
        (output / ('premium' if premium else 'free') / 'receipt.json').write_bytes(jobs.encoded(receipt))
    return receipt


def verify_green(profile, config_path, output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    (output / 'premium').mkdir(mode=0o700); (output / 'free').mkdir(mode=0o700)
    profile = profile.resolve(strict=True); config_path = config_path.resolve(strict=True)
    import nexus_iostore_jobs_verify as iostore
    iostore.assert_fixture_config(config_path, iostore.CAPTURED_PROFILE)
    scratch = requirements.profile_scratch(profile)
    if scratch != (config_path.parent / 'scratch').resolve(strict=True):
        raise RuntimeError('configured scratch does not match this owned fixture')
    before = fixed_inputs()
    oracle, archive_oracle = pair_oracles()
    archives = {file_id: path.read_bytes() for _, file_id, _, path in SOURCES}
    auth.MATRIX_DIR = output / 'relink'
    binary, app_identity = auth.relink_with_socket_shim(profile)
    premium = run_account(binary, config_path, scratch, output, archives, True, oracle, archive_oracle)
    free = run_account(binary, config_path, scratch, output, archives, False, oracle, archive_oracle)
    after = fixed_inputs()
    checks = {f'premium:{name}': passed for name, passed in premium['checks'].items()}
    checks.update({f'free:{name}': passed for name, passed in free['checks'].items()})
    checks['all_original_archive_and_capture_identities_unchanged'] = before == after
    receipt = {'phase': 'green', 'scope': 'actual_atomic_acquisition_idle_download_and_explicit_generation',
        'checks': checks, 'all_passed': premium['all_passed'] and free['all_passed'] and all(checks.values()),
        'app_identity': app_identity, 'fixed_inputs_before': before, 'fixed_inputs_after': after,
        'pair_oracle_sha256': generations.ORACLE_SHA256,
        'archive_pair_oracle_sha256': generations.ARCHIVE_ORACLE_SHA256,
        'premium_receipt_sha256': sha(output / 'premium/receipt.json'),
        'free_receipt_sha256': sha(output / 'free/receipt.json'),
        'verifier_sha256': sha(Path(__file__)), 'synthetic_prerequisite_metadata': True,
        'console_executed': False, 'production_account_contacted': False,
        'game_system_writes': False, 'runtime_or_installation_authority': False}
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
        'failed_checks': [name for name, passed in receipt['checks'].items() if not passed]}))
    return 0 if receipt.get('expected_red_observed') or receipt['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
