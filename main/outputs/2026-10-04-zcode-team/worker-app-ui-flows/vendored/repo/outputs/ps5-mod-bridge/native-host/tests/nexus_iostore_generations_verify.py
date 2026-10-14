#!/usr/bin/env python3
"""Owned HTTPS proof for explicit multi-archive IoStore generations.

This drives the actual app HTTP/auth/download/job/generation path against a
loopback-routed TLS fixture. Captured archives and base metadata are only read;
the result is private preparation, not installation or runtime evidence.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
from urllib.parse import urlsplit

import nexus_auth_verify as auth
import nexus_jobs_verify as jobs
import nexus_iostore_jobs_verify as iostore

ROOT = auth.ROOT
SPIKE = ROOT / '.superpowers/native-dependency-spike/native-iostore-generations-1'
CONFIG = iostore.CONFIG
PROFILE = iostore.CAPTURED_PROFILE
LEON = iostore.LEON_ZIP
MOVEMENT = ROOT / '.superpowers/native-observations/movement-speed-real-mod-intake-2026-10-01/movement-speed-2x.zip'
CAMERA = iostore.CAMERA_RAR
BASE_TOC = iostore.BASE_TOC
BASE_HEADER = iostore.BASE_HEADER
ORACLE = ROOT / '.superpowers/native-dependency-spike/iostore-batch-oracle-1/oracle-result.json'
ARCHIVE_ORACLE = ROOT / '.superpowers/native-dependency-spike/iostore-batch-oracle-1/archive-oracle-result.json'
CAMERA_LEON_ORACLE = ROOT / '.superpowers/native-dependency-spike/iostore-three-archive-oracle-1/oracle-result.json'
RED_RECEIPT = ROOT / '.superpowers/native-dependency-spike/native-iostore-prepare-jobs-1/green-2/receipt.json'
RED_COMMIT = '5861be3'
RED_PROFILE_ARTIFACT_SHA256 = '1f9479a72b58c519a37cfc8b8494b0b4c84a139259bd538cf1b7929b6c400df4'
RED_BINARY_SHA256 = '5121f4b3a86241b2c144530b7a506b7f9893d0a762d5365c23f877da67e02081'
ORACLE_SHA256 = 'a7539135fbd4c259d5cc4871e945775f5240c7b9216b74c0796b2fc1c80ec69f'
ARCHIVE_ORACLE_SHA256 = '93f4f4d0cce2d1782c7bf0ebbf56152c02083cf703a0c4c9e97d61ee4ddd41e0'
CAMERA_LEON_ORACLE_SHA256 = 'a5d934bfc67cd708e966f10ece87ebf7e0a8a7660995e7aa7690acd5989b1d80'
EXPECTED = {
    'leon': (105647244, '661aabf0e6a1cb91e9164501108b5d19db13c2ca21cee651f676e0706f3feef9'),
    'movement': (2805, '3850a2b51d96a0fb2055a26371b7a6b3dfd527a68f1e34ecf3cdd2c47293d6ad'),
    'camera': (8043992, 'ac511becb74c5e7d874806051b56e314e68896e6d29b0d510d5394412890c022'),
    'base_toc': (29707562, '35de27526c2ed91a7d21ab027b4e2ad6dc58cca137ab92c31e1f60c9e0bb75b5'),
    'base_header': (2272324, 'd7b6f7ca24d57aed9d4a3f460bc49ad7bd8f6b4623896e1e84dcdc37ac73a2a7'),
}
ARTIFACTS = ('generated-sidecar.ucas', 'generated-small.utoc',
             'merged-container-header.bin', 'generated-directory.bin', 'generated-final.utoc')
ARTIFACT_ORACLE_KEYS = ('sidecar', 'small_toc', 'header', 'directory', 'final_toc')


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for part in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(part)
    return digest.hexdigest()


def identity(path):
    path = Path(path)
    stat = path.lstat()
    if not path.is_file() or path.is_symlink():
        raise RuntimeError('fixed input is not a regular nonsymlink file')
    return {'bytes': stat.st_size, 'device': stat.st_dev, 'inode': stat.st_ino,
            'sha256': sha256_file(path)}


def fixed_inputs():
    inputs = {'leon': identity(LEON), 'movement': identity(MOVEMENT), 'camera': identity(CAMERA),
              'base_toc': identity(BASE_TOC), 'base_header': identity(BASE_HEADER),
              'profile': identity(PROFILE)}
    for key, expected in EXPECTED.items():
        if inputs[key]['bytes'] != expected[0] or inputs[key]['sha256'] != expected[1]:
            raise RuntimeError('retained local input identity changed: ' + key)
    return inputs


def validate_oracles():
    if sha256_file(ORACLE) != ORACLE_SHA256:
        raise RuntimeError('retained Leon+Movement batch oracle identity changed')
    oracle = json.loads(ORACLE.read_bytes())
    if (oracle.get('result') != 'pass' or oracle.get('batch_conflict_free') is not True
            or len(oracle.get('sources', [])) != 2
            or [row.get('label') for row in oracle['sources']] != ['Leon 2K original', 'Movement original']):
        raise RuntimeError('retained Leon+Movement oracle no longer describes the expected pair')
    if sha256_file(ARCHIVE_ORACLE) != ARCHIVE_ORACLE_SHA256:
        raise RuntimeError('retained archive-byte oracle identity changed')
    archive_oracle = json.loads(ARCHIVE_ORACLE.read_bytes())
    if (archive_oracle.get('result') != 'pass' or archive_oracle.get('batch_conflict_free') is not True
            or [row.get('label') for row in archive_oracle.get('sources', [])] !=
               ['Leon 2K original', 'Movement original']
            or archive_oracle.get('outputs', {}).get('sidecar_bytes') != oracle.get('outputs', {}).get('sidecar_bytes')):
        raise RuntimeError('retained archive-byte oracle does not describe the expected pair')
    if sha256_file(CAMERA_LEON_ORACLE) != CAMERA_LEON_ORACLE_SHA256:
        raise RuntimeError('retained camera/Leon overlap oracle identity changed')
    camera_oracle = json.loads(CAMERA_LEON_ORACLE.read_bytes())
    labels = [row.get('label') for row in camera_oracle.get('sources', [])]
    collisions = camera_oracle.get('inter_source_conflicts', {})
    if (camera_oracle.get('result') != 'pass' or camera_oracle.get('batch_conflict_free') is not True
            or 'Leon 2K original' not in labels or 'Original keyboard-bound third-person archive' not in labels
            or any(collisions.get(name) for name in ('asset_ids', 'package_ids', 'virtual_paths'))):
        raise RuntimeError('retained original camera/Leon overlap oracle is not conflict-free')
    return oracle, archive_oracle, camera_oracle


def local_call(port, method, path, headers=None, payload=b'', timeout=180):
    return auth.local_request(port, method, path, headers, payload, timeout=timeout)


def start_app(binary, app_port, fixture_port):
    process = subprocess.Popen([str(binary), '--port', str(app_port), '--lifetime', '300'],
        env={'PATH': '/usr/bin:/bin:/usr/sbin:/sbin',
             'MB_NEXUS_AUTH_FIXTURE_PORT': str(fixture_port)},
        stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    deadline = time.monotonic() + 10
    while time.monotonic() < deadline:
        try:
            response = local_call(app_port, 'GET', '/api/capabilities', timeout=3)
            return process, response
        except OSError:
            if process.poll() is not None:
                raise RuntimeError('fixture host exited before readiness')
            time.sleep(.025)
    process.terminate(); process.communicate(timeout=5)
    raise RuntimeError('fixture host readiness deadline expired')


def compiled_scratch(profile):
    header = Path(profile) / 'config.h'
    source = header.read_text(encoding='utf-8')
    match = re.search(r'^#define MB_HOST_JOB_DIRECTORY ("(?:\\.|[^"\\])*")$', source, re.MULTILINE)
    if not match:
        raise RuntimeError('application profile does not bind a private jobs directory')
    directory = Path(json.loads(match.group(1))).resolve(strict=True)
    expected = (SPIKE / 'fixture-config-1/scratch').resolve(strict=True)
    stat = directory.stat()
    if directory != expected or not directory.is_dir() or stat.st_uid != os.geteuid() or stat.st_mode & 0o777 != 0o700:
        raise RuntimeError('compiled application jobs directory is not the dedicated private fixture scratch')
    return directory


def sign_in(app_port, config, state):
    initial = local_call(app_port, 'GET', '/api/capabilities', timeout=5)
    cookie, csrf, valid = auth.get_cookie_and_csrf(initial[1], json.loads(initial[2]))
    origin = config['redirect_uri'].rsplit('/auth/callback', 1)[0]
    headers = {'Cookie': 'mb_session=' + cookie, 'X-CSRF-Token': csrf,
               'Origin': origin, 'Content-Type': 'application/json'}
    start = local_call(app_port, 'POST', '/api/auth/start', headers, jobs.encoded({}), timeout=5)
    authorization = auth.parse_authorization_url(start[2], config)
    if not valid or start[0] != 200 or not authorization:
        raise RuntimeError('owned OAuth start failed')
    state['expected_challenge'] = authorization['challenge']
    callback = auth.callback_request(app_port, cookie, authorization['state'])
    if callback[0] != 303:
        raise RuntimeError('owned OAuth callback failed')
    return headers


def headers_for_new_session(app_port, config):
    response = local_call(app_port, 'GET', '/api/capabilities', timeout=5)
    cookie, csrf, valid = auth.get_cookie_and_csrf(response[1], json.loads(response[2]))
    if not valid:
        raise RuntimeError('second isolated local session was not issued')
    return {'Cookie': 'mb_session=' + cookie, 'X-CSRF-Token': csrf,
            'Origin': config['redirect_uri'].rsplit('/auth/callback', 1)[0],
            'Content-Type': 'application/json'}


def download_analyze(app_port, headers, file_id, key, game='silenthilltownfall'):
    response, row = iostore.submit_get(app_port, headers, 'download_analyze', file_id, key, game=game)
    if row.get('id') and not re.fullmatch('[a-f0-9]{32}', row['id']):
        raise RuntimeError('download job ID was not canonical lowercase hex')
    return response, row


def generation_request(app_port, headers, source_ids, key):
    # Exactly the flat public generation contract: no URLs, target path, or
    # caller-selected base/context is accepted by this route.
    body = {'kind': 'prepare_iostore_generation', 'source_job_ids': list(source_ids),
            'idempotency_key': key}
    return local_call(app_port, 'POST', '/api/generations', headers, jobs.encoded(body), timeout=180)


def get_generation(app_port, headers, identifier):
    return local_call(app_port, 'GET', '/api/generations/' + identifier, headers, timeout=180)


def list_generations(app_port, headers):
    return local_call(app_port, 'GET', '/api/generations', headers, timeout=20)


def extract_json(response):
    try:
        result = json.loads(response[2])
        return result if isinstance(result, dict) else {}
    except (UnicodeError, json.JSONDecodeError):
        return {}


def oracle_artifacts(oracle, archive_oracle):
    output = oracle['outputs']
    archive_output = archive_oracle['outputs']
    if archive_output['sidecar_bytes'] != output['sidecar_bytes']:
        raise RuntimeError('sidecar length differs across retained batch oracles')
    return {
        'generated-sidecar.ucas': {'bytes': output['sidecar_bytes'], 'sha256': archive_output['sidecar_sha256']},
        'generated-small.utoc': output['small_toc'],
        'merged-container-header.bin': {'bytes': output['header_bytes'], 'sha256': output['header_sha256']},
        'generated-directory.bin': {'bytes': output['directory_bytes'], 'sha256': output['directory_sha256']},
        'generated-final.utoc': output['final_toc'],
    }


def find_generation_output(scratch):
    candidates = set()
    for name in ARTIFACTS:
        for path in Path(scratch).rglob(name):
            if path.is_file() and not path.is_symlink():
                candidates.add(path.parent)
    complete = [directory for directory in candidates
                if all((directory / name).is_file() and not (directory / name).is_symlink() for name in ARTIFACTS)]
    return complete[0] if len(complete) == 1 else None


def check_prepared_generation(record, directory, oracle, archive_oracle):
    prep = record.get('preparation') or {}
    descriptors = prep.get('artifacts')
    expected = oracle_artifacts(oracle, archive_oracle)
    okay = (record.get('status') == 'prepared' and record.get('kind') == 'prepare_iostore_generation'
            and record.get('source_job_ids') and len(record['source_job_ids']) == 2
            and prep.get('status') == 'prepared' and prep.get('handler') == 'unreal_iostore_generation'
            and prep.get('source_count') == 2 and prep.get('incoming_asset_hashes_verified') == 162
            and prep.get('emitted_chunks') == 163 and prep.get('artifact_complete') is True
            and prep.get('physical_readback_observed') is True and prep.get('whole_bundle_handled') is True
            and isinstance(descriptors, list) and len(descriptors) == 5
            and prep.get('verified_empty_pak_companions') == 2
            and prep.get('verified_plaintext_documents') == 2
            and record.get('installation_supported') is False and record.get('runtime_supported') is False)
    observed = []
    if directory is None:
        return False, observed
    for index, name in enumerate(ARTIFACTS):
        path = directory / name
        actual = {'bytes': path.stat().st_size, 'sha256': sha256_file(path)}
        expected_row = expected[name]
        descriptor = descriptors[index] if isinstance(descriptors, list) and index < len(descriptors) else {}
        okay = okay and actual == {'bytes': expected_row['bytes'], 'sha256': expected_row['sha256']}
        okay = okay and descriptor.get('index') == index and descriptor.get('bytes') == actual['bytes']
        okay = okay and descriptor.get('sha256') == actual['sha256']
        observed.append({'index': index, **actual})
    return bool(okay), observed


def make_archives():
    return {20: LEON.read_bytes(), 21: MOVEMENT.read_bytes(), 22: b'not a supported archive\n',
            23: MOVEMENT.read_bytes(), 24: MOVEMENT.read_bytes(), 25: LEON.read_bytes()}


def run_red(output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    receipt = json.loads(RED_RECEIPT.read_bytes())
    app = receipt.get('app_identity', {})
    binary = ROOT / app.get('private_directory', '') / 'modbridge-native-auth-fixture'
    if (receipt.get('all_passed') is not True
            or app.get('profile_artifact_sha256') != RED_PROFILE_ARTIFACT_SHA256
            or app.get('fixture_binary_sha256') != RED_BINARY_SHA256
            or not binary.is_file() or sha256_file(binary) != RED_BINARY_SHA256):
        raise RuntimeError('pinned pre-generation app evidence changed')
    profile = Path(app['profile']).resolve(strict=True)
    manifest = json.loads((profile / 'manifest.json').read_bytes())
    source_hash = manifest.get('source_sha256', {}).get('outputs/ps5-mod-bridge/native-host/jobs.cpp')
    old_source = subprocess.run(['git', 'show', RED_COMMIT + ':outputs/ps5-mod-bridge/native-host/jobs.cpp'],
                                cwd=ROOT, capture_output=True, timeout=15)
    if old_source.returncode or hashlib.sha256(old_source.stdout).hexdigest() != source_hash:
        raise RuntimeError('pinned pre-generation app is not the requested 5861be3 source')
    config = iostore.assert_fixture_config(CONFIG, PROFILE)
    before = fixed_inputs()
    oracle, archive_oracle, camera_oracle = validate_oracles()
    state = {'config': config, 'premium': True, 'records': [], 'archives': make_archives()}
    port, close_tls, _ = jobs.fixture_server(Path(config['ca_pem_path']).parent, state)
    app_port = urlsplit(config['redirect_uri']).port
    process = None; checks = {}; completed = False
    def check(name, passed): checks[name] = bool(passed)
    try:
        process, _ = start_app(binary, app_port, port)
        headers = sign_in(app_port, config, state)
        response = local_call(app_port, 'GET', '/api/generations', headers, timeout=5)
        check('5861be3_pre_generation_route_returns_404', response[0] == 404)
        check('pre_generation_probe_did_not_trigger_archive_transfer',
              not any(row['route'] == 'archive' for row in state['records']))
        check('login_used_only_owned_fixture_tls', bool(state['records'])
              and all(row.get('contract_ok') and row.get('response_sent') for row in state['records']))
        local_call(app_port, 'POST', '/shutdown', {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=5)
        _, stderr = process.communicate(timeout=10)
        check('pre_generation_host_shutdown_clean', process.returncode == 0 and not stderr)
        completed = True
    finally:
        if process and process.poll() is None:
            process.terminate(); process.communicate(timeout=6)
        close_tls()
        payload = {'phase': 'historical_5861be3_missing_generation_route', 'checks': checks,
            'all_passed': completed and bool(checks) and all(checks.values()),
            'historical_source_commit': RED_COMMIT, 'profile_artifact_sha256': RED_PROFILE_ARTIFACT_SHA256,
            'fixture_binary_sha256': RED_BINARY_SHA256, 'fixed_inputs_before': before,
            'fixed_inputs_after': fixed_inputs(), 'batch_oracle_sha256': ORACLE_SHA256,
            'archive_batch_oracle_sha256': ARCHIVE_ORACLE_SHA256,
            'camera_leon_oracle_sha256': CAMERA_LEON_ORACLE_SHA256,
            'camera_leon_pairwise_conflict_free': camera_oracle.get('batch_conflict_free') is True,
            'tls_routes': [{'route': row['route'], 'status': row.get('status'),
                'contract_ok': row.get('contract_ok')} for row in state['records']],
            'live_account': False, 'console_executed': False, 'runtime_or_install_claimed': False}
        (output / 'receipt.json').write_bytes(jobs.encoded(payload))
    return checks


def run_green(profile, config_path, output):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    profile = Path(profile).resolve(strict=True)
    config_path = Path(config_path).resolve(strict=True)
    config = iostore.assert_fixture_config(config_path, PROFILE)
    config_dir = config_path.parent; scratch = compiled_scratch(profile)
    if list(scratch.iterdir()): raise RuntimeError('generation fixture scratch is not empty')
    before = fixed_inputs()
    oracle, archive_oracle, camera_oracle = validate_oracles()
    auth.MATRIX_DIR = output / 'relink'
    binary, app_identity = auth.relink_with_socket_shim(profile)
    state = {'config': config, 'premium': True, 'records': [], 'archives': make_archives()}
    port, close_tls, _ = jobs.fixture_server(config_dir / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; summaries = {}; process = None; completed = False
    def check(name, passed): checks[name] = bool(passed)
    try:
        process, initial = start_app(binary, app_port, port)
        owner_a = sign_in(app_port, config, state)
        check('configured_app_owned_tls_login', initial[0] == 200 and any(r['route'] == 'token' for r in state['records']))

        # The source files remain original: the pair is the prior independently
        # verified batch oracle, not a caller-supplied expected output.
        leon_response, leon = download_analyze(app_port, owner_a, 20, 'owned_generation_leon_001')
        move_response, movement = download_analyze(app_port, owner_a, 21, 'owned_generation_movement_002')
        check('unchanged_leon_download_analyzed', leon_response[0] == 200 and leon['submit_status'] == 202
              and leon.get('outer_status') == 'analyzed' and leon.get('sha256') == EXPECTED['leon'][1]
              and leon.get('bytes_downloaded') == EXPECTED['leon'][0])
        check('unchanged_movement_download_analyzed', move_response[0] == 200 and movement['submit_status'] == 202
              and movement.get('outer_status') == 'analyzed' and movement.get('sha256') == EXPECTED['movement'][1]
              and movement.get('bytes_downloaded') == EXPECTED['movement'][0])
        if not leon.get('id') or not movement.get('id'):
            raise RuntimeError('source analysis jobs did not produce IDs')
        source_ids = [leon['id'], movement['id']]

        # A second cookie owns a separate downloaded record. It must not be
        # borrowed into the first cookie's explicitly submitted generation.
        owner_b = headers_for_new_session(app_port, config)
        second_response, second_owner = download_analyze(app_port, owner_b, 24,
            'owned_generation_second_owner_003')
        check('second_local_owner_download_analyzed', second_response[0] == 200
              and second_owner.get('outer_status') == 'analyzed' and second_owner.get('id'))

        # Cross-domain source is valid on its own, but cannot join Townfall.
        wrong_response, wrong_domain = download_analyze(app_port, owner_a, 23,
            'owned_generation_wrong_domain_004', game='skyrim')
        check('wrong_domain_source_analyzed_separately', wrong_response[0] == 200
              and wrong_domain.get('outer_status') == 'analyzed' and wrong_domain.get('id'))

        # A real HTTP download of an invalid archive terminates failed and is
        # not eligible for generation membership.
        failed_response, failed = download_analyze(app_port, owner_a, 22,
            'owned_generation_failed_source_005')
        check('bad_archive_source_terminal_failed', failed_response[0] == 200
              and failed.get('outer_status') == 'failed' and failed.get('id'))

        before_gen_children = {p.name for p in scratch.iterdir()}
        queued = generation_request(app_port, owner_a, source_ids, 'owned_generation_pair_001')
        queued_json = extract_json(queued)
        generation_id = queued_json.get('id')
        check('generation_queued_from_explicit_analyzed_sources', queued[0] == 202
              and queued_json.get('kind') == 'prepare_iostore_generation'
              and queued_json.get('status') == 'queued'
              and queued_json.get('source_job_ids') == source_ids
              and queued_json.get('installation_supported') is False
              and queued_json.get('runtime_supported') is False
              and isinstance(generation_id, str) and re.fullmatch('[a-f0-9]{32}', generation_id or '') is not None)
        if not generation_id:
            raise RuntimeError('successful generation submission did not return an ID')
        done_response = get_generation(app_port, owner_a, generation_id)
        generation = extract_json(done_response)
        out_dir = find_generation_output(scratch)
        generation_ok, artifact_rows = check_prepared_generation(generation, out_dir, oracle, archive_oracle)
        check('prepared_generation_matches_batch_oracle_and_physical_readback',
              done_response[0] == 200 and generation_ok)
        check('generation_public_report_has_no_paths_or_signed_uri',
              all(value not in done_response[2] for value in (str(scratch).encode(), str(LEON).encode(),
                    str(MOVEMENT).encode(), str(BASE_TOC).encode(), str(BASE_HEADER).encode(),
                    b'fixture_signature', b'https://api.nexusmods.com/archive',
                    b'owned-jobs-access-token', b'owned-jobs-refresh-token')))

        listed = list_generations(app_port, owner_a)
        listing = extract_json(listed)
        items = listing.get('items')
        check('generation_list_is_read_only_owner_scoped', listed[0] == 200 and isinstance(items, list)
              and len(items) == 1 and items[0].get('id') == generation_id)
        other_list = extract_json(list_generations(app_port, owner_b))
        check('other_cookie_cannot_read_generation',
              get_generation(app_port, owner_b, generation_id)[0] == 404
              and other_list.get('items') == [])

        unchanged_before = len(state['records'])
        duplicate = generation_request(app_port, owner_a, source_ids, 'owned_generation_pair_001')
        duplicate_json = extract_json(duplicate)
        check('same_generation_intent_is_idempotent', duplicate[0] == 202
              and duplicate_json.get('id') == generation_id and len(state['records']) == unchanged_before)
        reordered_conflict = generation_request(app_port, owner_a, list(reversed(source_ids)),
                                                 'owned_generation_pair_001')
        check('idempotency_key_conflicts_on_different_ordered_source_set', reordered_conflict[0] == 409
              and len(extract_json(list_generations(app_port, owner_a)).get('items', [])) == 1)

        duplicate_response, duplicate_source = download_analyze(app_port, owner_a, 25,
            'owned_generation_duplicate_archive_010')
        check('same_original_archive_analyzed_as_distinct_source', duplicate_response[0] == 200
              and duplicate_source.get('outer_status') == 'analyzed'
              and duplicate_source.get('id') != leon.get('id')
              and duplicate_source.get('sha256') == leon.get('sha256'))
        conflict_children_before = {p.name for p in scratch.iterdir()}
        conflict_queued = generation_request(app_port, owner_a, [leon['id'], duplicate_source['id']],
                                              'owned_generation_duplicate_identity_011')
        conflict_initial = extract_json(conflict_queued)
        conflict_id = conflict_initial.get('id')
        conflict_done_response = get_generation(app_port, owner_a, conflict_id) if conflict_id else (0, {}, b'')
        conflict_done = extract_json(conflict_done_response)
        conflict_report = conflict_done.get('preparation') or {}
        check('identical_original_sources_produce_explicit_conflict', conflict_queued[0] == 202
              and conflict_initial.get('status') == 'queued'
              and conflict_done_response[0] == 200 and conflict_done.get('status') == 'conflict'
              and conflict_report.get('status') == 'conflict'
              and conflict_report.get('conflict') == {'kind': 'chunk_id', 'source_a': 0, 'source_b': 1}
              and conflict_done.get('runtime_supported') is False
              and conflict_done.get('installation_supported') is False)
        check('duplicate_source_conflict_publishes_no_artifacts_or_new_private_output',
              'artifacts' not in conflict_report
              and find_generation_output(scratch) == out_dir
              and {p.name for p in scratch.iterdir()} == conflict_children_before)

        duplicate_ids = generation_request(app_port, owner_a, [leon['id'], leon['id']],
                                            'owned_generation_duplicate_id_006')
        check('duplicate_source_ids_refused_before_queue', duplicate_ids[0] == 400
              and len(extract_json(list_generations(app_port, owner_a)).get('items', [])) == 2)

        refusal_cases = [
            ('mixed_owner', [leon['id'], second_owner['id']], owner_a,
             'owned_generation_mixed_owner_007'),
            ('mixed_domain', [leon['id'], wrong_domain['id']], owner_a,
             'owned_generation_mixed_domain_008'),
            ('failed_source', [leon['id'], failed['id']], owner_a,
             'owned_generation_failed_member_009'),
        ]
        for label, ids, headers, key in refusal_cases:
            children_before = {p.name for p in scratch.iterdir()}
            refused = generation_request(app_port, headers, ids, key)
            check(label + '_source_refused', refused[0] in (400, 403, 404))
            check(label + '_refusal_creates_no_output',
                  find_generation_output(scratch) == out_dir
                  and {p.name for p in scratch.iterdir()} == children_before)

        public = local_call(app_port, 'GET', '/api/generations', owner_a, timeout=20)[2]
        check('public_generation_list_has_no_credentials_or_local_paths',
              all(value not in public for value in (str(scratch).encode(), str(LEON).encode(),
                    str(MOVEMENT).encode(), str(BASE_TOC).encode(), str(BASE_HEADER).encode(),
                    b'fixture_signature', b'https://api.nexusmods.com/archive',
                    b'owned-jobs-access-token', b'owned-jobs-refresh-token')))
        check('actual_camera_and_leon_sources_have_no_prior_pairwise_conflict',
              camera_oracle.get('batch_conflict_free') is True
              and camera_oracle.get('inter_source_conflicts') == {'asset_ids': [], 'package_ids': [], 'virtual_paths': []})
        check('all_fixed_original_inputs_and_profiles_unchanged', fixed_inputs() == before)
        check('archive_requests_never_carry_account_headers',
              all(row.get('no_account_headers') is True for row in state['records'] if row['route'] == 'archive'))
        check('all_owned_tls_routes_match_the_fixture', bool(state['records'])
              and all(row.get('contract_ok') and row.get('response_sent') for row in state['records']))

        shutdown = local_call(app_port, 'POST', '/shutdown',
            {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'}, timeout=5)
        _, stderr = process.communicate(timeout=20)
        check('shutdown_cleans_sources_and_generation_artifacts', shutdown[0] == 200
              and process.returncode == 0 and not list(scratch.iterdir()) and not stderr)
        completed = True
        summaries.update({'source_job_ids': source_ids,
            'source_download_hashes': [leon.get('sha256'), movement.get('sha256')],
            'generation_id': generation_id, 'generation_status': generation.get('status'),
            'generation_artifacts': artifact_rows})
    finally:
        if process and process.poll() is None:
            process.terminate(); process.communicate(timeout=8)
        close_tls()
        after = fixed_inputs()
        report = {'phase': 'actual_owned_tls_iostore_generation', 'checks': checks,
            'all_passed': completed and bool(checks) and all(checks.values()),
            'job_summaries': summaries, 'app_identity': app_identity,
            'fixed_inputs_before': before, 'fixed_inputs_after': after,
            'batch_oracle_sha256': ORACLE_SHA256,
            'archive_batch_oracle_sha256': ARCHIVE_ORACLE_SHA256,
            'batch_oracle_expected': oracle_artifacts(oracle, archive_oracle),
            'camera_leon_oracle_sha256': CAMERA_LEON_ORACLE_SHA256,
            'camera_leon_pairwise_conflict_free': camera_oracle.get('batch_conflict_free') is True,
            'tls_routes': [{'route': row['route'], 'status': row.get('status'),
                'contract_ok': row.get('contract_ok'), 'response_sent': row.get('response_sent'),
                'no_account_headers': row.get('no_account_headers')} for row in state['records']],
            'live_account': False, 'console_executed': False,
            'game_files_changed': False, 'runtime_or_install_claimed': False}
        (output / 'receipt.json').write_bytes(jobs.encoded(report))
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--phase', choices=('red', 'green'), required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--app-profile', type=Path)
    parser.add_argument('--oauth-config', type=Path, default=CONFIG)
    args = parser.parse_args()
    if args.phase == 'red':
        checks = run_red(args.output.resolve())
        passed = bool(checks) and all(checks.values())
    else:
        if args.app_profile is None:
            parser.error('green requires --app-profile')
        report = run_green(args.app_profile, args.oauth_config, args.output.resolve())
        checks = report['checks']; passed = report['all_passed']
    print(json.dumps({'phase': args.phase, 'checks': checks, 'passed': passed,
                      'receipt': str(args.output.resolve() / 'receipt.json')}, sort_keys=True))
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
