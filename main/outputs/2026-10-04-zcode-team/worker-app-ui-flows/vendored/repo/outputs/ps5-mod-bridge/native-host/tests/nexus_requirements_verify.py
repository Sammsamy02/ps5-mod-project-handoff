#!/usr/bin/env python3
"""Actual native app requirements discovery against an owned HTTPS fixture.

No console, production account, archive transfer or game/system writes. The
fixture verifies the canonical read-only query, source visibility, completeness
and public failure boundaries; declared requirements confer no file choice.
"""
import argparse
import base64
import hashlib
import json
import os
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

ROOT = auth.ROOT
SPIKE = ROOT / '.superpowers/native-dependency-spike/native-nexus-requirements-1'
CONFIG = SPIKE / 'fixture-config-1/oauth-config.json'
RED_PROFILE = ROOT / '.superpowers/native-dependency-spike/native-iostore-generations-1/configured-host-1/desktop_fixture-download'
RED_RECEIPT = ROOT / '.superpowers/native-dependency-spike/native-iostore-generations-1/green-3/receipt.json'
RED_HEAD = '0a344b2fa7b360a62d5e972cedee175c711896cd'
RED_ARTIFACT_SHA = 'c5bf559ee131b1b9dd93b100536487794df2631e72a90acc47b350d91a3cb46c'
RED_BINARY_SHA = '933486efe11a61ef5ad677c0bce6f2b7c96b1925f2806e4e5468b353f2edbaab'
QUERY = ('query ModBridgeRequirements($modId: Int!, $gameDomainName: String!) { '
         'modRequirements(modId: $modId, gameDomainName: $gameDomainName) { '
         'nexusRequirements { nodes { id externalRequirement gameId modId modName notes url } nodesCount totalCount } '
         'dlcRequirements { gameExpansion { gameId id name } notes } } }')


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def historical_binary():
    manifest = json.loads((RED_PROFILE / 'manifest.json').read_bytes())
    artifact = RED_PROFILE / manifest['artifact']
    identity = json.loads(RED_RECEIPT.read_bytes())['app_identity']
    binary = Path(identity['private_directory']) / 'modbridge-native-auth-fixture'
    if sha(artifact) != RED_ARTIFACT_SHA or sha(binary) != RED_BINARY_SHA:
        raise RuntimeError('pinned historical application bytes changed')
    tracked = 0
    for path, digest in manifest['source_sha256'].items():
        original = subprocess.run(['git', 'show', RED_HEAD + ':' + path], cwd=ROOT,
                                  capture_output=True)
        if original.returncode == 0:
            if hashlib.sha256(original.stdout).hexdigest() != digest:
                raise RuntimeError('historical manifest source does not match its commit')
            tracked += 1
        elif sha(ROOT / path) != digest:
            raise RuntimeError('retained historical build input changed')
    return binary, {'historical_head': RED_HEAD, 'tracked_source_checks': tracked,
                    'profile_artifact_sha256': RED_ARTIFACT_SHA,
                    'fixture_binary_sha256': RED_BINARY_SHA,
                    'test_only_socket_redirect': True}


def source_mod(game, mod_id, hidden=False):
    return {'mod_id': mod_id, 'game_id': 7, 'domain_name': game,
            'name': 'Owned requirements source', 'author': 'Fixture', 'summary': '',
            'description': '', 'version': '1.0', 'contains_adult_content': False,
            'available': not hidden, 'status': 'hidden' if hidden else 'published'}


def requirements_body(scenario, game):
    node = {'id': 'owned-nexus-requirement', 'externalRequirement': False,
            'gameId': game, 'modId': '42', 'modName': 'Owned framework',
            'notes': None, 'url': 'https://www.nexusmods.com/' + game + '/mods/42'}
    external = {'id': 'owned-external-requirement', 'externalRequirement': True,
                'gameId': '', 'modId': '', 'modName': 'Owned external tool',
                'notes': 'No automatic file selection',
                'url': 'https://example.invalid/owned-tool'}
    dlc = {'gameExpansion': {'gameId': game, 'id': 'owned-dlc-id', 'name': 'Owned DLC'},
           'notes': None}
    nodes = [] if scenario == 'empty' else [node, external]
    result = {'data': {'modRequirements': {'nexusRequirements': {
        'nodes': nodes, 'nodesCount': len(nodes), 'totalCount': len(nodes)},
        'dlcRequirements': [] if scenario == 'empty' else [dlc]}}}
    if scenario == 'partial':
        result['data']['modRequirements']['nexusRequirements']['totalCount'] += 1
    elif scenario == 'errors':
        result['errors'] = [{'message': 'fixture-sensitive-provider-detail'}]
    elif scenario == 'null':
        result['data']['modRequirements'] = None
    elif scenario == 'bad_node':
        node['externalRequirement'] = 'false'
    elif scenario == 'missing_dlc':
        del result['data']['modRequirements']['dlcRequirements']
    return jobs.encoded(result)


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
            parsed = urlsplit(self.path)
            record = {'method': method, 'route': 'unrecognized', 'contract_ok': False,
                      'response_sent': False, 'scenario': state['scenario']}
            state['records'].append(record)
            forbidden = any(k in self.headers for k in ('apikey', 'Cookie', 'Proxy-Authorization'))
            bearer = self.headers.get('Authorization') == 'Bearer owned-jobs-access-token'
            sni = getattr(self.connection, 'fixture_sni', '')
            users = self.headers.get('Host') == 'users.nexusmods.com' and sni == 'users.nexusmods.com'
            api = self.headers.get('Host') == 'api.nexusmods.com' and sni == 'api.nexusmods.com'
            length = int(self.headers.get('Content-Length', '0'))
            incoming = self.rfile.read(length) if length else b''
            status, body = 404, jobs.encoded({})
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
                status, body = 200, jobs.encoded({'access_token': 'owned-jobs-access-token',
                    'refresh_token': 'owned-jobs-refresh-token', 'token_type': 'Bearer', 'expires_in': 3600})
            elif method == 'GET' and parsed.path == '/oauth/userinfo':
                record['route'] = 'userinfo'
                record['contract_ok'] = users and bearer and not forbidden and not parsed.query and not incoming
                status, body = 200, jobs.encoded({'sub': '12345', 'name': 'Owned requirements fixture',
                    'membership_roles': [], 'preferences': {'adult': False}})
            elif method == 'GET' and parsed.path.startswith('/v1/games'):
                record['route'] = 'metadata'
                record['contract_ok'] = api and bearer and not forbidden and not parsed.query and not incoming
                match = re.fullmatch(r'/v1/games/([a-z0-9_-]+)/mods(?:/(latest_added|[1-9][0-9]*)(/files)?)?', parsed.path)
                if parsed.path == '/v1/games':
                    status, body = 200, jobs.encoded([{'id': 7, 'domain_name': 'silenthilltownfall', 'name': 'Owned title'}])
                elif match:
                    mod_id = 7 if match[2] == 'latest_added' else int(match[2])
                    mod = source_mod(match[1], mod_id, state['scenario'] == 'hidden')
                    record['source_game'] = match[1]; record['source_mod_id'] = mod_id
                    if match[3]:
                        status, body = 200, jobs.encoded({'files': [{'file_id': 8, 'name': 'Owned file',
                            'file_name': 'owned.zip', 'version': '1.0', 'description': '', 'size_kb': 1}]})
                    else:
                        status, body = 200, jobs.encoded([mod] if match[2] == 'latest_added' else mod)
            elif method == 'POST' and parsed.path == '/v2/graphql':
                record['route'] = 'requirements'
                request = json.loads(incoming)
                variables = request.get('variables', {})
                game, mod_id = variables.get('gameDomainName'), variables.get('modId')
                expected = jobs.encoded({'query': QUERY, 'variables': {'gameDomainName': game, 'modId': mod_id}})
                record['contract_ok'] = (api and bearer and not forbidden and not parsed.query
                    and self.headers.get('Content-Type') == 'application/json'
                    and set(request) == {'query', 'variables'} and set(variables) == {'gameDomainName', 'modId'}
                    and request['query'] == QUERY and incoming == expected
                    and isinstance(game, str) and re.fullmatch('[a-z0-9][a-z0-9_-]{0,79}', game) is not None
                    and type(mod_id) is int and 1 <= mod_id <= 2147483647)
                record['source_game'] = game; record['source_mod_id'] = mod_id
                record['request_sha256'] = hashlib.sha256(incoming).hexdigest()
                status, body = 200, requirements_body(state['scenario'], game)
                if state['scenario'] == 'timeout':
                    time.sleep(state['config']['timeout_ms'] / 1000 + .5)
            record['status'] = status
            try:
                self.send_response(status)
                self.send_header('Content-Type', 'application/json')
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


def profile_scratch(profile):
    text = (Path(profile) / 'config.h').read_text()
    match = re.search(r'^#define MB_HOST_JOB_DIRECTORY ("(?:\\.|[^"\\])*")$', text, re.MULTILINE)
    if not match:
        raise RuntimeError('fixture profile has no private job directory')
    scratch = Path(json.loads(match[1])).resolve(strict=True)
    observed = scratch.stat()
    if not scratch.is_dir() or observed.st_uid != os.geteuid() or observed.st_mode & 0o7777 != 0o700 or list(scratch.iterdir()):
        raise RuntimeError('fixture job directory is not an empty owned private directory')
    return scratch


def verify(phase, profile, config_path, output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    config = json.loads(config_path.read_bytes())
    if phase == 'red':
        profile = RED_PROFILE
        binary, app_identity = historical_binary()
    else:
        auth.MATRIX_DIR = output / 'relink'
        binary, app_identity = auth.relink_with_socket_shim(profile)
    scratch = profile_scratch(profile)
    before = {'device': scratch.stat().st_dev, 'inode': scratch.stat().st_ino}
    state = {'config': config, 'scenario': 'complete', 'records': []}
    port, close_tls = fixture_server(config_path.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; process = None; completed = False
    def check(name, condition):
        checks[name] = bool(condition)
    def call(method, target, headers=None):
        return auth.local_request(app_port, method, target, headers, timeout=12)
    try:
        process, initial = generations.start_app(binary, app_port, port)
        headers = generations.sign_in(app_port, config, state)
        check('actual_owned_pkce_login', initial[0] == 200 and any(r['route'] == 'token' and r['contract_ok'] for r in state['records']))
        path = '/api/games/silenthilltownfall/mods/7/requirements'
        first = call('GET', path, headers)
        if phase == 'red':
            check('historical_route_has_no_requirements_handler', first[0] == 400)
            check('historical_route_makes_no_requirements_request', not any(r['route'] == 'requirements' for r in state['records']))
        else:
            payload = json.loads(first[2]); requirements = payload.get('requirements', {})
            rows = requirements.get('nexus_requirements', []); dlc = requirements.get('dlc_requirements', [])
            check('complete_requirements_owned_object', first[0] == 200 and requirements.get('game') == 'silenthilltownfall'
                and requirements.get('mod_id') == 7 and requirements.get('discovery_complete') is True)
            check('internal_mod_identity_preserved_without_file_choice', len(rows) == 2 and rows[0].get('external_requirement') is False
                and rows[0].get('game') == 'silenthilltownfall' and rows[0].get('mod_id') == '42'
                and rows[0].get('name') == 'Owned framework' and rows[0].get('notes') == ''
                and rows[0].get('satisfaction') == 'unresolved' and 'file_id' not in rows[0] and 'version' not in rows[0])
            check('external_and_dlc_remain_unresolved', len(rows) == 2 and rows[1].get('external_requirement') is True
                and rows[1].get('url') == 'https://example.invalid/owned-tool' and rows[1].get('satisfaction') == 'unresolved'
                and len(dlc) == 1 and dlc[0].get('game') == 'silenthilltownfall' and dlc[0].get('id') == 'owned-dlc-id'
                and dlc[0].get('notes') == '' and dlc[0].get('satisfaction') == 'unresolved')
            check('discovery_grants_no_install_or_version_authority', requirements.get('file_version_constraints_provided') is False
                and requirements.get('installation_supported') is False)
            routes = state['records']
            post = next((i for i, r in enumerate(routes) if r['route'] == 'requirements'), -1)
            check('source_visibility_read_precedes_requirements_post', post > 0 and routes[post-1]['route'] == 'metadata'
                and routes[post-1].get('source_mod_id') == 7)
            state['scenario'] = 'empty'
            empty = call('GET', path, headers); obj = json.loads(empty[2]).get('requirements', {})
            check('explicit_complete_empty_set', empty[0] == 200 and obj.get('discovery_complete') is True
                and obj.get('nexus_requirements') == [] and obj.get('dlc_requirements') == [])
            state['scenario'] = 'complete'
            other = call('GET', '/api/games/skyrim/mods/2147483647/requirements', headers)
            obj = json.loads(other[2]).get('requirements', {})
            check('generic_other_domain_and_int32_max', other[0] == 200 and obj.get('game') == 'skyrim' and obj.get('mod_id') == 2147483647)
            for scenario in ('partial', 'errors', 'null', 'bad_node', 'missing_dlc'):
                state['scenario'] = scenario; response = call('GET', path, headers)
                obj = json.loads(response[2])
                check(scenario + '_cannot_publish_requirements', response[0] == 502 and 'requirements' not in obj
                    and b'fixture-sensitive-provider-detail' not in response[2] and b'owned-external-requirement' not in response[2])
            state['scenario'] = 'hidden'; start = len(state['records'])
            hidden = call('GET', path, headers)
            check('hidden_source_refused_before_requirements_post', hidden[0] == 403
                and not any(r['route'] == 'requirements' for r in state['records'][start:]))
            state['scenario'] = 'complete'; start = len(state['records'])
            bad_cookie = call('GET', path, {**headers, 'Cookie': 'mb_session=' + 'x' * 43})
            check('unknown_session_refused_without_provider_request', bad_cookie[0] == 401 and len(state['records']) == start)
            for invalid in ('0', '01', '2147483648'):
                start = len(state['records']); response = call('GET', '/api/games/skyrim/mods/' + invalid + '/requirements', headers)
                check('invalid_query_id_' + invalid + '_refused_before_provider', response[0] == 400 and len(state['records']) == start)
            state['scenario'] = 'timeout'; response = call('GET', path, headers)
            check('timeout_cannot_publish_provider_body', response[0] == 504 and 'requirements' not in json.loads(response[2])
                and b'owned-external-requirement' not in response[2])
            state['scenario'] = 'complete'
            for target, shape in [('/api/games', 'items'), ('/api/games/silenthilltownfall/mods', 'items'),
                                  ('/api/games/silenthilltownfall/mods/7', 'item'), ('/api/games/silenthilltownfall/mods/7/files', 'items')]:
                response = call('GET', target, headers)
                check('old_catalog_' + shape + '_' + target.rsplit('/', 1)[1], response[0] == 200 and shape in json.loads(response[2]))
            public = first[2]
            check('public_report_contains_no_credentials_or_paths', all(word not in public for word in (
                b'owned-jobs-access-token', b'owned-jobs-refresh-token', str(scratch).encode(), str(config_path).encode())))
            check('all_requirements_requests_canonical_authenticated_readonly_posts',
                any(r['route'] == 'requirements' for r in state['records'])
                and all(r['contract_ok'] for r in state['records'] if r['route'] == 'requirements'))
        check('no_archive_transfers', not any(r['route'] == 'archive' for r in state['records']))
        shutdown = auth.local_request(app_port, 'POST', '/shutdown', {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=5)
        _, stderr = process.communicate(timeout=10)
        check('clean_shutdown_no_private_files', shutdown[0] == 200 and process.returncode == 0 and not stderr and not list(scratch.iterdir()))
        check('fixture_directory_identity_unchanged', before == {'device': scratch.stat().st_dev, 'inode': scratch.stat().st_ino})
        check('all_owned_tls_requests_valid', bool(state['records']) and all(r['contract_ok'] for r in state['records']))
        completed = True
    finally:
        if process and process.poll() is None:
            process.terminate(); process.communicate(timeout=5)
        close_tls()
        receipt = {'phase': phase, 'scope': 'actual_app_owned_tls_requirements_discovery',
                   'checks': checks, 'all_passed': completed and bool(checks) and all(checks.values()),
                   'app_identity': app_identity, 'tls_records': state['records'],
                   'verifier_sha256': sha(Path(__file__)), 'console_contact': False,
                   'production_account_contact': False, 'game_system_writes': False,
                   'file_choice_or_install_authority': False}
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
        parser.error('--app-profile is required for green')
    receipt = verify(args.phase, args.app_profile, args.oauth_config.resolve(), args.output.resolve())
    print(json.dumps({'phase': args.phase, 'checks': len(receipt['checks']), 'all_passed': receipt['all_passed']}))
    return 0 if receipt['all_passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
