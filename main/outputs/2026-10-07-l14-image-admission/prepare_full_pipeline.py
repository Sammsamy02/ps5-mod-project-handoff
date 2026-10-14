#!/usr/bin/env python3
"""Clone the corrected, frozen l13 pipeline into isolated local l14 output.

Preserves the old editions and all strict gates. No console/network operations.
"""
import hashlib
import json
import shutil
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OLD = ROOT / 'outputs/2026-10-07-l13-full-build'
LOCAL = Path(__file__).resolve().parent
OUT = ROOT / 'outputs/2026-10-07-l14-full-build'
UNITS = LOCAL / 'units'


def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()


def save(p, value):
    p.write_text(json.dumps(value, indent=2) + '\n')


def replace(text, before, after):
    assert before in text, before
    return text.replace(before, after)


assert not OUT.exists(), 'Refuse to overwrite a prior l14 full build'
OUT.mkdir()
for name in ('receipts', 'logs', 'runs', 'integration', 'driver', 'reports', 'deployment'):
    (OUT / name).mkdir()

old_names = ('build_runtime_composition6_fixed_l13.py',
             'build_runtime_composition6_l13.py', 'package_prx_l13.py',
             'launch13_one_pass.py')
new_names = tuple(n.replace('l13', 'l14').replace('launch13', 'launch14') for n in old_names)
baseline = {str(OLD / n): sha(OLD / n) for n in (*old_names, 'EDITIONS-l13.json')}

# Keep the corrected typed-shim ordering and existing bootstrap import evidence.
shutil.copy2(OLD / 'package_prx_l13.py', OUT / 'package_prx_l14.py')
fixed = (OLD / old_names[0]).read_text()
fixed = replace(fixed, "'bridge_capture_envelope',", "'bridge_image_range','bridge_capture_envelope',")
(OUT / new_names[0]).write_text(fixed)

driver = (OLD / old_names[1]).read_text()
driver = replace(driver, str(OLD), str(OUT))
driver = replace(driver, 'build_runtime_composition6_fixed_l13.py', new_names[0])
driver = replace(driver, 'package_prx_l13.py', 'package_prx_l14.py')
driver = replace(driver, 'outputs/2026-10-07-l13-local-admission/units', str(UNITS.relative_to(ROOT)))
driver = replace(driver, sha(OLD / old_names[0]), sha(OUT / new_names[0]))
driver = replace(driver, sha(OLD / 'package_prx_l13.py'), sha(OUT / 'package_prx_l14.py'))
driver = replace(driver, "'bridge_capture_envelope',", "'bridge_image_range', 'bridge_capture_envelope',")
(OUT / new_names[1]).write_text(driver)

runner = (OLD / old_names[3]).read_text()
runner = replace(runner, str(OLD), str(OUT))
runner = replace(runner, 'build_runtime_composition6_l13.py', new_names[1])
runner = replace(runner, 'EDITIONS-l13.json', 'EDITIONS-l14.json')
runner = replace(runner, 'run-l13-', 'run-l14-')
runner = replace(runner, 'prx-candidate-l13-', 'prx-candidate-l14-')
runner = replace(runner, "('PoolAdmissionEngine.hpp','CopyinSink.hpp','CopyinStartup.hpp')",
                 "('PoolAdmissionEngine.hpp','CopyinSink.hpp','CopyinStartup.hpp','ImageCoverage.hpp','ImageLoadProfile.hpp','FNamePoolChecks.hpp')")
runner = replace(runner, "(Path(init)/'PoolAdmissionEngine.hpp',",
                 "(Path(init)/'ImageCoverage.hpp', 'L14 exact segment proof policy'),\n"
                 "                      (Path(init)/'ImageLoadProfile.hpp', 'L14 captured PT_LOAD per-game profile; never sufficient alone'),\n"
                 "                      (Path(init)/'FNamePoolChecks.hpp', 'L14 production collect-all admission evaluator'),\n"
                 "                      (Path(init)/'PoolAdmissionEngine.hpp',")
runner = replace(runner, "'bridge_capture_envelope',", "'bridge_image_range', 'bridge_capture_envelope',")
runner = replace(runner, 'L13 FULL PIPELINE COMPLETE', 'L14 FULL PIPELINE COMPLETE')
(OUT / new_names[3]).write_text(runner)
shutil.copy2(OLD / 'freshen-deps.txt', OUT / 'freshen-deps.txt')

ed = json.loads((OLD / 'EDITIONS-l13.json').read_text())
retained = []
for record in ed['units']:
    p = ROOT / record['file']
    assert sha(p) == record['sha256'], f'Frozen baseline drift: {p}'
    if p.parent in (ROOT / 'outputs/2026-10-07-l13-local-admission/units', OLD):
        continue
    retained.append(record)
for p in sorted(UNITS.iterdir()):
    if p.is_file():
        retained.append({'file': str(p), 'sha256': sha(p), 'what': 'L14 locally verified runtime/header'})
for name in new_names:
    p = OUT / name
    retained.append({'file': str(p), 'sha256': sha(p), 'what': 'L14 isolated pipeline edition'})
ed['units'] = retained
ed['baseline_edition'] = {'path': str(OLD / 'EDITIONS-l13.json'), 'sha256': sha(OLD / 'EDITIONS-l13.json')}
ed['bridge_cli'] = {'file': str(UNITS / 'runtime_start_bridge_r14_diag_spanpub.cpp'),
                    'sha256': sha(UNITS / 'runtime_start_bridge_r14_diag_spanpub.cpp'),
                    'provenance': str(LOCAL / 'receipts/LATEST-GATES.json')}
ed['compose']['initializer_armed']['dir'] = str(UNITS)
ed['admissions']['initializer_fold'] = {'status': 'ADMITTED-V4ARMED-FOLD',
    'files': {str(UNITS / 'UnrealInitializer.cpp'): sha(UNITS / 'UnrealInitializer.cpp')},
    'evidence': 'L14 local integration gates. User approved local implementation and full build; upload HOLD.'}
ed.pop('l13_scope', None)
ed['l14_scope'] = {'console_contact': False, 'upload': 'HOLD awaiting separate explicit OK',
    'pad_changes': False, 'expected_final_import_delta_vs_l13': {'added': [], 'removed': []}}
save(OUT / 'EDITIONS-l14.json', ed)

mf = json.loads((OLD / 'integration/change-manifest.json').read_text())
# Redirect only consumed source/receipt paths, retaining all inherited provenance.
mf = json.loads(json.dumps(mf).replace(str(OLD), str(OUT)).replace(
    str(ROOT / 'outputs/2026-10-07-l13-local-admission/units'), str(UNITS)))
mf['manifest_id'] = 'l14-segment-covered-image-collect-all-admission'
mf['manifest'] = 'l14-change'
mf['import_delta']['note'] = 'Expected delta against frozen composition5-r11 unchanged; vs l13 no additions/removals or identity changes.'
for record in mf['changed_units']:
    if record['unit'] == 'unreal-unrealinitializer':
        p = UNITS / 'UnrealInitializer.cpp'
        record.update(source=str(p), sha256=sha(p), sha256_after={str(p): sha(p)},
                      justification='L14 exact segment coverage for image metadata, VQ-first heap, collect-all safe diagnostics and total admission time')
for record in mf['packaging_units']:
    if record['unit'] == 'lifecycle/runtime_start_bridge.cpp':
        p = UNITS / 'runtime_start_bridge_r14_diag_spanpub.cpp'
        record.update(source=str(p), sha256=sha(p),
                      justification='L14 loaded-segment accessor: real module info when present; captured PT_LOAD plus live mappings otherwise')
mf['expected_next_launch'] = 'Fresh session pool admission source and exact ranges logged, full initializer completes, mod stays installed; pad code unchanged. Not console-proven.'
save(OUT / 'integration/change-manifest.json', mf)
save(OUT / 'receipts/pipeline-lineage.json', {
    'baseline': baseline, 'approved_local_gates': str(LOCAL / 'receipts/LATEST-GATES.json'),
    'changes': ['L14 source/header paths', 'internal bridge_image_range accessor', 'header staging and receipt pins'],
    'strict_gates_retained': True, 'console_contact': False})
print('Prepared isolated l14 pipeline:', OUT)
