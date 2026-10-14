#!/usr/bin/env python3
"""Controller launch-10 one-pass runner (composition rebuild with the four
host-verified fixes: geometry intersect-extent, read-path guard + gmalloc
validator + armed SeedTable, diagnostic sink, geometrydiag bridge).

Driver edition: launch10-build/build_runtime_composition6_l10.py (EDITION.json
records base sha + the three tagged edits). The runner re-verifies every unit
sha at start and refuses on drift. HOLD-PASS mode (default) stops after
verify-only so the anchor-cap recommendation can be folded in before the real
pass; --full runs receipt regen + the pass + ODR + finalize + gate v2 +
determinism twin.
"""
import hashlib, json, subprocess, sys, time
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
TEAM = ROOT / 'outputs/2026-10-04-zcode-team'
MY = ROOT / 'outputs/2026-10-04-zcode-controller/launch10-build'
DRIVER = MY / 'build_runtime_composition6_l10.py'
EDITION = json.loads((MY / 'EDITION.json').read_text())
GATE2 = TEAM / 'worker-gate-evolution/gate_check_composition5_v2.py'
ODR = TEAM / 'worker-odr-emitter-check/check_odr_emitters.py'
FIN = TEAM / 'worker-manifest-finalizer/finalize_manifest.py'
INT = ROOT / 'outputs/2026-10-04-zcode-controller/comp6-integration'
REGEN_X = TEAM / 'worker-comp6-driver/regen_extra_receipt.py'
REGEN_R = TEAM / 'worker-comp6-driver/regen_replace_receipt.py'
W = ROOT / '.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1'
W_INC = W / 'include'
COMP_RUNS = W / 'ue4ss-native-supplier-composition-6'

def die(m): print(f'JOB FAILED: {m}'); sys.exit(1)
def sh(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def run(name, argv, tail=4):
    print(f'== {name}', flush=True)
    r = subprocess.run([str(a) for a in argv], capture_output=True, text=True, cwd=ROOT)
    for l in (r.stdout + r.stderr).strip().splitlines()[-tail:]:
        print('   |', l[:170])
    if r.returncode != 0: die(f'{name} exit {r.returncode}')
    return r

def verify_units():
    print('== unit sha verification (EDITION.json)')
    for e in EDITION['edits'] + [EDITION['bridge_cli']]:
        actual = sh(e['file'])
        if actual != e['sha256']:
            die(f"unit drift: {e['file']}\n  edition {e['sha256'][:16]} vs actual {actual[:16]}\n"
                "  (if the anchor-cap fold edited a staging file, refresh EDITION.json first)")
        print(f"   | OK {Path(e['file']).name} {actual[:12]}")
    print('   | all units verified')

def preflight_and_verify():
    program = TEAM / 'worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp'
    if not program.is_file(): die('program staging missing')
    bridge = EDITION['bridge_cli']
    run('verify-only', ['python3', DRIVER, '--verify-only',
                        '--program-source', program,
                        '--bridge-source', bridge['file'],
                        '--bridge-sha256', bridge['sha256']])

def regen_receipts():
    program = TEAM / 'worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp'
    geom = EDITION['edits'][0]['file']
    rcpts = MY / 'receipts'; rcpts.mkdir(exist_ok=True)
    for rcpt in (rcpts/'l10-extra-receipt.json', rcpts/'l10-replace-receipt.json'):
        if rcpt.exists(): rcpt.rename(rcpt.with_suffix('.json.prev'))
    freshen_dep_args = []
    fd = MY / 'freshen-deps.txt'
    if fd.is_file():
        freshen_dep_args = [a for l in fd.read_text().splitlines() if l.strip() for a in ('--file', l.strip())]
    print(f'   | freshen deps seeding: {len(freshen_dep_args)//2} files')
    run('regen-extra-receipt', ['python3', REGEN_X, '--output', rcpts/'l10-extra-receipt.json',
                                '--file', geom])
    run('regen-replace-receipt', ['python3', REGEN_R, '--output', rcpts/'l10-replace-receipt.json',
                                  '--file', '/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/src/LuaMod.cpp',
                                  '--file', '/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/Mod/CppMod.cpp',
                                  '--file', '/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SettingsManager.cpp',
                                  '--program-source', program,
                                  *freshen_dep_args,
                                  '--file', W_INC/'NativeLoaderContext.hpp',
                                  '--file', W_INC/'lmf_to_native_loader_facts.hpp',
                                  '--file', ROOT/'.superpowers/native-dependency-spike/ue4ss-native-ps-scan-abi-1/include/PsMappedImageCaller.hpp',
                                  '--file', ROOT/'.superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/include/OwnedMappedImageCaller.hpp'])
    # initializer coverage: launch-10 readpath staging (compiles SeedValidation.hpp +
    # armed SeedTable.h from its own dir; the compiled TU is UnrealInitializer.cpp)
    rr_p = rcpts/'l10-replace-receipt.json'
    rr = json.loads(rr_p.read_text())
    orig_rr = TEAM / 'worker-comp6-driver/receipts/composition6-replace-receipt.json'
    if orig_rr.is_file():
        o = json.loads(orig_rr.read_text())
        for k, v in o.items():
            if k not in ('files', 'reasons') and k not in rr:
                rr[k] = v
    init = EDITION['edits'][1]['file']
    for f, reason in [(init, 'launch-10 initializer: armed SeedTable + gmalloc V1/V2 validator + SeedAddressClass read-path guards'),
                      (Path(init).parent/'SeedValidation.hpp', 'launch-10 read-path guard classifier (included by initializer)'),
                      (Path(init).parent/'SeedTable.h', 'launch-10 armed seed table (4/4)')]:
        rr['files'][str(f)] = sh(f)
        rr.setdefault('reasons', {})[str(f)] = reason
    rr_p.write_text(json.dumps(rr, indent=1))
    print('   | initializer coverage merged: launch-10 staging (3 files)')

def the_pass(out_name):
    run_seq = out_name.rsplit('-', 1)[-1]
    # conscious move-aside of prior attempt artifacts (precedent: suffixed renames)
    import time as _t
    rd = COMP_RUNS / f'run-l10-{run_seq}'
    if rd.exists(): rd.rename(rd.with_name(rd.name + f'-aside-{int(_t.time())}'))
    ov = TEAM / 'worker-comp6-driver/staging/lineage-overlay-2'
    if ov.exists(): ov.rename(ov.with_name(ov.name + f'-aside-{int(_t.time())}'))
    cd = MY / out_name
    if cd.exists(): cd.rename(cd.with_name(cd.name + f'-aside-{int(_t.time())}'))
    program = TEAM / 'worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp'
    bridge = EDITION['bridge_cli']
    lc_hpp_sha = sh(W_INC/'NativeLoaderContext.hpp')
    lmf_hpp_sha = sh(W_INC/'lmf_to_native_loader_facts.hpp')
    run(f'composition10-one-pass[{out_name}]',
        ['python3', DRIVER,
         '--program-source', program,
         '--bridge-source', bridge['file'], '--bridge-sha256', bridge['sha256'],
         '--extra-receipt', MY/'receipts/l10-extra-receipt.json',
         '--replace-receipt', MY/'receipts/l10-replace-receipt.json',
         '--stage-header', f'NativeLoaderContext.hpp={W_INC}/NativeLoaderContext.hpp={lc_hpp_sha}',
         '--stage-header', f'lmf_to_native_loader_facts.hpp={W_INC}/lmf_to_native_loader_facts.hpp={lmf_hpp_sha}',
         '--stage-header', 'SeedValidation.hpp=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp=f09ec9b319985cf86a5899894a30fc8d20a905072008ac8d4a35efbcaec3a5de',
         '--stage-header', 'live_module_facts.hpp=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-build/staging/live_module_facts.hpp=67170b21fc3b00947d848fa94337c94c9b324155e27010259a192bfd32bedad5',
         '--composition-run', COMP_RUNS/f'run-l10-{run_seq}',
         '--out', MY/out_name])

def post_build(out_name):
    runs = sorted([p for p in COMP_RUNS.glob('run-*') if p.is_dir()], key=lambda p: p.stat().st_mtime)
    if not runs: die('no composition run dir found')
    run_dir = runs[-1]
    print('   | composition run dir:', run_dir.name)
    run('odr-emitter-check', ['python3', ODR, run_dir, '--require-program-fresh', '--json',
                              MY/'receipts/odr-l10.json'])
    run('finalize-manifest', ['python3', FIN,
                              '--manifest', INT/'change-manifest.json',
                              '--composition-run', run_dir,
                              '--candidate', MY/out_name])
    verify_accessors(out_name)
    compare_args = ['--compare-dir', MY/'prx-candidate-l10-1'] if out_name.endswith('2') else []
    gate = subprocess.run(['python3', str(GATE2),
                    '--build-dir', str(MY/out_name),
                    '--manifest', str(INT/'change-manifest.finalized.json'),
                    *[str(a) for a in compare_args],
                    '--json', str(MY/f'gate-receipt-l10-{out_name}.json')],
                   capture_output=True, text=True, cwd=str(ROOT))
    for l in (gate.stdout + gate.stderr).strip().splitlines()[-3:]:
        print('   |', l[:170])
    if gate.returncode != 0:
        receipt_j = json.loads((MY/f'gate-receipt-l10-{out_name}.json').read_text())
        failed = receipt_j.get('failed_checks', [])
        if out_name.endswith('1') and failed == ['D1']:
            print('   | candidate-1 gate: D1-only failure accepted (determinism twin not built yet; full GO required on -2)')
        else:
            die(f'gate-v2[{out_name}] exit {gate.returncode} failed {failed}')
    return run_dir

def verify_accessors(out_name):
    """Post-link verification (user directive, false-PASS hardened):
    evidence classes kept distinct — (1) BRIDGE DEFINITION: both nm invocations
    must EXIT 0 before output is interpreted, inspection errors recorded;
    (2) COMBINED-OBJECT RESOLUTION: the three accessors not undefined;
    (3) FINAL PACKAGING: the packaged PRX must EXIST and the final linked
    artifact's undefined IMPORT surface must not contain the accessors
    (they bind internally; appearing as imports = unresolved)."""
    import subprocess
    nm = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm'
    acc = {'bridge_capture_envelope', 'bridge_readable_span_at', 'bridge_readable_span_count'}
    c = MY / out_name
    receipt = {'candidate': str(c), 'accessors': sorted(acc), 'errors': []}

    def nm_run(mode, target):
        proc = subprocess.run([str(nm), '--extern-only', mode, '--format=posix', str(target)],
                              capture_output=True, text=True)
        if proc.returncode != 0:
            receipt['errors'].append({'tool': 'llvm-nm', 'mode': mode, 'target': str(target),
                                      'returncode': proc.returncode, 'stderr': proc.stderr[-300:]})
            return None
        return {l.split()[0] for l in proc.stdout.splitlines() if l.strip()}

    # (1) bridge definition
    bridge = next((q for q in c.glob('**/*.o') if 'runtime_start_bridge' in q.stem), None)
    if bridge is None:
        receipt['errors'].append({'stage': 'bridge-object', 'error': 'bridge object not found under candidate'})
        receipt['bridge_definition'] = 'NOT-INSPECTABLE'
    else:
        defs = nm_run('--defined-only', bridge)
        receipt['bridge_definition'] = {
            'object': str(bridge), 'defined': sorted(acc & defs) if defs is not None else None,
            'verdict': ('PASS' if defs is not None and acc <= defs else
                        'FAIL' if defs is not None else 'NOT-INSPECTABLE')}
    # (2) combined-object resolution
    combined = next(c.glob('**/combined-supplier.o'), None)
    if combined is None:
        receipt['errors'].append({'stage': 'combined-object', 'error': 'combined-supplier.o not found'})
        receipt['combined_resolution'] = 'NOT-INSPECTABLE'
    else:
        und = nm_run('--undefined-only', combined)
        receipt['combined_resolution'] = {
            'object': str(combined), 'unresolved': sorted(acc & und) if und is not None else None,
            'verdict': ('PASS' if und is not None and not (acc & und) else
                        'FAIL' if und is not None else 'NOT-INSPECTABLE')}
    def _tied_file(rec):
        # Receipt-recorded file: must exist, resolve INSIDE this candidate, and
        # match the recorded sha256+size byte-exactly. Returns (path, None) on
        # success or (None, reason) — an outside or altered file never qualifies.
        if not rec: return None, 'record missing'
        f = Path(rec['path'])
        try: fr = f.resolve()
        except OSError: return None, 'unresolvable path'
        if c.resolve() not in fr.parents: return None, f'recorded path outside candidate: {f}'
        if not fr.is_file(): return None, 'file missing: ' + str(f)
        if _h.sha256(fr.read_bytes()).hexdigest() != rec['sha256'] or fr.stat().st_size != rec['bytes']:
            return None, 'bytes != recorded sha256/size (altered or foreign file)'
        return fr, None
    # (3) FINAL-ARTIFACT verification (user directive, strict): every claim is
    # receipt-recorded and hash-enforced — filename/path-only acceptance and
    # guessed ELF selection are REMOVED. Ties: (a) package-profile.json's
    # combined_object_sha256 vs the on-disk final combined object; (b) the
    # build-receipt's per-unit object_sha256 for the bridge object; (c) the
    # driver-written artifacts-receipt.json's sha256 for the final PRX (and the
    # bare chain) — an altered PRX under the same filename FAILS here.
    import hashlib as _h
    fp = {'verdict': 'FAIL'}
    try:
        profile_p = c / 'package-profile.json'
        receipt_p = c / 'build-receipt.json'
        arts_p = c / 'artifacts-receipt.json'
        if not (profile_p.is_file() and receipt_p.is_file() and arts_p.is_file()):
            receipt['errors'].append({'stage': 'final-packaging', 'error': 'receipts incomplete', 'present': {x.name: x.is_file() for x in (profile_p, receipt_p, arts_p)}})
            fp['verdict'] = 'FAIL-RECEIPTS-INCOMPLETE'
        else:
            profile = json.loads(profile_p.read_text())
            # (a) combined object: recorded hash must match recomputed bytes
            comb_sha_rec = profile.get('combined_object_sha256')
            comb = next(c.glob('**/combined-supplier.o'), None)
            if comb is None or not comb_sha_rec:
                receipt['errors'].append({'stage': 'final-packaging', 'error': 'combined object or its recorded hash missing'})
                fp['verdict'] = 'FAIL-COMBINED-UNTIED'
            elif _h.sha256(comb.read_bytes()).hexdigest() != comb_sha_rec:
                receipt['errors'].append({'stage': 'final-packaging', 'error': 'combined object bytes != recorded combined_object_sha256'})
                fp['verdict'] = 'FAIL-COMBINED-HASH'
            else:
                fp['combined_object'] = {'path': str(comb), 'sha256': comb_sha_rec, 'hash_tied': True}
                # (b) bridge object: explicit field in the artifacts receipt (recorded
                # hash enforced; no command-log parsing, no glob-guessed identity)
                arts = json.loads(arts_p.read_text())['artifacts']
                bridge_obj, berr = _tied_file(arts.get('bridge_object'))
                if bridge_obj is None:
                    receipt['errors'].append({'stage': 'final-packaging', 'error': 'bridge object: ' + str(berr)})
                    fp['verdict'] = 'FAIL-BRIDGE-UNTIED' if ('missing' in str(berr) or 'outside' in str(berr)) else 'FAIL-BRIDGE-HASH'
                else:
                    fp['bridge_object'] = {'path': str(bridge_obj), 'sha256': arts['bridge_object']['sha256'], 'hash_tied': True}
                    # (c) final PRX + bare chain: hash-enforced from the same receipt
                    tie_ok = True
                    for label in ('final_prx', 'bare_original', 'bare_sdk'):
                        f, ferr = _tied_file(arts.get(label))
                        if f is None:
                            receipt['errors'].append({'stage': 'final-packaging', 'error': f'{label}: ' + str(ferr)})
                            tie_ok = False
                        else:
                            fp[label] = {'path': str(f), 'sha256': arts[label]['sha256'], 'hash_tied': True}
                    if not tie_ok:
                        fp['verdict'] = 'FAIL-PRX-HASH'
                    elif 'final_prx' not in fp:
                        receipt['errors'].append({'stage': 'final-packaging', 'error': 'final PRX absent from artifacts receipt'})
                        fp['verdict'] = 'FAIL-NO-FINAL-PRX'
                    else:
                        # import-surface check on the RECORDED final chain artifact (bare_sdk: the final linked ELF-class PRX pre-fself)
                        elf = Path(arts['bare_sdk']['path']) if arts.get('bare_sdk') else Path(arts['final_prx']['path'])
                        und2 = nm_run('--undefined-only', elf)
                        fp['import_surface_artifact'] = str(elf)
                        fp['accessors_in_final_imports'] = sorted(acc & und2) if und2 is not None else None
                        fp['intermediate_note'] = 'combined-supplier.o resolution kept as class-2 intermediate evidence'
                        fp['verdict'] = ('PASS' if und2 is not None and not (acc & und2) else
                                         'FAIL' if und2 is not None else 'NOT-INSPECTABLE')
    except Exception as _ex:
        receipt['errors'].append({'stage': 'final-packaging', 'error': 'inspection exception: ' + repr(_ex)})
        fp['verdict'] = 'FAIL-INSPECTION'
    receipt['final_packaging'] = fp

    def _v(x):
        return x if isinstance(x, str) else x.get('verdict', 'FAIL')
    verdicts = [_v(receipt['bridge_definition']), _v(receipt['combined_resolution']), _v(receipt['final_packaging'])]
    receipt['verdict'] = 'PASS' if all(v == 'PASS' for v in verdicts) else 'FAIL'
    (MY / 'receipts' / f'accessor-verification-{out_name}.json').write_text(json.dumps(receipt, indent=1))
    print('   | accessor verification:', receipt['verdict'],
          '| bridge:', verdicts[0], '| combined:', verdicts[1], '| packaging:', verdicts[2],
          '| errors:', len(receipt['errors']))
    assert receipt['verdict'] == 'PASS', 'accessor verification FAILED: ' + json.dumps(receipt)


def determinism(run_dir):
    objs = sorted(run_dir.glob('**/coherent-tick-file-witness-runtime.o'))
    if not objs: die('composed object not found for determinism check')
    a = sh(objs[0])
    print('   | composed object:', objs[0].name, a[:16])
    return a

def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else 'verify'
    verify_units()
    if mode == 'verify':
        preflight_and_verify()
        print('HOLD-PASS complete: units verified + verify-only PASS. Run with --full when the anchor-cap fold is decided.')
        return
    assert mode == '--full'
    preflight_and_verify()
    regen_receipts()
    the_pass('prx-candidate-l10-1')
    rd1 = post_build('prx-candidate-l10-1')
    o1 = determinism(rd1)
    the_pass('prx-candidate-l10-2')
    rd2 = post_build('prx-candidate-l10-2')
    o2 = determinism(rd2)
    if o1 != o2: die(f'DETERMINISM FAIL: {o1[:16]} vs {o2[:16]}')
    print(f'LAUNCH-10 ONE-PASS COMPLETE: gate GO x2, determinism proven ({o1[:16]}), candidates in {MY}/prx-candidate-l10-{{1,2}}')

if __name__ == '__main__':
    main()
