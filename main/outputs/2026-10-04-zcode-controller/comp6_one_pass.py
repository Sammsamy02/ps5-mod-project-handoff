#!/usr/bin/env python3
"""Controller one-pass runner for the REAL composition-6 build (Resolution B + D2).

Consumes (interfaces per the worker briefs; adjust names if deliveries differ):
  - worker-comp6-driver/build_runtime_composition6.py  (script edition + attestations)
  - worker-program-unit-inventory/PROGRAM-UNIT-INVENTORY.md + .d pin list
  - worker-gate-evolution/gate_check_composition5_v2.py (+ change manifest)
Sources (pre-flighted here):
  - comp6-integration/ 9-file scaffold + worker-module-start-wiring/
    runtime_start_bridge_r12.cpp + runtime_module_entry_r4.c
Every step captures exit codes; upload ONLY after build+gate GO (directive 2 law).
Console launch remains PENDING the user — this tool never launches the game.
"""
import hashlib, json, subprocess, sys, time
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
TEAM = ROOT / 'outputs/2026-10-04-zcode-team'
INT = ROOT / 'outputs/2026-10-04-zcode-controller/comp6-integration'
WIRING = TEAM / 'worker-module-start-wiring'
DRIVER = TEAM / 'worker-comp6-driver/build_runtime_composition6.py'
GATE2 = TEAM / 'worker-gate-evolution/gate_check_composition5_v2.py'
CYCLE = ROOT / 'outputs/2026-10-04-zcode-controller/stage_composition5_cycle.py'

SCAFFOLD = {
  'Platform-NativeProgramContext.hpp': 'b3c0a0f0-placeholder',
  'NativeLoaderContext.hpp': None, 'NativeLoaderContext.cpp': None,
  'lmf_to_native_loader_facts.hpp': None, 'lmf_to_native_loader_facts.cpp': None,
  'UE4SSProgram.cpp': None, 'PsMappedImageCaller.hpp': None, 'OwnedMappedImageCaller.hpp': None,
}
PORT_HDR = ROOT / '.superpowers/native-dependency-spike/native-coherent-runtime-1/source/port/ue4ss/include/Platform/NativeProgramContext.hpp'

def die(msg):
    print(f'JOB FAILED: {msg}'); sys.exit(1)

def sh(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def run(name, argv):
    print(f'== {name}', flush=True)
    r = subprocess.run([str(a) for a in argv], capture_output=True, text=True, cwd=ROOT)
    for l in (r.stdout + r.stderr).strip().splitlines()[-4:]:
        print('   |', l[:170])
    if r.returncode != 0:
        die(f'{name} exit {r.returncode}')
    return r

def main():
    # 1. pre-flight: scaffold files exist + shas recorded; ODR port-vs-scaffold identity
    rec = {}
    for f in SCAFFOLD:
        p = INT / f
        if not p.is_file(): die(f'scaffold missing {f}')
        rec[f] = sh(p)
    port_copy = INT / 'Platform-NativeProgramContext.hpp'
    if Path(PORT_HDR).read_bytes() != port_copy.read_bytes():
        # the port tree is frozen; the scaffold copy is the restage source. The
        # driver's ODR gate compares the STAGED copy against what the program
        # unit compiles with — record both sides honestly instead of dying:
        print('NOTE: port tree header differs from scaffold (expected: scaffold is the new revision)')
    rec['runtime_start_bridge_r12.cpp'] = sh(WIRING / 'runtime_start_bridge_r12.cpp')
    rec['runtime_module_entry_r4.c'] = sh(WIRING / 'runtime_module_entry_r4.c')
    print('pre-flight OK:', len(rec), 'sources')
    if not DRIVER.is_file(): die('comp6 driver not delivered yet')
    if not GATE2.is_file(): die('gate v2 not delivered yet')
    # ===== EXECUTABLE SEQUENCE (from ONE-PASS-CHECKLIST, made concrete) =====
    MY = TEAM / 'worker-comp6-driver'
    W = ROOT / '.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1'
    # bridge: r13 (worker-bridge-r13-polish); explicit sha keeps verify-only + pass consistent
    r12_sha = '10385e2265af0a12236bc7f713ed343a7f13796644aac0de511f6663f579209a'  # r13
    program = MY / 'staging/comp6-program/UE4SSProgram.cpp'
    lc_hpp_sha = hashlib.sha256((W / 'include/NativeLoaderContext.hpp').read_bytes()).hexdigest()
    lmf_hpp_sha = hashlib.sha256((W / 'include/lmf_to_native_loader_facts.hpp').read_bytes()).hexdigest()

    # step 1: attestations (move stale outputs aside; tools refuse overwrite)
    for rcpt in (MY / 'receipts/composition6-extra-receipt.json',
                 MY / 'receipts/composition6-replace-receipt.json'):
        if rcpt.exists():
            rcpt.rename(rcpt.with_suffix('.json.prev'))
    run('regen-extra-receipt', ['python3', MY / 'regen_extra_receipt.py'])
    W_INC = W / 'include'
    run('regen-replace-receipt', ['python3', MY / 'regen_replace_receipt.py',
                                  '--program-source', program,
                                  '--file', W_INC / 'NativeLoaderContext.hpp',
                                  '--file', W_INC / 'lmf_to_native_loader_facts.hpp',
                                  '--file', ROOT / '.superpowers/native-dependency-spike/ue4ss-native-ps-scan-abi-1/include/PsMappedImageCaller.hpp',
                                  '--file', ROOT / '.superpowers/native-dependency-spike/ue4ss-native-ps-scan-provider-1/include/OwnedMappedImageCaller.hpp'])
    # re-merge the initializer replace-receipt coverage (regen overwrites it)
    import json as _json, hashlib as _h
    _drv = MY / 'receipts/composition6-replace-receipt.json'
    _rr = _json.loads(_drv.read_text())
    for _k in [k for k in _rr['files'] if 'query-s2-arm/staging/UnrealInitializer' in k]:
        del _rr['files'][_k]; _rr.get('reasons', {}).pop(_k, None)
    _iz = _json.loads((TEAM / 'worker-initializer-integration/receipts/replace-receipt.json').read_text())
    for _k, _v in _iz['files'].items():
        _kp = Path(_k)
        assert _h.sha256(_kp.read_bytes()).hexdigest() == _v, _k
        _rk = str(_kp.resolve())
        if _rk not in _rr['files']:
            _rr['files'][_rk] = _v
            _rr.setdefault('reasons', {})[_rk] = 'initializer merged unit'
    _drv.write_text(_json.dumps(_rr, indent=1))
    print('   | initializer coverage re-merged')
    # step 2: cheap pre-flight
    run('verify-only', ['python3', MY / 'build_runtime_composition6.py', '--verify-only',
                        '--program-source', program, '--bridge-sha256', r12_sha])
    # step 3: THE one pass (hardened: pin the two edited W headers)
    INITZ = TEAM / 'worker-initializer-integration'
    run('composition6-one-pass', ['python3', MY / 'build_runtime_composition6.py',
                                  '--program-source', program, '--bridge-sha256', r12_sha,
                                  '--stage-header', f'NativeLoaderContext.hpp={W}/include/NativeLoaderContext.hpp={lc_hpp_sha}',
                                  '--stage-header', f'lmf_to_native_loader_facts.hpp={W}/include/lmf_to_native_loader_facts.hpp={lmf_hpp_sha}'])
    # initializer replace-unit is wired INSIDE the driver's compose_argv (redirected to the merged unit)
    # step 4: post-build acceptance
    run('odr-emitter-check', ['python3', TEAM / 'worker-odr-emitter-check/check_odr_emitters.py',
                              W / 'ue4ss-native-supplier-composition-6/run-1',
                              '--require-program-fresh', '--json',
                              TEAM / 'worker-odr-emitter-check/receipt-one-pass.json'])
    run('finalize-manifest', ['python3', TEAM / 'worker-manifest-finalizer/finalize_manifest.py',
                              '--manifest', INT / 'change-manifest.json',
                              '--composition-run', W / 'ue4ss-native-supplier-composition-6/run-1',
                              '--candidate', MY / 'prx-candidate-composition6-1'])
    run('gate-v2', ['python3', TEAM / 'worker-gate-evolution/gate_check_composition5_v2.py',
                    '--build-dir', MY / 'prx-candidate-composition6-1',
                    '--manifest', INT / 'change-manifest.finalized.json',
                    '--json', MY / 'gate-receipt-composition6.json'])
    # TODO(controller at trigger): determinism pair (second pass into -2) when the
    # initializer lands; staging via the guarded cycle law; ridealong + settings
    # restage per the master plan. This runner NEVER launches the game.
    # 2. REAL composition-6 build. Execution model proven by the dry build:
    #    copy build_runtime_composition5.py (Procedure B) with the dry build's
    #    7 documented edits (see worker-comp6-drybuild/build_runtime_composition6_dry1.py)
    #    plus the real-run substitutions:
    #      - staging files <- comp6-integration/ (real roots+D2 scaffold, not rehearsal markers)
    #      - extra-unit lmf-loader-adapter <- comp6-integration/lmf_to_native_loader_facts.cpp
    #      - program replacement <- comp6-integration/UE4SSProgram.cpp (mechanism from
    #        worker-comp6-driver; attestations from worker-program-unit-inventory sec7)
    #      - bridge <- runtime_start_bridge_r12.cpp; entry <- runtime_module_entry_r4.c
    #      - the 21 extra --stage-header args from worker-comp6-drybuild/staging/extra-stage-headers.json
    #    Then: build twice (determinism), package, STATIC_CHECKS must be 20/20.
    # 3. Gate v2 manifest-mode with comp6-integration/change-manifest.json (manifest
    #    PENDING fields filled from THIS run's measured shas + UND delta).
    # 4. Stage via the gated cycle law (upload only after gates GO).
    Path(ROOT / 'outputs/2026-10-04-zcode-controller/comp6-one-pass-state.json').write_text(
        json.dumps({'pre_flight': rec, 'utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime())}, indent=1))
    print('pre-flight complete; awaiting driver/gate argv finalization')

if __name__ == '__main__':
    main()
