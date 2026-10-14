#!/usr/bin/env python3
"""Owned local-TLS proof for downloaded IoStore preparation jobs.

The app is exercised through its real HTTP/auth/job/native transport path. All
Nexus responses and TLS are owned fixtures; original mod and base files are
read-only. This proves private preparation, never game installation/runtime.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time
from urllib.parse import urlsplit
import zipfile

import nexus_auth_verify as auth
import nexus_jobs_verify as jobs
import nexus_config_jobs_verify as previous

ROOT = auth.ROOT
SPIKE = ROOT / '.superpowers/native-dependency-spike/native-iostore-prepare-jobs-1'
CONFIG = SPIKE / 'fixture-config-1/oauth-config.json'
CAPTURED_PROFILE = SPIKE / 'captured-iostore-profile.json'
RED_CONFIG = ROOT / '.superpowers/native-dependency-spike/native-downloaded-pak-jobs-1/fixture-config-1/oauth-config.json'
CAMERA_RAR = Path('/WORKSPACE/Downloads/Third-person view and character beautification 62 1 2026-09-24T18-35Z iFcgTCNEt.rar')
LEON_ZIP = ROOT / '.superpowers/native-observations/nexus-real-mods-2026-10-01/leon-2k-v1.1.zip'
BASE_TOC = ROOT / '.superpowers/native-observations/townfall-main-utoc-2026-10-01/pakchunk0-ps5.utoc'
BASE_HEADER = ROOT / '.superpowers/native-observations/townfall-original-container-header-1/container-header.bin'
LEON_ORACLE = ROOT / '.superpowers/native-dependency-spike/iostore-original-input-composition-1/trial-1/output'
ARTIFACT_NAMES = ('generated-sidecar.ucas', 'generated-small.utoc',
                  'merged-container-header.bin', 'generated-directory.bin',
                  'generated-final.utoc')
EXPECTED_INPUTS = {
    'camera_rar': (8043992, 'ac511becb74c5e7d874806051b56e314e68896e6d29b0d510d5394412890c022'),
    'leon_zip': (105647244, '661aabf0e6a1cb91e9164501108b5d19db13c2ca21cee651f676e0706f3feef9'),
    'base_toc': (29707562, '35de27526c2ed91a7d21ab027b4e2ad6dc58cca137ab92c31e1f60c9e0bb75b5'),
    'base_header': (2272324, 'd7b6f7ca24d57aed9d4a3f460bc49ad7bd8f6b4623896e1e84dcdc37ac73a2a7'),
}
RED_BINARY_SHA256 = 'df0044a0d36a8480d140e423055f8d9bcb593beea4e4a87ccd7635d8ef8c2f81'


def digest_file(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def identity(path):
    path = Path(path)
    return {'bytes': path.stat().st_size, 'sha256': digest_file(path)}


def source_identities():
    values = {'camera_rar': identity(CAMERA_RAR), 'leon_zip': identity(LEON_ZIP),
              'base_toc': identity(BASE_TOC), 'base_header': identity(BASE_HEADER),
              'captured_iostore_profile': identity(CAPTURED_PROFILE)}
    for name, (size, sha) in EXPECTED_INPUTS.items():
        if values[name] != {'bytes': size, 'sha256': sha}:
            raise RuntimeError('fixed local source identity changed: ' + name)
    return values


def mixed_leon_with_unknown():
    """Build an owned negative archive after the original ZIP is read-only checked."""
    result = io.BytesIO()
    with zipfile.ZipFile(LEON_ZIP, 'r') as original, zipfile.ZipFile(
            result, 'w', compression=zipfile.ZIP_DEFLATED, compresslevel=1) as mixed:
        for entry in original.infolist():
            # Every member is below the ordinary ZIP 32-bit size boundary.
            # Do not add Zip64 local extras: the normal intake policy refuses
            # those before the IoStore selector can examine the unknown member.
            with original.open(entry, 'r') as source, mixed.open(entry.filename, 'w') as target:
                shutil.copyfileobj(source, target, 1024 * 1024)
        mixed.writestr('Mods/Unknown/opaque-extra.bin', b'owned unknown member fixture\n')
    return result.getvalue()


def fixture_archives(*, include_mixed=False):
    archives = {8: CAMERA_RAR.read_bytes(), 9: LEON_ZIP.read_bytes(),
                11: CAMERA_RAR.read_bytes(), 12: LEON_ZIP.read_bytes()}
    if include_mixed:
        archives[10] = mixed_leon_with_unknown()
    return archives


def app_call(port, method, target, headers=None, body=b'', timeout=180):
    return auth.local_request(port, method, target, headers, body, timeout=timeout)


def launch(binary, app_port, fixture_port, lifetime=240):
    proc = subprocess.Popen([str(binary), '--port', str(app_port), '--lifetime', str(lifetime)],
        env={'PATH': '/usr/bin:/bin:/usr/sbin:/sbin',
             'MB_NEXUS_AUTH_FIXTURE_PORT': str(fixture_port)},
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            initial = app_call(app_port, 'GET', '/api/capabilities', timeout=3)
            return proc, initial
        except OSError:
            if proc.poll() is not None:
                raise RuntimeError('owned app exited before readiness')
            time.sleep(.025)
    proc.terminate(); proc.communicate(timeout=5)
    raise RuntimeError('owned app readiness deadline expired')


def login(app_port, config, state):
    initial = app_call(app_port, 'GET', '/api/capabilities', timeout=5)
    cookie, csrf, valid = auth.get_cookie_and_csrf(initial[1], json.loads(initial[2]))
    origin = config['redirect_uri'].rsplit('/auth/callback', 1)[0]
    headers = {'Cookie': 'mb_session=' + cookie, 'X-CSRF-Token': csrf,
               'Origin': origin, 'Content-Type': 'application/json'}
    started = app_call(app_port, 'POST', '/api/auth/start', headers, jobs.encoded({}), timeout=5)
    authorization = auth.parse_authorization_url(started[2], config)
    if not valid or started[0] != 200 or not authorization:
        raise RuntimeError('owned local OAuth start failed')
    state['expected_challenge'] = authorization['challenge']
    callback = auth.callback_request(app_port, cookie, authorization['state'])
    if callback[0] != 303:
        raise RuntimeError('owned local OAuth callback failed')
    return headers


def submit_get(app_port, headers, kind, file_id, key, game='silenthilltownfall'):
    intent = {'kind': kind, 'game': game, 'mod_id': 7, 'file_id': file_id,
              'idempotency_key': key}
    queued = app_call(app_port, 'POST', '/api/jobs', headers, jobs.encoded(intent), timeout=10)
    result = {'intent': intent, 'submit_status': queued[0], 'id': None,
              'outer_status': None, 'preparation': {}}
    if queued[0] != 202:
        try: result['error_code'] = json.loads(queued[2]).get('error', {}).get('code')
        except (UnicodeError, json.JSONDecodeError): pass
        return queued, result
    try: identifier = json.loads(queued[2]).get('id')
    except (UnicodeError, json.JSONDecodeError): return queued, result
    result['id'] = identifier
    if not isinstance(identifier, str) or not re.fullmatch('[a-f0-9]{32}', identifier):
        return queued, result
    completed = app_call(app_port, 'GET', '/api/jobs/' + identifier, headers, timeout=180)
    if completed[0] != 200:
        result['get_status'] = completed[0]
        return completed, result
    try: report = json.loads(completed[2])
    except (UnicodeError, json.JSONDecodeError): return completed, result
    result.update({'outer_status': report.get('status'), 'reported_kind': report.get('kind'),
                   'sha256': report.get('sha256'), 'bytes_downloaded': report.get('bytes_downloaded'),
                   'compatibility': report.get('compatibility'),
                   'preparation': report.get('preparation') or {}})
    return completed, result


def output_paths(scratch, identifier):
    return scratch / ('mb-io-' + identifier + '-out'), scratch / ('mb-io-' + identifier + '-q')


def inspect_artifacts(scratch, row, compare_leon_oracle=False):
    identifier = row.get('id') or ''
    output_dir, _ = output_paths(scratch, identifier)
    prep = row.get('preparation') or {}
    artifacts = prep.get('artifacts')
    if (not identifier or prep.get('status') != 'prepared' or prep.get('artifact_complete') is not True
            or prep.get('physical_readback_observed') is not True or not isinstance(artifacts, list)
            or len(artifacts) != 5):
        return False, []
    observed = []
    okay = True
    for index, name in enumerate(ARTIFACT_NAMES):
        path = output_dir / name
        descriptor = artifacts[index] if index < len(artifacts) else {}
        if not path.is_file():
            okay = False
            continue
        row_identity = identity(path)
        okay = okay and descriptor.get('index') == index
        okay = okay and descriptor.get('bytes') == row_identity['bytes']
        okay = okay and descriptor.get('sha256') == row_identity['sha256']
        if compare_leon_oracle:
            oracle = LEON_ORACLE / name
            okay = okay and oracle.is_file() and identity(oracle) == row_identity
        observed.append({'index': index, **row_identity})
    return bool(okay), observed


def assert_fixture_config(config_path, profile_path=None):
    config_path = Path(config_path).resolve(strict=True)
    config = json.loads(config_path.read_bytes())
    if config.get('client_id') not in ('owned_native_jobs_client', 'fixture_public_client'):
        raise RuntimeError('test accepts only its fixed nonsecret public client id')
    if config.get('users_resolve') != 'users.nexusmods.com:443:127.0.0.1' or config.get('api_resolve') != 'api.nexusmods.com:443:127.0.0.1':
        raise RuntimeError('fixture DNS mappings must remain pinned to loopback')
    if not Path(config['ca_pem_path']).is_file():
        raise RuntimeError('owned fixture CA missing')
    if profile_path:
        profile = json.loads(Path(profile_path).read_bytes())
        expected = {'base_toc_path': str(BASE_TOC), 'base_toc_bytes': EXPECTED_INPUTS['base_toc'][0],
            'base_toc_sha256': EXPECTED_INPUTS['base_toc'][1], 'base_header_path': str(BASE_HEADER),
            'base_header_bytes': EXPECTED_INPUTS['base_header'][0],
            'base_header_sha256': EXPECTED_INPUTS['base_header'][1],
            'observed_base_ucas_bytes': 128240377200, 'nexus_game': 'silenthilltownfall', 'timeout_ms': 300000}
        if profile != expected:
            raise RuntimeError('captured IoStore profile identity/scope mismatch')
    return config


def run_red(output):
    """Use only the retained pre-IoStore app binary to record the expected refusal."""
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    prior_receipt = previous.RED_RECEIPT
    prior = json.loads(prior_receipt.read_text(encoding='utf-8'))
    app_id = prior.get('app_identity', {})
    binary_path = ROOT / app_id.get('private_directory', '') / 'modbridge-native-auth-fixture'
    if (prior.get('all_passed') is not True or app_id.get('fixture_binary_sha256') != RED_BINARY_SHA256
            or not binary_path.is_file() or digest_file(binary_path) != RED_BINARY_SHA256):
        raise RuntimeError('historical old-app RED binary/receipt identity mismatch')
    old_profile = Path(app_id.get('profile', '')).resolve(strict=True)
    old_manifest = json.loads((old_profile / 'manifest.json').read_text(encoding='utf-8'))
    old_job_source = old_manifest.get('source_sha256', {}).get(
        'outputs/ps5-mod-bridge/native-host/jobs.cpp')
    historical_source = subprocess.run(['git', 'show',
        previous.RED_SOURCE_COMMIT + ':outputs/ps5-mod-bridge/native-host/jobs.cpp'],
        cwd=ROOT, capture_output=True, timeout=15)
    historical_sha = hashlib.sha256(historical_source.stdout).hexdigest()
    if historical_source.returncode != 0 or not old_job_source or historical_sha != old_job_source:
        raise RuntimeError('historical RED app is not bound to the pinned A196 source')
    old_config_path = RED_CONFIG.resolve(strict=True)
    config = assert_fixture_config(old_config_path)
    scratch = old_config_path.parent / 'scratch'
    if list(scratch.iterdir()): raise RuntimeError('historical fixture scratch is not clean')
    inputs_before = source_identities()
    state = {'config': config, 'premium': True, 'records': [], 'archives': fixture_archives()}
    port, close_tls, _ = jobs.fixture_server(old_config_path.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    proc = None; checks = {}; observed = {}
    def check(name, value): checks[name] = bool(value)
    try:
        proc, initial = launch(binary_path, app_port, port)
        headers = login(app_port, config, state)
        explicit = app_call(app_port, 'POST', '/api/jobs', headers, jobs.encoded({
            'kind': 'download_prepare_iostore', 'game': 'silenthilltownfall', 'mod_id': 7,
            'file_id': 8, 'idempotency_key': 'owned_iostore_historical_red_001'}), timeout=10)
        observed['explicit_status'] = explicit[0]
        try: observed['explicit_error'] = json.loads(explicit[2]).get('error', {}).get('code')
        except (UnicodeError, json.JSONDecodeError): observed['explicit_error'] = None
        check('historical_binary_refuses_new_iostore_intent', explicit[0] != 202)
        check('historical_refusal_precedes_archive_transfer', not any(r['route'] == 'archive' for r in state['records']))
        # The pre-IoStore automatic handler may analyze the source, but must not
        # claim that an IoStore bundle was prepared or emit private artifacts.
        automatic, row = submit_get(app_port, headers, 'download_prepare', 9,
                                    'owned_iostore_auto_historical_red_002')
        observed['automatic_submit_status'] = row['submit_status']
        observed['automatic_preparation_status'] = (row.get('preparation') or {}).get('status')
        observed['automatic_http_status'] = automatic[0]
        check('historical_auto_does_not_claim_iostore_prepared',
              row.get('submit_status') != 202 or (row.get('preparation') or {}).get('status') != 'prepared')
        check('historical_red_keeps_no_private_output', not list(scratch.iterdir()))
        check('historical_red_source_identity_unchanged', source_identities() == inputs_before)
        # This phase intentionally expects all RED checks to pass as observations
        # of the old app's missing capability, not as an IoStore success claim.
        app_call(app_port, 'POST', '/shutdown', {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=5)
        stdout, stderr = proc.communicate(timeout=10)
        check('historical_app_shutdown_clean', proc.returncode == 0 and not stderr)
    finally:
        if proc and proc.poll() is None:
            proc.terminate(); proc.communicate(timeout=6)
        close_tls()
        (output / 'receipt.json').write_bytes(jobs.encoded({
            'phase': 'historical_a196_pre_iostore_app_red', 'checks': checks,
            'all_passed': bool(checks) and all(checks.values()), 'observed': observed,
            'app_binary_sha256': RED_BINARY_SHA256, 'prior_receipt': str(prior_receipt),
            'historical_source_commit': previous.RED_SOURCE_COMMIT,
            'historical_jobs_cpp_sha256': old_job_source,
            'source_identities_before': inputs_before, 'source_identities_after': source_identities(),
            'tls_routes': [{'route': r['route'], 'status': r.get('status'), 'contract_ok': r.get('contract_ok')}
                           for r in state['records']],
            'live_account': False, 'console_executed': False, 'runtime_or_install_claimed': False}))
    return checks


def run_green(profile, config_path, output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    profile = Path(profile).resolve(strict=True)
    config_path = Path(config_path).resolve(strict=True)
    config = assert_fixture_config(config_path, CAPTURED_PROFILE)
    scratch = config_path.parent / 'scratch'
    if list(scratch.iterdir()): raise RuntimeError('fixture scratch is not clean before GREEN')
    before = source_identities()
    archives = fixture_archives(include_mixed=True)
    auth.MATRIX_DIR = output / 'relink'
    binary, app_identity = auth.relink_with_socket_shim(profile)
    state = {'config': config, 'premium': True, 'records': [], 'archives': archives}
    port, close_tls, _ = jobs.fixture_server(config_path.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; summary = {}; proc = None; completed = False
    def check(name, value): checks[name] = bool(value)
    try:
        proc, initial = launch(binary, app_port, port)
        headers = login(app_port, config, state)
        check('actual_owned_tls_sign_in', initial[0] == 200 and any(r['route'] == 'token' for r in state['records']))

        # First preserve and prepare the original keyboard/camera RAR unchanged.
        response, camera = submit_get(app_port, headers, 'download_prepare_iostore', 8,
                                     'owned_iostore_camera_original_001')
        camera_prep = camera.get('preparation') or {}; camera_id = camera.get('id')
        camera_artifacts_ok, camera_artifacts = inspect_artifacts(scratch, camera)
        check('camera_original_download_identity', response[0] == 200 and camera['submit_status'] == 202
              and camera['outer_status'] == 'analyzed' and camera.get('sha256') == EXPECTED_INPUTS['camera_rar'][1]
              and camera.get('bytes_downloaded') == EXPECTED_INPUTS['camera_rar'][0])
        check('camera_iostore_report_and_physical_readback', camera_prep.get('handler') == 'unreal_iostore'
              and camera_prep.get('status') == 'prepared' and camera_prep.get('whole_bundle_handled') is True
              and camera_prep.get('physical_readback_observed') is True and camera_artifacts_ok
              and camera_prep.get('incoming_asset_hashes_verified') == 26
              and camera_prep.get('emitted_chunks') == 27
              and camera_prep.get('verified_empty_pak_companions') == 1)
        check('camera_prepared_not_installed_or_runtime', camera_prep.get('installation_supported') is False
              and camera_prep.get('runtime_supported') is False and camera.get('compatibility') == 'unresolved')
        summary['camera'] = {'id': camera_id, 'outer_status': camera.get('outer_status'),
            'sha256': camera.get('sha256'), 'bytes': camera.get('bytes_downloaded'),
            'preparation_status': camera_prep.get('status'), 'incoming_assets': camera_prep.get('incoming_asset_hashes_verified'),
            'emitted_chunks': camera_prep.get('emitted_chunks'), 'artifacts': camera_artifacts}

        # Then the unchanged Leon source, with all five physical files compared
        # against the prior one-source original-archive oracle.
        response, leon = submit_get(app_port, headers, 'download_prepare_iostore', 9,
                                   'owned_iostore_leon_original_002')
        leon_prep = leon.get('preparation') or {}; leon_id = leon.get('id')
        leon_artifacts_ok, leon_artifacts = inspect_artifacts(scratch, leon, compare_leon_oracle=True)
        check('leon_original_download_identity', response[0] == 200 and leon['submit_status'] == 202
              and leon['outer_status'] == 'analyzed' and leon.get('sha256') == EXPECTED_INPUTS['leon_zip'][1]
              and leon.get('bytes_downloaded') == EXPECTED_INPUTS['leon_zip'][0])
        check('leon_all_five_artifacts_match_original_source_oracle', leon_prep.get('status') == 'prepared'
              and leon_prep.get('whole_bundle_handled') is True and leon_prep.get('physical_readback_observed') is True
              and leon_artifacts_ok and leon_prep.get('verified_empty_pak_companions') == 1
              and leon_prep.get('verified_plaintext_documents') == 1)
        check('leon_no_install_or_runtime_claim', leon_prep.get('installation_supported') is False
              and leon_prep.get('runtime_supported') is False and leon.get('compatibility') == 'unresolved')
        summary['leon'] = {'id': leon_id, 'outer_status': leon.get('outer_status'),
            'sha256': leon.get('sha256'), 'bytes': leon.get('bytes_downloaded'),
            'preparation_status': leon_prep.get('status'), 'incoming_assets': leon_prep.get('incoming_asset_hashes_verified'),
            'emitted_chunks': leon_prep.get('emitted_chunks'), 'artifacts': leon_artifacts}

        # Automatic dispatch is only a hint: actual unchanged Leon content has
        # one valid IoStore group, one semantically empty PAK and one README.
        auto_response, auto = submit_get(app_port, headers, 'download_prepare', 12,
                                        'owned_iostore_automatic_leon_003')
        auto_prep = auto.get('preparation') or {}
        auto_artifacts_ok, auto_artifacts = inspect_artifacts(scratch, auto, compare_leon_oracle=True)
        check('automatic_iostore_hint_dispatches_after_full_validation', auto_response[0] == 200
              and auto['outer_status'] == 'analyzed' and auto_prep.get('status') == 'prepared'
              and auto_prep.get('automatic_dispatch') is True
              and auto_prep.get('dispatch_basis') == 'inventory_hint_then_core_validation'
              and auto_prep.get('handler') == 'unreal_iostore' and auto_prep.get('whole_bundle_handled') is True
              and auto_artifacts_ok and auto_prep.get('verified_empty_pak_companions') == 1
              and auto_prep.get('verified_plaintext_documents') == 1)
        check('automatic_iostore_no_install_or_runtime_claim', auto_prep.get('installation_supported') is False
              and auto_prep.get('runtime_supported') is False and auto.get('compatibility') == 'unresolved')
        summary['automatic'] = {'id': auto.get('id'), 'outer_status': auto.get('outer_status'),
            'preparation_status': auto_prep.get('status'), 'artifacts': auto_artifacts}

        # Even though the same file contains a valid Leon IoStore group, an
        # unrecognized extra makes the whole original bundle actionable only.
        mixed_response, mixed = submit_get(app_port, headers, 'download_prepare', 10,
                                           'owned_iostore_unknown_extra_004')
        mixed_prep = mixed.get('preparation') or {}; mixed_id = mixed.get('id')
        mixed_out, mixed_q = output_paths(scratch, mixed_id or '')
        check('mixed_unknown_member_refuses_whole_bundle_without_output',
              mixed_response[0] == 200 and mixed['outer_status'] == 'analyzed'
              and mixed_prep.get('status') == 'additional_action_required'
              and mixed_prep.get('whole_bundle_handled') is False
              and mixed_prep.get('unhandled_members', 0) > 0
              and mixed_prep.get('artifact_complete') is False and not mixed_out.exists()
              and not mixed_q.exists())
        summary['mixed_unknown'] = {'id': mixed_id, 'outer_status': mixed.get('outer_status'),
            'preparation_status': mixed_prep.get('status'), 'unhandled_members': mixed_prep.get('unhandled_members')}

        # Existing source profile only binds Townfall; the archive is real and
        # downloadable, but a different source game cannot borrow the target.
        wrong_response, wrong = submit_get(app_port, headers, 'download_prepare_iostore', 11,
                                           'owned_iostore_wrong_profile_005', game='skyrim')
        wrong_prep = wrong.get('preparation') or {}; wrong_id = wrong.get('id')
        wrong_out, wrong_q = output_paths(scratch, wrong_id or '')
        check('wrong_game_profile_stays_unresolved', wrong_response[0] == 200
              and wrong['outer_status'] == 'analyzed' and wrong_prep.get('status') == 'unresolved'
              and wrong_prep.get('artifact_complete') is not True and not wrong_out.exists() and not wrong_q.exists())
        summary['wrong_profile'] = {'id': wrong_id, 'outer_status': wrong.get('outer_status'),
            'preparation_status': wrong_prep.get('status')}

        before_records = len(state['records'])
        same_intent = leon['intent']
        duplicate = app_call(app_port, 'POST', '/api/jobs', headers, jobs.encoded(same_intent), timeout=10)
        check('same_kind_idempotency_reuses_job_without_second_transfer', duplicate[0] == 202
              and json.loads(duplicate[2]).get('id') == leon_id and len(state['records']) == before_records)
        conflict = app_call(app_port, 'POST', '/api/jobs', headers,
            jobs.encoded({**same_intent, 'kind': 'download_prepare'}), timeout=10)
        check('kind_conflict_does_not_reuse_iostore_job', conflict[0] == 409
              and len(state['records']) == before_records)

        public = app_call(app_port, 'GET', '/api/jobs', headers, timeout=10)[2]
        forbidden = [b'owned-jobs-access-token', b'owned-jobs-refresh-token', b'fixture_signature',
            b'https://api.nexusmods.com/archive', str(scratch).encode(), str(BASE_TOC).encode(),
            str(BASE_HEADER).encode(), str(CAMERA_RAR).encode(), str(LEON_ZIP).encode()]
        check('public_jobs_exclude_signed_uris_credentials_and_local_paths',
              all(secret not in public for secret in forbidden))
        check('all_archive_requests_are_credential_free',
              all(row.get('no_account_headers') is True for row in state['records'] if row['route'] == 'archive'))
        check('all_owned_tls_requests_match_fixture_contract', bool(state['records'])
              and all(row.get('contract_ok') and row.get('response_sent') for row in state['records']))
        check('all_original_source_and_profile_inputs_unchanged', source_identities() == before
              and json.loads(CAPTURED_PROFILE.read_bytes())['nexus_game'] == 'silenthilltownfall')

        shutdown = app_call(app_port, 'POST', '/shutdown',
            {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=5)
        stdout, stderr = proc.communicate(timeout=15)
        check('owned_shutdown_removes_all_private_job_artifacts', shutdown[0] == 200
              and proc.returncode == 0 and not list(scratch.iterdir()) and not stderr)
        completed = True
    finally:
        if proc and proc.poll() is None:
            proc.terminate(); proc.communicate(timeout=8)
        close_tls()
        after = source_identities()
        receipt = {'phase': 'actual_owned_tls_iostore_job_preparation', 'checks': checks,
            'all_passed': completed and bool(checks) and all(checks.values()),
            'job_summaries': summary, 'app_identity': app_identity,
            'fixed_input_identities_before': before, 'fixed_input_identities_after': after,
            'leon_original_oracle': str(LEON_ORACLE),
            'leon_original_oracle_sha256': {name: identity(LEON_ORACLE / name) for name in ARTIFACT_NAMES},
            'tls_routes': [{'route': row['route'], 'status': row.get('status'),
                'contract_ok': row.get('contract_ok'), 'response_sent': row.get('response_sent'),
                'no_account_headers': row.get('no_account_headers')} for row in state['records']],
            'fixture_archive_bytes': {str(key): len(value) for key, value in archives.items()},
            'private_scratch_empty_after_shutdown': not list(scratch.iterdir()),
            'live_account': False, 'console_executed': False,
            'game_files_changed': False, 'runtime_or_install_claimed': False}
        (output / 'receipt.json').write_bytes(jobs.encoded(receipt))
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('red', 'green'), required=True)
    parser.add_argument('--app-profile', type=Path)
    parser.add_argument('--oauth-config', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.phase == 'red':
        checks = run_red(args.output.resolve())
    else:
        if args.app_profile is None or args.oauth_config is None:
            parser.error('green requires --app-profile and --oauth-config')
        receipt = run_green(args.app_profile, args.oauth_config, args.output.resolve())
        checks = receipt['checks']
    print(json.dumps({'phase': args.phase, 'checks': checks,
                      'passed': bool(checks) and all(checks.values()),
                      'receipt': str(args.output.resolve() / 'receipt.json')}, sort_keys=True))
    return 0 if checks and all(checks.values()) else 1


if __name__ == '__main__':
    raise SystemExit(main())
