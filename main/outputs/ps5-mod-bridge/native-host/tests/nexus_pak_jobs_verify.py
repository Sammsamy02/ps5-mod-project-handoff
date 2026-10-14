#!/usr/bin/env python3
"""Actual owned HTTPS job to private PAK proof, without console/game writes."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time
from urllib.parse import urlsplit
import zipfile
import nexus_jobs_verify as jobs
import nexus_auth_verify as auth

ROOT = auth.ROOT
ZIP = ROOT / '.superpowers/native-observations/performance-pak-real-mod-intake-1/original.zip'
BASE = ROOT / '.superpowers/native-observations/townfall-main-pak-full-2026-10-01/pakchunk0-ps5.pak'

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1048576), b''): h.update(block)
    return h.hexdigest()

def verify(profile, config_path, output, repak=None, expect_configured=True):
    output.mkdir(parents=True, exist_ok=False, mode=0o700)
    config_path = config_path.resolve()
    config = json.loads(config_path.read_bytes())
    scratch = config_path.parent / 'scratch'
    auth.MATRIX_DIR = output / 'relink'
    binary, identity = auth.relink_with_socket_shim(profile)
    data = ZIP.read_bytes()
    before = {'zip': digest(ZIP), 'base': digest(BASE)}
    state = {'config': config, 'premium': True, 'records': [], 'archives': {8: data, 9: data}}
    port, close_tls, archives = jobs.fixture_server(config_path.parent / 'certificates', state)
    app_port = urlsplit(config['redirect_uri']).port
    checks = {}; reports = {}; proc = None; completed = False
    def check(name, condition): checks[name] = bool(condition)
    def call(method, target, body=None, headers=None):
        return auth.local_request(app_port, method, target, headers,
            b'' if body is None else jobs.encoded(body), timeout=40)
    try:
        proc = subprocess.Popen([str(binary), '--port', str(app_port), '--lifetime', '120'],
            env={'PATH': '/usr/bin:/bin:/usr/sbin:/sbin', 'MB_NEXUS_AUTH_FIXTURE_PORT': str(port)},
            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        for _ in range(100):
            try: initial = call('GET', '/api/capabilities'); break
            except OSError:
                if proc.poll() is not None: raise RuntimeError('app exited before readiness')
                time.sleep(.02)
        else: raise RuntimeError('app readiness timeout')
        cookie, csrf, valid = auth.get_cookie_and_csrf(initial[1], json.loads(initial[2]))
        headers = {'Cookie': 'mb_session=' + cookie, 'X-CSRF-Token': csrf,
            'Origin': config['redirect_uri'].rsplit('/auth/callback', 1)[0], 'Content-Type': 'application/json'}
        start = call('POST', '/api/auth/start', {}, headers)
        authorization = auth.parse_authorization_url(start[2], config)
        if not authorization: raise RuntimeError('PKCE start failed')
        state['expected_challenge'] = authorization['challenge']
        check('actual_owned_login', valid and auth.callback_request(app_port, cookie, authorization['state'])[0] == 303)
        for kind, number, key in [('download_prepare_pak', 8, 'owned_whole_pak_1234'),
                                  ('download_prepare_pak_component', 9, 'owned_component_pak_1234')]:
            intent = {'kind': kind, 'game': 'silenthilltownfall', 'mod_id': 7,
                      'file_id': number, 'idempotency_key': key}
            queued = call('POST', '/api/jobs', intent, headers)
            check(kind + '_accepted', queued[0] == 202)
            if queued[0] != 202: continue
            identifier = json.loads(queued[2])['id']
            done = call('GET', '/api/jobs/' + identifier, headers=headers)
            report = json.loads(done[2]); reports[kind] = report
            check(kind + '_original_archive', report.get('sha256') == before['zip'] and report.get('bytes_downloaded') == len(data))
            check(kind + '_analysis_retained', isinstance(report.get('analysis_report'), dict))
            prep = report.get('preparation', {})
            if not expect_configured:
                check(kind + '_unconfigured_unresolved', prep.get('status') == 'unresolved' and not prep.get('artifact_complete'))
            elif number == 8:
                check('whole_bundle_extra_refusal', prep.get('status') == 'additional_action_required' and prep.get('unhandled_members') == 2 and not prep.get('artifact_complete'))
                check('whole_bundle_no_output', not list(scratch.glob('mb-pak-' + identifier + '.pak')))
            else:
                check('component_prepared', prep.get('status') == 'prepared' and prep.get('artifact_complete') is True)
                check('component_extras_explicit', prep.get('whole_bundle_handled') is False and prep.get('unhandled_members') == 2)
                check('component_not_runtime_install', not report.get('runtime_supported') and not report.get('installation_supported') and not prep.get('runtime_supported') and not prep.get('installation_supported'))
                artifacts = list(scratch.glob('mb-pak-' + identifier + '.pak'))
                if len(artifacts) == 1:
                    artifact = artifacts[0]
                    check('physical_readback_receipt', prep.get('artifact_sha256') == digest(artifact) and prep.get('artifact_bytes') == artifact.stat().st_size)
                    q = scratch / ('mb-pak-' + identifier)
                    with zipfile.ZipFile(ZIP) as incoming:
                        rows = [(i, item) for i, item in enumerate(incoming.infolist()) if not item.is_dir()]
                        check('all_regular_members_preserved', all((q / ('member-%08u.bin' % i)).read_bytes() == incoming.read(item) for i, item in rows))
                        pak = next(q / ('member-%08u.bin' % i) for i, item in rows if item.filename.lower().endswith('.pak'))
                    if repak:
                        oracle = ROOT / 'outputs/ps5-mod-bridge/native-host/format/examples/pak_mod_union/oracle.py'
                        result = subprocess.run(['python3', str(oracle), str(repak), str(BASE), str(pak), str(artifact), str(output / 'oracle.json')], capture_output=True, text=True, timeout=60)
                        check('independent_decoded_union', result.returncode == 0)
                        reports['oracle_command'] = {'exit': result.returncode, 'stdout': result.stdout, 'stderr': result.stderr}
                else: check('exactly_one_private_artifact', False)
            count = len(state['records'])
            duplicate = call('POST', '/api/jobs', intent, headers)
            check(kind + '_idempotent', duplicate[0] == 202 and json.loads(duplicate[2]).get('id') == identifier and len(state['records']) == count)
            conflict = call('POST', '/api/jobs', {**intent, 'kind': 'download_analyze'}, headers)
            check(kind + '_kind_conflict', conflict[0] == 409)
        public = call('GET', '/api/jobs', headers=headers)[2]
        mismatch = call('POST', '/api/jobs', {'kind': 'download_prepare_pak_component', 'game': 'skyrim',
            'mod_id': 7, 'file_id': 8, 'idempotency_key': 'owned_other_game_pak_1234'}, headers)
        check('other_game_request_accepted', mismatch[0] == 202)
        if mismatch[0] == 202:
            identifier = json.loads(mismatch[2])['id']
            mismatch_done = json.loads(call('GET', '/api/jobs/' + identifier, headers=headers)[2])
            prep = mismatch_done.get('preparation', {})
            check('other_game_never_infers_target', prep.get('status') == 'unresolved' and not prep.get('artifact_complete') and not list(scratch.glob('mb-pak-' + identifier + '*')))
        check('no_public_secrets_paths', all(word not in public for word in (b'owned-jobs-access-token', b'fixture_signature', str(scratch).encode(), str(BASE).encode())))
        shutdown = auth.local_request(app_port, 'POST', '/shutdown', {'Content-Length': '0', 'X-Modbridge-Shutdown': '1'})
        stdout, stderr = proc.communicate(timeout=5)
        check('owner_cleanup_complete', shutdown[0] == 200 and proc.returncode == 0 and not list(scratch.iterdir()) and not stderr)
        check('inputs_unchanged', digest(ZIP) == before['zip'] and digest(BASE) == before['base'])
        check('credential_free_archive_requests', all(r.get('no_account_headers') for r in state['records'] if r['route'] == 'archive'))
        completed = True
    finally:
        if proc and proc.poll() is None: proc.terminate(); proc.communicate(timeout=4)
        close_tls()
        receipt = {'checks': checks, 'all_passed': completed and bool(checks) and all(checks.values()),
            'reports': reports, 'app_identity': identity, 'tls_records': state['records'],
            'inputs': before, 'console_executed': False, 'live_account': False,
            'game_files_changed': False, 'installation_supported': False}
        (output / 'receipt.json').write_bytes(jobs.encoded(receipt))
    return receipt

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--app-profile', type=Path, required=True)
    p.add_argument('--oauth-config', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--repak', type=Path)
    p.add_argument('--unconfigured', action='store_true')
    a = p.parse_args()
    result = verify(a.app_profile, a.oauth_config, a.output, a.repak, not a.unconfigured)
    print(json.dumps({'checks': len(result['checks']), 'all_passed': result['all_passed']}))
    raise SystemExit(0 if result['all_passed'] else 1)
