import hashlib
import json
import os
import re
import struct
import subprocess
from pathlib import Path

w = Path.cwd()
p = w / 'outputs/2026-10-07-l19-local-profile-r3'
r2 = w / 'outputs/2026-10-07-l19-local-profile-r2'
v1 = w / 'outputs/2026-10-07-l19-local-profile'
out = Path(__file__).resolve().parent

def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def load(path):
    return json.loads(Path(path).read_text())

def save(name, data):
    path = out / name
    assert not path.exists(), str(path)
    path.write_text(json.dumps(data, indent=2) + '\n')
    return {'path': str(path), 'sha256': sha(path)}

def check_sources(sources):
    for path, digest in sources.items():
        path = Path(path)
        if not path.is_absolute():
            path = p / path if len(path.parts) > 1 else p / 'units' / path
        assert sha(path) == digest, str(path)

selection_path = p / 'reports/SOURCE-SELECTION.json'
s = load(selection_path)
assert sha(selection_path) == 'cc0043df2f68d8ca1a61b49a5cef6d34e2a598fd49ded91e47cf3640d5f22337'
assert len(s['source_files']) == 75 and len(s['evidence_files']) == 36
for rel, digest in s['source_files'].items():
    assert sha(p / rel) == digest, rel
for rel, digest in s['evidence_files'].items():
    assert sha(w / rel) == digest, rel
old = load(r2 / 'reports/SOURCE-SELECTION.json')
assert sha(r2 / 'reports/SOURCE-SELECTION.json') == s['baseline_selection_sha256'] == '70e26759ff736d5dd167e5cae2d59700ff0dc1eee36ebe39fa0a784ddfc5cdd2'
original = load(v1 / 'reports/SOURCE-SELECTION.json')
assert sha(v1 / 'reports/SOURCE-SELECTION.json') == '0a6315e616922fab9922c2aff10c98af023654807ae7b2ab52957c05fb5da642'
for base, prior in [(r2, old), (v1, original)]:
    for rel, digest in prior['source_files'].items():
        assert sha(base / rel) == digest, str(base / rel)
for rel, digest in s['unchanged_r2_unit_hashes'].items():
    assert s['source_files'][rel] == old['source_files'][rel] == digest
assert len(s['unchanged_r2_unit_hashes']) == 29
for rel, digest in old['unchanged_v1_unit_hashes'].items():
    assert s['source_files'][rel] == original['source_files'][rel] == digest
changed = [rel for rel, digest in s['source_files'].items() if old['source_files'].get(rel) != digest]
assert [rel for rel in changed if rel.startswith('units/')] == ['units/NativeLayoutActiveWidths.hpp', 'units/RuntimeBuildProfile.hpp']
final_path = p / 'reports/FINAL-RECEIPT.json'
final = load(final_path)
assert sha(final_path) == '2f4f5acf095a4c32229e95d2140449da0a3ed417ce4110b248660e98d476bc01'
assert final['pass'] is True and final['source_selection_sha256'] == sha(selection_path)
for name, digest in final['receipt_hashes'].items():
    assert sha(p / 'reports' / name) == digest, name
assert sha(final['retained_v1_host_receipt']['path']) == final['retained_v1_host_receipt']['sha256']

profile_path = p / 'profiles/townfall-build-facts.json'
profile = load(profile_path)
old_profile = load(r2 / 'profiles/townfall-build-facts.json')
assert sha(profile_path) == final['profile_sha256'] == '66dc76fd41c96dc2faf9bc02bc098aaace20d05a0793fd633a81d977588c29af'
for key in ['schema', 'name', 'image', 'layout', 'axes', 'fname']:
    assert profile[key] == old_profile[key], key
promotions = []
for kind in ['required_members', 'required_vtables']:
    assert [x['name'] for x in profile[kind]] == [x['name'] for x in old_profile[kind]]
    for a, b in zip(old_profile[kind], profile[kind]):
        if (a['offset'], a['width']) != (b['offset'], b['width']):
            assert a['offset'] is None
            fact_path, fact_key = b['source'].split(':', 1)
            native = load(w / fact_path)
            facts = [x for x in native['facts'] if x['key'] == fact_key]
            assert len(facts) == 1
            fact = facts[0]
            assert int(fact['value'], 0) == b['offset'] and fact['width_bytes'] == b['width']
            if b['name'] == 'UEngine::Tick':
                audit = load(out / 'TICK-AUDIT.json')
                assert audit['status'] == 'ENGINE_TICK_STATIC_AUDIT_PASS'
                assert audit['fact_receipt']['sha256'] == sha(w / fact_path)
                assert fact['status'] == 'validated-static-pending-independent-review'
                accepted = audit['accepted_fact']
                assert accepted['status'] == 'validated-static' and accepted['confidence'] == 'high'
                for k in ['key', 'value', 'width_bytes']:
                    assert fact[k] == accepted[k]
            else:
                assert fact['status'] == 'validated-static' and fact['confidence'] == 'high'
            promotions.append(b)
assert [(x['name'], x['offset'], x['width']) for x in promotions] == [('UClass::Interfaces', 0x1d8, 16), ('UEngine::Tick', 0x310, 8)]
unknown_members = [x['name'] for x in profile['required_members'] if x['offset'] is None]
unknown_slots = [x['name'] for x in profile['required_vtables'] if x['offset'] is None]
assert unknown_members == final['remaining_unknown_members'] == ['FField::UEP_TotalSize', 'FFieldClass::UEP_TotalSize']
assert unknown_slots == final['remaining_unknown_vtables'] == ['UEngine::LoadMap', 'AGameModeBase::InitGameState']
assert profile['unknown_required_static_facts'] == []
owner = next(x for x in profile['required_members'] if x['name'] == 'FField::Owner')
assert owner['width'] == 8 and owner == next(x for x in old_profile['required_members'] if x['name'] == owner['name'])
evidence = load(p / 'reports/EVIDENCE.json')
assert evidence['profile_digest'] == final['profile_digest'] == 'efa1c15e6da091298d9764b9d0fa18b1196d8825776b383799806ef0397329fd'
assert all(x['exit'] == 0 for x in evidence['commands'])
schema = load(p / 'reports/PROFILE-SCHEMA-GREEN.json')
assert len(schema) == 18 and set(schema.values()) == {'PASS refused malformed/incomplete profile'}

def symbols(path):
    b = Path(path).read_bytes()
    header = struct.unpack_from('<16sHHIQQQIHHHHHH', b)
    assert header[0][:7] == b'\x7fELF\x02\x01\x01'
    sec = [struct.unpack_from('<IIQQQQIIQQ', b, header[6] + i * header[11]) for i in range(header[12])]
    found = []
    for r in sec:
        if r[1] != 2:
            continue
        st = sec[r[6]]
        strings = b[st[4]:st[4] + st[5]]
        for offset in range(r[4], r[4] + r[5], r[9]):
            n, info, other, idx, val, size = struct.unpack_from('<IBBHQQ', b, offset)
            found.append((strings[n:strings.index(0, n)].decode(), info >> 4, info & 15, idx, val, size))
    return b, sec, found

def constant_symbol(path, name):
    b, sec, syms = symbols(path)
    match = [x for x in syms if x[0] == name]
    assert len(match) == 1 and match[0][1] == 1 and match[0][3] != 0
    _, _, _, idx, value, size = match[0]
    offset = sec[idx][4] + value
    return struct.unpack_from('<' + str(size // 4) + 'I', b, offset), offset, b[offset:offset + size].hex()

census = load(p / 'reports/ALL-REQUIRED-WIDTHS-GREEN.json')
assert census['exit'] == 0 and census['missing_provider'] == [] and census['narrower_than_measured'] == []
check_sources(census['source_hashes'])
census_obj = Path(census['argv'][census['argv'].index('-o') + 1])
assert sha(census_obj) == census['object_sha256']
values, offset, symbol_hex = constant_symbol(census_obj, 'AllRequiredWidthFacts')
assert len(values) == 54 and all(values) and symbol_hex == census['symbol_bytes'] and offset == census['symbol_file_offset']
required = [x for x in profile['required_members'] if not x['name'].endswith('::UEP_TotalSize')]
assert census['required_keys'] == required
assert list(census['observed_widths']) == [x['name'] for x in required]
assert tuple(census['observed_widths'].values()) == values
assert all(width >= (x['width'] or 1) for x, width in zip(required, values))
assert census['observed_widths']['FField::Owner'] == 16
policy = {
    'UFunction::Func': 'FunctionWidth', 'UStruct::MinAlignment': 'MinAlignmentWidth',
    'UClass::ClassCastFlags': 'ClassCastFlagsWidth', 'FFieldClass::CastFlags': 'FieldCastFlagsWidth',
    'UStruct::SuperStruct': 'SuperStructWidth', 'FField::Owner': 'FieldOwnerWidth',
    'FField::Next': 'FieldNextWidth', 'FField::NamePrivate': 'FieldNameWidth',
    'FProperty::PropertyFlags': 'PropertyFlagsWidth', 'UFunction::FunctionFlags': 'FunctionFlagsWidth',
}
census_source = Path(census['argv'][census['argv'].index('-c') + 1]).read_text()
expressions = census_source.split('AllRequiredWidthFacts[]{', 1)[1].split('};}', 1)[0].strip().splitlines()
assert len(expressions) == 54
helper = (p / 'units/NativeLayoutActiveWidths.hpp').read_text()
for fact, expr in zip(required, expressions):
    cls, key = fact['name'].split('::')
    expected = ('ActiveWidthDetail::' + policy[fact['name']] + '<Unreal::' + cls + '>().value_or(0)') if fact['name'] in policy else f'Find("{cls}","{key}").value_or(0)'
    assert expr.rstrip(',') == expected, fact['name']
    if fact['name'] in policy:
        assert f'if (class_name == "{cls}" && key == "{key}")\n        return ActiveWidthDetail::{policy[fact["name"]]}<Unreal::{cls}>();' in helper
assert helper.count('if (class_name == ') == 10
assert 'return GetAccessorWidth(class_name, key);' in helper
assert 'GetBindingAccessorWidth(schema->target, schema->canonical)' in (p / 'units/NativeLayoutBinding.cpp').read_text()

target = load(p / 'reports/ACTIVE-WIDTHS-TARGET.json')
assert target['exit'] == 0
check_sources(target['source_hashes'])
target_obj = Path(target['argv'][target['argv'].index('-o') + 1])
target_values, _, _ = constant_symbol(target_obj, 'ActiveWidthControlFacts')
assert target_values == (0, 4, 8, 2, 8, 8, 8, 16, 8, 8, 8, 4)
target_dep = Path(target['argv'][target['argv'].index('-MF') + 1]).read_text()
assert str(p / 'units/NativeLayoutActiveWidths.hpp') in target_dep

native = load(p / 'reports/NATIVE-FOCUS.json')
check_sources(native['source_hashes'])
provider = '_ZN2RC8Platform13NativeLayouts26GetStagedNativeLayoutFactsEv'
providers = []
for cmd in native['commands']:
    assert cmd['exit'] == 0
    obj = Path(cmd['argv'][cmd['argv'].index('-o') + 1])
    _, _, syms = symbols(obj)
    matches = [x for x in syms if x[0] == provider]
    assert len(matches) == 1 and matches[0][1] == 1
    source = Path(cmd['argv'][cmd['argv'].index('-c') + 1])
    defined = matches[0][3] != 0
    assert defined == (source.name == 'NativeLayoutBinding.cpp')
    dep = Path(cmd['argv'][cmd['argv'].index('-MF') + 1])
    if defined:
        assert str(p / 'units/NativeLayoutActiveWidths.hpp') in dep.read_text()
    providers.append({'source': str(source), 'object': str(obj), 'sha256': sha(obj), 'strong_definition': defined, 'dependency_file': str(dep), 'dependency_sha256': sha(dep)})
assert len(providers) == 2
red_census = load(p / 'reports/ALL-REQUIRED-WIDTHS-RED.json')
assert red_census['exit'] != 0
red_binding = load(p / 'reports/REQUIRED-BINDING-RED.json')
assert red_binding['commands'][0]['exit'] == 0 and red_binding['commands'][1]['exit'] == 1
assert 'Native accessor width unavailable: UStruct::SuperStruct' in red_binding['commands'][1]['stderr']

env = dict(os.environ, ASAN_OPTIONS='halt_on_error=1:abort_on_error=1', UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1')
executed = []
counts = {}
for name in ['PROFILE-CHECKS-GREEN.json', 'INITIALIZER-PROFILE-GREEN.json', 'REQUIRED-BINDING-GREEN.json']:
    receipt = load(p / 'reports' / name)
    assert all(x['exit'] == 0 for x in receipt['commands'])
    check_sources(receipt.get('source_hashes', receipt.get('actual_sources', receipt.get('sources', {}))))
    commands = [x for x in receipt['commands'] if not x['argv'][0].endswith('clang++')]
    counts[name] = len(commands)
    for cmd in commands:
        binary = Path(cmd['argv'][0])
        digest = sha(binary)
        run = subprocess.run(cmd['argv'], capture_output=True, text=True, env=env, timeout=60)
        item = {'receipt': name, 'argv': cmd['argv'], 'binary_sha256': digest, 'exit': run.returncode, 'stdout': run.stdout, 'stderr': run.stderr}
        executed.append(item)
        assert run.returncode == 0 and 'PASS' in run.stdout, item
        assert sha(binary) == digest
assert list(counts.values()) == [2, 29, 25]
for rel, digest in s['source_files'].items():
    assert sha(p / rel) == digest, rel
assert sha(selection_path) == final['source_selection_sha256']

parent = w / 'outputs/2026-10-07-l19-r3-full-build'
clone = load(parent / 'PIPELINE-CLONE-RECEIPT.json')
assert len(clone['clone_inputs']) == 14
for pin in clone['clone_inputs']:
    old_path = Path(pin['path'])
    assert sha(old_path) == pin['sha256']
    assert sha(parent / old_path.name) == pin['sha256'], old_path.name

focused = save('FOCUSED-SOURCE-AUDIT.json', {
    'status': 'R3_FROZEN_SOURCE_AND_FOCUSED_TEST_AUDIT_PASS', 'pass': True,
    'worker_source_selection_sha256': sha(selection_path), 'worker_final_receipt_sha256': sha(final_path),
    'source_pins_exact': 75, 'evidence_pins_exact': 36, 'immutable_v1_source_pins_exact': 60, 'immutable_r2_source_pins_exact': 67,
    'unchanged_r2_units_exact': 29, 'changed_source_paths': changed,
    'new_evidence_promotions': promotions, 'required_members_known_total': [67, 69], 'required_slots_known_total': [5, 7],
    'remaining_unknown_members': unknown_members, 'remaining_unknown_slots': unknown_slots,
    'required_target_widths': census['observed_widths'], 'census_object_sha256': sha(census_obj),
    'target_public_controls': list(target_values), 'target_public_control_object_sha256': sha(target_obj),
    'target_strong_layout_provider': providers, 'strict_schema_refusals': 18,
    'independent_executed_commands': executed, 'retained_v1_suites': final['retained_v1_host_receipt'],
    'pipeline_clone_pins_exact': 14, 'findings': [], 'source_mutations': 0,
    'limits': 'Local static/host/target compilation only. Four native facts remain unknown; saved17/1 declaration remains incomplete and refuses. No menu/spawn or native Owner variant ABI claim.'
})
initializer = save('PARENT-INITIALIZER-TEST.json', {
    'status': 'R3_PARENT_INITIALIZER_TEST_PASS', 'pass': True, 'source_selection_sha256': sha(selection_path),
    'profile_sha256': sha(profile_path), 'profile_digest': final['profile_digest'],
    'actual_initializer_sha256': s['source_files']['units/UnrealInitializer.cpp'],
    'worker_receipt_sha256': sha(p / 'reports/INITIALIZER-PROFILE-GREEN.json'),
    'commands': [x for x in executed if x['receipt'] == 'INITIALIZER-PROFILE-GREEN.json'],
    'scenario_count': 29, 'engine_calls_after_preflight': 0, 'unknown_native_facts': unknown_members + unknown_slots,
    'cleanup_assertion': 'One callback invocation per refusal; actual bank retirement remains covered by unchanged retained suites.'
})
native_pins = save('PARENT-NATIVE-PINS.json', {
    'status': 'R3_PARENT_NATIVE_PINS_PASS', 'pass': True, 'source_selection_sha256': sha(selection_path),
    'worker_native_receipt_sha256': sha(p / 'reports/NATIVE-FOCUS.json'), 'objects': providers,
    'all54_widths': {'receipt_sha256': sha(p / 'reports/ALL-REQUIRED-WIDTHS-GREEN.json'), 'object': str(census_obj), 'sha256': sha(census_obj), 'values': census['observed_widths']},
    'public10_controls': {'receipt_sha256': sha(p / 'reports/ACTIVE-WIDTHS-TARGET.json'), 'object': str(target_obj), 'sha256': sha(target_obj), 'values': list(target_values)},
    'fresh_binding_helper_dependency': str(p / 'units/NativeLayoutActiveWidths.hpp'),
    'owner_native_storage_width': 8, 'owner_public_wrapper_bound': 16, 'native_owner_variant_ABI_equivalence': False,
    'source_pins': s['source_files'], 'evidence_pins': s['evidence_files']
})
print(json.dumps({'pass': True, 'sources': 75, 'evidence': 36, 'independent_commands': len(executed), 'census_widths': 54, 'receipts': [focused, initializer, native_pins]}))
