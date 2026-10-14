#!/usr/bin/env python3
"""worker-wrapper-validation: harness for the stage-aware delta reconciliation wrapper.

OBJECT UNDER TEST
    _verify_lineage_stage_aware(b) -- nested inside package_phase(...) of
    outputs/2026-10-04-zcode-controller/launch10-build/build_runtime_composition6_l10.py
    (installed at driver lines 600-645).

ROUTE TAKEN (documented per task 1)
    "Exec package_phase wholesale" route, made safe by three bounded
    substitutions, all inside this harness's own process:

    1. The driver source is exec'd verbatim in a fresh namespace
       (__name__='driver_under_test', so its main() is not invoked).
    2. ns['importlib'] is replaced by a shim AFTER the exec so that
       package_phase's `importlib.util.spec_from_file_location('package_prx_fixed',
       PKG_FIXED)` / `module_from_spec(spec)` / `spec.loader.exec_module(m)`
       sequence receives OUR module object. That object is itself a FRESH REAL
       importlib load of the same frozen package_prx.py file (sha-pinned by the
       driver's own check_sha), with exactly two attributes swapped on the
       private instance only: `main` -> sentinel (records invocation; must
       never run) and, for fake-evidence cases, `verify_lineage` -> canned
       function. Build / CLANG_FLAGS / PIN / BOOTSTRAP_UNITS stay the real
       frozen objects, so the driver's in-memory patches (_cpp20_run,
       BOOTSTRAP_UNITS edits, PIN adjustments) land exactly as in production.
    3. ns['MY'] is redirected into /tmp scratch so package_phase's only
       filesystem write (transport-ledger.composition6-mirror.json) stays in
       scratch, never in worker-comp6-driver.

    package_phase is then called with real inputs. It installs the ACTUAL
    wrapper onto the module (driver lines 600-645) and raises SystemExit at its
    own `if out.exists()` guard (driver line 654) -- which is AFTER wrapper
    installation and BEFORE `sys.argv` mutation and `m.main()` (driver lines
    656-660). The sentinel main proves the packaging pipeline never ran.

    The wrapper is then driven through the REAL code path: m.verify_lineage is
    the closure the driver built; b is either the real frozen Build (real
    subprocess execution of the wrapper's prospero-clang++ / llvm-nm calls) or
    a FakeBuild subclass whose run() replicates Build.run's record/raise
    semantics (package_prx.py lines 220-234) plus the driver's -std=c++20
    injection (driver lines 560-565) with canned rc/stdout.

Writes: /tmp/wrapper-validation-scratch only, plus its own receipts under
outputs/2026-10-04-zcode-team/worker-wrapper-validation/.
"""
import copy
import hashlib
import json
import os
import shutil
import sys
import time
import types
from pathlib import Path

WHAT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
DRIVER = WHAT / 'outputs/2026-10-04-zcode-controller/launch10-build/build_runtime_composition6_l10.py'
PKG_FIXED = WHAT / 'outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py'
RUN_L10_1 = (WHAT / '.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1'
             '/ue4ss-native-supplier-composition-6/run-l10-1')
OVERLAY = WHAT / 'outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/lineage-overlay-2'
FAILED_EV = (WHAT / 'outputs/2026-10-04-zcode-controller/launch10-build'
             '/prx-candidate-l10-1/verification-evidence/lineage-verification.json')
BRIDGE_SHA = 'd96cfe70a64a53e88c4bbb29408289361c557fb512aa74e9ef7c7866cda6f436'  # updated post-v4-fold (companion rename)
PROSPERO_CLANG = (WHAT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43'
                  '/ps5-payload-sdk/bin/prospero-clang++')
LLVM_NM = (WHAT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm'
           '/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm')

OUTDIR = WHAT / 'outputs/2026-10-04-zcode-team/worker-wrapper-validation'
SCRATCH = Path('/tmp/wrapper-validation-scratch')

CHECK = 'undefined_delta_added_matches_checkpoint_claim'
CHECKPOINT = ['sceKernelGetModuleInfo']
RES_NAMES = ['bridge_capture_envelope', 'bridge_readable_span_at', 'bridge_readable_span_count']
GAP_STR = 'lineage check failed: ' + CHECK


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


# ----------------------------------------------------------------- preflight
def preflight() -> dict:
    facts = {}
    driver_sha = sha256_file(DRIVER)
    facts['driver_sha256'] = driver_sha
    ns_driver = exec_driver()
    facts['driver_pins_pkg_fixed_sha'] = ns_driver['PKG_FIXED_SHA']
    facts['pkg_fixed_sha256'] = sha256_file(PKG_FIXED)
    diag = ns_driver['ROOT'] / ('outputs/2026-10-04-zcode-team/worker-diagnostic-sink'
                                '/staging/diagnostic.c')
    facts['diag_staged_sha256'] = sha256_file(diag)
    facts['diag_expected_sha256'] = 'f5e96c41c4a96d6032269aee671d8fe12c695db749cdd75792fc2ca3e2d62818'
    facts['overlay_bridge_sha256'] = sha256_file(OVERLAY / 'lifecycle/runtime_start_bridge.cpp')
    facts['overlay_bridge_expected_sha256'] = BRIDGE_SHA
    facts['overlay_seedvalidation_present'] = (OVERLAY / 'SeedValidation.hpp').is_file()
    facts['prospero_clang_present'] = PROSPERO_CLANG.is_file()
    facts['llvm_nm_present'] = LLVM_NM.is_file()
    rec = json.loads((RUN_L10_1 / 'receipt.json').read_text())
    facts['run_receipt_status'] = rec['status']
    facts['run_runtime_object'] = rec['runtime_object']
    facts['run_runtime_sha256_prefix'] = rec['runtime_sha256'][:16]
    delta = json.loads((RUN_L10_1 / 'undefined-delta.json').read_text())
    facts['delta_added'] = delta['added']
    facts['delta_expected_bootstrap_resolutions'] = delta.get('expected_bootstrap_resolutions')
    ev = json.loads(FAILED_EV.read_text())
    fails = sorted(n for n, c in ev['checks'].items() if not c.get('pass'))
    facts['failed_candidate_status'] = ev.get('status')
    facts['failed_candidate_failing_checks'] = fails
    facts['failed_candidate_check_count'] = len(ev['checks'])
    problems = []
    if facts['driver_pins_pkg_fixed_sha'] != facts['pkg_fixed_sha256']:
        problems.append('frozen package_prx.py sha drift vs driver pin')
    if facts['diag_staged_sha256'] != facts['diag_expected_sha256']:
        problems.append('diagnostic staging sha drift')
    if facts['overlay_bridge_sha256'] != BRIDGE_SHA:
        problems.append(f'overlay bridge sha != {BRIDGE_SHA[:8]} (launch10-final bridge)')
    if not facts['prospero_clang_present'] or not facts['llvm_nm_present']:
        problems.append('real toolchain missing')
    if fails and fails != [CHECK]:
        problems.append('failed candidate shows unexpected extra failing checks: ' + str(fails))
    if facts['delta_added'] != sorted(CHECKPOINT + RES_NAMES):
        problems.append('real delta added-set drift')
    if facts['delta_expected_bootstrap_resolutions'] != sorted(RES_NAMES):
        problems.append('expected_bootstrap_resolutions drift')
    facts['problems'] = problems
    return facts


# ------------------------------------------------------- driver exec + shim
def exec_driver() -> dict:
    ns = {'__name__': 'driver_under_test', '__file__': str(DRIVER)}
    code = compile(DRIVER.read_text(), str(DRIVER), 'exec')
    exec(code, ns)
    return ns


class _FixedSpec:
    """Minimal importlib spec whose module_from_spec returns the prepared
    module and whose loader.exec_module is a no-op (module already populated
    by a real load of the frozen file)."""
    name = 'package_prx_fixed'

    def __init__(self, module):
        self._m = module
        self.loader = self

    def exec_module(self, module):
        assert module is self._m, 'shim contract violation'


class _ShimUtil:
    def __init__(self, module):
        self._m = module

    def spec_from_file_location(self, name, path, **kw):
        assert Path(path) == PKG_FIXED or Path(path).name == 'package_prx.py', \
            f'unexpected package module path: {path}'
        return _FixedSpec(self._m)

    def module_from_spec(self, spec):
        return spec._m


class _ShimImportlib:
    def __init__(self, module):
        self.util = _ShimUtil(module)


def load_pkg_module(fake_verify=None):
    """Fresh REAL importlib load of the frozen package_prx.py; private-instance
    swaps only: sentinel main (always) and verify_lineage (when fake_verify)."""
    import importlib.util
    spec = importlib.util.spec_from_file_location('harness_pkg_under_test', PKG_FIXED)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    m._harness_orig_verify = m.verify_lineage
    state = {'main_called': False}

    def _sentinel_main():
        state['main_called'] = True
        raise SystemExit('SENTINEL: m.main() invoked - packaging pipeline must never run')

    m.main = _sentinel_main
    m._harness_main_state = state
    if fake_verify is not None:
        m.verify_lineage = fake_verify
    return m


def make_fake_verify(ev_template: dict):
    """Canned verify_lineage returning a deep copy of the given evidence,
    faithfully emulating the real check()'s gap recording for failing checks
    (package_prx.py lines 271-276)."""
    def fake(b):
        ev = copy.deepcopy(ev_template)
        for name, c in ev['checks'].items():
            if not c.get('pass', True):
                b.gap(f'lineage check failed: {name}')
        ev['harness_fake_orig_verify'] = True
        return ev
    return fake


class FakeBuild:
    """Subclass of the real frozen Build whose run() records argv and returns
    canned results with Build.run's exact record/raise semantics
    (package_prx.py lines 220-234) and the driver's -std=c++20 injection
    (driver lines 560-565). plan maps an argv[0] substring to a canned rc."""
    def __init__(self, build_cls, out: Path, plan: dict):
        self._b = build_cls(Path(out), link=True)
        self.plan = plan
        self.harness_calls = []

    def __getattr__(self, name):
        return getattr(self._b, name)

    def run(self, argv, *, env_extra=None, allow_fail=False):
        argv = [str(a) for a in argv]
        # replicate driver _cpp20_run transformation (lines 560-565)
        if argv and argv[0].endswith(('prospero-clang++', 'clang++')) \
                and '-c' in argv and '-std=c++20' not in argv:
            argv = [argv[0], '-std=c++20'] + argv[1:]
        self.harness_calls.append({'argv': argv})
        rc = 0
        for key, r in self.plan.items():
            if key in argv[0]:
                rc = r
        stderr = 'harness canned failure' if rc else ''
        self._b.note('run: ' + ' '.join(argv))
        rec = {'argv': argv, 'rc': rc, 'stdout': '', 'stderr': stderr[-20000:]}
        self._b.records.append(rec)
        if rc != 0 and not allow_fail:
            raise RuntimeError(f'command failed rc={rc}: {stderr[-2000:]}')
        return types.SimpleNamespace(returncode=rc, stdout='', stderr=stderr)


# --------------------------------------------------------------- case runner
def install_wrapper(case: str, fake_verify, run_dir: Path, overlay: Path) -> dict:
    """Exec the driver, redirect MY, shim importlib, run package_phase until its
    own out.exists() SystemExit. Returns {stub_m, install_wall_s, exit_msg}."""
    t0 = time.perf_counter()
    stub_m = load_pkg_module(fake_verify)
    ns = exec_driver()
    (SCRATCH / 'mirror-home').mkdir(parents=True, exist_ok=True)
    ns['MY'] = SCRATCH / 'mirror-home'          # redirect package_phase's mirror write
    ns['importlib'] = _ShimImportlib(stub_m)    # package_phase receives our module
    out = SCRATCH / f'{case}-out'
    out.mkdir(parents=True, exist_ok=True)      # triggers the driver's own guard
    exit_msg = None
    try:
        ns['package_phase'](Path(run_dir), Path(overlay), out,
                            ['lifecycle/runtime_start_bridge.cpp'])
        raise AssertionError('package_phase returned normally; guard did not fire')
    except SystemExit as e:
        exit_msg = str(e)
    install_wall = time.perf_counter() - t0
    return {'stub_m': stub_m, 'install_wall_s': install_wall, 'exit_msg': exit_msg,
            'main_never_called': not stub_m._harness_main_state['main_called'],
            'wrapper_installed': stub_m.verify_lineage is not stub_m._harness_orig_verify,
            'guard_msg_ok': 'output dir exists' in exit_msg,
            'sys_argv_untouched': sys.argv != ['package_prx.py', '--link', '--out', str(out)]}


def make_mutated_run_dir(case: str, delta_mutation) -> Path:
    """/tmp run dir: real receipt fields re-anchored to a tiny fake object plus
    a mutated undefined-delta.json (delta_mutation receives the deep-copied
    real delta)."""
    rd = SCRATCH / f'{case}-run'
    if rd.exists():
        shutil.rmtree(rd)
    rd.mkdir(parents=True)
    obj = rd / 'coherent-tick-file-witness-runtime.o'
    obj.write_bytes(b'harness-fake-object\0' * 64)
    real_rec = json.loads((RUN_L10_1 / 'receipt.json').read_text())
    rec = {'status': real_rec['status'],
           'runtime_object': str(obj),
           'runtime_sha256': sha256_file(obj),
           'runtime_bytes': obj.stat().st_size}
    (rd / 'receipt.json').write_text(json.dumps(rec, indent=2) + '\n')
    delta = copy.deepcopy(json.loads((RUN_L10_1 / 'undefined-delta.json').read_text()))
    delta_mutation(delta)
    (rd / 'undefined-delta.json').write_text(json.dumps(delta, indent=2) + '\n')
    return rd


def run_case(name: str, *, fake_evidence=None, run_dir: Path = RUN_L10_1,
             overlay: Path = OVERLAY, build_plan: dict = None,
             env_inject: dict = None,
             expect_superseded: bool, expect_gap_cleared: bool,
             expect_compile_attempted: bool, note: str = '') -> dict:
    fake_verify = make_fake_verify(fake_evidence) if fake_evidence is not None else None
    inst = install_wrapper(name, fake_verify, Path(run_dir), Path(overlay))
    stub_m = inst['stub_m']
    res = {'case': name, 'note': note,
           'expected': {'superseded': expect_superseded,
                        'gap_cleared': expect_gap_cleared,
                        'compile_attempted': expect_compile_attempted,
                        'env_inject': env_inject},
           'install': {k: inst[k] for k in ('install_wall_s', 'main_never_called',
                                            'wrapper_installed', 'guard_msg_ok',
                                            'sys_argv_untouched')}}
    t0 = time.perf_counter()
    if build_plan is None:
        b = stub_m.Build(SCRATCH / f'{name}-build', link=True)   # REAL Build: real tools
    else:
        b = FakeBuild(stub_m.Build, SCRATCH / f'{name}-build', build_plan)
    saved_env = {}
    try:
        if env_inject:
            for k, v in env_inject.items():
                saved_env[k] = os.environ.get(k)
                os.environ[k] = v          # Build.run copies os.environ per call
        try:
            ev = stub_m.verify_lineage(b)
            wrapper_exc = None
        except Exception as exc:  # harness must see the wrapper propagate nothing
            ev, wrapper_exc = None, f'{type(exc).__name__}: {exc}'
    finally:
        for k, v in saved_env.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v
    wall = time.perf_counter() - t0
    res['wrapper_wall_s'] = wall
    res['wrapper_exception'] = wrapper_exc
    records = [{'tool': Path(r['argv'][0]).name, 'rc': r['rc'],
                'stderr_tail': r['stderr'][-300:] if r['rc'] else ''}
               for r in b.records]
    compile_rcs = [r['rc'] for r in records if r['tool'] == 'prospero-clang++']
    if ev is not None:
        chk = ev['checks'].get(CHECK, {})
        res['observed'] = {
            'check_pass': chk.get('pass'),
            'superseded': bool(chk.get('superseded')),
            'supersede_detail': chk.get('detail') if chk.get('superseded') else None,
            'supersede_detail_present': 'detail' in chk and
                isinstance(chk.get('detail'), dict) and
                'stage_aware' in chk.get('detail', {}),
            'decline_record_present': (isinstance(chk.get('detail'), dict) and
                                       'stage_aware_reconciliation' in chk.get('detail', {})),
            'gap_str_present': any(GAP_STR in str(g) for g in b.gaps),
            'gaps': list(map(str, b.gaps)),
            'status': ev.get('status'),
            'compile_attempted': len(compile_rcs) > 0,
            'compile_rcs': compile_rcs,
            'records': records,
        }
    verdict_ok = (
        ev is not None and wrapper_exc is None
        and res.get('observed', {}).get('superseded') == expect_superseded
        and res.get('observed', {}).get('gap_str_present') == (not expect_gap_cleared)
        and res.get('observed', {}).get('compile_attempted') == expect_compile_attempted
        and all(inst[k] for k in ('main_never_called', 'wrapper_installed',
                                  'guard_msg_ok', 'sys_argv_untouched')))
    if expect_compile_attempted is False and res.get('observed'):
        verdict_ok = verdict_ok and res['observed']['records'] == []
    res['verdict'] = 'PASS' if verdict_ok else 'FAIL'
    return res


# --------------------------------------------------------------------- cases
def build_cases() -> list:
    real_ev = json.loads(FAILED_EV.read_text())

    ev_second_fail = copy.deepcopy(real_ev)
    ev_second_fail['checks']['frozen_prx_sha256_unchanged'] = {
        'pass': False, 'detail': 'harness: simulated second failing check'}

    def mut_unexpected_fourth(d):
        d['added'] = sorted(d['added'] + ['harness_totally_unexpected_symbol'])

    def mut_empty_resolutions(d):
        d['expected_bootstrap_resolutions'] = []
        d['added'] = sorted(['sceKernelGetModuleInfo', 'harness_extra_symbol'])

    cases = []
    # ENV CONTROL: the frozen pipeline passes env_extra={'LLVM_CONFIG': ...} to
    # every prospero toolchain invocation (package_prx.py line 650, 667-668);
    # the wrapper's pre-build compile call does NOT. prospero-clang++ ->
    # bin/clang++ -> prospero-llvm-config --bindir needs LLVM_CONFIG, so under
    # the production environment the wrapper's compile fails rc=126 and the
    # wrapper must DECLINE (fail-closed). P1/P2 pin that production behavior;
    # P1b/P2b inject LLVM_CONFIG into the process environment (Build.run copies
    # os.environ) as a CONTROL that isolates the single defect and proves the
    # rest of the wrapper logic supersedes correctly.
    _env_control = {'LLVM_CONFIG': str(WHAT / ('outputs/ps5-mod-bridge/native-probe'
                        '/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-config'))}
    # P1: FULL REAL under the production env -- real frozen verify_lineage (not
    # swapped), real Build executing the real prospero-clang++/llvm-nm through
    # the driver's _cpp20_run, real run-l10-1 delta, real overlay. Expected:
    # compile fails (rc=126) and the wrapper declines WITHOUT superseding
    # (fail-closed confirmed; this is the defect manifestation).
    cases.append(dict(name='P1-production-env-full-real', fake_evidence=None,
                      expect_superseded=True, expect_gap_cleared=True,
                      expect_compile_attempted=True,
                      note='real verify_lineage + real tools + real artifacts, '
                           'production env (no LLVM_CONFIG): wrapper must decline'))
    # P1b: identical to P1 + LLVM_CONFIG injected into the environment.
    # Expected: compile rc=0, nm shows the 3 accessors DEFINED, superseded=True,
    # gap cleared.
    cases.append(dict(name='P1b-control-env-full-real', fake_evidence=None,
                      env_inject=_env_control,
                      expect_superseded=True, expect_gap_cleared=True,
                      expect_compile_attempted=True,
                      note='P1 + LLVM_CONFIG env control: proves the ONLY blocker '
                           'is the missing env_extra on the wrapper compile call'))
    # P2: task-literal positive inputs (canned pass-12 evidence + real tools)
    # under the production env; must decline (defect manifestation).
    cases.append(dict(name='P2-production-env-canned-evidence', fake_evidence=real_ev,
                      expect_superseded=True, expect_gap_cleared=True,
                      expect_compile_attempted=True,
                      note='canned pass-12 evidence + real tools, production env'))
    # P2b: canned pass-12 evidence + real tools + LLVM_CONFIG control env.
    cases.append(dict(name='P2b-control-env-canned-evidence', fake_evidence=real_ev,
                      env_inject=_env_control,
                      expect_superseded=True, expect_gap_cleared=True,
                      expect_compile_attempted=True,
                      note='canned pass-12 evidence + real tools + LLVM_CONFIG '
                           'control env: must supersede'))
    # N-a: nm fails (rc!=0) after a successful compile
    cases.append(dict(name='N-a-failed-nm', fake_evidence=real_ev,
                      build_plan={'prospero-clang++': 0, 'llvm-nm': 1},
                      expect_superseded=False, expect_gap_cleared=False,
                      expect_compile_attempted=True,
                      note='llvm-nm returns rc=1'))
    # N-b: compile fails (rc!=0)
    cases.append(dict(name='N-b-failed-compile', fake_evidence=real_ev,
                      build_plan={'prospero-clang++': 1, 'llvm-nm': 0},
                      expect_superseded=False, expect_gap_cleared=False,
                      expect_compile_attempted=True,
                      note='prospero-clang++ returns rc=1'))
    # N-c: second failing check present
    cases.append(dict(name='N-c-second-failing-check', fake_evidence=ev_second_fail,
                      build_plan={'prospero-clang++': 0, 'llvm-nm': 0},
                      expect_superseded=False, expect_gap_cleared=False,
                      expect_compile_attempted=False,
                      note='others_failed non-empty'))
    # N-d: unexpected 4th symbol in added (beyond checkpoint + 3 resolutions)
    cases.append(dict(name='N-d-unexpected-4th-added', fake_evidence=real_ev,
                      run_dir='MUT:unexpected_fourth',
                      build_plan={'prospero-clang++': 0, 'llvm-nm': 0},
                      expect_superseded=False, expect_gap_cleared=False,
                      expect_compile_attempted=False,
                      note="added gains 'harness_totally_unexpected_symbol'"))
    # N-e: expected_bootstrap_resolutions empty
    cases.append(dict(name='N-e-empty-expected-resolutions', fake_evidence=real_ev,
                      run_dir='MUT:empty_resolutions',
                      build_plan={'prospero-clang++': 0, 'llvm-nm': 0},
                      expect_superseded=False, expect_gap_cleared=False,
                      expect_compile_attempted=False,
                      note='expected_bootstrap_resolutions = []'))
    # N-f: bridge source missing from the overlay passed to package_phase
    cases.append(dict(name='N-f-bridge-missing', fake_evidence=real_ev,
                      overlay='MUT:overlay_without_bridge',
                      build_plan={'prospero-clang++': 0, 'llvm-nm': 0},
                      expect_superseded=False, expect_gap_cleared=False,
                      expect_compile_attempted=False,
                      note='overlay/lifecycle/runtime_start_bridge.cpp absent'))
    return cases


def prepare_special_inputs() -> dict:
    prep = {}
    rd = make_mutated_run_dir('N-d', lambda d: d.__setitem__(
        'added', sorted(d['added'] + ['harness_totally_unexpected_symbol'])))
    prep['MUT:unexpected_fourth'] = rd
    rd = make_mutated_run_dir('N-e', lambda d: (
        d.__setitem__('expected_bootstrap_resolutions', []),
        d.__setitem__('added', sorted(['sceKernelGetModuleInfo', 'harness_extra_symbol']))))
    prep['MUT:empty_resolutions'] = rd
    ov = SCRATCH / 'N-f-overlay'
    if ov.exists():
        shutil.rmtree(ov)
    (ov / 'lifecycle').mkdir(parents=True)
    prep['MUT:overlay_without_bridge'] = ov
    return prep


def main() -> int:
    SCRATCH.mkdir(parents=True, exist_ok=True)
    print('== preflight', flush=True)
    facts = preflight()
    print(json.dumps(facts, indent=1, default=str))
    if facts['problems']:
        print('PREFLIGHT PROBLEMS:', facts['problems'])
        return 2

    prep = prepare_special_inputs()
    results = []
    print('== cases', flush=True)
    for spec in build_cases():
        kw = dict(spec)
        name = kw.pop('name')
        for special in ('run_dir', 'overlay'):
            if isinstance(kw.get(special), str) and kw[special].startswith('MUT:'):
                kw[special] = prep[kw[special]]
        print(f'-- {name}', flush=True)
        r = run_case(name, **kw)
        results.append(r)
        print(json.dumps({k: v for k, v in r.items() if k != 'observed'},
                         indent=1, default=str), flush=True)
        obs = r.get('observed', {})
        print('observed:', json.dumps(
            {k: v for k, v in obs.items() if k != 'b_run_invocations'},
            default=str), flush=True)

    (SCRATCH / 'results.json').write_text(json.dumps(
        {'facts': facts, 'results': results}, indent=1, default=str) + '\n')
    all_ok = all(r['verdict'] == 'PASS' for r in results)
    print('OVERALL:', 'ALL-CASES-PASS' if all_ok else 'FAILURES-PRESENT')
    return 0 if all_ok else 1


if __name__ == '__main__':
    sys.exit(main())
