#!/usr/bin/env python3
"""Actual app acquisition planning against owned sign-in/metadata TLS fixtures.

No production account, console, archive transfer or game/system publication.
Historical RED binds every manifest source to its commit, independently of
concurrent current source edits. GREEN binds the actual composed current app.
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
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qsl, urlsplit

import nexus_auth_verify as auth
import nexus_jobs_verify as jobs
import nexus_iostore_generations_verify as generations
import nexus_requirements_verify as requirements

ROOT = auth.ROOT
SPIKE = ROOT / '.superpowers/native-dependency-spike/native-nexus-acquisition-plans-1'
CONFIG = SPIKE / 'fixture-config-1/oauth-config.json'
RED_PROFILE = requirements.SPIKE / 'configured-host-1/desktop_fixture-download'
RED_CONFIG = requirements.SPIKE / 'fixture-config-1/oauth-config.json'
RED_RECEIPT = requirements.SPIKE / 'green-1/receipt.json'
RED_HEAD = 'a1c9543eda53a35239f9d7ea217970ccc87cf43f'
RED_ARTIFACT_SHA = '7db757e346041b50d6ae868306d274f9e3f7b344bc8a341b449292358a7f226d'
RED_BINARY_SHA = '82e9512101829ebf6f5b0bba7328f3ecc4607fc9ddb08eb1515ffc05da61c7da'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def historical_binary():
    manifest = json.loads((RED_PROFILE / 'manifest.json').read_bytes())
    identity = json.loads(RED_RECEIPT.read_bytes())['app_identity']
    binary = Path(identity['private_directory']) / 'modbridge-native-auth-fixture'
    if sha(RED_PROFILE / manifest['artifact']) != RED_ARTIFACT_SHA or sha(binary) != RED_BINARY_SHA:
        raise RuntimeError('historical application bytes differ')
    tracked = 0
    for path, digest in manifest['source_sha256'].items():
        original = subprocess.run(['git', 'show', RED_HEAD + ':' + path], cwd=ROOT, capture_output=True)
        if original.returncode or hashlib.sha256(original.stdout).hexdigest() != digest:
            raise RuntimeError('historical source closure differs from its commit')
        tracked += 1
    if tracked != 136:
        raise RuntimeError('historical source count differs')
    return binary, {'historical_head': RED_HEAD, 'tracked_source_checks': tracked,
                    'profile_artifact_sha256': RED_ARTIFACT_SHA,
                    'fixture_binary_sha256': RED_BINARY_SHA, 'test_only_socket_redirect': True}


def graph(scenario):
    if scenario in ('diamond', 'explicit_diamond', 'malformed_child'):
        return {62: [42, 43], 42: [44], 43: [44], 44: []}
    if scenario in ('hidden_child', 'partial_child'):
        return {62: [42], 42: []}
    if scenario == 'cycle':
        return {62: [42], 42: [62]}
    if scenario == 'closure_limit':
        return {node: [node + 1] for node in range(62, 70)}
    if scenario == 'cumulative_deadline':
        return {62: [42], 42: [43], 43: [44], 44: []}
    return {62: []}


def file_record(file_id, primary=True, category=1, category_name='MAIN', version='current-label'):
    return {'file_id': file_id, 'name': 'Owned file label', 'file_name': 'owned-fixture.zip',
            'version': version, 'description': '', 'size_kb': 1,
            'is_primary': primary, 'category_id': category, 'category_name': category_name}


def files(scenario, mod_id):
    current = file_record(mod_id * 100 + 1)
    if scenario == 'historical_explicit':
        return [current, file_record(9223372036854775807, False, 7, 'ARCHIVED', 'historical-label')]
    if scenario == 'unknown_hint':
        current['is_primary'] = 'true'
    elif scenario == 'ambiguous_files':
        return [current, file_record(mod_id * 100 + 2)]
    elif scenario == 'archived_only':
        return [file_record(mod_id * 100 + 9, True, 7, 'ARCHIVED')]
    elif scenario == 'contradictory_hint':
        current['category_name'] = 'OPTION'
    elif scenario == 'duplicate_files':
        return [current, dict(current)]
    return [current]


def requirement_record(source, child, game, target_game=None, external=False):
    return {'id': f'edge-{source}-{child}', 'externalRequirement': external,
            'gameId': target_game if target_game is not None else game,
            'modId': str(child), 'modName': 'Owned prerequisite label',
            'notes': 'fixture-sensitive-plan-notes',
            'url': 'https://example.invalid/private-provider-hint'}


def requirement_response(scenario, game, mod_id):
    nodes = [requirement_record(mod_id, child, game) for child in graph(scenario).get(mod_id, [])]
    dlc = []
    if scenario == 'foreign_external_dlc':
        nodes = [requirement_record(mod_id, 42, game, 'skyrim'),
                 requirement_record(mod_id, 'external', game, '', True)]
        dlc = [{'gameExpansion': {'gameId': game, 'id': 'owned-dlc', 'name': 'Owned DLC label'},
                'notes': 'fixture-sensitive-plan-notes'}]
    elif scenario == 'large_child':
        nodes = [requirement_record(mod_id, 2147483648, game)]
    body = {'data': {'modRequirements': {'nexusRequirements': {
        'nodes': nodes, 'nodesCount': len(nodes), 'totalCount': len(nodes)}, 'dlcRequirements': dlc}}}
    if scenario == 'partial_child' and mod_id == 42:
        body['data']['modRequirements']['nexusRequirements']['totalCount'] += 1
    if scenario == 'malformed_child' and mod_id == 43:
        body['errors'] = [{'message': 'fixture-sensitive-provider-detail'}]
    return body


def fixture_server(certs, state):
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_2
    context.load_cert_chain(certs / 'fixture-leaf.pem', certs / 'fixture-leaf.key')
    context.set_servername_callback(lambda stream, name, _: setattr(stream, 'fixture_sni', name))

    class Handler(BaseHTTPRequestHandler):
        protocol_version = 'HTTP/1.1'
        def log_message(self, *_):
            pass
        def handle_request(self, method):
            scenario = state['scenario']
            parsed = urlsplit(self.path)
            record = {'method': method, 'route': 'unrecognized', 'contract_ok': False,
                      'response_sent': False, 'scenario': scenario}
            state['records'].append(record)
            forbidden = any(k in self.headers for k in ('apikey', 'Cookie', 'Proxy-Authorization'))
            bearer = self.headers.get('Authorization') == 'Bearer owned-jobs-access-token'
            sni = getattr(self.connection, 'fixture_sni', '')
            users = self.headers.get('Host') == 'users.nexusmods.com' and sni == 'users.nexusmods.com'
            api = self.headers.get('Host') == 'api.nexusmods.com' and sni == 'api.nexusmods.com'
            length = int(self.headers.get('Content-Length', '0'))
            incoming = self.rfile.read(length) if length else b''
            status, body = 404, {}
            if method == 'POST' and parsed.path == '/oauth/token':
                record['route'] = 'token'
                pairs = parse_qsl(incoming.decode('ascii'), keep_blank_values=True)
                form = dict(pairs)
                challenge = base64.urlsafe_b64encode(hashlib.sha256(
                    form.get('code_verifier', '').encode()).digest()).rstrip(b'=').decode()
                record['contract_ok'] = (users and not forbidden and not bearer and not parsed.query
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
                status, body = 200, {'sub': '12345', 'name': 'Owned planning fixture',
                    'membership_roles': [], 'preferences': {'adult': False}}
            elif method == 'GET':
                match = re.fullmatch(r'/v1/games/([a-z0-9_-]+)/mods/([1-9][0-9]*)(/files)?', parsed.path)
                if match:
                    game, mod_id = match[1], int(match[2])
                    record['route'] = 'files' if match[3] else 'mod'
                    record['source_game'] = game; record['source_mod_id'] = mod_id
                    record['contract_ok'] = api and bearer and not forbidden and not parsed.query and not incoming
                    status, body = (200, {'files': files(scenario, mod_id)}) if match[3] else (
                        200, requirements.source_mod(game, mod_id, scenario == 'hidden_child' and mod_id == 42))
            elif method == 'POST' and parsed.path == '/v2/graphql':
                record['route'] = 'requirements'
                request = json.loads(incoming)
                variables = request.get('variables', {})
                game, mod_id = variables.get('gameDomainName'), variables.get('modId')
                expected = jobs.encoded({'query': requirements.QUERY, 'variables': {'gameDomainName': game, 'modId': mod_id}})
                record['contract_ok'] = (api and bearer and not forbidden and not parsed.query
                    and self.headers.get('Content-Type') == 'application/json'
                    and set(request) == {'query', 'variables'} and set(variables) == {'gameDomainName', 'modId'}
                    and request['query'] == requirements.QUERY and incoming == expected
                    and isinstance(game, str) and re.fullmatch('[a-z0-9][a-z0-9_-]{0,79}', game) is not None
                    and type(mod_id) is int and 1 <= mod_id <= 2147483647)
                record['source_game'] = game; record['source_mod_id'] = mod_id
                status, body = 200, requirement_response(scenario, game, mod_id)
            if scenario == 'cumulative_deadline' and record['route'] in ('mod', 'files', 'requirements'):
                # Every individual wait is below the total budget. Their sum
                # crosses it after real visible/source/file/requirement reads.
                time.sleep(state['config']['timeout_ms'] / 6000)
            record['status'] = status
            body = jobs.encoded(body)
            try:
                self.send_response(status); self.send_header('Content-Type', 'application/json')
                self.send_header('Content-Length', str(len(body)))
                self.send_header('Connection', 'close'); self.end_headers()
                self.wfile.write(body); self.wfile.flush(); record['response_sent'] = True
            except (BrokenPipeError, ConnectionResetError, ssl.SSLError):
                record['peer_closed'] = True
            finally:
                self.close_connection = True
        def do_GET(self):
            self.handle_request('GET')
        def do_POST(self):
            self.handle_request('POST')

    server = ThreadingHTTPServer(('127.0.0.1', 0), Handler)
    server.socket = context.wrap_socket(server.socket, server_side=True)
    thread = threading.Thread(target=server.serve_forever, kwargs={'poll_interval': .1}, daemon=True)
    thread.start()
    def close():
        server.shutdown(); server.server_close(); thread.join(timeout=3)
    return server.server_port, close


def verify_red(output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    binary, identity = historical_binary()
    config = json.loads(RED_CONFIG.read_bytes())
    scratch = requirements.profile_scratch(RED_PROFILE)
    state = {'config': config, 'scenario': 'complete', 'records': []}
    fixture_port, close_tls = requirements.fixture_server(RED_CONFIG.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; process = None; status = None; completed = False
    try:
        process, initial = generations.start_app(binary, app_port, fixture_port)
        headers = generations.sign_in(app_port, config, state)
        checks['actual_owned_pkce_login'] = initial[0] == 200 and any(r['route'] == 'token' and r['contract_ok'] for r in state['records'])
        before = len(state['records'])
        intent = {'game': 'silenthilltownfall', 'mod_ids': [62], 'file_choices': [], 'file_policy': 'unique_current_primary'}
        response = auth.local_request(app_port, 'POST', '/api/acquisition-plan', headers, jobs.encoded(intent), timeout=12)
        status = response[0]
        checks['intended_acquisition_plan_available'] = response[0] == 200 and 'plan' in json.loads(response[2])
        checks['old_route_refuses_before_any_metadata'] = response[0] == 400 and len(state['records']) == before
        shutdown = auth.local_request(app_port, 'POST', '/shutdown', {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=5)
        _, stderr = process.communicate(timeout=10)
        checks['clean_shutdown_no_private_files'] = shutdown[0] == 200 and process.returncode == 0 and not stderr and not list(scratch.iterdir())
        checks['all_owned_tls_requests_valid'] = bool(state['records']) and all(r['contract_ok'] for r in state['records'])
        completed = True
    finally:
        if process and process.poll() is None:
            process.terminate(); process.communicate(timeout=5)
        close_tls()
        expected_red = (completed and checks.get('intended_acquisition_plan_available') is False and
                        all(value for name, value in checks.items() if name != 'intended_acquisition_plan_available'))
        receipt = {'phase': 'red', 'scope': 'actual pinned application owned TLS sign-in, missing acquisition-plan framing/route',
                   'checks': checks, 'actual_http_status': status, 'intended_http_status': 200,
                   'all_passed': completed and bool(checks) and all(checks.values()), 'expected_red_observed': expected_red,
                   'app_identity': identity, 'tls_records': state['records'], 'verifier_sha256': sha(Path(__file__)),
                   'console_contact': False, 'production_account_contact': False, 'game_system_writes': False,
                   'archive_transfers': False, 'download_or_install_authority': False}
        (output / 'receipt.json').write_bytes(jobs.encoded(receipt))
    return receipt


def verify_green(profile, config_path, output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    config = json.loads(config_path.read_bytes())
    auth.MATRIX_DIR = output / 'relink'
    binary, app_identity = auth.relink_with_socket_shim(profile)
    scratch = requirements.profile_scratch(profile)
    before_identity = (scratch.stat().st_dev, scratch.stat().st_ino)
    state = {'config': config, 'scenario': 'diamond', 'records': []}
    fixture_port, close_tls = fixture_server(config_path.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; process = None; completed = False; public = []; cases = []

    def check(name, okay):
        checks[name] = bool(okay)

    def call(method, target, headers=None, body=b'', **kwargs):
        response = auth.local_request(app_port, method, target, headers, body, timeout=12, **kwargs)
        public.append(response[2])
        return response

    def planned(scenario, roots=None, choices=None, policy='unique_current_primary', game='silenthilltownfall', split=False):
        state['scenario'] = scenario
        start = len(state['records'])
        body = {'game': game, 'mod_ids': roots or [62], 'file_choices': choices or [], 'file_policy': policy}
        started = time.monotonic()
        response = call('POST', '/api/acquisition-plan', headers, jobs.encoded(body), split_body=split)
        payload = json.loads(response[2])
        plan = payload.get('plan', {})
        record = {'scenario': scenario, 'http_status': response[0], 'seconds': time.monotonic() - started,
                  'acquisition_ready': plan.get('acquisition_ready'),
                  'selected_mod_ids': [row.get('mod_id') for row in plan.get('selected_sources', [])],
                  'unresolved_reasons': sorted({row.get('reason', '') for row in plan.get('unresolved', [])})}
        cases.append(record)
        return response, payload, plan, state['records'][start:], record

    def unresolved(scenario, reason, **kwargs):
        response, _, plan, records, record = planned(scenario, **kwargs)
        check(scenario + '_whole_unresolved_plan_without_ready_prefix', response[0] == 200 and
              plan.get('acquisition_ready') is False and plan.get('selected_sources') == [] and
              reason in record['unresolved_reasons'])
        return plan, records

    try:
        process, initial = generations.start_app(binary, app_port, fixture_port)
        headers = generations.sign_in(app_port, config, state)
        check('actual_owned_pkce_login', initial[0] == 200 and any(r['route'] == 'token' and r['contract_ok'] for r in state['records']))
        response, _, plan, records, _ = planned('diamond', split=True)
        expected_order = [44, 42, 43, 62]
        selected = plan.get('selected_sources', [])
        check('complete_diamond_prerequisite_first_stable_source_order', response[0] == 200 and
              plan.get('schema_version') == 1 and plan.get('scope') == 'nexus_acquisition_metadata' and
              plan.get('root_mod_ids') == [62] and plan.get('file_policy') == 'unique_current_primary' and
              plan.get('discovery_complete') is True and plan.get('acquisition_ready') is True and plan.get('unresolved') == [] and
              [row.get('mod_id') for row in selected] == expected_order and
              [row.get('file_id') for row in selected] == [node * 100 + 1 for node in expected_order] and
              all(row.get('selection_basis') == 'unique_current_primary' for row in selected))
        counts = Counter((row['route'], row.get('source_mod_id')) for row in records if row['route'] in ('mod', 'files', 'requirements'))
        check('diamond_fetches_each_identity_once', len(plan.get('nodes', [])) == 4 and
              counts == Counter({(kind, node): 1 for kind in ('mod', 'files', 'requirements') for node in expected_order}))
        relations = plan.get('relations', [])
        check('diamond_preserves_both_inbound_relation_provenances', len(relations) == 4 and
              {(row.get('from_mod_id'), row.get('relation_id')) for row in relations if row.get('target_mod_id') == '44'} ==
              {(42, 'edge-42-44'), (43, 'edge-43-44')} and all(row.get('satisfaction') == 'unresolved' for row in relations))
        check('visibility_precedes_each_files_and_requirements_read', all(
              (lambda kinds: bool(kinds) and kinds[0] == 'mod' and sorted(kinds[1:]) == ['files', 'requirements'])(
                  [r['route'] for r in records if r.get('source_mod_id') == node]) for node in expected_order))
        authority = {'compatibility': 'unresolved', 'version_satisfaction': 'unresolved',
                     'file_version_constraints_provided': False, 'download_authorized': False,
                     'installation_supported': False, 'acquisition_order_is_game_load_order': False}
        check('ready_plan_confers_no_game_version_download_or_install_authority', all(plan.get(key) == value for key, value in authority.items()) and
              all(set(row) == {'game', 'mod_id', 'file_id', 'selection_basis', 'file_version'} for row in selected))
        explicit = [{'mod_id': node, 'file_id': node * 100 + 1} for node in [62, 42, 43, 44]]
        response, _, plan, _, _ = planned('explicit_diamond', choices=explicit, policy='explicit_only')
        check('explicit_membership_choices_preserve_prerequisite_order', response[0] == 200 and plan.get('acquisition_ready') is True and
              [row.get('mod_id') for row in plan.get('selected_sources', [])] == expected_order and
              all(row.get('selection_basis') == 'explicit_file' for row in plan.get('selected_sources', [])))
        response, _, plan, _, _ = planned('historical_explicit', choices=[{'mod_id': 62, 'file_id': 9223372036854775807}])
        selected = plan.get('selected_sources', [])
        check('explicit_historical_signed63_file_is_membership_choice_labelonly_version', response[0] == 200 and plan.get('acquisition_ready') is True and
              len(selected) == 1 and selected[0].get('file_id') == 9223372036854775807 and
              selected[0].get('selection_basis') == 'explicit_file' and selected[0].get('file_version') == 'historical-label' and
              plan.get('version_satisfaction') == 'unresolved')
        for scenario, reason in [('unknown_hint', 'file_choice_missing'), ('ambiguous_files', 'file_choice_ambiguous'),
                                 ('archived_only', 'file_choice_missing'), ('contradictory_hint', 'file_choice_invalid')]:
            unresolved(scenario, reason)
        unresolved('explicit_only_missing', 'file_choice_missing', policy='explicit_only')
        unresolved('invalid_explicit', 'file_choice_invalid', choices=[{'mod_id': 62, 'file_id': 99999}])
        unresolved('unused_explicit', 'unused_file_choice', choices=[{'mod_id': 999, 'file_id': 99901}])
        _, hidden_records = unresolved('hidden_child', 'hidden_mod')
        check('hidden_child_refused_before_files_or_requirements_post',
              [row['route'] for row in hidden_records if row.get('source_mod_id') == 42] == ['mod'])
        unresolved('cycle', 'cycle')
        _, mixed_records = unresolved('foreign_external_dlc', 'cross_game')
        mixed = cases[-1]
        check('cross_domain_external_and_dlc_are_typed_unresolved_without_fetch',
              {'cross_game', 'external_requirement', 'dlc_requirement'} <= set(mixed['unresolved_reasons']) and
              all(row.get('source_game', 'silenthilltownfall') == 'silenthilltownfall' for row in mixed_records))
        _, large_records = unresolved('large_child', 'child_id_out_of_range')
        check('int32_unqueryable_child_makes_no_child_provider_request', all(row.get('source_mod_id', 62) == 62 for row in large_records))
        plan, closure_records = unresolved('closure_limit', 'closure_limit')
        check('closure_limit_bounds_visible_fetches_to_eight_identities', len(plan.get('nodes', [])) <= 8 and
              {row['source_mod_id'] for row in closure_records if row['route'] == 'mod'} == set(range(62, 70)) and
              not any(row.get('source_mod_id') == 70 for row in closure_records))
        for scenario in ('duplicate_files', 'malformed_child', 'partial_child'):
            response, payload, _, _, _ = planned(scenario)
            check(scenario + '_revokes_whole_plan', response[0] == 502 and 'plan' not in payload and b'selected_sources' not in response[2])
        response, _, plan, records, _ = planned('max_root', roots=[2147483647], game='othergame_2030')
        check('generic_domain_and_int32_max_source_id', response[0] == 200 and plan.get('acquisition_ready') is True and
              plan.get('game') == 'othergame_2030' and plan.get('root_mod_ids') == [2147483647] and
              any(row['route'] == 'requirements' and row['source_mod_id'] == 2147483647 for row in records))
        state['scenario'] = 'diamond'
        valid = jobs.encoded({'game': 'silenthilltownfall', 'mod_ids': [62], 'file_choices': [], 'file_policy': 'unique_current_primary'})
        state['scenario'] = 'at_body_cap'
        capped = call('POST', '/api/acquisition-plan', headers, valid + b' ' * (4096 - len(valid)))
        check('complete_4096_byte_JSON_frame_admitted', capped[0] == 200 and json.loads(capped[2]).get('plan', {}).get('acquisition_ready') is True)
        state['scenario'] = 'diamond'
        invalid_intents = [
            {'game': 'silenthilltownfall', 'mod_ids': [62, 62], 'file_choices': [], 'file_policy': 'unique_current_primary'},
            {'game': 'silenthilltownfall', 'mod_ids': [2147483648], 'file_choices': [], 'file_policy': 'unique_current_primary'},
            {'game': 'silenthilltownfall', 'mod_ids': [62], 'file_choices': [{'mod_id': 62, 'file_id': 1}, {'mod_id': 62, 'file_id': 2}], 'file_policy': 'unique_current_primary'},
            {'game': 'silenthilltownfall', 'mod_ids': [62], 'file_choices': [], 'file_policy': 'newest_filename'},
            {'game': 'silenthilltownfall', 'mod_ids': [62], 'file_choices': [], 'file_policy': 'unique_current_primary', 'url': 'https://example.invalid'},
        ]
        for index, invalid in enumerate(invalid_intents):
            before = len(state['records']); response = call('POST', '/api/acquisition-plan', headers, jobs.encoded(invalid))
            check('strict_intent_' + str(index) + '_before_provider', response[0] == 400 and 'plan' not in json.loads(response[2]) and len(state['records']) == before)
        duplicate = valid[:-1] + b',"game":"skyrim"}'
        before = len(state['records']); response = call('POST', '/api/acquisition-plan', headers, duplicate)
        check('duplicate_json_property_before_provider', response[0] == 400 and len(state['records']) == before)
        refusal_headers = [
            ('unknown_cookie', {**headers, 'Cookie': 'mb_session=' + 'x' * 43}, 401),
            ('missing_cookie', {key: value for key, value in headers.items() if key != 'Cookie'}, 401),
            ('wrong_csrf', {**headers, 'X-CSRF-Token': 'x' * 43}, 403),
            ('missing_csrf', {key: value for key, value in headers.items() if key != 'X-CSRF-Token'}, 403),
            ('missing_origin', {key: value for key, value in headers.items() if key != 'Origin'}, 403),
            ('foreign_origin', {**headers, 'Origin': 'http://wrong.example'}, 403),
            ('wrong_content_type', {**headers, 'Content-Type': 'text/plain'}, 400),
        ]
        for name, altered, status in refusal_headers:
            before = len(state['records']); response = call('POST', '/api/acquisition-plan', altered, valid)
            check(name + '_before_provider', response[0] == status and 'plan' not in json.loads(response[2]) and len(state['records']) == before)
        framing = [
            ('GET', '/api/acquisition-plan', headers, b'', 405),
            ('POST', '/api/acquisition-plan?extra=1', headers, valid, 400),
            ('POST', '/api/acquisition-plan/', headers, valid, 400),
            ('POST', '/api/acquisition-plan', {**headers, 'Content-Length': '4097'}, b'', 413),
            ('POST', '/api/acquisition-plan', list(headers.items()) + [('Content-Length', str(len(valid))), ('Content-Length', str(len(valid)))], valid, 400),
            ('POST', '/api/acquisition-plan', {**headers, 'Transfer-Encoding': 'chunked'}, valid, 400),
        ]
        for index, (method, target, altered, body, status) in enumerate(framing):
            before = len(state['records']); response = call(method, target, altered, body)
            check('HTTP_framing_' + str(index) + '_before_provider', response[0] == status and 'plan' not in json.loads(response[2]) and len(state['records']) == before)
        response, payload, _, records, record = planned('cumulative_deadline')
        check('one_cumulative_deadline_revokes_all_staged_sources', response[0] == 504 and 'plan' not in payload and b'selected_sources' not in response[2] and
              sum(row['response_sent'] for row in records if row['route'] in ('mod', 'files', 'requirements')) >= 3 and
              record['seconds'] < config['timeout_ms'] / 1000 + 2)
        response, _, plan, _, _ = planned('after_timeout')
        check('ordinary_timeout_preserves_reusable_account_and_owner', response[0] == 200 and plan.get('acquisition_ready') is True)
        check('all_public_bodies_private_and_bounded', all(len(body) <= 1048576 and not any(word in body for word in (
              b'owned-jobs-access-token', b'owned-jobs-refresh-token', b'fixture-sensitive-plan-notes', b'private-provider-hint',
              b'fixture-sensitive-provider-detail', str(scratch).encode(), str(config_path).encode())) for body in public))
        check('no_archive_or_download_link_requests', all(row['route'] in ('token', 'userinfo', 'mod', 'files', 'requirements') for row in state['records']))
        shutdown = call('POST', '/shutdown', {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'})
        _, stderr = process.communicate(timeout=10)
        check('clean_shutdown_no_private_files', shutdown[0] == 200 and process.returncode == 0 and not stderr and not list(scratch.iterdir()))
        check('fixture_directory_identity_unchanged', before_identity == (scratch.stat().st_dev, scratch.stat().st_ino))
        check('all_owned_tls_requests_canonical_authenticated', bool(state['records']) and all(row['contract_ok'] for row in state['records']))
        completed = True
    finally:
        if process and process.poll() is None:
            process.terminate(); process.communicate(timeout=5)
        close_tls()
        receipt = {'phase': 'green', 'scope': 'actual_app_owned_tls_acquisition_metadata_plan', 'checks': checks,
                   'all_passed': completed and bool(checks) and all(checks.values()), 'app_identity': app_identity,
                   'cases': cases, 'tls_records': state['records'], 'verifier_sha256': sha(Path(__file__)),
                   'console_contact': False, 'production_account_contact': False, 'game_system_writes': False,
                   'archive_transfers': False, 'download_or_install_authority': False}
        (output / 'receipt.json').write_bytes(jobs.encoded(receipt))
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('red', 'green'), required=True)
    parser.add_argument('--app-profile', type=Path)
    parser.add_argument('--oauth-config', type=Path, default=CONFIG)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.phase == 'green' and not args.app_profile:
        parser.error('--app-profile is required for GREEN')
    receipt = verify_red(args.output.resolve()) if args.phase == 'red' else verify_green(
        args.app_profile.resolve(), args.oauth_config.resolve(), args.output.resolve())
    print(json.dumps({'phase': args.phase, 'checks': len(receipt['checks']), 'all_passed': receipt['all_passed'],
                      'expected_red_observed': receipt.get('expected_red_observed'),
                      'failed_checks': [name for name, passed in receipt['checks'].items() if not passed]}))
    return 0 if receipt.get('expected_red_observed') or receipt['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
