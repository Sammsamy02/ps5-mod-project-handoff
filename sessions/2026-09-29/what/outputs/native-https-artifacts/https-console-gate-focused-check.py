"""Focused approval/identity controls. No console or network operations."""
import ast
import copy
import hashlib
import json
import tempfile
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
source = REPO / '.superpowers/native-observations/https-console-gate.py'
tree = ast.parse(source.read_text())
nodes = [n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name in
         ('require', 'approved', 'validate_report', 'validate_https')]
if len(nodes) != 4:
    raise RuntimeError('expected real gate helper functions missing')
root = REPO / '.superpowers/native-dependency-spike/native-https-console-fixture-1/root.der'
fixture = {'server_address': 'ps5-host-not-configured.invalid', 'server_port': 8443,
           'expected_hostname': 'native-https.modbridge.test',
           'root_der_sha256': hashlib.sha256(root.read_bytes()).hexdigest()}
namespace = {'hashlib': hashlib, 'json': json, 'REPO': REPO, 'FIXTURE': fixture,
             'IP': 'ps5-host-not-configured.invalid', 'BUILD': 'b' * 64}
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), namespace)
cases = []
def rejected(name, operation):
    try:
        operation()
    except RuntimeError:
        cases.append({'case': name, 'rejected': True})
    else:
        raise RuntimeError('control accepted: ' + name)

with tempfile.TemporaryDirectory(prefix='https-gate-controls-', dir=REPO / '.superpowers/native-observations') as directory:
    p = Path(directory)
    artifact = p / 'owned-fixture.bin'
    artifact.write_bytes(b'owned development test only')
    sha = hashlib.sha256(artifact.read_bytes()).hexdigest()
    identity = {'sha256': sha, 'build_id': namespace['BUILD'], 'size': artifact.stat().st_size,
                'filename': 'owned-fixture.bin', 'authority': 'http://ps5-host-not-configured.invalid:8085',
                'config_id': 'c' * 64, 'tls_fixture': copy.deepcopy(fixture)}
    operation_files = {'GATE_SOURCE': p/'gate.py', 'MANIFEST_SOURCE': p/'manifest.json',
                       'FIXTURE_SERVER_SOURCE': p/'fixture.py'}
    for variable, file in operation_files.items():
        file.write_text('owned development input ' + variable)
        namespace[variable] = file
    for variable, field in [('GATE_SOURCE', 'gate_sha256'), ('MANIFEST_SOURCE', 'manifest_sha256'),
                            ('FIXTURE_SERVER_SOURCE', 'fixture_server_source_sha256')]:
        identity[field] = hashlib.sha256(operation_files[variable].read_bytes()).hexdigest()
    approval = p / 'approval.json'
    valid = {'approved': True, 'phase_identity': copy.deepcopy(identity)}
    approval.write_text(json.dumps(valid))
    namespace.update(ARTIFACT=artifact, SHA=sha, IDENTITY=identity, APPROVAL=approval,
                     MANIFEST={'artifact_size': artifact.stat().st_size, 'version': 'owned-test'})
    namespace['approved']()
    cases.append({'case': 'exact_approval_and_artifact', 'passed': True})
    bad = copy.deepcopy(valid); bad['phase_identity']['build_id'] = 'd' * 64
    approval.write_text(json.dumps(bad))
    rejected('wrong_build_approval', namespace['approved'])
    approval.write_text(json.dumps(valid))
    artifact.write_bytes(b'changed development fixture')
    rejected('changed_artifact', namespace['approved'])
    artifact.write_bytes(b'owned development test only')
    namespace['FIXTURE'] = {**fixture, 'server_address': 'ps5-host-not-configured.invalid'}
    rejected('changed_server_address', namespace['approved'])
    namespace['FIXTURE'] = {**fixture, 'root_der_sha256': '0' * 64}
    rejected('changed_trust_identity', namespace['approved'])
    namespace['FIXTURE'] = fixture
    for variable in operation_files:
        file = operation_files[variable]; prior = file.read_bytes()
        file.write_bytes(b'changed owned development input')
        rejected('changed_' + variable.lower(), namespace['approved'])
        file.write_bytes(prior)

    report = {'version': 'owned-test', 'origin': 'ps5_sdk_entry', 'profile': 'development_lan',
              'build_id': namespace['BUILD'], 'authority': 'ps5-host-not-configured.invalid:8085',
              'installation_supported': False, 'firmware': {'reason': 'not_queried'},
              'game': {'reason': 'not_queried'}, 'observations': {'getpid': {'value': 191}}}
    if namespace['validate_report'](report) != 191:
        raise RuntimeError('positive metadata fixture failed')
    cases.append({'case': 'exact_owned_identity', 'passed': True})
    for name, key, value in [('wrong_origin', 'origin', 'desktop_fixture'),
                             ('wrong_authority', 'authority', 'ps5-host-not-configured.invalid:8085'),
                             ('install_claim', 'installation_supported', True)]:
        bad = {**report, key: value}
        rejected(name, lambda bad=bad: namespace['validate_report'](bad))
    for name, value in [('zero_pid', 0), ('negative_pid', -1), ('boolean_pid', True)]:
        bad = copy.deepcopy(report); bad['observations']['getpid']['value'] = value
        rejected(name, lambda bad=bad: namespace['validate_report'](bad))

rows = json.loads((REPO / '.superpowers/native-dependency-spike/native-https-host-experiment-2/host-tests.json').read_text())
for row in rows:
    h = row['report']['observations']['https']
    namespace['CONFIG'] = h['config_id']
    rejected('actual_host_fixture_' + row['case'], lambda h=h: namespace['validate_https'](h))
    if row['case'] != 'valid':
        h = {**h, 'source_origin': 'reviewed_kern_arnd'}
        rejected('actual_failed_operation_' + row['case'], lambda h=h: namespace['validate_https'](h))
positive = copy.deepcopy(rows[0]['report']['observations']['https'])
positive['source_origin'] = 'reviewed_kern_arnd'
namespace['validate_https'](positive)
cases.append({'case': 'native_shaped_metadata_fixture_only', 'passed': True})
for name, key, value in [('open_seed_source', 'source_closed', False),
                         ('extra_source_call', 'total_api_call_count', 3),
                         ('wrong_crypto_config', 'config_id', '0' * 64)]:
    h = {**positive, key: value}
    rejected(name, lambda h=h: namespace['validate_https'](h))

out = {'scope': 'actual helper code with owned development metadata only; no network/device calls',
       'cases': cases, 'passed': True}
(REPO / '.superpowers/native-observations/https-console-gate-focused-results.json').write_text(json.dumps(out, indent=2)+'\n')
print(json.dumps({'focused_cases': len(cases), 'passed': True, 'console_contacted': False}))
