"""Root-only fresh HTTPS console gate. Never controls FTP; never redispatches."""
import datetime, errno, ftplib, hashlib, json, os, signal, sys, time, urllib.error, urllib.parse, urllib.request
from contextlib import contextmanager
from pathlib import Path
REPO = Path(__file__).resolve().parents[2]
PUBLIC = REPO / 'outputs/native-https-artifacts'
WORK = REPO / '.superpowers/native-observations'
STATE = WORK / 'https-console-root-state.json'
APPROVAL = WORK / 'https-final-review-approval.json'
IP = 'ps5-host-not-configured.invalid'
MANAGER = f'http://{IP}:8084'
HOST = f'http://{IP}:8085'

class RejectRedirects(urllib.request.HTTPRedirectHandler):

    def redirect_request(self, req, fp, code, msg, headers, newurl):
        raise urllib.error.HTTPError(req.full_url, code, 'redirect refused', headers, fp)
OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}), RejectRedirects())
MANIFEST = json.loads((PUBLIC / 'development_lan-manifest.json').read_text())
ARTIFACT = PUBLIC / MANIFEST['artifact']
SHA = MANIFEST['artifact_sha256']
BUILD = MANIFEST['build_id']
FILENAME = 'ModbridgeNativeHttpsDev_' + SHA[:12] + '.elf'
CONFIG = MANIFEST['tls_descriptor']['config_id']
FIXTURE = MANIFEST['tls_fixture']
GATE_SOURCE = Path(__file__).resolve()
MANIFEST_SOURCE = PUBLIC / 'development_lan-manifest.json'
FIXTURE_SERVER_SOURCE = REPO / '.superpowers/native-dependency-spike/native-https-console-fixture-1/fixture.py'
IDENTITY = {'sha256': SHA, 'build_id': BUILD, 'size': MANIFEST['artifact_size'],
    'filename': FILENAME, 'authority': HOST, 'config_id': CONFIG, 'tls_fixture': FIXTURE,
    'gate_sha256': hashlib.sha256(GATE_SOURCE.read_bytes()).hexdigest(),
    'manifest_sha256': hashlib.sha256(MANIFEST_SOURCE.read_bytes()).hexdigest(),
    'fixture_server_source_sha256': hashlib.sha256(FIXTURE_SERVER_SOURCE.read_bytes()).hexdigest()}
PHASE_DEADLINE = None

def require(condition, message='console gate check failed'):
    if not condition:
        raise RuntimeError(message)

@contextmanager
def transfer_budget(seconds):
    started = time.monotonic()
    remaining = seconds if PHASE_DEADLINE is None else min(seconds, PHASE_DEADLINE - started)
    require(remaining > 0, 'phase deadline exhausted')
    require(signal.getitimer(signal.ITIMER_REAL)[0] == 0, 'nested timer is not allowed')
    previous = signal.getsignal(signal.SIGALRM)

    def expired(signum, frame):
        raise TimeoutError('total transfer deadline expired')
    signal.signal(signal.SIGALRM, expired)
    signal.setitimer(signal.ITIMER_REAL, remaining)
    try:
        yield
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, previous)

def now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()

def save(s):
    temporary = STATE.with_suffix('.tmp')
    with temporary.open('w') as f:
        f.write(json.dumps(s, indent=2) + '\n')
        f.flush()
        os.fsync(f.fileno())
    os.replace(temporary, STATE)
    fd = os.open(str(STATE.parent), os.O_RDONLY)
    try:
        os.fsync(fd)
    finally:
        os.close(fd)

def read_state():
    if STATE.exists():
        s = json.loads(STATE.read_text())
        require(s.get('phase_identity') == IDENTITY, 'state belongs to another artifact or target')
        return s
    return {'phase_identity': IDENTITY, 'scope': 'controlled_owned_ephemeral_HTTPS_checkpoint', 'random_or_tls_bytes_published': False, 'installation_supported': False, 'loopback_executed': False}

def http(url, method='GET', data=None, headers=None, timeout=3, limit=262144):
    req = urllib.request.Request(url, data=data, headers=headers or {}, method=method)
    started = time.monotonic()
    with transfer_budget(timeout):
        with OPENER.open(req, timeout=timeout) as r:
            body = r.read(limit + 1)
            if len(body) > limit:
                raise ValueError('response exceeds bound')
            return {'utc': now(), 'status': r.status, 'body': body.decode('utf-8'), 'elapsed_seconds': round(time.monotonic() - started, 6), 'total_deadline_seconds': timeout}

def jsget(url, timeout=3, limit=262144):
    r = http(url, timeout=timeout, limit=limit)
    require(r['status'] == 200, 'console gate check failed at original line 62')
    return json.loads(r['body'])

def ftp_banner():
    f = ftplib.FTP(timeout=3)
    try:
        with transfer_budget(5):
            f.connect(IP, 2121)
            banner = f.getwelcome()
            f.login()
            pwd = f.pwd()
            f.quit()
        return {'utc': now(), 'connected': True, 'banner': banner, 'pwd': pwd, 'service_control': False}
    finally:
        f.close()

def approved():
    a = json.loads(APPROVAL.read_text())
    require(a['approved'] is True and a['phase_identity'] == IDENTITY, 'console gate check failed at original line 73')
    for path, field in ((GATE_SOURCE, 'gate_sha256'), (MANIFEST_SOURCE, 'manifest_sha256'),
                        (FIXTURE_SERVER_SOURCE, 'fixture_server_source_sha256')):
        require(hashlib.sha256(path.read_bytes()).hexdigest() == IDENTITY[field],
                'reviewed operation input changed: ' + field)
    require(FIXTURE['server_address'] == 'ps5-host-not-configured.invalid' and
            FIXTURE['server_port'] == 8443 and
            FIXTURE['expected_hostname'] == 'native-https.modbridge.test',
            'unexpected controlled fixture')
    fixture_root = REPO / '.superpowers/native-dependency-spike/native-https-console-fixture-1/root.der'
    require(hashlib.sha256(fixture_root.read_bytes()).hexdigest() ==
            FIXTURE['root_der_sha256'], 'controlled trust root changed')
    require(hashlib.sha256(ARTIFACT.read_bytes()).hexdigest() == SHA, 'console gate check failed at original line 74')
    require(ARTIFACT.stat().st_size == MANIFEST['artifact_size'], 'console gate check failed at original line 75')

def preflight():
    v = http(MANAGER + '/version')
    require(v['status'] == 200 and v['body'].strip() == '0.5.2', 'console gate check failed at original line 77')
    processes = jsget(MANAGER + '/processes_list')
    catalog = jsget(MANAGER + '/list_payloads')
    try:
        http(HOST + '/report', timeout=1, limit=8192)
    except urllib.error.HTTPError:
        raise RuntimeError('an HTTP service answered on owned host port; no deployment')
    except urllib.error.URLError as e:
        if not isinstance(e.reason, OSError) or e.reason.errno != errno.ECONNREFUSED:
            raise
        closed = {'error': str(e)}
    else:
        raise RuntimeError('owned host endpoint already reachable; do not upload or dispatch')
    return {'utc': now(), 'version': v, 'processes': processes, 'catalog': catalog, 'owned_report_closed': closed, 'ftp': ftp_banner()}

def upload():
    approved()
    s = read_state()
    require('upload_attempt' not in s, 'console gate check failed at original line 88')
    s['preflight'] = preflight()
    require(not any((Path(p).name == FILENAME for p in s['preflight']['catalog']['payloads'])), 'console gate check failed at original line 90')
    s['upload_attempt'] = {'utc': now(), 'filename': FILENAME, 'sha256': SHA, 'bytes': ARTIFACT.stat().st_size, 'build_id': BUILD, 'autoload_modified': False, 'game_files_modified': False}
    save(s)
    s['upload_response'] = http(MANAGER + '/manage:upload?filename=' + urllib.parse.quote(FILENAME), method='POST', data=ARTIFACT.read_bytes(), headers={'Content-Type': 'application/octet-stream'}, timeout=8)
    save(s)
    require(s['upload_response']['status'] == 200 and s['upload_response']['body'].strip() == 'OK', 'console gate check failed at original line 93')
    catalog = jsget(MANAGER + '/list_payloads')
    paths = [p for p in catalog['payloads'] if Path(p).name == FILENAME]
    require(len(paths) == 1, 'console gate check failed at original line 94')
    s['stored_path'] = paths[0]
    require(s['stored_path'].startswith('/data/pldmgr/payloads/') and '..' not in Path(s['stored_path']).parts and all((ord(c) >= 32 for c in s['stored_path'])), 'invalid owned stored path')
    save(s)
    digest = hashlib.sha256()
    count = 0

    def consume(block):
        nonlocal count
        count += len(block)
        if count > ARTIFACT.stat().st_size:
            raise ValueError('owned readback exceeds reviewed size')
        digest.update(block)
    f = ftplib.FTP(timeout=4)
    try:
        started = time.monotonic()
        with transfer_budget(20):
            f.connect(IP, 2121)
            f.login()
            f.retrbinary('RETR ' + s['stored_path'], consume, blocksize=65536)
            f.quit()
    finally:
        f.close()
    s['stored_readback'] = {'utc': now(), 'bytes': count, 'sha256': digest.hexdigest(), 'elapsed_seconds': round(time.monotonic() - started, 6), 'total_deadline_seconds': 20, 'scope': 'normal_FTP_RETR_of_owned_ELF_only', 'matches_reviewed': count == ARTIFACT.stat().st_size and digest.hexdigest() == SHA}
    save(s)
    require(s['stored_readback']['matches_reviewed'], 'console gate check failed at original line 111')
    print(json.dumps({'status': 'uploaded_and_readback_verified', 'stored_path': s['stored_path'], 'bytes': count, 'sha256': digest.hexdigest()}))

def validate_report(r):
    require(r['version'] == MANIFEST['version'] and r['origin'] == 'ps5_sdk_entry' and (r['profile'] == 'development_lan') and (r['build_id'] == BUILD), 'console gate check failed at original line 114')
    require(r['authority'] == f'{IP}:8085' and r['installation_supported'] is False, 'console gate check failed at original line 115')
    require(r['firmware']['reason'] == 'not_queried' and r['game']['reason'] == 'not_queried', 'console gate check failed at original line 116')
    pid = r['observations']['getpid']['value']
    require(type(pid) is int and pid > 0, 'owned PID must be a positive integer, not a Boolean')
    return pid

def validate_https(https):
    require(https['config_id'] == CONFIG and https['scope'] == 'owned_development_https_once',
            'HTTPS configuration/scope mismatch')
    require(https['source_origin'] == 'reviewed_kern_arnd', 'host entropy fixture cannot pass target gate')
    require(https['status'] == 'passed', 'native HTTPS operation did not pass')
    for field in ('certificate_verified', 'framed_response_complete',
                  'peer_close_confirmed', 'local_close_notify_confirmed',
                  'local_cleanup_complete', 'source_closed', 'verify_available'):
        require(https[field] is True, 'HTTPS outcome failed: ' + field)
    require(https['source_failed'] is False and https['hard_deadline_enforced'] is False,
            'source/deadline metadata mismatch')
    require(https['source_callback_count'] == https['source_os_call_count'] ==
            https['total_api_call_count'] == 2 and https['api_call_limit'] == 4,
            'unexpected source/API count')
    require(https['handshake_rc'] == https['seed_rc'] == https['verify_flags'] == 0,
            'HTTPS seed/handshake/certificate failure')
    require(https['http_request_bytes'] == 114 and https['response_bytes'] == 108,
            'fixed harmless HTTP contract mismatch')
    actual = https['last_api']
    require(actual['present'] is True and actual['status'] == 'passed' and
            actual['return_code'] == actual['errno'] == 0 and actual['returned_length_bytes'] == 32,
            'last actual entropy API result mismatch')
    clock = https['clock']
    require(clock['authenticated_time'] is False and clock['failure'] == '' and
            clock['library_time_reads'] > 0 and clock['certificate_utc_calls'] > 0 and
            type(clock['initial_epoch']) is int and type(clock['last_epoch']) is int and
            isinstance(clock['last_utc'], list) and len(clock['last_utc']) == 6,
            'target clock API/certificate observations missing')

def dispatch():
    approved()
    s = read_state()
    require(s['stored_readback']['matches_reviewed'] and 'dispatch_attempt' not in s, 'console gate check failed at original line 121')
    require(s['upload_attempt']['sha256'] == SHA and s['upload_attempt']['build_id'] == BUILD and (s['upload_attempt']['filename'] == FILENAME) and (s['upload_attempt']['bytes'] == MANIFEST['artifact_size']), 'upload identity mismatch')
    require(s['stored_readback']['sha256'] == SHA and s['stored_readback']['bytes'] == MANIFEST['artifact_size'], 'readback identity mismatch')
    require(Path(s['stored_path']).name == FILENAME and s['stored_path'].startswith('/data/pldmgr/payloads/'), 'stored path identity mismatch')
    s['predispatch_preflight'] = preflight()
    require(s['stored_path'] in s['predispatch_preflight']['catalog']['payloads'], 'console gate check failed at original line 126')
    route = MANAGER + '/loadpayload:' + urllib.parse.quote(s['stored_path'], safe='')
    s['dispatch_attempt'] = {'utc': now(), 'route': route, 'count': 1, 'sha256': SHA, 'build_id': BUILD, 'scope': 'manager_transport_acknowledgment_only'}
    save(s)
    try:
        s['dispatch_response'] = http(route, timeout=4)
    except Exception as e:
        s['dispatch_response'] = {'error': type(e).__name__ + ': ' + str(e)}
    save(s)
    deadline = time.monotonic() + 30
    errors = []
    r = None
    while time.monotonic() < deadline:
        try:
            r = jsget(HOST + '/report', timeout=min(1, max(0.1, deadline - time.monotonic())), limit=8192)
            break
        except Exception as e:
            errors.append(type(e).__name__ + ': ' + str(e))
            time.sleep(0.1)
    s['readiness_errors'] = errors
    if r is None:
        s['native_run'] = {'status': 'unavailable', 'reason': 'undifferentiated_startup_readiness_failure', 'redispatch_performed': False}
        save(s)
        print(json.dumps(s['native_run']))
        return
    save(s)
    pid = validate_report(r)
    s['owned_identity'] = {'pid': pid, 'build_id': BUILD, 'validated_utc': now()}
    save(s)
    (PUBLIC / 'native-report.json').write_text(json.dumps(r, indent=2) + '\n')
    v = jsget(HOST + '/version', limit=8192)
    c = jsget(HOST + '/core', limit=8192)
    r2 = jsget(HOST + '/report', limit=8192)
    (PUBLIC / 'native-version.json').write_text(json.dumps(v, indent=2) + '\n')
    (PUBLIC / 'native-core.json').write_text(json.dumps(c, indent=2) + '\n')
    require(v['build_id'] == BUILD and v['origin'] == 'ps5_sdk_entry' and (v['profile'] == 'development_lan'), 'console gate check failed at original line 145')
    require(r2 == r and c == r['core'] and (c['status'] == 'passed') and (len(c['checks']) == 7) and all(c['checks'].values()), 'console gate check failed at original line 146')
    https = r['observations']['https']
    require(https['config_id'] == CONFIG, 'HTTPS configuration identity mismatch')
    processes = jsget(MANAGER + '/processes_list')
    require(any(p['pid'] == pid for p in processes['processes']), 'owned PID not observed')
    # Persist identity and complete report before positive policy checks. A TLS
    # failure must not prevent the separate owned-shutdown phase.
    s['native_run'] = {'utc': now(), 'status': https['status'], 'pid': pid,
        'origin': r['origin'], 'build_id': BUILD, 'https': https,
        'core_checks_passed': 7, 'cached_reports_equal': True,
        'manager_pid_present': True, 'installation_supported': False}
    save(s)
    validate_https(https)
    save(s)
    print(json.dumps(s['native_run']))

def shutdown():
    approved()
    s = read_state()
    owned = s['owned_identity']
    r = jsget(HOST + '/report', limit=8192)
    require(validate_report(r) == owned['pid'] and r['build_id'] == owned['build_id'], 'console gate check failed at original line 158')
    require('shutdown_attempt' not in s, 'console gate check failed at original line 158')
    s['shutdown_attempt'] = {'utc': now(), 'pid': owned['pid'], 'build_id': BUILD}
    save(s)
    response = http(HOST + '/shutdown', method='POST', data=b'', headers={'Content-Length': '0', 'X-Modbridge-Shutdown': '1', 'Origin': HOST}, limit=8192)
    deadline = time.monotonic() + 6
    closed = False
    absent = False
    cycles = []
    confirmed = False
    while time.monotonic() < deadline:
        closed = False
        absent = False
        cycle = {'utc': now()}
        try:
            response_probe = http(HOST + '/version', timeout=min(0.5, max(0.001, deadline - time.monotonic())), limit=8192)
            cycle['endpoint_response'] = response_probe
        except urllib.error.HTTPError as e:
            cycle['endpoint_http_error'] = e.code
        except urllib.error.URLError as e:
            closed = isinstance(e.reason, OSError) and e.reason.errno == errno.ECONNREFUSED
            cycle['endpoint_error'] = str(e)
        if time.monotonic() >= deadline:
            cycle.update({'complete': False, 'endpoint_closed': closed, 'owned_pid_absent': None, 'reason': 'deadline_before_pid_query'})
            cycles.append(cycle)
            break
        processes = jsget(MANAGER + '/processes_list', timeout=min(1, max(0.001, deadline - time.monotonic())))
        absent = not any((p['pid'] == owned['pid'] for p in processes['processes']))
        cycle.update({'complete': True, 'endpoint_closed': closed, 'owned_pid_absent': absent})
        cycles.append(cycle)
        if closed and absent:
            confirmed = True
            break
        time.sleep(0.1)
    s['owned_shutdown'] = {'utc': now(), 'native_pid': owned['pid'], 'build_id': BUILD, 'response': response, 'endpoint_closed': cycles[-1].get('endpoint_closed') if cycles else None, 'manager_pid_absent_after_shutdown': cycles[-1].get('owned_pid_absent') if cycles else None, 'observation_cycles': cycles, 'waitpid_exit_status_observed': False, 'manager_version': http(MANAGER + '/version'), 'ftp_after': ftp_banner()}
    save(s)
    (PUBLIC / 'console-receipt.json').write_text(json.dumps(s, indent=2) + '\n')
    require(response['status'] == 200 and confirmed, 'console gate check failed at original line 181')
    require(s['predispatch_preflight']['ftp']['banner'] == s['owned_shutdown']['ftp_after']['banner'], 'console gate check failed at original line 182')
    print(json.dumps({'status': 'owned_shutdown_observed', 'pid': owned['pid'], 'endpoint_closed': closed, 'pid_absent': absent, 'ftp_running': True}))
if __name__ == '__main__':
    phase = sys.argv[1]
    require(phase in ('upload', 'dispatch', 'shutdown'), 'unknown phase')
    claim = STATE.with_suffix('.lock')
    fd = os.open(str(claim), os.O_CREAT | os.O_EXCL | os.O_WRONLY, 384)
    claim_inode = os.fstat(fd).st_ino
    try:
        os.write(fd, json.dumps({'pid': os.getpid(), 'phase': phase, 'utc': now()}).encode())
        os.fsync(fd)
        PHASE_DEADLINE = time.monotonic() + {'upload': 50, 'dispatch': 55, 'shutdown': 24}[phase]
        {'upload': upload, 'dispatch': dispatch, 'shutdown': shutdown}[phase]()
    except Exception as e:
        (WORK / ('https-console-' + phase + '-failure.json')).write_text(json.dumps({'utc': now(), 'phase': phase, 'phase_identity': IDENTITY, 'status': 'failed', 'error': type(e).__name__, 'message': str(e)[:512], 'redispatch_performed': False}, indent=2) + '\n')
        raise
    finally:
        os.close(fd)
        if claim.exists() and claim.stat().st_ino == claim_inode:
            claim.unlink()
