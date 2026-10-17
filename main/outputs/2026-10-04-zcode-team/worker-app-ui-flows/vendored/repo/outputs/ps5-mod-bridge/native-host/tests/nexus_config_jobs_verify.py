#!/usr/bin/env python3
"""Owned HTTPS app-to-INI-preparation job proof; no console or live account."""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import time
from urllib.parse import urlsplit
import zipfile
import warnings

import nexus_auth_verify as auth
import nexus_jobs_verify as jobs

ROOT = auth.ROOT
RAR = Path('/WORKSPACE/Downloads/Engine 110 1 2026-09-29T21-36Z iFcgTCNBr.rar')
INI = ROOT / '.superpowers/native-observations/engine-config-real-mod-intake-2026-10-01/Engine.ini'
BASE = ROOT / '.superpowers/native-observations/townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak'
EXPECTED_PAK = ROOT / '.superpowers/native-dependency-spike/unreal-config-main-pak-trial-1/trial-1/derived.pak'
PERFORMANCE_ZIP = ROOT / '.superpowers/native-observations/performance-pak-real-mod-intake-1/original.zip'
REPAK = ROOT / '.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/build/aarch64-apple-darwin/release/repak'
REPAK_RECEIPT = ROOT / '.superpowers/native-dependency-spike/unreal-library-container-survey-1/mount-research/tools/repak-build-receipt.json'
RED_RECEIPT = ROOT / '.superpowers/native-dependency-spike/native-downloaded-pak-jobs-1/actual-http-tls-final-1/receipt.json'
RED_BINARY_SHA256 = 'df0044a0d36a8480d140e423055f8d9bcb593beea4e4a87ccd7635d8ef8c2f81'
RED_PROFILE_ARTIFACT_SHA256 = 'a068adf4e3eb0df166074cc7bdbbf7c745ac9e4496c73bc47a6dc8496b854491'
RED_SOURCE_COMMIT = 'a196f3ef4fc59c941e3f916d470137d688ecb7dc'

EXPECTED = {
    'rar_bytes': 582,
    'rar_sha256': '24ea77e211aa95c7800035de62fde127fd514454c3f7cb9957a672a038afb906',
    'member_bytes': 950,
    'member_sha256': '13a7ed33a3f841c4f150f3b39a188bd6b51483a2558b9da6bd4e6f50cad58e99',
    'base_bytes': 297572462,
    'base_sha256': 'ee145cbc808f96508a626c2e26592371f9bc4f2ef64840784a4a2f332d8cd867',
    'pak_bytes': 297483249,
    'pak_sha256': '8bcf5eb17599e40a02575943eeb6515e7c70e669a74fc962f2d6cbd32aea8d58',
    'merged_bytes': 3626,
    'merged_sha256': '94fc0da404c56927217a71838f67afa1d0671bca32d8d0fbfdf217d696aaed46',
    'performance_zip_bytes': 5810,
    'performance_zip_sha256': '27db7e1b6ea2bf5b574a68a1fa6d7d89ccd5795fd11ac98b4549c2e69132e06e',
}


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def identity(path):
    path = Path(path)
    return {'bytes': path.stat().st_size, 'sha256': digest(path)}


def encoded(value):
    return json.dumps(value, separators=(',', ':'), ensure_ascii=True).encode('ascii')


def zip_bytes(rows):
    storage = io.BytesIO()
    with zipfile.ZipFile(storage, 'w', compression=zipfile.ZIP_STORED) as archive:
        for name, payload in rows:
            archive.writestr(name, payload)
    return storage.getvalue()


def fixture_archives():
    rar = RAR.read_bytes()
    performance = PERFORMANCE_ZIP.read_bytes()
    # Negative fixture: the parser must refuse a source document with an
    # unselected header instead of silently applying only SystemSettings.
    wrong_section = zip_bytes([('Engine.ini', b'[SystemSettings]\nr.Fog=0\n[OtherSettings]\nr.Fog=1\n')])
    # Negative fixture: duplicate canonical candidate names must fail closed.
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', UserWarning)
        duplicate_ini = zip_bytes([
            ('Engine.ini', b'[SystemSettings]\nr.Fog=0\n'),
            ('Engine.ini', b'[SystemSettings]\nr.Fog=1\n'),
        ])
    unknown_dll = zip_bytes([('Binaries/Unknown.dll', b'owned non-executable fixture bytes\n')])
    return {8: rar, 9: performance, 10: wrong_section, 11: duplicate_ini,
            12: rar, 13: unknown_dll}


def require_inputs():
    observed = {
        'rar': identity(RAR), 'member': identity(INI), 'base': identity(BASE),
        'expected_pak': identity(EXPECTED_PAK), 'performance_zip': identity(PERFORMANCE_ZIP),
    }
    pairs = {
        'rar': ('rar_bytes', 'rar_sha256'), 'member': ('member_bytes', 'member_sha256'),
        'base': ('base_bytes', 'base_sha256'), 'expected_pak': ('pak_bytes', 'pak_sha256'),
        'performance_zip': ('performance_zip_bytes', 'performance_zip_sha256'),
    }
    for label, (bytes_key, hash_key) in pairs.items():
        if observed[label] != {'bytes': EXPECTED[bytes_key], 'sha256': EXPECTED[hash_key]}:
            raise RuntimeError('fixed local oracle changed: ' + label)
    return observed


def _start_app(binary, app_port, fixture_port, timeout=180):
    proc = subprocess.Popen(
        [str(binary), '--port', str(app_port), '--lifetime', '240'],
        env={'PATH': '/usr/bin:/bin:/usr/sbin:/sbin',
             'MB_NEXUS_AUTH_FIXTURE_PORT': str(fixture_port)},
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            response = auth.local_request(app_port, 'GET', '/api/capabilities', timeout=3)
            return proc, response
        except OSError:
            if proc.poll() is not None:
                raise RuntimeError('owned app exited before readiness')
            time.sleep(.025)
    proc.terminate()
    proc.communicate(timeout=4)
    raise RuntimeError('owned app readiness deadline expired')


def _authenticate(app_port, config, state):
    initial = auth.local_request(app_port, 'GET', '/api/capabilities', timeout=5)
    cookie, csrf, valid = auth.get_cookie_and_csrf(initial[1], json.loads(initial[2]))
    if not valid:
        raise RuntimeError('owned app did not issue a valid local session')
    origin = config['redirect_uri'].rsplit('/auth/callback', 1)[0]
    headers = {'Cookie': 'mb_session=' + cookie, 'X-CSRF-Token': csrf,
               'Origin': origin, 'Content-Type': 'application/json'}
    started = auth.local_request(app_port, 'POST', '/api/auth/start', headers,
                                 jobs.encoded({}), timeout=5)
    authorization = auth.parse_authorization_url(started[2], config)
    if started[0] != 200 or not authorization:
        raise RuntimeError('owned local sign-in start failed')
    state['expected_challenge'] = authorization['challenge']
    callback = auth.callback_request(app_port, cookie, authorization['state'])
    if callback[0] != 303:
        raise RuntimeError('owned local sign-in callback failed')
    return headers


def run_red(profile, config_path, output):
    """Prove the captured pre-change host rejects the not-yet-supported intent."""
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    config_path = Path(config_path).resolve()
    config = json.loads(config_path.read_bytes())
    scratch = config_path.parent / 'scratch'
    if list(scratch.iterdir()):
        raise RuntimeError('fixture scratch is not empty before RED')
    archives = fixture_archives()
    before = require_inputs()
    profile = Path(profile).resolve()
    manifest = json.loads((profile / 'manifest.json').read_text(encoding='utf-8'))
    prior = json.loads(RED_RECEIPT.read_text(encoding='utf-8'))
    prior_identity = prior.get('app_identity', {})
    if (prior.get('all_passed') is not True
            or prior_identity.get('profile_artifact_sha256') != RED_PROFILE_ARTIFACT_SHA256
            or manifest.get('artifact_sha256') != RED_PROFILE_ARTIFACT_SHA256
            or prior_identity.get('fixture_binary_sha256') != RED_BINARY_SHA256):
        raise RuntimeError('retained historical RED receipt identity mismatch')
    old_job_source = next((value for name, value in manifest.get('source_sha256', {}).items()
                           if Path(name).name == 'jobs.cpp'), None)
    historical = subprocess.run(['git', 'show',
        RED_SOURCE_COMMIT + ':outputs/ps5-mod-bridge/native-host/jobs.cpp'],
        cwd=ROOT, capture_output=True, timeout=15)
    historical_sha = hashlib.sha256(historical.stdout).hexdigest()
    if (historical.returncode != 0 or not old_job_source
            or historical_sha != old_job_source):
        raise RuntimeError('historical RED jobs.cpp does not match the pinned source commit')
    binary = Path(prior_identity.get('private_directory', '')) / 'modbridge-native-auth-fixture'
    if not binary.is_absolute():
        binary = ROOT / binary
    if not binary.is_file() or auth.sha256(binary) != RED_BINARY_SHA256:
        raise RuntimeError('retained historical RED executable identity mismatch')
    app_identity = {
        'historical_fixture_binary_sha256': RED_BINARY_SHA256,
        'profile_artifact_sha256': RED_PROFILE_ARTIFACT_SHA256,
        'jobs_source_sha256': old_job_source,
        'prior_fixture_receipt': str(RED_RECEIPT),
        'test_only_curl_callsite_redirect': True,
    }
    old_jobs_sha = old_job_source
    state = {'config': config, 'premium': True, 'records': [], 'archives': archives}
    port, close_tls, _ = jobs.fixture_server(config_path.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; proc = None; completed = False; refusal_status = None
    def check(name, value): checks[name] = bool(value)
    try:
        proc, initial = _start_app(binary, app_port, port)
        cookie, csrf, valid = auth.get_cookie_and_csrf(initial[1], json.loads(initial[2]))
        origin = config['redirect_uri'].rsplit('/auth/callback', 1)[0]
        headers = {'Cookie': 'mb_session=' + cookie, 'X-CSRF-Token': csrf,
                   'Origin': origin, 'Content-Type': 'application/json'}
        started = auth.local_request(app_port, 'POST', '/api/auth/start', headers,
                                     jobs.encoded({}), timeout=5)
        authorization = auth.parse_authorization_url(started[2], config)
        if started[0] != 200 or not authorization:
            raise RuntimeError('owned local sign-in start failed')
        state['expected_challenge'] = authorization['challenge']
        callback = auth.callback_request(app_port, cookie, authorization['state'])
        check('actual_owned_tls_login', valid and callback[0] == 303)
        intent = {'kind': 'download_prepare_ini', 'game': 'silenthilltownfall',
                  'mod_id': 7, 'file_id': 8, 'idempotency_key': 'owned_ini_prepare_red_1234'}
        response = auth.local_request(app_port, 'POST', '/api/jobs', headers,
                                      jobs.encoded(intent), timeout=5)
        refusal_status = response[0]
        try:
            refusal = json.loads(response[2])
        except (UnicodeError, json.JSONDecodeError):
            refusal = {}
        red_observed = (response[0] == 403
                        and refusal.get('error', {}).get('code') == 'auth_refused')
        check('old_host_refuses_new_ini_job_kind', red_observed)
        check('refusal_before_archive_network', not any(r['route'] == 'archive' for r in state['records']))
        check('no_private_output_after_refusal', not list(scratch.iterdir()))
        check('all_tls_contracts_valid', all(r['contract_ok'] and r['response_sent'] for r in state['records']))
        check('oracles_unchanged', require_inputs() == before)
        shutdown = auth.local_request(app_port, 'POST', '/shutdown',
            {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=5)
        stdout, stderr = proc.communicate(timeout=5)
        check('normal_shutdown', shutdown[0] == 200 and proc.returncode == 0 and not stderr)
        completed = True
    finally:
        if proc and proc.poll() is None:
            proc.terminate(); proc.communicate(timeout=5)
        close_tls()
        receipt = {
            'phase': 'behavioral_red_before_ini_job_support', 'checks': checks,
            # A correctly reproduced refusal is evidence of the missing
            # behavior, not a product pass.  The desired route is still RED.
            'all_passed': False,
            'red_evidence_complete': completed and red_observed and all(checks.values()),
            'desired_job_behavior_available': False,
            'old_host_refusal_http_status': refusal_status,
            'historical_source_commit': RED_SOURCE_COMMIT,
            'old_jobs_source_sha256': old_jobs_sha,
            'app_identity': app_identity, 'fixed_inputs': before,
            'tls_records': state['records'], 'live_account': False,
            'console_executed': False, 'game_files_changed': False,
        }
        (output / 'receipt.json').write_bytes(encoded(receipt))
    return receipt


def _repak_hashes(tool, pak):
    result = subprocess.run([str(tool), 'hash-list', '--strip-prefix', '', str(pak)],
        capture_output=True, timeout=180)
    if result.returncode != 0:
        raise RuntimeError('independent repak hash-list failed')
    rows = {}
    for line in result.stdout.decode('utf-8').splitlines():
        digest_hex, separator, virtual_path = line.partition(' ')
        if not separator or not re.fullmatch('[0-9a-f]{64}', digest_hex) or not virtual_path:
            raise RuntimeError('independent repak hash-list row malformed')
        if virtual_path in rows:
            raise RuntimeError('independent repak hash-list contains a duplicate path')
        rows[virtual_path] = digest_hex
    return rows


def verify_repak(tool):
    tool = Path(tool).resolve(strict=True)
    tool_receipt = json.loads(REPAK_RECEIPT.read_text(encoding='utf-8'))
    if (tool_receipt.get('source_commit') != '355b5f62f51959c7cc6dd5a51708646ef483065d'
            or tool != REPAK.resolve()
            or digest(tool) != '57b321cca5714997e6aad7300e713c5137a5ea7fa60b3281bd8eb322c4e36960'):
        raise RuntimeError('independent repak tool identity mismatch')
    return tool, tool_receipt


def verify_full(profile, config_path, output, repak=REPAK):
    """Actual TLS account -> Nexus file -> private Engine.ini PAK preparation."""
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    config_path = Path(config_path).resolve()
    config = json.loads(config_path.read_bytes())
    scratch = config_path.parent / 'scratch'
    if list(scratch.iterdir()):
        raise RuntimeError('fixture scratch is not empty before full run')
    inputs_before = require_inputs()
    archive_map = fixture_archives()
    rar_member = subprocess.run(['bsdtar', '-xOf', str(RAR), 'Engine.ini'],
        capture_output=True, timeout=15)
    if (rar_member.returncode != 0 or len(rar_member.stdout) != EXPECTED['member_bytes']
            or hashlib.sha256(rar_member.stdout).hexdigest() != EXPECTED['member_sha256']
            or rar_member.stdout != INI.read_bytes()):
        raise RuntimeError('fixed RAR member did not match the retained Engine.ini oracle')
    tool, tool_receipt = verify_repak(repak)
    auth.MATRIX_DIR = output / 'relink'
    binary, app_identity = auth.relink_with_socket_shim(profile)
    state = {'config': config, 'premium': True, 'records': [], 'archives': archive_map}
    port, close_tls, archives = jobs.fixture_server(config_path.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; summaries = {}; proc = None; completed = False

    def check(name, value):
        checks[name] = bool(value)

    def call(method, target, body=None, headers=None, timeout=180):
        return auth.local_request(app_port, method, target, headers,
            b'' if body is None else jobs.encoded(body), timeout=timeout)

    def submit_get(kind, file_id, game='silenthilltownfall', key=None):
        intent = {'kind': kind, 'game': game, 'mod_id': 7, 'file_id': file_id,
                  'idempotency_key': key or f'owned_ini_{file_id}_{game}_1234'}
        queued = call('POST', '/api/jobs', intent, headers=headers)
        row = {'submit_status': queued[0], 'outer_status': None, 'id': None,
               'sha256': None, 'bytes_downloaded': None, 'preparation': None}
        if queued[0] != 202:
            return intent, queued, row
        try:
            identifier = json.loads(queued[2]).get('id')
        except (UnicodeError, json.JSONDecodeError):
            return intent, queued, row
        row['id'] = identifier
        if not isinstance(identifier, str) or not re.fullmatch('[a-f0-9]{32}', identifier):
            return intent, queued, row
        done = call('GET', '/api/jobs/' + identifier, headers=headers, timeout=180)
        if done[0] != 200:
            return intent, done, row
        try:
            report = json.loads(done[2])
        except (UnicodeError, json.JSONDecodeError):
            return intent, done, row
        row.update({'outer_status': report.get('status'), 'kind': report.get('kind'),
                    'compatibility': report.get('compatibility'),
                    'sha256': report.get('sha256'), 'bytes_downloaded': report.get('bytes_downloaded'),
                    'preparation': report.get('preparation')})
        summaries[identifier] = row
        return intent, done, row

    try:
        proc, initial = _start_app(binary, app_port, port)
        cookie, csrf, valid = auth.get_cookie_and_csrf(initial[1], json.loads(initial[2]))
        origin = config['redirect_uri'].rsplit('/auth/callback', 1)[0]
        headers = {'Cookie': 'mb_session=' + cookie, 'X-CSRF-Token': csrf,
                   'Origin': origin, 'Content-Type': 'application/json'}
        started = call('POST', '/api/auth/start', {}, headers, timeout=5)
        authorization = auth.parse_authorization_url(started[2], config)
        if started[0] != 200 or not authorization:
            raise RuntimeError('owned local sign-in start failed')
        state['expected_challenge'] = authorization['challenge']
        callback = auth.callback_request(app_port, cookie, authorization['state'])
        check('actual_owned_tls_login', valid and callback[0] == 303)

        positive_key = 'owned_ini_real_rar_1234'
        intent, response, positive = submit_get('download_prepare_ini', 8,
            key=positive_key)
        prep = positive.get('preparation') or {}
        job_id = positive.get('id')
        check('real_ini_job_accepted', response[0] == 200 and positive['submit_status'] == 202
              and positive['outer_status'] == 'analyzed')
        check('real_rar_download_identity', positive.get('sha256') == EXPECTED['rar_sha256']
              and positive.get('bytes_downloaded') == EXPECTED['rar_bytes'])
        check('ini_preparation_report_contract', prep.get('status') == 'prepared'
              and prep.get('handler') == 'unreal_engine_ini'
              and prep.get('selected_sections') == ['SystemSettings']
              and prep.get('section_policy') == 'all_source_headers_must_be_selected'
              and prep.get('scalar_winners') == 30
              and prep.get('replacements') == 2 and prep.get('additions') == 28
              and prep.get('merged_bytes') == EXPECTED['merged_bytes']
              and prep.get('merged_sha256') == EXPECTED['merged_sha256']
              and prep.get('target_derived') is True)
        check('no_runtime_or_install_claim', prep.get('runtime_supported') is False
              and prep.get('installation_supported') is False)
        artifact = scratch / ('mb-pak-' + (job_id or '') + '.pak')
        quarantine = scratch / ('mb-pak-' + (job_id or ''))
        check('private_prepared_artifact_readback', artifact.is_file()
              and artifact.stat().st_size == EXPECTED['pak_bytes']
              and digest(artifact) == EXPECTED['pak_sha256']
              and prep.get('artifact_complete') is True
              and prep.get('artifact_bytes') == EXPECTED['pak_bytes']
              and prep.get('artifact_sha256') == EXPECTED['pak_sha256']
              and prep.get('physical_readback_observed') is True)
        members = sorted(quarantine.glob('member-*.bin')) if quarantine.is_dir() else []
        check('exact_retained_real_rar_member', len(members) == 1
              and members[0].read_bytes() == rar_member.stdout
              and digest(members[0]) == EXPECTED['member_sha256'])

        repak_rows = _repak_hashes(tool, artifact) if artifact.is_file() else {}
        expected_rows = _repak_hashes(tool, EXPECTED_PAK)
        check('independent_repak_decoded_all_4699_payloads', len(repak_rows) == 4699
              and len(expected_rows) == 4699 and repak_rows == expected_rows)
        check('artifact_matches_retained_config_pak', artifact.is_file()
              and artifact.stat().st_size == EXPECTED['pak_bytes']
              and digest(artifact) == EXPECTED['pak_sha256']
              and digest(EXPECTED_PAK) == EXPECTED['pak_sha256'])

        before_records = len(state['records'])
        duplicate = call('POST', '/api/jobs', intent, headers, timeout=5)
        check('same_intent_idempotent_no_second_transfer', duplicate[0] == 202
              and json.loads(duplicate[2]).get('id') == job_id
              and len(state['records']) == before_records)
        kind_conflict = call('POST', '/api/jobs', {**intent, 'kind': 'download_prepare_pak'},
                             headers, timeout=5)
        check('idempotency_kind_conflict_refused', kind_conflict[0] == 409
              and len(state['records']) == before_records)

        # Whole-file default policy refuses two extras in this established ZIP.
        _, _, performance = submit_get('download_prepare_ini', 9,
            key='owned_ini_performance_zip_1234')
        perf_prep = performance.get('preparation') or {}
        perf_id = performance.get('id')
        check('performance_zip_refused_with_two_unhandled_members',
              performance.get('outer_status') == 'analyzed'
              and performance.get('sha256') == EXPECTED['performance_zip_sha256']
              and performance.get('bytes_downloaded') == EXPECTED['performance_zip_bytes']
              and perf_prep.get('status') == 'additional_action_required'
              and perf_prep.get('unhandled_members') == 2
              and perf_prep.get('artifact_complete') is False
              and not (scratch / ('mb-pak-' + (perf_id or '') + '.pak')).exists())

        # A valid mod/file on another domain is accepted as a request but never
        # borrows the captured Townfall target.
        _, _, wrong_game = submit_get('download_prepare_ini', 8, game='skyrim',
            key='owned_ini_wrong_game_1234')
        wrong_prep = wrong_game.get('preparation') or {}
        wrong_id = wrong_game.get('id')
        check('wrong_game_target_stays_unresolved',
              wrong_game.get('outer_status') == 'analyzed'
              and wrong_prep.get('status') == 'unresolved'
              and wrong_prep.get('artifact_complete') is not True
              and not (scratch / ('mb-pak-' + (wrong_id or '') + '.pak')).exists())

        # Parser-level strict-section refusal and duplicate path refusal use
        # local synthetic ZIP bodies, never production content or assumptions.
        _, _, wrong_section = submit_get('download_prepare_ini', 10,
            key='owned_ini_wrong_section_1234')
        section_prep = wrong_section.get('preparation') or {}
        section_id = wrong_section.get('id')
        check('unselected_ini_header_refused_without_output',
              wrong_section.get('outer_status') in ('analyzed', 'failed')
              and section_prep.get('status') != 'prepared'
              and section_prep.get('artifact_complete') is not True
              and not (scratch / ('mb-pak-' + (section_id or '') + '.pak')).exists())
        _, _, duplicate_path = submit_get('download_prepare_ini', 11,
            key='owned_ini_duplicate_path_1234')
        duplicate_prep = duplicate_path.get('preparation') or {}
        duplicate_id = duplicate_path.get('id')
        check('duplicate_ini_path_refused_without_output',
              duplicate_path.get('outer_status') in ('analyzed', 'failed')
              and duplicate_prep.get('status') != 'prepared'
              and duplicate_prep.get('artifact_complete') is not True
              and not (scratch / ('mb-pak-' + (duplicate_id or '') + '.pak')).exists())

        # Automatic dispatch is only an inventory hint followed by the same
        # strict core validation; the real INI archive must produce identical
        # bytes without a manual handler recipe.
        auto_intent, auto_response, automatic = submit_get('download_prepare', 12,
            key='owned_ini_automatic_rar_1234')
        auto_prep = automatic.get('preparation') or {}
        auto_id = automatic.get('id')
        auto_artifact = scratch / ('mb-pak-' + (auto_id or '') + '.pak')
        auto_q = scratch / ('mb-pak-' + (auto_id or ''))
        auto_members = sorted(auto_q.glob('member-*.bin')) if auto_q.is_dir() else []
        check('automatic_rar_dispatch_prepared_same_output',
              auto_response[0] == 200 and automatic.get('outer_status') == 'analyzed'
              and automatic.get('sha256') == EXPECTED['rar_sha256']
              and automatic.get('bytes_downloaded') == EXPECTED['rar_bytes']
              and auto_prep.get('status') == 'prepared'
              and auto_prep.get('automatic_dispatch') is True
              and auto_prep.get('dispatch_basis') == 'inventory_hint_then_core_validation'
              and auto_prep.get('handler') == 'unreal_engine_ini'
              and auto_prep.get('artifact_complete') is True
              and auto_prep.get('artifact_sha256') == EXPECTED['pak_sha256']
              and auto_artifact.is_file() and digest(auto_artifact) == EXPECTED['pak_sha256'])
        check('automatic_ini_member_and_runtime_claim_boundary',
              len(auto_members) == 1 and auto_members[0].read_bytes() == rar_member.stdout
              and auto_prep.get('runtime_supported') is False
              and auto_prep.get('installation_supported') is False
              and automatic.get('compatibility') == 'unresolved')
        before_auto_duplicate = len(state['records'])
        auto_duplicate = call('POST', '/api/jobs', auto_intent, headers, timeout=5)
        check('automatic_intent_idempotent', auto_duplicate[0] == 202
              and json.loads(auto_duplicate[2]).get('id') == auto_id
              and len(state['records']) == before_auto_duplicate)
        auto_kind_conflict = call('POST', '/api/jobs',
            {**auto_intent, 'kind': 'download_prepare_ini'}, headers, timeout=5)
        check('automatic_kind_conflict_refused', auto_kind_conflict[0] == 409
              and len(state['records']) == before_auto_duplicate)

        _, _, unknown = submit_get('download_prepare', 13,
            key='owned_unknown_dll_1234')
        unknown_prep = unknown.get('preparation') or {}
        unknown_id = unknown.get('id')
        check('unknown_dll_requires_action_without_output',
              unknown.get('outer_status') == 'analyzed'
              and unknown_prep.get('status') == 'additional_action_required'
              and unknown_prep.get('unhandled_members') == 1
              and unknown_prep.get('artifact_complete') is False
              and not (scratch / ('mb-pak-' + (unknown_id or '') + '.pak')).exists())

        public = call('GET', '/api/jobs', headers=headers, timeout=10)[2]
        forbidden_public_bytes = [
            b'owned-jobs-access-token', b'owned-jobs-refresh-token', b'fixture_signature',
            b'https://api.nexusmods.com/archive', str(scratch).encode(), str(BASE).encode(),
            str(INI).encode(), str(RAR).encode(), str(EXPECTED_PAK).encode(),
        ]
        check('no_public_secrets_or_private_paths',
              all(value not in public for value in forbidden_public_bytes))
        check('sources_unchanged', require_inputs() == inputs_before
              and rar_member.stdout == INI.read_bytes())
        check('all_archive_requests_credential_free',
              all(row.get('no_account_headers') is True for row in state['records']
                  if row['route'] == 'archive'))
        check('all_owned_tls_requests_valid', bool(state['records'])
              and all(row.get('contract_ok') and row.get('response_sent')
                      for row in state['records']))

        shutdown = call('POST', '/shutdown', headers={'Content-Length': '0',
            'X-Modbridge-Shutdown': '1'}, timeout=5)
        stdout, stderr = proc.communicate(timeout=8)
        check('shutdown_cleans_private_files', shutdown[0] == 200 and proc.returncode == 0
              and not list(scratch.iterdir()) and not stderr)
        check('report_describes_preparation_not_runtime', prep.get('status') == 'prepared'
              and prep.get('handler') == 'unreal_engine_ini'
              and prep.get('runtime_supported') is False
              and prep.get('installation_supported') is False)
        completed = True
    finally:
        if proc and proc.poll() is None:
            proc.terminate(); proc.communicate(timeout=6)
        close_tls()
        receipt = {
            'phase': 'actual_owned_tls_download_and_ini_pak_preparation',
            'checks': checks,
            'all_passed': completed and bool(checks) and all(checks.values()),
            'job_summaries': summaries,
            'app_identity': app_identity,
            'fixed_inputs_before': inputs_before,
            'fixed_inputs_after': require_inputs(),
            'repak': {'sha256': digest(tool), 'source_commit': tool_receipt.get('source_commit'),
                      'decoded_artifact_paths': 4699 if checks.get('independent_repak_decoded_all_4699_payloads') else None,
                      'decoded_map_equal_to_retained_config_artifact': checks.get('independent_repak_decoded_all_4699_payloads', False)},
            'tls_records': state['records'],
            'live_account': False, 'console_executed': False,
            'game_files_changed': False, 'runtime_or_install_claimed': False,
        }
        (output / 'receipt.json').write_bytes(encoded(receipt))
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('red', 'full'), required=True)
    parser.add_argument('--app-profile', type=Path, required=True)
    parser.add_argument('--oauth-config', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--repak', type=Path, default=REPAK)
    args = parser.parse_args()
    if args.phase == 'red':
        result = run_red(args.app_profile, args.oauth_config, args.output.resolve())
    else:
        result = verify_full(args.app_profile, args.oauth_config, args.output.resolve(), args.repak)
    print(json.dumps({'phase': result['phase'], 'checks': len(result['checks']),
                      'all_passed': result['all_passed'],
                      'red_evidence_complete': result.get('red_evidence_complete', False),
                      'desired_job_behavior_available': result.get('desired_job_behavior_available', True),
                      'refusal_http_status': result.get('old_host_refusal_http_status')}))
    if result['phase'] == 'behavioral_red_before_ini_job_support':
        # A valid RED reproduction intentionally exits nonzero: the app is
        # missing the requested route.
        raise SystemExit(1)
    raise SystemExit(0 if result['all_passed'] else 1)


if __name__ == '__main__':
    main()
