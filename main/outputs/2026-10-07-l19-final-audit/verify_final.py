#!/usr/bin/env python3
"""Independent read-only l19 artifact census; writes only this audit directory."""
import base64
import hashlib
import json
import struct
import subprocess
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
P = ROOT / 'outputs/2026-10-07-l19-full-build'
OUT = Path(__file__).resolve().parent
LLVM_NM = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm'
PROVIDER = '_ZN2RC8Platform13NativeLayouts26GetStagedNativeLayoutFactsEv'


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for chunk in iter(lambda: f.read(1048576), b''):
            h.update(chunk)
    return h.hexdigest()


def load(path):
    return json.loads(Path(path).read_text())


def elf_symbols(path):
    data = Path(path).read_bytes()
    assert data[:6] == b'\x7fELF\x02\x01', path
    hdr = struct.unpack_from('<16sHHIQQQIHHHHHH', data)
    offset, entsize, count = hdr[6], hdr[11], hdr[12]
    if count == 0:
        count = struct.unpack_from('<IIQQQQIIQQ', data, offset)[5]
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, offset + i * entsize) for i in range(count)]
    symbols = []
    for sec in sections:
        if sec[1] not in (2, 11):
            continue
        strings = sections[sec[6]]
        text = data[strings[4]:strings[4] + strings[5]]
        for off in range(sec[4], sec[4] + sec[5], sec[9]):
            name, info, other, shndx, value, size = struct.unpack_from('<IBBHQQ', data, off)
            end = text.find(b'\0', name)
            symbols.append({'name': text[name:end].decode(), 'bind': info >> 4,
                            'type': info & 15, 'shndx': shndx, 'table_type': sec[1]})
    return symbols


def external_undefined(path):
    return {s['name'] for s in elf_symbols(path) if s['name'] and s['shndx'] == 0 and s['bind'] != 0}


def nm_provider(path):
    result = subprocess.run([str(LLVM_NM), '--extern-only', '--format=posix', str(path)],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stderr
    rows = [s for s in result.stdout.splitlines() if s.split() and s.split()[0] == PROVIDER]
    return {'object': str(path), 'sha256': sha(path), 'rows': rows,
            'strong_T': sum(s.split()[1] == 'T' for s in rows),
            'strong_U': sum(s.split()[1] == 'U' for s in rows)}


def import_census(candidate):
    contract = load(candidate / 'supplier-contract.json')['contract']['imports']
    names = [r['ImportSymbol']['Name'] for r in contract]
    assert len(names) == len(set(names)) == 206
    suffix = bytes.fromhex('518d64a635ded8c1e6b039b1c3e55230')
    nid_names = {base64.b64encode(hashlib.sha1(n.encode() + suffix).digest()[:8][::-1]).decode()[:11].replace('/', '-'): n for n in names}
    assert len(nid_names) == len(names)
    artifacts = load(candidate / 'artifacts-receipt.json')['artifacts']
    result = {}
    for label in ['bare_original', 'bare_sdk']:
        path = Path(artifacts[label]['path'])
        dyn = [s['name'] for s in elf_symbols(path) if s['table_type'] == 11 and s['name'] and s['shndx'] == 0 and s['bind'] in (1, 2)]
        decoded = []
        for name in dyn:
            fields = name.split('#')
            assert len(fields) == 3 and len(fields[0]) == 11 and fields[0] in nid_names, name
            decoded.append(nid_names[fields[0]])
        assert len(dyn) == len(set(dyn)) == len(decoded) == len(set(decoded)) == 206
        assert set(decoded) == set(names)
        result[label] = {'path': str(path), 'sha256': sha(path), 'actual_dynamic_import_count': len(dyn),
                         'decoded_names': sorted(decoded), 'raw_nid_imports': sorted(dyn),
                         'matches_expected_names': True}
    assert result['bare_original']['raw_nid_imports'] == result['bare_sdk']['raw_nid_imports']
    return {'expected_count': len(names), 'contract_sha256': sha(candidate / 'supplier-contract.json'), 'elf_census': result}


def verify():
    checks = []
    gate_records = []
    artifacts = []
    imports = []
    providers = []
    manifests = []
    selected_objects = []
    for i in [1, 2]:
        candidate = P / ('prx-candidate-l19-' + str(i))
        run = P / 'runs' / ('run-l19-' + str(i))
        archive = P / 'receipts' / candidate.name
        gatepath = P / ('gate-receipt-l11-' + candidate.name + '.json')
        gate = load(gatepath)
        assert len(gate['checks']) == 11 and gate['expected_imports'] == 206
        assert gate['failed_checks'] == (['D1'] if i == 1 else [])
        assert all(x['status'] == 'PASS' for x in gate['checks'] if x['id'] != 'D1' or i == 2)
        gate_records.append({'path': str(gatepath), 'sha256': sha(gatepath), 'verdict': gate['verdict'],
                             'failed_checks': gate['failed_checks'], 'passed_checks': sum(x['status'] == 'PASS' for x in gate['checks'])})
        staticpath = candidate / 'verification-evidence/static-checks.json'
        static = load(staticpath)
        assert static['status'] == 'STATIC_CHECKS_PASSED'
        assert len(static['checks']) == 20 and all(v['pass'] is True for v in static['checks'].values())
        checks.append({'path': str(staticpath), 'sha256': sha(staticpath), 'checks': static['checks']})
        art = load(candidate / 'artifacts-receipt.json')['artifacts']
        for label, row in art.items():
            f = Path(row['path'])
            assert f.resolve().is_relative_to(candidate.resolve())
            assert sha(f) == row['sha256'] and f.stat().st_size == row['bytes'], (i, label)
        rr = load(run / 'receipt.json')
        obj = Path(rr['runtime_object'])
        assert sha(obj) == rr['runtime_sha256'] and obj.stat().st_size == rr['runtime_bytes']
        assert rr['reused_composed_runtime_object'] is False and rr['console_contact'] is False and rr['target_executed'] is False
        for name, row in rr['selected_runtime_units'].items():
            assert sha(row['source']) == row['source_sha256'], name
            assert sha(row['object']) == row['object_sha256'], name
        selected_objects.append({name:row['object_sha256'] for name,row in rr['selected_runtime_units'].items()})
        profile = load(candidate / 'package-profile.json')
        assert profile['composed_runtime_sha256'] == rr['runtime_sha256']
        assert profile['combined_object_sha256'] == art['combined_object']['sha256']
        assert profile['native_executed'] is False and profile['promotion_allowed'] is False
        art['composed_runtime'] = {'path': str(obj), 'sha256': rr['runtime_sha256'], 'bytes': rr['runtime_bytes']}
        artifacts.append(art)
        imports.append(import_census(candidate))
        symbol_records = {'initializer': nm_provider(rr['selected_runtime_units']['unreal-unrealinitializer']['object']),
                          'selected_binding': nm_provider(rr['selected_runtime_units']['native-layoutbinding']['object']),
                          'composed_runtime': nm_provider(obj),
                          'combined_supplier': nm_provider(art['combined_object']['path'])}
        assert symbol_records['initializer']['strong_U'] == 1 and symbol_records['initializer']['strong_T'] == 0
        assert all(v['strong_T'] == 1 and v['strong_U'] == 0 for k,v in symbol_records.items() if k != 'initializer')
        wholepath = archive / ('whole-selected-' + candidate.name + '.json')
        whole = load(wholepath)
        assert whole['status'] == 'PASS-WHOLE-SELECTED-ARTIFACTS' and whole['engine_execution_proved'] is False
        assert whole['new_layout_provider']['selected-binding']['sha256'] == symbol_records['selected_binding']['sha256']
        assert whole['new_layout_provider']['composed-runtime']['sha256'] == rr['runtime_sha256']
        providers.append({'measured': symbol_records, 'whole_receipt_sha256': sha(wholepath)})
        mp = archive / 'change-manifest.finalized.json'
        manifest = load(mp)
        combined_und = external_undefined(art['combined_object']['path'])
        baseline = load(P / 'inputs/baseline-composition5-r11.json')
        base_und = set(baseline['combined_und'])
        assert sorted(combined_und - base_und) == manifest['import_delta']['added']
        assert sorted(base_und - combined_und) == manifest['import_delta']['removed'] == []
        assert manifest['import_delta']['measurement']['combined_object_sha256'] == art['combined_object']['sha256']
        assert manifest['import_delta']['measurement']['measured_und_count'] == len(combined_und)
        manifests.append({'path':str(mp),'sha256':sha(mp),'combined_und_count':len(combined_und),
                          'measured_added':sorted(combined_und-base_und),'measured_removed':sorted(base_und-combined_und)})
        assert not load(candidate / 'build-receipt.json')['gaps']
    labels = ['final_prx', 'bare_original', 'bare_sdk', 'bridge_object', 'combined_object', 'composed_runtime']
    assert all(artifacts[0][label]['sha256'] == artifacts[1][label]['sha256'] for label in labels)
    assert selected_objects[0] == selected_objects[1]
    assert imports[0]['elf_census']['bare_sdk']['raw_nid_imports'] == imports[1]['elf_census']['bare_sdk']['raw_nid_imports']
    previous = import_census(ROOT / 'outputs/2026-10-07-l18-full-build/prx-candidate-l18-2')
    assert imports[1]['elf_census']['bare_sdk']['decoded_names'] == previous['elf_census']['bare_sdk']['decoded_names']
    frozenroot = ROOT / 'outputs/2026-10-07-l19-local-profile'
    frozenpath = frozenroot / 'reports/SOURCE-SELECTION.json'
    frozen = load(frozenpath)
    assert len(frozen['source_files']) == 60
    for f, h in frozen['source_files'].items():
        assert sha(frozenroot / f) == h
    record = {'scope':'Independent local artifact audit; no build rerun, console, network, source or mod changes',
              'status':'FINAL_ARTIFACT_AUDIT_PASS','gate_receipts':gate_records,'static_checks':checks,
              'artifacts':artifacts,'byte_determinism':{k:artifacts[0][k]['sha256'] for k in labels},
              'selected_object_hashes_equal':True,'imports':imports,'imports_unchanged_from_l18':True,
              'new_layout_provider':providers,'manifests':manifests,'frozen_source_count':60,
              'frozen_source_sha256':sha(frozenpath),'all_frozen_sources_unchanged':True,
              'upload':'HOLD','engine_menu_or_spawn_proved':False,
              'limitation':'Required unresolved native layout facts remain refusal; successful Mac packaging does not establish engine execution.'}
    (OUT / 'ARTIFACT-AUDIT.json').write_text(json.dumps(record,indent=2)+'\n')
    print('PASS: two artifact chains, all selected objects byte-deterministic; 206 exact dynamic imports each, unchanged from l18; exact provider U/T; frozen60 intact')
    print('ARTIFACT-AUDIT SHA256', sha(OUT/'ARTIFACT-AUDIT.json'))


if __name__ == '__main__':
    verify()
