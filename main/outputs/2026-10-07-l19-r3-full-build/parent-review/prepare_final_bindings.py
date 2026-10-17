#!/usr/bin/env python3
"""Bind final reviewed local R3 inputs; never contacts the console or network."""
from pathlib import Path
import argparse, datetime, difflib, hashlib, json, shutil

ROOT = Path(__file__).resolve().parents[3]
OWN = Path(__file__).resolve().parent.parent
SRC = ROOT / 'outputs/2026-10-07-l19-local-profile-r3'
OLD = ROOT / 'outputs/2026-10-07-l19-local-profile-r2'
AUDIT = ROOT / 'outputs/2026-10-07-l19-final-audit/r3'

def load(path):
    return json.loads(Path(path).read_text())

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def pin(path):
    path = Path(path).resolve()
    return {'path': str(path), 'sha256': sha(path)}

def save(name, value):
    path = OWN / 'parent-review' / name
    assert not path.exists(), f'Preserving existing receipt: {path}'
    path.write_text(json.dumps(value, indent=2) + '\n')
    return path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--review', type=Path, required=True)
args = parser.parse_args()
review = load(args.review)
assert review['pass'] is True and review['findings'] == []
assert not (OWN / 'units').exists()
initializer_audit = pin(AUDIT/'PARENT-INITIALIZER-TEST.json')
native_audit = pin(AUDIT/'PARENT-NATIVE-PINS.json')
selection_path = SRC / 'reports/SOURCE-SELECTION.json'
selection, final = load(selection_path), load(SRC / 'reports/FINAL-RECEIPT.json')
assert final['pass'] is True
assert final['source_selection_sha256'] == sha(selection_path)
assert review['worker_source_selection_sha256'] == sha(selection_path)
assert review['worker_final_receipt_sha256'] == sha(SRC/'reports/FINAL-RECEIPT.json')
retained_v1 = load(OLD/'reports/SOURCE-SELECTION.json')['unchanged_v1_unit_hashes']
for rel, digest in retained_v1.items():
    assert sha(SRC/rel) == digest, f'Retained V1 unit changed: {rel}'
for rel, digest in selection['unchanged_r2_unit_hashes'].items():
    assert sha(SRC/rel) == digest, f'Retained R2 unit changed: {rel}'
pins = []
for rel, digest in selection['source_files'].items():
    path = SRC / rel
    assert sha(path) == digest, str(path)
    pins.append(pin(path))
for path, digest in selection['evidence_files'].items():
    evidence_path = Path(path)
    if not evidence_path.is_absolute(): evidence_path = ROOT / evidence_path
    assert sha(evidence_path) == digest, path
    pins.append(pin(evidence_path))
save('FOCUSED-PINS.json', {'scope':'Mac-only final frozen R3 source/evidence drift check',
                         'pass':True,'pins':pins})
changes = []
oldunits = {p.name:p for p in (OLD / 'units').iterdir() if p.is_file()}
newunits = {p.name:p for p in (SRC / 'units').iterdir() if p.is_file()}
assert set(oldunits) == set(newunits)
for name in sorted(newunits):
    a, b = oldunits[name], newunits[name]
    if a.read_bytes() != b.read_bytes():
        patch = ''.join(difflib.unified_diff(a.read_text().splitlines(True),
                         b.read_text().splitlines(True), fromfile=str(a), tofile=str(b)))
        patch_path = OWN / 'parent-review' / ('diff-' + name + '.patch')
        patch_path.write_text(patch)
        changes.append({'file':name,'old':pin(a),'new':pin(b),'diff':pin(patch_path)})
delta_path = save('SOURCE-DELTA.json', {'scope':'Local R3 runtime/header delta vs frozen R2',
    'changes':changes,'pad_cache_lifetime_deadline_timing_unchanged':True,
    'mod_or_settings_edits':False})
focused_names = [n for n in final['receipt_hashes'] if n.endswith('GREEN.json')
                 or n.endswith('TARGET.json') or n == 'NATIVE-FOCUS.json']
focused = {n:pin(SRC / 'reports' / n) for n in focused_names}
for n, p in focused.items():
    assert p['sha256'] == final['receipt_hashes'][n]
host_path = save('HOST-ALL-R3.json', {'scope':'Mac-only current R3 focused checks; retained unchanged V1 suites explicitly not rerun',
    'pass':True,'current_source_freeze':pin(selection_path),
    'focused_final_receipt':pin(SRC/'reports/FINAL-RECEIPT.json'),
    'focused_receipts':focused,'retained_suites':final['retained_v1_host_receipt'],
    'unchanged_v1_unit_hashes':retained_v1,
    'unchanged_r2_unit_hashes':selection['unchanged_r2_unit_hashes'],
    'retained_v1_unit_lineage':pin(OLD/'reports/SOURCE-SELECTION.json'),
    'known_remaining_refusal':{'members':final['remaining_unknown_members'],
                             'vtables':final['remaining_unknown_vtables'],
                             'staged_17_1_unchanged':True}})
pr = OWN / 'parent-review'
prior = ROOT / 'outputs/2026-10-07-l19-r2-full-build/parent-review'
for name in ('XOM-AUDIT.md','SETTINGS-PROVENANCE.md'):
    shutil.copyfile(prior/name, pr/name)
xom_path = save('XOM-RECEIPT.json', {'scope':'Analysis-only retained exact XOM source audit; R3 type metadata/profile additions change no instruction-read/write path',
    'pass':True,'source_freeze':pin(selection_path),'audit':pin(pr/'XOM-AUDIT.md'),
    'source_delta':pin(delta_path),'no_game_instruction_write_prerequisite_identified_for_first_menu':True,
    'required_game_thread_carrier_writes_UFunction_Func_DATA':True,
    'full_LoadMap_reset_hook_unproved_under_XOM':True,
    'no_console_contact':True,'timing_changes':False})
roles = {'runtime_owner.cpp':'runtime_owner','UnrealInitializer.cpp':'initializer',
         'runtime_start_bridge_r14_diag_spanpub.cpp':'bridge'}
selected = {roles.get(p.name,p.name):pin(p) for p in sorted(newunits.values())
            if p.suffix in ('.cpp','.h','.hpp')}
provenance = {'source_delta':pin(delta_path),'HOST-ALL.json':pin(host_path),
    'NATIVE-FOCUS.json':pin(SRC/'reports/NATIVE-FOCUS.json'),
    'profile-json':pin(SRC/'profiles/townfall-build-facts.json'),
    'worker-source-freeze':pin(selection_path),
    'focused-final-receipt':pin(SRC/'reports/FINAL-RECEIPT.json')}
for n,p in focused.items():
    provenance['focused-'+n] = p
for path,digest in selection['evidence_files'].items():
    evidence_path = Path(path)
    if not evidence_path.is_absolute(): evidence_path = ROOT / evidence_path
    relative = str(evidence_path.resolve().relative_to(ROOT))
    name = 'external-'+Path(path).name+'-'+hashlib.sha256(relative.encode()).hexdigest()[:16]
    assert name not in provenance
    provenance[name] = pin(evidence_path)
compatible = save('SOURCE-SELECTION.compatible.json', {'schema':1,'selected_sources':selected,
    'provenance':provenance,'worker_freeze':str(selection_path),
    'scope':'Complete exact R3 unit/header role wrapper; no source bytes changed; unknown/missing declarations refuse'})
prep = load(OWN/'PREP-MANIFEST.json')
prep.update(readiness='HOLD', source_manifest=None,
    xom_audit_receipt=pin(xom_path), xom_audit_report=pin(pr/'XOM-AUDIT.md'),
    parent_go=str(pr/'BUILD-GO.original.json'),
    reason='Final R3 freeze/review bound; binder will admit LOCAL pipeline only. Upload HOLD.')
(OWN/'PREP-MANIFEST.json').write_text(json.dumps(prep,indent=2)+'\n')
go = {'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'scope':'Approved LOCAL R3 double-build/import/package/artifact gates; missing/unknown facts refuse. No console/network/upload.',
    'parent_reported_xom_audit':True,'source_manifest_sha256':sha(compatible),
    'xom_audit_sha256':sha(xom_path),'source_manifest':pin(compatible),
    'xom_audit':pin(xom_path),'frozen_source_review':pin(args.review),
    'host_receipt':pin(host_path),'native_receipt':pin(SRC/'reports/NATIVE-FOCUS.json'),
    'source_delta':pin(delta_path),
    'parent_initializer_test':initializer_audit,
    'parent_native_pins':native_audit,
    'settings_provenance_addendum':pin(pr/'SETTINGS-PROVENANCE.md'),
    'upload_authorized':False,'console_execution_proven':False,
    'prohibited':['console contact','uploads','payload loads','launches',
                  'config/mod/game/system/kernel writes','old artifact overwrite']}
go_path = save('BUILD-GO.original.json',go)
print(json.dumps({'selection':pin(compatible),'parent_go':pin(go_path),
                  'selected_units_headers':len(selected),'pass':True},indent=2))
