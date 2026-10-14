import hashlib
import json
import os
import struct
import subprocess
from pathlib import Path

workspace = Path.cwd()
p = workspace / 'outputs/2026-10-07-l19-local-profile-r2'
v1 = workspace / 'outputs/2026-10-07-l19-local-profile'
out = Path(__file__).resolve().parent
sha = lambda f: hashlib.sha256(Path(f).read_bytes()).hexdigest()
selection = p / 'reports/SOURCE-SELECTION.json'
s = json.loads(selection.read_text())
assert sha(selection) == '70e26759ff736d5dd167e5cae2d59700ff0dc1eee36ebe39fa0a784ddfc5cdd2'
for f, digest in s['source_files'].items():
    assert sha(p / f) == digest, f
for f, digest in s['evidence_files'].items():
    assert sha(workspace / f) == digest, f
old = json.loads((v1 / 'reports/SOURCE-SELECTION.json').read_text())
assert sha(v1 / 'reports/SOURCE-SELECTION.json') == s['baseline_selection_sha256']
for f, digest in old['source_files'].items():
    assert sha(v1 / f) == digest, f
for f, digest in s['unchanged_v1_unit_hashes'].items():
    assert s['source_files'][f] == old['source_files'][f] == digest
changed = [f for f, digest in s['source_files'].items() if old['source_files'].get(f) != digest]
assert [f for f in changed if f.startswith('units/')] == [
    'units/NativeLayoutActiveWidths.hpp', 'units/NativeLayoutBinding.cpp', 'units/RuntimeBuildProfile.hpp']
final = json.loads((p / 'reports/FINAL-RECEIPT.json').read_text())
assert final['pass'] and final['source_selection_sha256'] == sha(selection)
for f, digest in final['receipt_hashes'].items():
    assert sha(p / 'reports' / f) == digest, f
f = json.loads((p / 'profiles/townfall-build-facts.json').read_text())
oldf = json.loads((v1 / 'profiles/townfall-build-facts.json').read_text())
for key in ['schema', 'name', 'image', 'layout', 'axes', 'fname']:
    assert f[key] == oldf[key], key
promotions = []
for kind in ['required_members', 'required_vtables']:
    assert [v['name'] for v in f[kind]] == [v['name'] for v in oldf[kind]]
    for a, b in zip(oldf[kind], f[kind]):
        if a['offset'] != b['offset'] or a['width'] != b['width']:
            assert a['offset'] is None, (a, b)
            static_name, fact_name = b['source'].split(':', 1)
            d = json.loads((workspace / static_name).read_text())
            facts = [v for v in d['facts'] if v['key'] == fact_name]
            assert len(facts) == 1
            fact = facts[0]
            assert fact['status'] == 'validated-static' and fact['confidence'] == 'high'
            assert int(fact['value'], 0) == b['offset'] and fact['width_bytes'] == b['width']
            promotions.append({'kind': kind, 'name': b['name'], 'offset': b['offset'], 'width': b['width'], 'source': b['source']})
assert len(promotions) == 25  # 22 members plus 3 required slots; CDO400 remains supplemental.
assert len(f['required_members']) == 69 and sum(v['offset'] is not None for v in f['required_members']) == 66
assert len(f['required_vtables']) == 7 and sum(v['offset'] is not None for v in f['required_vtables']) == 4
assert f['unknown_required_static_facts'] == []
unknown_members = [v['name'] for v in f['required_members'] if v['offset'] is None]
unknown_vtables = [v['name'] for v in f['required_vtables'] if v['offset'] is None]
assert unknown_members == final['remaining_unknown_members'] and unknown_vtables == final['remaining_unknown_vtables']

def symbols(path):
    b = Path(path).read_bytes()
    h = struct.unpack_from('<16sHHIQQQIHHHHHH', b)
    sec = [struct.unpack_from('<IIQQQQIIQQ', b, h[6] + i * h[11]) for i in range(h[12])]
    found = []
    for r in sec:
        if r[1] != 2:
            continue
        st = sec[r[6]]
        strings = b[st[4]:st[4] + st[5]]
        for o in range(r[4], r[4] + r[5], r[9]):
            n, info, other, idx, val, size = struct.unpack_from('<IBBHQQ', b, o)
            name = strings[n:strings.index(0, n)].decode()
            found.append((name, info >> 4, info & 15, idx, val, size))
    return b, sec, found

target = json.loads((p / 'reports/ACTIVE-WIDTHS-TARGET.json').read_text())
assert target['exit'] == 0
for path, digest in target['source_hashes'].items():
    assert sha(path) == digest
target_obj = Path(target['argv'][target['argv'].index('-o') + 1])
b, sec, syms = symbols(target_obj)
control = [r for r in syms if r[0] == 'ActiveWidthControlFacts']
assert len(control) == 1
_, binding, typ, idx, val, size = control[0]
assert size == 24 and struct.unpack_from('<6I', b, sec[idx][4] + val) == (0, 4, 8, 2, 8, 8)
deps = Path(target['argv'][target['argv'].index('-MF') + 1]).read_text()
for key in ['NativeLayoutActiveWidths.hpp', 'CoreUObject/UObject/Class.hpp', 'include/Unreal/FField.hpp',
            'MemberVariableLayout_HeaderWrapper_FFieldClass.hpp', 'src/NativeLayoutWidths.cpp']:
    assert key in deps
native = json.loads((p / 'reports/NATIVE-FOCUS.json').read_text())
provider = '_ZN2RC8Platform13NativeLayouts26GetStagedNativeLayoutFactsEv'
providers = []
for r in native['commands']:
    assert r['exit'] == 0
    obj = Path(r['argv'][r['argv'].index('-o') + 1])
    _, _, syms = symbols(obj)
    matches = [v for v in syms if v[0] == provider]
    assert len(matches) == 1 and matches[0][1] == 1
    source = next(v for v in r['argv'] if v.endswith('.cpp'))
    defined = matches[0][3] != 0
    assert defined == source.endswith('/NativeLayoutBinding.cpp')
    dependency_file = Path(r['argv'][r['argv'].index('-MF') + 1])
    if defined:
        assert 'NativeLayoutActiveWidths.hpp' in dependency_file.read_text()
    providers.append({'source': source, 'object': str(obj), 'sha256': sha(obj), 'strong_definition': defined})
for path, digest in native['source_hashes'].items():
    path = Path(path) if '/' in path else p / 'units' / path
    assert sha(path) == digest
for name in ['PROFILE-CHECKS-GREEN.json', 'INITIALIZER-PROFILE-GREEN.json', 'ACTIVE-WIDTHS-GREEN.json']:
    r = json.loads((p / 'reports' / name).read_text())
    assert all(v['exit'] == 0 for v in r['commands']), name
    for path, digest in r.get('source_hashes', r.get('actual_sources', {})).items():
        path = Path(path)
        if not path.is_absolute():
            path = p / path if len(path.parts) > 1 else p / 'units' / path
        assert sha(path) == digest, str(path)

env = dict(os.environ)
env['ASAN_OPTIONS'] = 'halt_on_error=1:abort_on_error=1'
env['UBSAN_OPTIONS'] = 'halt_on_error=1:print_stacktrace=1'
executed = []
for name in ['PROFILE-CHECKS-GREEN.json', 'INITIALIZER-PROFILE-GREEN.json', 'ACTIVE-WIDTHS-GREEN.json']:
    r = json.loads((p / 'reports' / name).read_text())
    for cmd in r['commands']:
        if cmd['argv'][0].endswith('clang++'):
            continue
        run = subprocess.run(cmd['argv'], capture_output=True, text=True, env=env, timeout=60)
        item = {'receipt': name, 'argv': cmd['argv'], 'exit': run.returncode, 'stdout': run.stdout, 'stderr': run.stderr}
        executed.append(item)
        assert run.returncode == 0, item
for path, digest in s['source_files'].items():
    assert sha(p / path) == digest
assert sha(selection) == final['source_selection_sha256']
receipt = {
    'status': 'R2_FROZEN_SOURCE_AND_FOCUSED_TEST_AUDIT_PASS',
    'source_selection_sha256': sha(selection), 'worker_final_receipt_sha256': sha(p / 'reports/FINAL-RECEIPT.json'),
    'source_pins_exact': len(s['source_files']), 'evidence_pins_exact': len(s['evidence_files']),
    'unchanged_v1_units_exact': len(s['unchanged_v1_unit_hashes']), 'changed_source_paths': changed,
    'profile_digest': final['profile_digest'], 'new_evidence_promotions_exact': promotions,
    'required_members_known_total': [66, 69], 'required_vtables_known_total': [4, 7],
    'unknown_members': unknown_members, 'unknown_vtables': unknown_vtables, 'unknown_static_widths': [],
    'actual_target_control_values': [0, 4, 8, 2, 8, 8], 'actual_target_control_object_sha256': sha(target_obj),
    'target_strong_layout_provider': providers,
    'independent_executed_commands': executed,
    'review': {
        'generic_initializer': 'Unchanged exact source; preflight remains before seeds/maps/engine calls and logs all required checks before refusal.',
        'active_width_adapter': 'Only Func/MinAlignment/ClassCastFlags/CastFlags use genuine public accessor value types; missing getters remain null and all other keys delegate unchanged. Generated getter bodies consume the actual published canonical maps.',
        'virtual_snapshot': 'Read-only staged virtual fact width is sizeof(void*) after staged identity pointer8 precondition. Actual-stage RED observed0 and GREEN8; staged engine maps remain empty. Wrong/missing required virtual facts still refuse in actual initializer cases.',
        'static_facts': 'Named type and semantic flow accepted by three separate independent saved-image audit receipts; six required gaps remain explicit. CDO400 remains supplemental.',
        'retained_suites': 'Nine unchanged V1 suites reused by exact immutable receipt and28 unchanged unit hashes; not rerun.'
    }, 'findings': [], 'source_mutations': 0, 'console_network_contact': False,
    'limits': 'Local static/host/target compilation only. Saved original17-member map remains declaration, no current expanded map/menu/spawn proof. Full two-build artifact audit remains parent-owned.'
}
dest = out / 'FOCUSED-SOURCE-AUDIT.json'
dest.write_text(json.dumps(receipt, indent=2) + '\n')
print('PASS', len(s['source_files']), 'source pins;', len(executed), 'independently executed commands; SHA', sha(dest))
