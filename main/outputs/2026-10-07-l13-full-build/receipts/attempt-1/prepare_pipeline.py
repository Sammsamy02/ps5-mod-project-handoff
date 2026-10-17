#!/usr/bin/env python3
"""Make isolated l13 editions. Only writes this directory; never contacts console."""
from pathlib import Path
import hashlib, json, shutil

ROOT=Path('/WORKSPACE/.codex/worktrees/translation-core/what')
OUT=Path(__file__).resolve().parent
OLD=ROOT/'outputs/2026-10-04-zcode-controller/launch11-combined'
UNITS=ROOT/'outputs/2026-10-07-l13-local-admission/units'
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def replace(s, old, new):
    assert old in s, old
    return s.replace(old,new)
def save(p,x): p.write_text(json.dumps(x,indent=2)+'\n')

for d in ('receipts','logs','runs','integration','driver','reports'):
    (OUT/d).mkdir(exist_ok=True)
fixed=(OLD/'build_runtime_composition6_fixed_l11.py').read_text()
fixed=replace(fixed,"'bridge_write_copyin_capture', 'scePadGetHandle'", "'bridge_write_copyin_capture', 'bridge_kernel_write_copyin_probe_until', 'bridge_copyin_range_faulted', 'scePadGetHandle'")
(OUT/'build_runtime_composition6_fixed_l13.py').write_text(fixed)

driver=(OLD/'build_runtime_composition6_l12.py').read_text()
driver=replace(driver,"MY = TEAM / 'worker-comp6-driver'",f"MY = Path({str(OUT/'driver')!r})")
driver=replace(driver,str(OLD/'build_runtime_composition6_fixed_l11.py').removeprefix(str(ROOT)+'/'),str(OUT/'build_runtime_composition6_fixed_l13.py'))
driver=replace(driver,'c42dc229b31bc5501cb1b1218cc97b8092f776d17c90facd532cd56c4d86ea08',sha(OUT/'build_runtime_composition6_fixed_l13.py'))
driver=replace(driver,"PKG_FIXED = TEAM / 'worker-prx-link-gaps/package_prx.py'",f"PKG_FIXED = Path({str(OUT/'package_prx_l13.py')!r})")
driver=replace(driver,'921d8397957df6fe39e19ca010fdcb6ba469ba0c6e80ef458ea41ad49f08be64',sha(OUT/'package_prx_l13.py'))
driver=replace(driver,'outputs/2026-10-04-zcode-controller/launch11-combined/units-l12',str(UNITS.relative_to(ROOT)))
driver=replace(driver,"_overlay = ROOT / 'outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/lineage-overlay-2'", "_overlay = MY / 'staging/lineage-overlay-2'")
driver=replace(driver,"'bridge_write_copyin_capture', 'scePadGetHandle'", "'bridge_write_copyin_capture', 'bridge_kernel_write_copyin_probe_until', 'bridge_copyin_range_faulted', 'scePadGetHandle'")
driver=replace(driver,"'bridge_kernel_write_copyin_probe', 'bridge_write_copyin_capture'}", "'bridge_kernel_write_copyin_probe', 'bridge_write_copyin_capture', 'bridge_kernel_write_copyin_probe_until', 'bridge_copyin_range_faulted'}")
(OUT/'build_runtime_composition6_l13.py').write_text(driver)

runner=(OLD/'launch12_one_pass.py').read_text()
runner=replace(runner,"MY = ROOT / 'outputs/2026-10-04-zcode-controller/launch11-combined'",f"MY = Path({str(OUT)!r})")
runner=replace(runner,'build_runtime_composition6_l12.py','build_runtime_composition6_l13.py')
runner=replace(runner,'EDITIONS-l12.json','EDITIONS-l13.json')
runner=replace(runner,"INT = ROOT / 'outputs/2026-10-04-zcode-controller/comp6-integration'", "INT = MY / 'integration'")
runner=replace(runner,"COMP_RUNS = W / 'ue4ss-native-supplier-composition-6'", "COMP_RUNS = MY / 'runs'")
runner=replace(runner,"ov = TEAM / 'worker-comp6-driver/staging/lineage-overlay-2'", "ov = MY / 'driver/staging/lineage-overlay-2'")
runner=replace(runner,'run-l12-','run-l13-')
runner=replace(runner,'prx-candidate-l12-','prx-candidate-l13-')
runner=replace(runner,"import hashlib, json, subprocess, sys, time", "import hashlib, json, subprocess, sys, time, shutil")
runner=replace(runner,"    for l in (r.stdout + r.stderr).strip().splitlines()[-tail:]:", "    (MY/'logs'/f'{time.time_ns()}-{name.replace(chr(47), chr(95))}.json').write_text(json.dumps({'argv':list(map(str,argv)), 'exit':r.returncode,'stdout':r.stdout,'stderr':r.stderr},indent=2))\n    for l in (r.stdout + r.stderr).strip().splitlines()[-tail:]:")
runner=replace(runner,"(Path(init)/'shadow_arm.hpp',", "(Path(init)/'PoolAdmissionEngine.hpp', 'L13 production admission helper'),\n                      (Path(init)/'CopyinSink.hpp', 'L13 production sink helper'),\n                      (Path(init)/'CopyinStartup.hpp', 'L13 production startup lifetime helper'),\n                      (Path(init)/'shadow_arm.hpp',")
runner=replace(runner,"         '--composition-run', COMP_RUNS/f'run-l13-{run_seq}',", "         *[v for name in ('PoolAdmissionEngine.hpp','CopyinSink.hpp','CopyinStartup.hpp') for v in ('--stage-header', f\"{name}={EDITION['compose']['initializer_armed']['dir']}/{name}={sh(Path(EDITION['compose']['initializer_armed']['dir'])/name)}\")],\n         '--composition-run', COMP_RUNS/f'run-l13-{run_seq}',")
runner=replace(runner,"    runs = sorted([p for p in COMP_RUNS.glob('run-l13-*') if p.is_dir()], key=lambda p: p.stat().st_mtime)\n    if not runs: die('no composition run dir found')\n    run_dir = runs[-1]", "    run_dir = COMP_RUNS / ('run-l13-' + out_name.rsplit('-',1)[-1])\n    if not run_dir.is_dir(): die('exact composition run missing')")
runner=replace(runner,"    return run_dir\n\ndef verify_accessors", "    archive = MY/'receipts'/out_name\n    if archive.exists(): archive.rename(archive.with_name(archive.name + '-aside-' + str(time.time_ns())))\n    archive.mkdir(exist_ok=False)\n    for p in (MY/'receipts').glob('*.json'): shutil.copy2(p,archive/p.name)\n    for p in INT.glob('*'): \n        if p.is_file(): shutil.copy2(p,archive/p.name)\n    driver_archive=archive/'driver'; driver_archive.mkdir()\n    for p in (MY/'driver/staging').iterdir():\n        if p.is_file() and not p.is_symlink(): shutil.copy2(p,driver_archive/p.name)\n    shutil.copy2(MY/'driver/transport-ledger.composition6-mirror.json',driver_archive/'transport-ledger.composition6-mirror.json')\n    return run_dir\n\ndef verify_accessors")
runner=replace(runner,"acc = {'bridge_capture_envelope', 'bridge_readable_span_at', 'bridge_readable_span_count'}", "acc = {'bridge_capture_envelope', 'bridge_readable_span_at', 'bridge_readable_span_count', 'bridge_kernel_write_copyin_probe_until', 'bridge_copyin_range_faulted'}")
runner=replace(runner,"LAUNCH-11 ONE-PASS COMPLETE: gate GO x2", "L13 FULL PIPELINE COMPLETE: candidate-1 provisional D1; candidate-2 gate GO with twin")
(OUT/'launch13_one_pass.py').write_text(runner)
shutil.copy2(OLD/'freshen-deps.txt',OUT/'freshen-deps.txt')

ed=json.loads((OLD/'EDITIONS-l12.json').read_text())
ed['baseline_edition']={'path':str(OLD/'EDITIONS-l12.json'),'sha256':sha(OLD/'EDITIONS-l12.json')}
new=[]
for e in ed['units']:
    p=ROOT/e['file']
    if p.parent==OLD/'units-l12': continue
    assert sha(p)==e['sha256'],f'baseline drift {p}'
    new.append(e)
for p in sorted(UNITS.iterdir()):
    if p.is_file(): new.append({'file':str(p),'sha256':sha(p),'what':'L13 approved source/header; frozen local gates'})
for name in ('build_runtime_composition6_fixed_l13.py','build_runtime_composition6_l13.py','package_prx_l13.py','launch13_one_pass.py'):
    p=OUT/name; new.append({'file':str(p),'sha256':sha(p),'what':'L13 isolated build edition'})
ed['units']=new
ed['bridge_cli']={'file':str(UNITS/'runtime_start_bridge_r14_diag_spanpub.cpp'),'sha256':sha(UNITS/'runtime_start_bridge_r14_diag_spanpub.cpp'),'provenance':'L13 approved local gates'}
ed['compose']['initializer_armed']['dir']=str(UNITS)
ed['admissions']['initializer_fold']={'status':'ADMITTED-V4ARMED-FOLD','files':{str(UNITS/'UnrealInitializer.cpp'):sha(UNITS/'UnrealInitializer.cpp')},'evidence':'L13 local admission report; user approved full build, upload HOLD'}
ed['l13_scope']={'console_contact':False,'upload':'HOLD awaiting explicit user OK','pad_changes':False,'expected_added_final_imports_vs_l12':['_openat','sceKernelRead']}
save(OUT/'EDITIONS-l13.json',ed)

mf=json.loads((ROOT/'outputs/2026-10-04-zcode-controller/comp6-integration/change-manifest.json').read_text())
mf['manifest_id']='l13-vq-first-shared-budget-retained-sink'
mf['manifest']='l13-change'
mf['import_delta']={'added':['_openat','sceKernelRead','sceKernelVirtualQuery','scePadGetHandle'],'removed':[], 'note':'Exact expected final UND delta against frozen composition5-r11; vs frozen l12 only _openat/sceKernelRead added; both bootstrap-only SDK imports.'}
for e in mf['changed_units']:
    e['sha256']='PENDING-FROM-ONE-PASS'
    if e['unit']=='lmf-loader-adapter':
        e['source']=str(ROOT/'.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp')
    if e['unit']=='native-loadercontext':
        e['source']=str(ROOT/'.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/NativeLoaderContext.cpp')
    e['receipt']=str(OUT/'receipts/l11-replace-receipt.json')
    e['sha256_after']={str(ROOT/e['source']):sha(ROOT/e['source'])}
mf['changed_units'].append({'unit':'unreal-unrealinitializer','kind':'replaced-affected','source':str(UNITS/'UnrealInitializer.cpp'),'sha256':sha(UNITS/'UnrealInitializer.cpp'),'sha256_after':{str(UNITS/'UnrealInitializer.cpp'):sha(UNITS/'UnrealInitializer.cpp')},'justification':'Approved L13 VQ-first, shared deadline, hard-negative exclusions and no unproved heap reads','receipt':str(OUT/'receipts/l11-replace-receipt.json')})
for e in mf['packaging_units']:
    if e['unit']=='lifecycle/runtime_start_bridge.cpp':
        e.update(source=str(UNITS/'runtime_start_bridge_r14_diag_spanpub.cpp'),sha256=sha(UNITS/'runtime_start_bridge_r14_diag_spanpub.cpp'),justification='L13 retained directory sink, SCE error normalization, deadline-aware oracle')
mf['expected_next_launch']='Pool admitted via a logged source and mod stays installed; menu not a gate for this build. No console result yet.'
save(OUT/'integration/change-manifest.json',mf)
save(OUT/'receipts/pipeline-lineage.json',{'baseline':{str(OLD/n):sha(OLD/n) for n in ('launch12_one_pass.py','build_runtime_composition6_l12.py','build_runtime_composition6_fixed_l11.py','EDITIONS-l12.json')},'approved_local_gates':str(ROOT/'outputs/2026-10-07-l13-local-admission/receipts/LATEST-GATES.json'),'console_contact':False})
print('Prepared isolated l13 pipeline:',OUT)
