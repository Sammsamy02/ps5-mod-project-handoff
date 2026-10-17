#!/usr/bin/env python3
"""worker-wrapper-validation-l11: harness for the UPDATED stage-aware delta
reconciliation wrapper, exercised through the l11 combined driver.

OBJECT UNDER TEST
    _verify_lineage_stage_aware(b) -- nested inside package_phase(...) of
    outputs/2026-10-04-zcode-controller/launch11-combined/build_runtime_composition6_l11.py
    (V2: driver sha af309a3a... after the controller's supersede-detail fix;
    V1 was 291d3b9a...). Wrapper block at driver lines 681-753. The wrapper is
    byte-carried from launch10 driver f4103572 (diff_driver_l10-to-l11.diff
    has NO wrapper hunk) except the V2 fix; it is the UPDATED edition vs the
    one the original worker-wrapper-validation run exercised, with exactly
    three changes:
      (1) allowed-names tuple now includes 'diagnostic_event'
          (driver line 689);
      (2) pre-proof is DUAL: bridge compile (prospero-clang++) PLUS staged
          diagnostic.c compile (prospero-clang), EACH with
          env_extra={'LLVM_CONFIG': str(m.PIN['llvm'] / 'llvm-config')}
          (driver lines 707-709 and 721-723);
      (3) resolution check is PROVIDER-SPLIT: bridge accessors must appear in
          the BRIDGE pre-proof's nm defs, diagnostic_event in the DIAGNOSTIC.c
          pre-proof's nm defs (driver lines 728-731:
          resolved_ok = (set(res) - {'diagnostic_event'}) <= defs and _diag_ok).
    The driver is additionally instrumented with a preproof_exception decline
    record (driver lines 732-734, set inside the reconciliation except) --
    step 4 of this validation verifies whether it actually survives into the
    evidence.

GO-POSTFIX LINEAGE (read before writing this edition)
    outputs/2026-10-04-zcode-team/worker-wrapper-validation/ is the ORIGINAL
    harness (11 cases against the l10 driver). Its post-fix cycle is UNRESOLVED
    on disk: GO-POSTFIX-RECEIPT.txt records a 2026-10-05T15:15:33Z run that
    crashed with NameError: 'failed_checks' (an incomplete edit of
    harness.py preflight), after GO-POSTFIX-harness-results.json (10:15) had
    recorded a full earlier 11-case PASS run against the OLD wrapper semantics
    (rc=126 env defect + LLVM_CONFIG control-env supersede). The UPDATED
    wrapper removed that defect by passing env_extra itself (change 2), so the
    control-env P1b/P2b cases are obsolete. This l11 edition supersedes that
    lineage with an all-real case plan; the l10 harness/directory is left
    byte-untouched.

ROUTE TAKEN (same proven route as the original harness, documented per task 1)
    "Exec package_phase wholesale" route, made safe by three bounded
    substitutions, all inside this harness's own process:

    1. The driver source is exec'd verbatim in a fresh namespace
       (__name__='driver_under_test', so its main() is not invoked).
    2. ns['importlib'] is replaced by a shim AFTER the exec so that
       package_phase's importlib.util.spec_from_file_location /
       module_from_spec / exec_module sequence receives OUR module object:
       a fresh REAL importlib load of the same frozen package_prx.py
       (sha-pinned by the driver's own check_sha, PKG_FIXED_SHA e6fbe6c5...)
       with exactly one attribute swapped on the private instance:
       main -> sentinel (records invocation; must never run). Build /
       CLANG_FLAGS / PIN / BOOTSTRAP_UNITS stay the real frozen objects, so
       the driver's in-memory patches (_cpp20_run, BOOTSTRAP_UNITS edits,
       PIN adjustments, wrapper installation) land exactly as in production.
    3. ns['MY'] is redirected into /tmp scratch so package_phase's only
       filesystem write (transport-ledger.composition6-mirror.json) stays in
       scratch, never in worker-comp6-driver.

    package_phase is then called with REAL inputs. It installs the ACTUAL
    wrapper onto the module (driver lines 678-746) and raises SystemExit at its
    own `if out.exists()` guard (driver line 755) -- AFTER wrapper installation
    and BEFORE sys.argv mutation and m.main() (driver lines 756-758). The
    sentinel main proves the packaging pipeline never ran.

    The wrapper is then driven through the REAL code path: m.verify_lineage is
    the closure the driver built; b is the REAL frozen Build executing the real
    prospero-clang++ / prospero-clang / llvm-nm / llvm-readelf subprocesses.
    EVERY case below is fully real: no canned evidence, no FakeBuild, no canned
    exit plans. Sub-process rc values are read from Build.records entries,
    whose 'rc' is subprocess.run(...).returncode captured in-process from the
    run result object (package_prx.py line 228) -- never from a piped $?.

CASE PLAN (positive must supersede; negatives must decline + record)
    P1-l11-full-real
        REAL run-l11-1 delta (added == checkpoint + 4 resolutions incl.
        diagnostic_event), REAL overlay (bridge d96cfe70...), REAL staged
        diagnostic.c, REAL toolchain, production env (the harness injects
        NOTHING; the wrapper must supply env_extra itself). Expect BOTH
        pre-proof compiles rc=0, provider-split nm evidence, superseded=True,
        the gap cleared.
    N-a-diag-source-missing
        ns['ROOT'] is re-pointed (AFTER install) to a /tmp symlink farm that
        mirrors the real tree but LACKS worker-diagnostic-sink/staging/
        diagnostic.c. The wrapper's diag pre-proof compile then fails against
        a genuinely missing file (real clang rc=1) -> exception -> decline,
        no supersede. EXCEPTION case for step 4.
    N-b-baseline-r13-bridge
        /tmp overlay copy (symlink tree of the REAL overlay) with the bridge
        swapped for the baseline r13 bridge from worker-module-start-wiring
        (sha 10385e22...; compiles rc=0, defines ZERO of the accessors --
        verified in preflight). Bridge proof fails as a PURE DEFINITION-MISS
        (no exception): decline, preproof_exception must be ABSENT.
    N-c-unexpected-extra-added
        Mutated /tmp run dir: real delta 'added' gains a 6th symbol
        harness_totally_unexpected_symbol -> recon delta match fails ->
        decline, compile never attempted.
    N-d-second-failing-check
        Mutated /tmp run dir: receipt runtime_bytes +1 (object/sha stay real)
        -> orig verify fails composition_object_bytes IN ADDITION to the delta
        check -> others_failed non-empty -> decline, compile never attempted.
    N-e-unexpected-5th-resolution (supplementary: gates change (1))
        Mutated /tmp run dir: expected_bootstrap_resolutions gains
        harness_extra_resolution (5th) and 'added' is extended to match, so
        the ONLY failing conjunct is the allowed-names filter -> decline.

    Step 4 (preproof_exception semantics) is folded into every case:
    expect_preproof_exception=True for N-a, False for the pure
    definition-miss / recon-fail cases; the recorded presence must match.

Writes: /tmp/wrapper-validation-l11-scratch only, plus its own receipts under
outputs/2026-10-04-zcode-team/worker-wrapper-validation-l11/.
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
DRIVER = WHAT / 'outputs/2026-10-04-zcode-controller/launch11-combined/build_runtime_composition6_l11.py'
PKG_FIXED = WHAT / 'outputs/2026-10-04-zcode-team/worker-prx-link-gaps/package_prx.py'
RUN_L11_1 = (WHAT / '.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1'
             '/ue4ss-native-supplier-composition-6/run-l11-1')
OVERLAY = WHAT / 'outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/lineage-overlay-2'
CAND_EV = (WHAT / 'outputs/2026-10-04-zcode-controller/launch11-combined'
           '/prx-candidate-l11-1/verification-evidence/lineage-verification.json')
R13_BRIDGE = WHAT / ('outputs/2026-10-04-zcode-team/worker-module-start-wiring'
                     '/runtime_start_bridge_r13.cpp')
DIAG_STAGED = WHAT / 'outputs/2026-10-04-zcode-team/worker-diagnostic-sink/staging/diagnostic.c'
BRIDGE_SHA = 'd96cfe70a64a53e88c4bbb29408289361c557fb512aa74e9ef7c7866cda6f436'
R13_SHA = '10385e2265af0a12236bc7f713ed343a7f13796644aac0de511f6663f579209a'
DIAG_SHA = 'f5e96c41c4a96d6032269aee671d8fe12c695db749cdd75792fc2ca3e2d62818'
PROSPERO_CLANGXX = (WHAT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43'
                    '/ps5-payload-sdk/bin/prospero-clang++')
PROSPERO_CLANG = (WHAT / 'outputs/ps5-mod-bridge/native-host/build/sdk-v0.43'
                  '/ps5-payload-sdk/bin/prospero-clang')
LLVM_NM = (WHAT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm'
           '/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm')
LLVM_CONFIG = (WHAT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm'
               '/LLVM-21.1.8-macOS-ARM64/bin/llvm-config')
LLVM_READELF = (WHAT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm'
                '/LLVM-21.1.8-macOS-ARM64/bin/llvm-readelf')

OUTDIR = WHAT / 'outputs/2026-10-04-zcode-team/worker-wrapper-validation-l11'
SCRATCH = Path('/tmp/wrapper-validation-l11-scratch')

CHECK = 'undefined_delta_added_matches_checkpoint_claim'
CHECKPOINT = ['sceKernelGetModuleInfo']
RES_NAMES = ['bridge_capture_envelope', 'bridge_readable_span_at',
             'bridge_readable_span_count', 'diagnostic_event']
BRIDGE_ACCESSORS = ['bridge_capture_envelope', 'bridge_readable_span_at',
                    'bridge_readable_span_count']
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
    problems = []
    facts['driver_sha256'] = sha256_file(DRIVER)
    facts['driver_sha256_expected'] = ('af309a3a8a36420b7cd202a6d67338bc568859608097319a74896a79003ea0fa'
                                       ' (V2: controller supersede-detail fix; V1 was 291d3b9a...)')
    facts['driver_is_v2'] = facts['driver_sha256'].startswith('af309a3a')
    ns_driver = exec_driver()
    facts['driver_pins_pkg_fixed_sha'] = ns_driver['PKG_FIXED_SHA']
    facts['pkg_fixed_sha256'] = sha256_file(PKG_FIXED)
    facts['diag_staged_sha256'] = sha256_file(DIAG_STAGED)
    facts['diag_expected_sha256'] = DIAG_SHA
    facts['overlay_bridge_sha256'] = sha256_file(OVERLAY / 'lifecycle/runtime_start_bridge.cpp')
    facts['overlay_bridge_expected_sha256'] = BRIDGE_SHA
    facts['r13_bridge_sha256'] = sha256_file(R13_BRIDGE)
    facts['r13_bridge_expected_sha256'] = R13_SHA
    r13_text = R13_BRIDGE.read_text()
    facts['r13_accessor_mentions'] = sum(r13_text.count(a) for a in BRIDGE_ACCESSORS)
    facts['overlay_seedvalidation_present'] = (OVERLAY / 'SeedValidation.hpp').is_file()
    facts['tools_present'] = {p.name: p.is_file() for p in
                              (PROSPERO_CLANGXX, PROSPERO_CLANG, LLVM_NM,
                               LLVM_CONFIG, LLVM_READELF)}
    rec = json.loads((RUN_L11_1 / 'receipt.json').read_text())
    facts['run_receipt_status'] = rec['status']
    facts['run_runtime_object'] = rec['runtime_object']
    facts['run_runtime_sha256_prefix'] = rec['runtime_sha256'][:16]
    facts['run_runtime_bytes'] = rec['runtime_bytes']
    delta = json.loads((RUN_L11_1 / 'undefined-delta.json').read_text())
    facts['delta_added'] = delta['added']
    facts['delta_expected_bootstrap_resolutions'] = delta.get('expected_bootstrap_resolutions')
    ev = json.loads(CAND_EV.read_text())
    chk = ev['checks'].get(CHECK, {})
    fails = sorted(n for n, c in ev['checks'].items() if not c.get('pass'))
    facts['candidate_status'] = ev.get('status')
    facts['candidate_failing_checks'] = fails
    facts['candidate_delta_check'] = {
        'pass': chk.get('pass'), 'superseded': chk.get('superseded'),
        'stage_aware_mentions_diagnostic_event':
            'diagnostic_event' in json.dumps(chk.get('detail', {}))}
    # textual pins: the three wrapper changes + the preproof_exception instrumentation
    dtext = DRIVER.read_text()
    facts['wrapper_text_pins'] = {
        'allowed_names_include_diagnostic_event':
            "'bridge_capture_envelope', 'bridge_readable_span_at', 'bridge_readable_span_count', 'diagnostic_event'" in dtext,
        'dual_preproof_prospero_clang_c': "bin/prospero-clang')," in dtext,
        'provider_split': "set(res) - {'diagnostic_event'}) <= defs and _diag_ok" in dtext,
        'preproof_exception_instrumented': 'preproof_exception' in dtext,
        'env_extra_llvm_config_count': dtext.count("env_extra={'LLVM_CONFIG'"),
    }
    facts['production_env_llvm_config_absent'] = 'LLVM_CONFIG' not in os.environ
    # hard assertions
    if facts['driver_pins_pkg_fixed_sha'] != facts['pkg_fixed_sha256']:
        problems.append('frozen package_prx.py sha drift vs driver pin')
    if facts['diag_staged_sha256'] != DIAG_SHA:
        problems.append('diagnostic staging sha drift')
    if facts['overlay_bridge_sha256'] != BRIDGE_SHA:
        problems.append(f'overlay bridge sha != {BRIDGE_SHA[:8]}')
    if facts['r13_bridge_sha256'] != R13_SHA:
        problems.append('r13 baseline bridge sha drift')
    if facts['r13_accessor_mentions'] != 0:
        problems.append('r13 baseline bridge mentions accessors; not a clean provider-miss input')
    if not all(facts['tools_present'].values()):
        problems.append('real toolchain missing')
    if rec['status'] != 'NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED':
        problems.append('run-l11-1 receipt status drift')
    if sha256_file(Path(rec['runtime_object'])) != rec['runtime_sha256']:
        problems.append('run-l11-1 composed object hash drift vs receipt')
    if facts['delta_added'] != sorted(CHECKPOINT + RES_NAMES):
        problems.append('real l11 delta added-set drift')
    if facts['delta_expected_bootstrap_resolutions'] != sorted(RES_NAMES):
        problems.append('expected_bootstrap_resolutions drift (must include diagnostic_event)')
    if fails:
        problems.append('candidate evidence has hard-failing checks: ' + str(fails))
    if not (chk.get('pass') and chk.get('superseded')
            and facts['candidate_delta_check']['stage_aware_mentions_diagnostic_event']):
        problems.append('candidate evidence does not show the production diagnostic_event supersede')
    pins = facts['wrapper_text_pins']
    if not (pins['allowed_names_include_diagnostic_event'] and pins['dual_preproof_prospero_clang_c']
            and pins['provider_split'] and pins['preproof_exception_instrumented']):
        problems.append('driver wrapper text missing an expected UPDATED-wrapper pin')
    if pins['env_extra_llvm_config_count'] != 2:
        problems.append('wrapper must carry exactly 2 env_extra LLVM_CONFIG compile calls')
    if not facts['production_env_llvm_config_absent']:
        problems.append('LLVM_CONFIG present in harness env; positive case must run production env')
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


def load_pkg_module():
    """Fresh REAL importlib load of the frozen package_prx.py; private-instance
    swap only: sentinel main (packaging pipeline must never run)."""
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
    return m


def install_wrapper(case: str, run_dir: Path, overlay: Path) -> dict:
    """Exec the driver, redirect MY, shim importlib, run package_phase until its
    own out.exists() SystemExit. Returns the module, the driver namespace (for
    the N-a ROOT swap) and install facts."""
    t0 = time.perf_counter()
    stub_m = load_pkg_module()
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
    return {'stub_m': stub_m, 'ns': ns, 'install_wall_s': install_wall,
            'exit_msg': exit_msg,
            'main_never_called': not stub_m._harness_main_state['main_called'],
            'wrapper_installed': stub_m.verify_lineage is not stub_m._harness_orig_verify,
            'guard_msg_ok': 'output dir exists' in exit_msg,
            'sys_argv_untouched': sys.argv != ['package_prx.py', '--link', '--out', str(out)]}


# ------------------------------------------------------------ mutated inputs
def make_run_dir(case: str, *, receipt_mut=None, delta_mut=None) -> Path:
    """/tmp run dir: a SYMLINK FARM of the real run-l11-1 (every child, incl.
    the 117MB composed object and the sdk-nm evidence files, linked -- nothing
    copied), with receipt.json / undefined-delta.json materialized as mutated
    copies of the REAL files. The orig verify_lineage therefore sees a complete
    composition dir, so the ONLY failures are the intended mutations."""
    rd = SCRATCH / f'{case}-run'
    if rd.exists():
        shutil.rmtree(rd)
    rd.mkdir(parents=True)
    for child in RUN_L11_1.iterdir():
        if child.name in ('receipt.json', 'undefined-delta.json'):
            continue
        (rd / child.name).symlink_to(child)
    real_rec = json.loads((RUN_L11_1 / 'receipt.json').read_text())
    rec = copy.deepcopy(real_rec)
    if receipt_mut:
        receipt_mut(rec)
    (rd / 'receipt.json').write_text(json.dumps(rec, indent=2) + '\n')
    delta = copy.deepcopy(json.loads((RUN_L11_1 / 'undefined-delta.json').read_text()))
    if delta_mut:
        delta_mut(delta)
    (rd / 'undefined-delta.json').write_text(json.dumps(delta, indent=2) + '\n')
    return rd


def build_overlay_with_bridge(case: str, bridge_src: Path) -> Path:
    """/tmp symlink tree of the REAL overlay with lifecycle/
    runtime_start_bridge.cpp replaced by a copy of bridge_src (the baseline
    r13 bridge for N-b). Nothing in the real tree is touched."""
    ov = SCRATCH / f'{case}-overlay'
    if ov.exists():
        shutil.rmtree(ov)
    ov.mkdir(parents=True)
    for child in OVERLAY.iterdir():
        if child.name == 'lifecycle':
            continue
        (ov / child.name).symlink_to(child)
    lc = ov / 'lifecycle'
    lc.mkdir()
    for child in (OVERLAY / 'lifecycle').iterdir():
        if child.name == 'runtime_start_bridge.cpp':
            continue
        (lc / child.name).symlink_to(child)
    shutil.copy2(bridge_src, lc / 'runtime_start_bridge.cpp')
    return ov


def build_root_farm(case: str) -> Path:
    """Symlink farm mimicking ROOT for the wrapper's ROOT-relative lookups,
    EXCEPT worker-diagnostic-sink/staging which is an empty real dir (the
    staged diagnostic.c is genuinely MISSING from the wrapper's viewpoint)."""
    farm = SCRATCH / f'{case}-root'
    if farm.exists():
        shutil.rmtree(farm)
    team = farm / 'outputs/2026-10-04-zcode-team'
    team.mkdir(parents=True)
    (farm / 'outputs/ps5-mod-bridge').symlink_to(WHAT / 'outputs/ps5-mod-bridge')
    (team / 'worker-comp6-driver').symlink_to(WHAT / 'outputs/2026-10-04-zcode-team/worker-comp6-driver')
    (team / 'worker-diagnostic-sink/staging').mkdir(parents=True)  # diagnostic.c ABSENT
    return farm


# --------------------------------------------------------------- case runner
def _tool(rec):
    return Path(rec['argv'][0]).name


def _nm_defined_symbols(rec):
    return sorted({line.split()[0] for line in rec['stdout'].splitlines() if line.strip()})


def run_case(name: str, *, run_dir: Path, overlay: Path, root_farm: Path = None,
             expect_superseded: bool, expect_others_failed: list = None,
             expect_delta_matched: bool, expect_compile_attempted: bool,
             expect_tail: list = None, expect_preproof_exception: bool,
             note: str = '') -> dict:
    inst = install_wrapper(name, Path(run_dir), Path(overlay))
    stub_m, ns = inst['stub_m'], inst['ns']
    if root_farm is not None:
        ns['ROOT'] = Path(root_farm)   # wrapper resolves ROOT via its __globals__ at call time
    res = {'case': name, 'note': note,
           'expected': {'superseded': expect_superseded,
                        'others_failed': expect_others_failed,
                        'delta_matched': expect_delta_matched,
                        'compile_attempted': expect_compile_attempted,
                        'tail': expect_tail,
                        'preproof_exception_present': expect_preproof_exception},
           'install': {k: inst[k] for k in ('install_wall_s', 'main_never_called',
                                            'wrapper_installed', 'guard_msg_ok',
                                            'sys_argv_untouched')}}
    t0 = time.perf_counter()
    b = stub_m.Build(SCRATCH / f'{name}-build', link=True)   # REAL Build: real tools
    wrapper_exc = None
    try:
        ev = stub_m.verify_lineage(b)                        # REAL frozen pipeline + ACTUAL wrapper
    except Exception as exc:  # harness must observe what the wrapper propagates
        ev, wrapper_exc = None, f'{type(exc).__name__}: {exc}'
    wall = time.perf_counter() - t0
    res['wrapper_wall_s'] = wall
    res['wrapper_exception'] = wrapper_exc
    # rc provenance: Build.run records rec['rc'] = subprocess.run(...).returncode
    # (package_prx.py line 228) captured in-process from the run result object.
    all_records = [{'tool': _tool(r), 'rc': r['rc'],
                    'argv_tail': r['argv'][-2:],
                    'stderr_tail': r['stderr'][-200:] if r['rc'] else '',
                    'nm_defined': _nm_defined_symbols(r) if _tool(r) == 'llvm-nm' else None}
                   for r in b.records]
    res['records_all'] = all_records
    if all_records and all_records[0]['tool'] == 'llvm-readelf':
        wrapper_tail = all_records[1:]
    else:
        wrapper_tail = all_records
    res['wrapper_tail'] = wrapper_tail
    tail_rcs = [r['rc'] for r in wrapper_tail]
    tail_tools = [r['tool'] for r in wrapper_tail]
    nm_recs = [r for r in wrapper_tail if r['tool'] == 'llvm-nm']
    bridge_defs = nm_recs[0]['nm_defined'] if len(nm_recs) > 0 else None
    diag_defs = nm_recs[1]['nm_defined'] if len(nm_recs) > 1 else None
    if ev is not None:
        chk = ev.get('checks', {}).get(CHECK, {})
        detail = chk.get('detail') if isinstance(chk.get('detail'), dict) else {}
        recon_rec = detail.get('stage_aware_reconciliation')
        res['observed'] = {
            'status': ev.get('status'),
            'check_pass': chk.get('pass'),
            'superseded': bool(chk.get('superseded')),
            'gap_str_present': any(GAP_STR in str(g) for g in b.gaps),
            'gaps': list(map(str, b.gaps)),
            'decline_record': recon_rec,
            'decline_record_present': isinstance(recon_rec, dict),
            'attempted': recon_rec.get('attempted') if isinstance(recon_rec, dict) else None,
            'accepted': recon_rec.get('accepted') if isinstance(recon_rec, dict) else None,
            'others_failed': recon_rec.get('others_failed') if isinstance(recon_rec, dict) else None,
            'delta_matched': recon_rec.get('delta_matched') if isinstance(recon_rec, dict) else None,
            'preproof_compile_verified':
                recon_rec.get('preproof_compile_verified') if isinstance(recon_rec, dict) else None,
            'preproof_exception_present':
                isinstance(recon_rec, dict) and 'preproof_exception' in recon_rec,
            'preproof_exception_value':
                recon_rec.get('preproof_exception') if isinstance(recon_rec, dict) else None,
            'compile_attempted': len(wrapper_tail) > 0,
            'tail_tools': tail_tools,
            'tail_rcs': tail_rcs,
            'bridge_preproof_defs_accessors':
                sorted(set(bridge_defs or []) & set(BRIDGE_ACCESSORS)),
            'bridge_preproof_defs_has_diagnostic_event':
                bool(bridge_defs) and 'diagnostic_event' in bridge_defs,
            'diag_preproof_defs_has_diagnostic_event':
                bool(diag_defs) and 'diagnostic_event' in diag_defs,
        }
    checks = [
        all(inst[k] for k in ('main_never_called', 'wrapper_installed',
                              'guard_msg_ok', 'sys_argv_untouched')),
        wrapper_exc is None,   # the wrapper itself must propagate nothing (fail-closed inside)
        ev is not None,
        res.get('observed', {}).get('superseded') == expect_superseded,
        res.get('observed', {}).get('gap_str_present') == (not expect_superseded),
    ]
    obs = res.get('observed', {})
    if not expect_superseded:
        # decline path: the reconciliation record must be present and truthful
        checks.extend([
            obs.get('decline_record_present'),
            obs.get('attempted') is True,
            obs.get('accepted') is False,
            obs.get('delta_matched') == expect_delta_matched,
            obs.get('preproof_compile_verified') is False,
            obs.get('compile_attempted') == expect_compile_attempted,
        ])
        if expect_others_failed is not None:
            checks.append(obs.get('others_failed') == expect_others_failed)
    else:
        # supersede path (FIXED driver af309a3a...): the check dict is replaced,
        # but its detail now carries the reconciliation record explicitly --
        # require it with the accepted=true fields ("all fields readable") plus
        # the provider-split nm proof.
        det = None
        if ev is not None:
            chk = ev.get('checks', {}).get(CHECK, {})
            det = chk.get('detail') if isinstance(chk.get('detail'), dict) else None
        stage = (det or {}).get('stage_aware', '')
        sup_rec = (det or {}).get('stage_aware_reconciliation')
        checks.extend([
            obs.get('check_pass') is True,
            isinstance(det, dict) and 'original' in det and 'policy' in det,
            all(rn in stage for rn in RES_NAMES),
            isinstance(sup_rec, dict)
            and sup_rec.get('attempted') is True
            and sup_rec.get('accepted') is True
            and sup_rec.get('others_failed') == []
            and sup_rec.get('delta_matched') is True
            and sup_rec.get('preproof_compile_verified') is True,
            obs.get('bridge_preproof_defs_accessors') == sorted(BRIDGE_ACCESSORS),
            obs.get('diag_preproof_defs_has_diagnostic_event') is True,
            obs.get('bridge_preproof_defs_has_diagnostic_event') is False,
        ])
    if expect_tail is not None:
        checks.append(([ (t, rc) for t, rc in zip(tail_tools, tail_rcs) ] == expect_tail))
    # step 4: preproof_exception presence must match the expectation
    checks.append(obs.get('preproof_exception_present') == expect_preproof_exception)
    res['step4_preproof_exception'] = {
        'expected_present': expect_preproof_exception,
        'present': obs.get('preproof_exception_present'),
        'ok': obs.get('preproof_exception_present') == expect_preproof_exception}
    res['verdict'] = 'PASS' if all(checks) else 'FAIL'
    res['failed_checks'] = [i for i, ok in enumerate(checks) if not ok]
    return res


# --------------------------------------------------------------------- cases
def build_cases(prep: dict) -> list:
    real_rec = json.loads((RUN_L11_1 / 'receipt.json').read_text())
    cases = []
    # POSITIVE: real l11 delta + real overlay + real diagnostic.c + real
    # toolchain + production env, through the ACTUAL wrapper code path.
    cases.append(dict(
        name='P1-l11-full-real', run_dir=RUN_L11_1, overlay=OVERLAY, root_farm=None,
        expect_superseded=True, expect_others_failed=[], expect_delta_matched=True,
        expect_compile_attempted=True,
        expect_tail=[('prospero-clang++', 0), ('llvm-nm', 0),
                     ('prospero-clang', 0), ('llvm-nm', 0)],
        expect_preproof_exception=False,
        note='UPDATED wrapper, all-real inputs: dual pre-proof must compile '
             '(bridge clang++ + diagnostic.c clang, each with env_extra '
             'LLVM_CONFIG); provider-split nm defs; production env (no harness '
             'LLVM_CONFIG injection)'))
    # (a) diag.c genuinely missing -> diag pre-proof compile rc=1 -> decline
    cases.append(dict(
        name='N-a-diag-source-missing', run_dir=RUN_L11_1, overlay=OVERLAY,
        root_farm=prep['root_farm'],
        expect_superseded=False, expect_others_failed=[],
        expect_delta_matched=False,   # driver semantics: the except resets recon, so the
                                      # recorded delta_matched flips False even though the
                                      # delta set matched (recorded as a fidelity nit)
        expect_compile_attempted=True,
        expect_tail=[('prospero-clang++', 0), ('llvm-nm', 0), ('prospero-clang', 1)],
        expect_preproof_exception=True,
        note='staged diagnostic.c missing from the wrapper ROOT view: diag '
             'proof fails, no supersede; EXCEPTION case (step 4)'))
    # (b) baseline r13 bridge (compiles, defines ZERO accessors) -> pure
    #     definition-miss decline
    cases.append(dict(
        name='N-b-baseline-r13-bridge', run_dir=RUN_L11_1,
        overlay=prep['r13_overlay'], root_farm=None,
        expect_superseded=False, expect_others_failed=[], expect_delta_matched=True,
        expect_compile_attempted=True,
        expect_tail=[('prospero-clang++', 0), ('llvm-nm', 0),
                     ('prospero-clang', 0), ('llvm-nm', 0)],
        expect_preproof_exception=False,
        note='bridge swapped for baseline r13 (sha 10385e22, no accessors): '
             'bridge proof fails as PURE DEFINITION-MISS; preproof_exception '
             'must be ABSENT'))
    # (c) unexpected extra symbol in added (beyond checkpoint + 4 resolutions)
    cases.append(dict(
        name='N-c-unexpected-extra-added', run_dir=prep['extra_added_run'],
        overlay=OVERLAY, root_farm=None,
        expect_superseded=False, expect_others_failed=[], expect_delta_matched=False,
        expect_compile_attempted=False,  # recon delta-set conjunct fails -> proof skipped
        expect_tail=[], expect_preproof_exception=False,
        note="added gains 'harness_totally_unexpected_symbol'"))
    # (d) a second failing check (mutated receipt runtime_bytes)
    cases.append(dict(
        name='N-d-second-failing-check', run_dir=prep['bytes_mut_run'],
        overlay=OVERLAY, root_farm=None,
        expect_superseded=False, expect_others_failed=['composition_object_bytes'],
        expect_delta_matched=False, expect_compile_attempted=False,
        expect_tail=[], expect_preproof_exception=False,
        note='composition_object_bytes fails alongside the delta check'))
    # (e) supplementary: 5th resolution not in the allowed-names list
    cases.append(dict(
        name='N-e-unexpected-5th-resolution', run_dir=prep['extra_res_run'],
        overlay=OVERLAY, root_farm=None,
        expect_superseded=False, expect_others_failed=[], expect_delta_matched=False,
        expect_compile_attempted=False, expect_tail=[],
        expect_preproof_exception=False,
        note="expected_bootstrap_resolutions gains 'harness_extra_resolution' "
             '(added extended to match): ONLY the allowed-names filter rejects '
             '- gates wrapper change (1)'))
    return cases


def prepare_special_inputs() -> dict:
    prep = {}
    prep['root_farm'] = build_root_farm('N-a')
    prep['r13_overlay'] = build_overlay_with_bridge('N-b', R13_BRIDGE)
    prep['extra_added_run'] = make_run_dir(
        'N-c', delta_mut=lambda d: d.__setitem__(
            'added', sorted(d['added'] + ['harness_totally_unexpected_symbol'])))
    prep['bytes_mut_run'] = make_run_dir(
        'N-d', receipt_mut=lambda r: r.__setitem__('runtime_bytes', r['runtime_bytes'] + 1))
    def _extra_res(d):
        res5 = sorted(d['expected_bootstrap_resolutions'] + ['harness_extra_resolution'])
        d['expected_bootstrap_resolutions'] = res5
        d['added'] = sorted(set(d['added']) | set(res5))
    prep['extra_res_run'] = make_run_dir('N-e', delta_mut=_extra_res)
    return prep


def main() -> int:
    t_start = time.perf_counter()
    SCRATCH.mkdir(parents=True, exist_ok=True)
    print('== preflight', flush=True)
    facts = preflight()
    print(json.dumps(facts, indent=1, default=str))
    if facts['problems']:
        print('PREFLIGHT PROBLEMS:', facts['problems'])
        (OUTDIR / 'harness-results.json').write_text(json.dumps(
            {'facts': facts, 'results': [], 'preflight': 'FAILED'}, indent=1,
            default=str) + '\n')
        return 2

    prep = prepare_special_inputs()
    results = []
    print('== cases', flush=True)
    for spec in build_cases(prep):
        kw = dict(spec)
        name = kw.pop('name')
        print(f'-- {name}', flush=True)
        r = run_case(name, **kw)
        results.append(r)
        slim = {k: v for k, v in r.items() if k not in ('records_all', 'wrapper_tail')}
        print(json.dumps(slim, indent=1, default=str), flush=True)
        print('wrapper tail:', json.dumps(r.get('wrapper_tail'), default=str), flush=True)

    wall_total = time.perf_counter() - t_start
    all_ok = all(r['verdict'] == 'PASS' for r in results)
    step4_ok = all(r['step4_preproof_exception']['ok'] for r in results)
    summary = {'cases': len(results),
               'case_verdicts': {r['case']: r['verdict'] for r in results},
               'all_cases_pass': all_ok,
               'step4_all_ok': step4_ok,
               'wall_clock_s': round(wall_total, 2),
               'overall': 'GO' if (all_ok and step4_ok) else 'NO-GO'}
    (OUTDIR / 'harness-results.json').write_text(json.dumps(
        {'facts': facts, 'results': results, 'summary': summary}, indent=1,
        default=str) + '\n')
    print('== summary', json.dumps(summary, indent=1))
    return 0 if all_ok and step4_ok else 1


if __name__ == '__main__':
    sys.exit(main())
