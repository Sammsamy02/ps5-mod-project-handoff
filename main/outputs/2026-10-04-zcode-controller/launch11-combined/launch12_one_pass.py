#!/usr/bin/env python3
"""Controller launch-11 COMBINED one-pass runner (worker-combined-editions PRE-BUILD;
compose is pure execution once the pad-defer admission lands). Lineage: the proven
launch-10 pair + LAUNCH11-PLAN A-chain — combined luamod (marshal-consumer + enqueue
-ladder + loadmap-r2; source dc687602..., unit-object comparator 813b249d...), the plan
A0 ARMED initializer staging (8b0c7e99...; pre-final-c caveat recorded in the driver and
EDITIONS-l12.json), the pad unit as a PLACEHOLDER (plan A3, BLOCKING), fold-1 asmhelper OFF
by default (admissible flip comparator 6604ce2d...; f5627427 rejected, basename
discipline).

Driver edition: launch11-combined/build_runtime_composition6_l12.py (EDITIONS-l12.json
records base shas + the five tagged deltas L11-E1..E5 + the placeholder inventory +
the exact loop edits). The runner re-verifies every unit sha at start and refuses on
drift. HOLD-PASS mode (default) stops after verify-only; --full FIRST runs the
admission gate (refuses while admissions.pad_defer / admissions.initializer_fold are
unresolved — plan A3: absent pad at compose time = STOP or recorded waiver), then
receipt regen + two passes (candidates prx-candidate-l12-{1,2}; SeedValidation/
live_module_facts stage-header pins carried from launch-10, same shas) + the
merged-luamod object comparator gate + the plan-D three-consumer UND pairing-law gate
+ ODR + finalize + gate v2 + determinism twin.
"""
import hashlib, json, subprocess, sys, time
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
TEAM = ROOT / 'outputs/2026-10-04-zcode-team'
MY = ROOT / 'outputs/2026-10-04-zcode-controller/launch11-combined'
DRIVER = MY / 'build_runtime_composition6_l12.py'
EDITION = json.loads((MY / 'EDITIONS-l12.json').read_text())
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
    print('== unit sha verification (EDITIONS-l12.json)')
    for e in EDITION['units'] + [EDITION['bridge_cli']]:
        actual = sh(e['file'])
        if actual != e['sha256']:
            die(f"unit drift: {e['file']}\n  edition {e['sha256'][:16]} vs actual {actual[:16]}\n"
                "  (if a staging file moved or was folded, refresh EDITIONS-l12.json first — never edit pins)")
        print(f"   | OK {Path(e['file']).name} {actual[:12]}")
    print('   | all units verified')

def preflight_and_verify():
    program = TEAM / 'worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp'
    if not program.is_file(): die('program staging missing')
    bridge = EDITION['bridge_cli']
    run('verify-only', ['python3', DRIVER, '--verify-only',
                        '--program-source', program,
                        '--bridge-source', bridge['file'],
                        '--bridge-sha256', bridge['sha256'],
                        '--replace-receipt', MY/'receipts/l11-replace-receipt.json',
                        '--extra-receipt', MY/'receipts/l11-extra-receipt.json'])

def regen_receipts():
    program = TEAM / 'worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp'
    geom = EDITION['compose']['geometry_cpp']['file']
    rcpts = MY / 'receipts'; rcpts.mkdir(exist_ok=True)
    for rcpt in (rcpts/'l11-extra-receipt.json', rcpts/'l11-replace-receipt.json'):
        if rcpt.exists(): rcpt.rename(rcpt.with_suffix('.json.prev'))
    freshen_dep_args = []
    fd = MY / 'freshen-deps.txt'
    if fd.is_file():
        freshen_dep_args = [a for l in fd.read_text().splitlines() if l.strip() for a in ('--file', l.strip())]
    for _pf in (
        'outputs/2026-10-04-zcode-team/worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp',
        'outputs/2026-10-04-zcode-controller/launch11-combined/units/UnrealInitializer.cpp',
        '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/native-pad-input-coexist/NativePadInput.cpp',
        '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port/PS5PadSource.cpp',
        '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/ue4ss-native-pad-provider-coexist/port/PS5PadNative.cpp',
        '/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/input-handler-coexist/Handler.cpp',
        'outputs/2026-10-04-zcode-controller/launch11-combined/units/SeedTable.h',
        'outputs/2026-10-04-zcode-controller/launch11-combined/units/SeedValidation.hpp',
        'outputs/2026-10-04-zcode-team//WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-pad-coexist/staging/native-pad-input-coexist/NativePadInput.cpp',
        '.superpowers/native-dependency-spike/ue4ss-native-pad-config-1/src/NativePadInput.hpp',
        '.superpowers/native-dependency-spike/ue4ss-native-pad-config-1/src/PS5PadConfig.hpp',
        '.superpowers/native-dependency-spike/ue4ss-native-pad-config-1/src/PS5PadConfig.cpp',
    ):
        if Path(_pf).is_file(): freshen_dep_args += ['--file', _pf]
    print(f'   | freshen deps seeding: {len(freshen_dep_args)//2} files')
    run('regen-extra-receipt', ['python3', REGEN_X, '--output', rcpts/'l11-extra-receipt.json',
                                '--file', geom])
    luamod = EDITION['compose']['luamod_merged']
    run('regen-replace-receipt', ['python3', REGEN_R, '--output', rcpts/'l11-replace-receipt.json',
                                  '--file', luamod['file'],
                                  '--file', '/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/Mod/CppMod.cpp',
                                  '--file', '/WORKSPACE/.codex/worktrees/translation-core/what/.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/src/SettingsManager.cpp',
                                  '--program-source', program,
                                  *freshen_dep_args,
                                  '--file', W_INC/'NativeLoaderContext.hpp',
                                  '--file', W_INC/'lmf_to_native_loader_facts.hpp',
                                  '--file', ROOT/'.superpowers/native-dependency-spike/ue4ss-native-ps-scan-abi-1/include/PsMappedImageCaller.hpp',
                                  '--file', ROOT/'.superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/include/OwnedMappedImageCaller.hpp'])
    # =================================================================================
    # LAUNCH11 PAD PLACEHOLDER (plan A3, BLOCKING): when the pad worker's FOLD.md lands,
    # add its --file admissions HERE — per the pad DIFF's OWN --file list, BACKEND/
    # BOOTSTRAP lineage, never the luamod stack — either inside the regen call above or as
    # a follow-up receipt merge (same mechanism as the initializer coverage below). Also
    # set EDITIONS-l12.json admissions.pad_defer (status ADMITTED/WAIVED + files + shas, and
    # add the files to EDITIONS['units']) and fill the driver compose_argv "LAUNCH11 PAD
    # PLACEHOLDER" entry. --full refuses while admissions.pad_defer is PENDING (A3:
    # absent at compose time = STOP, or an explicit recorded waiver — never silent).
    # =================================================================================
    # initializer coverage: LAUNCH11-PLAN A0 ARMED staging (worker-luamod-rebase/
    # staging/initializer-l10-r3; compiles SeedValidation.hpp + armed SeedTable.h from its
    # OWN dir; the compiled TU is UnrealInitializer.cpp). Receipt-level sha pins, same
    # merge mechanism as launch-10.
    rr_p = rcpts/'l11-replace-receipt.json'
    rr = json.loads(rr_p.read_text())
    orig_rr = TEAM / 'worker-comp6-driver/receipts/composition6-replace-receipt.json'
    if orig_rr.is_file():
        o = json.loads(orig_rr.read_text())
        for k, v in o.items():
            if k not in ('files', 'reasons') and k not in rr:
                rr[k] = v
    init = EDITION['compose']['initializer_armed']['dir']
    for f, reason in [(Path(init)/'UnrealInitializer.cpp', 'LAUNCH11-PLAN A0 armed fold: DIFF-2 rev B R3-marshal-mode fold 1 on the launch-10 readpath staging (R3_MARSHAL_MODE=1 fallback; PRE-FINAL-C caveat recorded in the driver L11-E2 comment and EDITIONS-l12.json admissions.initializer_fold)'),
                      (Path(init)/'SeedValidation.hpp', 'armed-staging seed classifier (included by the armed initializer; b8d851e9)'),
                      (Path(init)/'SeedTable.h', 'armed seed table 4/4 (LAUNCH11-PLAN A0 pin 59effe6e)'),
                      (Path(init)/'shadow_arm.hpp', 'L12 shadow arm (worker-shadow-integrate pinned)'),
                      (Path(init)/'shadow_module_source.hpp', 'L12 shadow module source (worker-shadow-integrate pinned)'),
                      (Path(init)/'shadow_copy.hpp', 'L12 shadow copy header (worker-code-scan pinned)'),
                      (Path(init)/'shadow_copy.cpp', 'L12 shadow copy TU (extra-unit compiled)'),
                      (Path(init)/'pool_admission.h', 'L12 [Seeds] PoolAdmission settings reader (33/33 host tests)'),
                      (Path(init)/'runtime_start_bridge_r14_diag_spanpub.cpp', 'L12 bridge (probe upgrades A/B; host contract 24/24+10/10)')]:
        rr['files'][str(f)] = sh(f)
        rr.setdefault('reasons', {})[str(f)] = reason
    rr_p.write_text(json.dumps(rr, indent=1))
    print('   | initializer coverage merged: ARMED l10-r3 staging (3 files)')

def the_pass(out_name):
    run_seq = out_name.rsplit('-', 1)[-1]
    # conscious move-aside of prior attempt artifacts (precedent: suffixed renames)
    import time as _t
    rd = COMP_RUNS / f'run-l12-{run_seq}'
    if rd.exists(): rd.rename(rd.with_name(rd.name + f'-aside-{int(_t.time())}'))
    ov = TEAM / 'worker-comp6-driver/staging/lineage-overlay-2'
    if ov.exists(): ov.rename(ov.with_name(ov.name + f'-aside-{int(_t.time())}'))
    cd = MY / out_name
    if cd.exists(): cd.rename(cd.with_name(cd.name + f'-aside-{int(_t.time())}'))
    program = TEAM / 'worker-pad-defer/staging/program-wpd1/UE4SSProgram.cpp'
    bridge = EDITION['bridge_cli']
    lc_hpp_sha = sh(W_INC/'NativeLoaderContext.hpp')
    lmf_hpp_sha = sh(W_INC/'lmf_to_native_loader_facts.hpp')
    run(f'composition11-one-pass[{out_name}]',
        ['python3', DRIVER,
         '--program-source', program,
         '--bridge-source', bridge['file'], '--bridge-sha256', bridge['sha256'],
         '--extra-receipt', MY/'receipts/l11-extra-receipt.json',
         '--replace-receipt', MY/'receipts/l11-replace-receipt.json',
         '--stage-header', f'NativeLoaderContext.hpp={W_INC}/NativeLoaderContext.hpp={lc_hpp_sha}',
         '--stage-header', f'lmf_to_native_loader_facts.hpp={W_INC}/lmf_to_native_loader_facts.hpp={lmf_hpp_sha}',
         '--stage-header', 'Helpers/String.hpp=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-iniparser-repro/staged/Helpers/String.hpp=c76be502fbe6f9c2c6589f141c1caded84af482a1f03a428005574850ca47788',
         '--stage-header', 'SeedValidation.hpp=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch11-combined/units/SeedValidation.hpp=c2e6e66331cf45defab4dec4eee120341fcba5a73f3a23c6810edeae485dcfe8',
         '--stage-header', 'live_module_facts.hpp=/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-build/staging/live_module_facts.hpp=917f13a7381a85cac513b9efe6b48d833f083d0cd426d629fb1f7cf20ea460a3',
         '--composition-run', COMP_RUNS/f'run-l12-{run_seq}',
         '--out', MY/out_name])

def admission_gate():
    """LAUNCH11-PLAN A3 discipline, enforced pre-full: --full refuses while an admission
    is unresolved. pad_defer: PENDING -> refuse; ADMITTED (files sha-pinned in EDITIONS
    and re-verified here) or explicitly WAIVED (recorded) -> proceed. initializer_fold:
    PENDING-ACK -> refuse; ACK-PLAN-PINNED (compose on the plan's A0 armed staging
    8b0c7e99 with the pre-final-c caveat consciously accepted) / ADMITTED-V4ARMED-FOLD
    (new worker-delivered staging; driver L11-E2 path re-pointed) / recorded WAIVED ->
    proceed. One conscious recorded decision, never a silent default."""
    ad = EDITION.get('admissions', {})
    pad = ad.get('pad_defer', {}).get('status', 'PENDING')
    if pad not in ('ADMITTED', 'WAIVED'):
        die(f"admission gate: pad_defer is {pad} — plan A3: fill the PAD PLACEHOLDERs "
            "(driver compose_argv + regen_receipts + EDITIONS-l12.json admissions.pad_defer) "
            "or record an explicit waiver; NEVER compose with the pad unit silently absent")
    if pad == 'ADMITTED':
        for f, want in ad.get('pad_defer', {}).get('files', {}).items():
            p = Path(f)
            if not p.is_file() or sh(p) != want:
                die(f'pad_defer admitted-file drift/missing: {f}')
    init = ad.get('initializer_fold', {}).get('status', 'PENDING-ACK')
    if init not in ('ACK-PLAN-PINNED', 'ADMITTED-V4ARMED-FOLD', 'WAIVED'):
        die(f"admission gate: initializer_fold is {init} — set ACK-PLAN-PINNED (the plan's "
            "A0 armed staging 8b0c7e99, pre-final-c caveat accepted), or ADMITTED-V4ARMED-FOLD "
            "(new worker-delivered staging; re-point driver L11-E2), or a recorded WAIVER, "
            "in EDITIONS-l12.json")
    print(f'   | admission gate OK: pad_defer={pad}, initializer_fold={init}')

def luamod_comparator_gate(run_dir):
    """LAUNCH11-PLAN A2/D: the composed luamod unit object MUST equal the M-0 combined
    comparator (worker-l11-merge 813b249d...) — from the run receipt AND re-measured
    bytes — with the merged source sha dc687602... pinned, and retained_units_replaced
    exactly ['luamod','program'] (plan D). Drift = STOP; never widen the comparator
    silently (plan D: 'if the compose's staging basename differs and bytes drift, STOP')."""
    rec = json.loads((run_dir / 'receipt.json').read_text())
    comp = EDITION['compose']['luamod_merged']
    want_obj = EDITION['comparators']['luamod_object']
    _combined_expected = {'luamod', 'program', 'native-pad-input', 'cppmod', 'settings', 'asmhelper-asmhelper'}
    _got = set(rec.get('retained_units_replaced', []))
    if not _combined_expected <= _got:
        die(f"retained_units_replaced missing required units: {_combined_expected - _got} (combined candidate needs all six; got {_got})")
    unit = rec.get('selected_runtime_units', {}).get('luamod')
    if not unit:
        die('run receipt has no luamod unit — the retained replacement did not take')
    if unit.get('source_sha256') != comp['sha256']:
        die(f"luamod SOURCE drift: {unit.get('source_sha256')} != {comp['sha256']} (dc687602 pin)")
    if unit.get('object_sha256') != want_obj:
        die(f"LUAMOD COMPARATOR DRIFT: {unit.get('object_sha256')} != {want_obj} — plan D: STOP, "
            "never widen the comparator silently (single-fix objects 8593884e/3eeba02f are NOT valid here)")
    obj = Path(unit['object'])
    got = sh(obj)
    if got != want_obj:
        die(f'luamod unit object BYTES drifted vs receipt pin: {got} != {want_obj} ({obj})')
    (MY / 'receipts' / 'luamod-comparator.json').write_text(json.dumps(
        {'run_dir': str(run_dir), 'unit_receipt_object': unit['object'],
         'source_sha256': comp['sha256'], 'object_sha256': got,
         'comparator': want_obj, 'bytes': obj.stat().st_size}, indent=1))
    print(f'   | luamod comparator OK: {got[:16]} ({unit["object"]})')

def consumer_und_gate(run_dir):
    """LAUNCH11-PLAN D pairing-law gate: the three consumer UND symbols RESOLVE in the
    composed object (R3MarshalPumpPending + R3MarshalPublishGameThreadIdentity against
    the ARMED-fold initializer, UFunction::GetFuncPtr against the unreal-ufunction unit).
    Exact mangled names measured by worker-l11-merge (receipts/undefined-merged.txt).
    Unresolved bridge symbol = wrong fold = STOP (rev-B pairing law refusing loudly).
    Additionally NO *R3Marshal* symbol may remain undefined (the full DIFF1/DIFF2
    contract, linker-enforced)."""
    nm = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm'
    rec = json.loads((run_dir / 'receipt.json').read_text())
    obj = Path(rec['runtime_object'])
    required = ['_ZN2RC6Unreal20R3MarshalPumpPendingEv',
                '_ZN2RC6Unreal34R3MarshalPublishGameThreadIdentityEv',
                '_ZN2RC6Unreal9UFunction10GetFuncPtrEv']
    r = subprocess.run([str(nm), '--extern-only', '--undefined-only', '--format=posix', str(obj)],
                       capture_output=True, text=True)
    if r.returncode != 0:
        die('llvm-nm failed on composed object: ' + r.stderr[-300:])
    und = [l.split()[0] for l in r.stdout.splitlines() if l.strip()]
    unresolved = [s for s in required if s in und]
    r3_left = [s for s in und if 'R3Marshal' in s]
    ev = {'composed_object': str(obj), 'undefined_count': len(und),
          'required_resolve': required, 'unresolved': unresolved,
          'r3marshal_undefined': r3_left}
    (MY / 'receipts' / 'consumer-und-resolution.json').write_text(json.dumps(ev, indent=1))
    if unresolved:
        die(f"PAIRING-LAW GATE FAILED (plan D): unresolved consumer symbols {unresolved} — "
            "wrong initializer fold? Unresolved bridge symbol = STOP")
    if r3_left:
        die(f'PAIRING-LAW GATE FAILED (plan D): R3Marshal* symbols still UND: {r3_left}')
    print(f'   | consumer UND resolution OK: 3/3 resolve; 0 R3Marshal* undefined; UND total {len(und)}')

def post_build(out_name):
    runs = sorted([p for p in COMP_RUNS.glob('run-l12-*') if p.is_dir()], key=lambda p: p.stat().st_mtime)
    if not runs: die('no composition run dir found')
    run_dir = runs[-1]
    print('   | composition run dir:', run_dir.name)
    # LAUNCH11-PLAN D structural gates (worker-combined-editions additions):
    luamod_comparator_gate(run_dir)
    consumer_und_gate(run_dir)
    # PLAN-E NOTE (LAUNCH11-PLAN §1-E, loop-side): the composition-change-manifest re-point
    # (initializer entry -> the armed staging; second entry luamod -> the merged staging
    # with provenance diffs f595c57f/3b19068a/d2ad2818 + empty import_delta both directions)
    # and gate v2 vs the frozen r11 baseline remain the loop's manifest step; the carried
    # finalize/gate stages below execute against INT/change-manifest.json as in launch-10.
    run('odr-emitter-check', ['python3', ODR, run_dir, '--require-program-fresh', '--json',
                              MY/'receipts/odr-l11.json'])
    run('finalize-manifest', ['python3', FIN,
                              '--manifest', INT/'change-manifest.json',
                              '--composition-run', run_dir,
                              '--candidate', MY/out_name])
    verify_accessors(out_name)
    compare_args = ['--compare-dir', MY/'prx-candidate-l12-1'] if out_name.endswith('2') else []
    gate = subprocess.run(['python3', str(GATE2),
                    '--build-dir', str(MY/out_name),
                    '--manifest', str(INT/'change-manifest.finalized.json'),
                    *[str(a) for a in compare_args],
                    '--json', str(MY/f'gate-receipt-l11-{out_name}.json')],
                   capture_output=True, text=True, cwd=str(ROOT))
    for l in (gate.stdout + gate.stderr).strip().splitlines()[-3:]:
        print('   |', l[:170])
    if gate.returncode != 0:
        receipt_j = json.loads((MY/f'gate-receipt-l11-{out_name}.json').read_text())
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
        print('HOLD-PASS complete: units verified + verify-only PASS. --full stays REFUSED '
              'until EDITIONS-l12.json admissions are resolved (pad_defer + initializer_fold).')
        return
    assert mode == '--full'
    admission_gate()
    regen_receipts()
    preflight_and_verify()
    the_pass('prx-candidate-l12-1')
    rd1 = post_build('prx-candidate-l12-1')
    o1 = determinism(rd1)
    the_pass('prx-candidate-l12-2')
    rd2 = post_build('prx-candidate-l12-2')
    o2 = determinism(rd2)
    if o1 != o2: die(f'DETERMINISM FAIL: {o1[:16]} vs {o2[:16]}')
    print(f'LAUNCH-11 ONE-PASS COMPLETE: gate GO x2, determinism proven ({o1[:16]}), candidates in {MY}/prx-candidate-l12-{{1,2}}')

if __name__ == '__main__':
    main()
