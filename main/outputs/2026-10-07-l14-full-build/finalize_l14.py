#!/usr/bin/env python3
"""Aggregate local l14 build receipts; no console contact or executable edits."""
from pathlib import Path
from datetime import datetime, timezone
import hashlib
import json

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[1]
LOCAL = ROOT / 'outputs/2026-10-07-l14-image-admission'
L13 = ROOT / 'outputs/2026-10-07-l13-full-build/prx-candidate-l13-2'


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def read(p):
    return json.loads(Path(p).read_text())


def save(p, d):
    p.write_text(json.dumps(d, indent=2) + '\n')


def artifact(p):
    return {'path': str(p), 'sha256': sha(p), 'bytes': p.stat().st_size}


def contracts(p):
    return {r['ImportSymbol']['Name']: r['ImportSymbol']
            for r in read(p / 'supplier-contract.json')['contract']['imports']}


g = read(LOCAL / 'receipts/LATEST-GATES.json')
assert g['status'] == 'PASS_MAC_ONLY'
for rel, h in g['source_hashes'].items():
    assert sha(LOCAL / rel) == h, rel
for rec in g['preserved']:
    assert sha(ROOT / rec['path']) == rec['expected'], rec['path']
for p, h in read(LOCAL / 'baseline.json')['l13_sources'].items():
    assert sha(ROOT / p) == h, p
lineage = read(OUT / 'receipts/pipeline-lineage.json')
for p, h in lineage['baseline'].items():
    assert sha(p) == h, p
ed = read(OUT / 'EDITIONS-l14.json')
for rec in ed['units'] + [ed['bridge_cli']]:
    assert sha(ROOT / rec['file']) == rec['sha256'], rec['file']

passes = []
for number in (1, 2):
    c = OUT / f'prx-candidate-l14-{number}'
    r = OUT / f'runs/run-l14-{number}'
    static = read(c / 'verification-evidence/static-checks.json')
    assert len(static['checks']) == 20 and all(v['pass'] for v in static['checks'].values())
    access = read(OUT / f'receipts/accessor-verification-prx-candidate-l14-{number}.json')
    assert access['verdict'] == 'PASS'
    final = c / 'modbridge_supplier_composition.prx'
    assert final.read_bytes()[:4].hex() == '4f153d1d'
    assert read(c / 'artifacts-receipt.json')['artifacts']['final_prx'] == artifact(final)
    gate_path = OUT / ('gate-receipt-l14-1-final.json' if number == 1
                       else 'gate-receipt-l11-prx-candidate-l14-2.json')
    gate = read(gate_path)
    assert len(gate['checks']) == 11 and gate['failed_checks'] == []
    archive = OUT / f'receipts/prx-candidate-l14-{number}'
    assert (archive / 'driver/transport-ledger.composition6-mirror.json').is_file()
    finalization = read(archive / 'change-manifest.finalized.json.receipt.json')
    assert not finalization.get('divergences', []), finalization.get('divergences')
    passes.append({'number': number, 'prx': artifact(final),
        'runtime_object': artifact(r / 'coherent-tick-file-witness-runtime.o'),
        'combined_object': artifact(c / 'build/combined-supplier.o'),
        'static_checks_passed': len(static['checks']), 'gate_receipt': str(gate_path),
        'gate_ids': [v['id'] for v in gate['checks']], 'failed_checks': gate['failed_checks'],
        'receipt_archive': str(archive)})
for kind in ('prx', 'runtime_object', 'combined_object'):
    assert passes[0][kind]['sha256'] == passes[1][kind]['sha256'], kind
old, new = contracts(L13), contracts(OUT / 'prx-candidate-l14-2')
assert len(new) == 200 and old == new, 'Import additions/removals/identity drift vs l13'
src = {str(LOCAL / rel): h for rel, h in g['source_hashes'].items()}
receipt = {'status': 'PASS_MAC_ONLY_UPLOAD_HOLD',
    'created_utc': datetime.now(timezone.utc).isoformat(), 'console_contact': False,
    'console_success': False, 'passes': passes,
    'determinism': {'prx': True, 'runtime_object': True, 'combined_object': True},
    'sdk': '0x11000000', 'magic': '4f153d1d', 'final_strong_import_count': 200,
    'import_delta_vs_l13': {'added': [], 'removed': [], 'all_identities_unchanged': True},
    'local_source_and_frozen_checks': 'PASS', 'approved_source_hashes': src,
    'source_gate_receipt': str(LOCAL / 'receipts/LATEST-GATES.json'),
    'not_run_gate_ids': ['B1', 'B2', 'B3', 'D2'],
    'not_run_reason': 'These optional sections were absent in the inherited manifest; not claimed passed.',
    'segment_evidence': 'Real module-info when present; otherwise saved per-build PT_LOAD + current retained live mappings and base. Profile alone never grants readability.',
    'remaining_proof': 'l14 load/admission/mod lifetime and input/menu/spawn remain untested on PS5.',
    'first_pass_note': 'Provisional D1 preserved; separate final compare-to-2 passes 11/11.',
    'scope': 'Two fresh launch11-derived composition/package passes; no pad changes, no console contact.'}
save(OUT / 'receipts/FINAL-GATES.json', receipt)
save(OUT / 'deployment/candidate-manifest.json', {'schema': 1,
    'remote_path': '/data/plugins/modbridge_supplier_composition.prx', 'candidate': passes[1]['prx'],
    'expected_remote_sha256': 'cd58fa12c1864eb11f3f1eb72a66361c8f7468f9b349629e98261a3aac6a705f',
    'state': 'HOLD: separate user upload approval required', 'console_contact': False})
save(OUT / 'SOURCE-MANIFEST.json', {'files': {**src,
    **{str(p): sha(p) for p in OUT.glob('*.py')},
    str(OUT / 'EDITIONS-l14.json'): sha(OUT / 'EDITIONS-l14.json'),
    str(OUT / 'integration/change-manifest.finalized.json'): sha(OUT / 'integration/change-manifest.finalized.json')},
    'runtime_input_pins': ed['units'], 'artifact': passes[1]['prx']})
print(json.dumps({k: receipt[k] for k in ('status', 'determinism', 'final_strong_import_count', 'import_delta_vs_l13')}, indent=2))
print(json.dumps(passes[1]['prx'], indent=2))
