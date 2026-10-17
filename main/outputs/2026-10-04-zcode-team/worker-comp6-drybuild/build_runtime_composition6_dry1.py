#!/usr/bin/env python3
"""composition-6 DRY BUILD driver (worker-comp6-drybuild) - Procedure B copy.

Provenance (worker-comp6-drybuild, 2026-10-04): byte-identical copy of
worker-composition-5-rebuild/build_runtime_composition5.py sha256
aaf47bdaf752e62ff845e37828cda4a89a0a612d47809ab2eb99aa5a6dcd9904
except the minimal edits listed below (REPLACE-MECHANICS.md Procedure B /
section 2 driver table). The live composition-5 driver and the pinned
composition-4-fix script are NOT modified; the pinned script sha gate below
still verifies the REAL pinned script. Edits:
  1. MY -> worker-comp6-drybuild (staging logs, chain-symbols.json, ledger
     mirror, overlay default all land under this worker dir).
  2. COMPOSE_ARGV_TAIL: --extra-unit lmf-loader-adapter= path redirected to
     this worker's staging-d1 copy (the ONLY Phase A change; D2 scaffold).
  3. checkpoint id zcode_supplier_composition_5 ->
     zcode_supplier_composition_6_dry1 (mirror key + packaging PIN) so the
     real composition-5 checkpoint entry is not overwritten.
  7. BRIDGE_H_MEASURED_SHA re-measured to ecada77e... (the real comp5 build
     staged exactly this content via --stage-header override; the live
     driver's default pin c47f7184... is a stale pre-finalization default).
  4. New --overlay-dir argument (default staging/lineage-overlay-dry1) so a
     determinism double packaging run uses a distinct overlay dir; the
     original hardcoded lineage-overlay-1 fallback is preserved.
Original docstring follows verbatim.

composition-5 one-step rebuild driver (worker-composition-5-rebuild).

Adds the missing module_start wiring to the composed runtime by replacing the
packaging-time module entry unit. Findings this driver encodes (see
REBUILD-PROCEDURE.md for the full rationale):

  * The composed runtime object (build_runtime_composition4_fixed.py -> run
    receipt, 368 units) does NOT contain module_entry: run-3's runtime.map has
    no module_start/module_entry input. The current module_entry is compiled at
    PRX-PACKAGING time by worker-prx-link-gaps/package_prx.py (sha256
    e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313) from the
    BOOTSTRAP_UNITS entry "lifecycle/module_entry.c", resolved against
    PIN["lineage"] = .superpowers/native-dependency-spike/
    native-full-runtime-aligned-entry-1 (byte-identical to
    outputs/ps5-mod-bridge/native-runtime-startup/lifecycle/module_entry.c).
  * The composition-4 packaging precedent executes that fixed script UNMODIFIED
    and adjusts only its PIN block (worker-composition-4-fix/
    package_composition4.py pattern). This driver does exactly that, plus one
    additional pinned-values adjustment of the same class: the
    module_entry BOOTSTRAP_UNITS list entry is swapped to the staged, hash-
    attested wiring delivery (lifecycle/runtime_module_entry.c behind a
    symlink overlay lineage mirror, so relative #includes keep resolving).
  * All other composition units remain unchanged: Phase A re-runs the fixed
    composition-4 script (sha256
    36cc8199b7e5a2528badd4da53ff3750075cbbe4d76ca115490b50a008ed8ff3)
    UNMODIFIED with the APPLY-ORDER Step 2 argv (identical staging copies and
    attestation receipts; every input is hash-gated by the script itself) into
    a fresh composition-5 run dir. --reuse-composition-run instead re-uses the
    existing composition-4 run-3 object after re-verifying its receipt hash
    (the composed object is bit-identical work by definition of this task).

Gates (fail-closed, no bypass flags):
  1. Both pinned scripts match their attested sha256 before load/exec.
  2. Delivered entry source matches --entry-sha256 (from the wiring worker's
     REPORT; no attestation -> refuse).
  3. Entry text defines module_start AND module_stop AND carries
     force_align_arg_pointer at least twice (the packaging static check
     measures the realignment prologue in the final PRX and fails closed).
  4. Every required startup-chain symbol (--require-symbol, default:
     mfa_activate, RuntimeOwner::start, PrepareActivatedUE4SSRuntime,
     NativeModSource ctor) is DEFINED in the composed runtime object
     (llvm-nm extern-only defined-only) BEFORE packaging is attempted, so the
     wired entry resolves the chain internally and contributes no new imports.
  5. package_prx.py's own frozen gates: lineage verification (now attesting the
     staged entry), combined-object import-contract pre-check (any new
     undefined import -> gap -> refuse), 17/17 static checks.

No console contact, no target execution, no nested subagents. Writes only
under this worker's output dir plus the script-owned run/candidate dirs.
"""
import argparse
import hashlib
import importlib.util
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path('/WORKSPACE/.codex/worktrees/translation-core/what')
TEAM = ROOT / 'outputs/2026-10-04-zcode-team'
MY = TEAM / 'worker-comp6-drybuild'

COMPOSE4_FIXED = TEAM / 'worker-composition-4-fix/build_runtime_composition4_fixed.py'
COMPOSE4_FIXED_SHA = '36cc8199b7e5a2528badd4da53ff3750075cbbe4d76ca115490b50a008ed8ff3'
PKG_FIXED = TEAM / 'worker-prx-link-gaps/package_prx.py'
PKG_FIXED_SHA = 'e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313'

SPIKE = ROOT / '.superpowers/native-dependency-spike'
OWNER = SPIKE / 'ue4ss-native-tick-file-owner-1'
RUN4 = OWNER / 'ue4ss-native-supplier-composition-4/run-3'
RUN5 = OWNER / 'ue4ss-native-supplier-composition-5/run-1'
LINEAGE = SPIKE / 'native-full-runtime-aligned-entry-1'
LLVM_BIN = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'

OLD_ENTRY_UNIT = 'lifecycle/module_entry.c'
NEW_ENTRY_UNIT = 'lifecycle/runtime_module_entry.c'
BRIDGE_UNIT = 'lifecycle/runtime_start_bridge.cpp'
TEAM_WIRING = TEAM / 'worker-module-start-wiring'
ENTRY_DEFAULT = TEAM_WIRING / 'runtime_module_entry_r3.c'
# controller r3 (2026-10-04): r2 + entry-review P2-1 fix (skip terminal O_TRUNC
# status write at MRS_ERR_START; ack sink owns the session there).
# r2 (97283afe) and r1 (1c041f8b) preserved in the same dir.
ENTRY_MEASURED_SHA = '51771ab5c5dce60334740af9e8808e69865690eb4cfbcaee9ba752518fc5a04f'
BRIDGE_DEFAULT = TEAM_WIRING / 'runtime_start_bridge_r11.cpp'
# controller r5 (2026-10-04): r4 + per-record kve_structsize VMMAP walk (the
# proven in-game parser; fixed 1160-stride was wrong for variable kernel
# records). r4/r3/r2/r1 preserved in dir.
BRIDGE_MEASURED_SHA = 'ebef44bfc42cfaa2ee8298cb3ee4ad36555d5d5828f2901c45720b78bbc391ce'
BRIDGE_H_DEFAULT = TEAM_WIRING / 'runtime_start_bridge.h'
# comp6-drybuild edit 7: re-measured 2026-10-04. The live driver's default pin
# (c47f7184...) predates the finalized header (18:28, ecada77e...); the real
# composition-5 build staged ecada77e via an explicit --stage-header override
# (see worker-composition-5-rebuild/staging/overlay-manifest.json, sha256
# ecada77e...). This copy pins the measured current delivery directly.
BRIDGE_H_MEASURED_SHA = 'ecada77e1dffe90cfa43586d8f6caed6384b7ec5ef8e06ba9280a6ada436931c'

# Headers the packaging-time units include but the lineage does not carry.
# All are hash-pinned here (measured 2026-10-04); drift refuses the build.
# The seven composed-unit headers + modforge_activation.h are the EXACT files
# the composition-4 build consumed (W/include glob pins + composition4
# replace/extra receipts); runtime_ack_supplier.hpp is the attested
# integration-patch header. Staged FLAT at the overlay root (overlay root is
# on the packaging -I path; quoted includes fall through to it).
W_INCLUDE = OWNER / 'include'
DEFAULT_STAGE_HEADERS = [
    # (overlay-relative name, source path, attested sha256)
    ('runtime_start_bridge.h', BRIDGE_H_DEFAULT, BRIDGE_H_MEASURED_SHA),
    ('modforge_activation.h', W_INCLUDE / 'modforge_activation.h',
     '4d79eb0529ba05f631405c758195e710fb979289ed2010cd9a9ef05a7549d7e6'),
    ('activated_mod_source.hpp', W_INCLUDE / 'activated_mod_source.hpp',
     'e7d92aaf70e871ee0d119e4c0644ff9c7280e0e7b5feee2d26e283642b513be0'),
    ('live_module_facts.hpp', W_INCLUDE / 'live_module_facts.hpp',
     'f114f876d04efcd83d9f79631da6fccb141f5452285795c25d3b881c62a21f93'),
    ('lmf_to_native_loader_facts.hpp', W_INCLUDE / 'lmf_to_native_loader_facts.hpp',
     'a5bc3b70fee03f988553fa99138fb2b77b855c40f3065fb40118c59bfee7a8cd'),
    ('NativeLoaderContext.hpp', W_INCLUDE / 'NativeLoaderContext.hpp',
     '18c76cdc16a7427bc056bf079a89e1383578e8374075b7244b6afb19d364dd46'),
    ('runtime_ack_supplier.hpp',
     TEAM / 'worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.hpp',
     '509037a2059d67a7d1143f0f0fb7bf8fa21fd5a3ce39ee51d22d6eaa6a59c466'),
    ('runtime_owner.hpp', W_INCLUDE / 'runtime_owner.hpp',
     '5428f8c322c3aaf9761188f53255fea6cb5aedabb5f60bd7c26ba89953dcbd2a'),
    ('ue4ss_backend.hpp', W_INCLUDE / 'ue4ss_backend.hpp',
     '7f932a587ad58c355c407e44a31434937aa1fd6d2d194ec4a75cd8c3b4ac3b0f'),
]

# Every symbol the entry + bridge TUs need from the COMPOSED runtime (plain C
# names or mangled prefixes). Measured DEFINED in run-3 exports.stdout. If any
# is missing, the wired link would turn it into a new import and the packaging
# pre-check would refuse — so we prove it up front.
DEFAULT_REQUIRE_SYMBOLS = [
    # entry, C side
    'mfa_activate',
    'mfa_status_open',
    'mfa_status_write',
    'mfa_status_close',
    'mfa_result_name',
    'mfa_preloaded_bytes',
    # bridge, C++ side (mangled prefixes)
    '_ZN9modbridge6native28StartActivatedRuntimeWithAckE',
    '_ZN9modbridge6native18StopRuntimeWithAckE',  # controller fix 2026-10-04: length byte was 19, "StopRuntimeWithAck" is 18 (matches run-3 exports.stdout)
    '_ZN2RC8Platform36TryMakeNativeLoaderFactsFromSupplierE',
    '_ZN2RC8Platform24MakeNativeProgramContextE',
    '_ZN3lmf27make_sce_live_module_os_opsE',
    '_ZN9modbridge6native12RuntimeOwner5startEv',
    '_ZN9modbridge6native28PrepareActivatedUE4SSRuntimeE',
    '_ZN2RC8Platform15NativeModSourceC',
]

# APPLY-ORDER Step 2 (worker-query-s2-arm/integration-patch/APPLY-ORDER.md),
# verbatim except --output, which this driver points at the fresh run-5 dir.
COMPOSE_ARGV_TAIL = [
    '--rust-receipt', '.superpowers/native-dependency-spike/ue4ss-native-tick-witness-1/native-1/receipt.json',
    '--host-receipt', '.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/host-final-1/receipt.json',
    '--extra-unit', 'lmf-live-module-facts=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp',
    '--extra-unit', 'lmf-sce-ops=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_os_ops_sce.cpp',
    # comp6-drybuild: D2 scaffold staging copy (Diff A + Resolution B); the staging dir also
    # carries the quoted-include intercept chain + Diff B/C header overrides
    '--extra-unit', 'lmf-loader-adapter=outputs/2026-10-04-zcode-team/worker-comp6-drybuild/staging-d1/lmf_to_native_loader_facts.cpp',
    '--extra-unit', 'lmf-base-scan=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_base_scan.cpp',
    '--extra-unit', 'native-query-harness=outputs/2026-10-04-zcode-team/worker-query-harness/src/NativeQueryHarness.cpp',
    '--extra-unit', 'module-runtime-ack-supplier=outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp',
    '--extra-define', 'LMF_DISABLE_DL_RETENTION',
    '--extra-receipt', 'outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/receipts/composition4-extra-receipt.json',
    '--extend-undefined-baseline',
    '--replace-bootstrap-unit', 'runtime_owner=outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp',
    '--replace-bootstrap-unit', 'ue4ss_backend=outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp',
    '--replace-receipt', 'outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/receipts/composition4-replace-receipt.json',
    '--replace-runtime-unit', 'unreal-unrealinitializer=outputs/2026-10-04-zcode-team/worker-query-s2-arm/staging/UnrealInitializer.cpp',
]

def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, 'rb') as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def check_sha(path: Path, expected: str, what: str) -> str:
    if not path.is_file():
        raise SystemExit(f'{what} missing: {path}')
    got = sha256_file(path)
    if got != expected:
        raise SystemExit(f'{what} sha drift: {got} != {expected} ({path})')
    print(f'[gate] {what} sha OK: {got[:16]}...')
    return got


def run(argv, log_name, log_dir: Path):
    print('[run]', ' '.join(map(str, argv))[:240], flush=True)
    p = subprocess.run([str(a) for a in argv], capture_output=True, text=True)
    (log_dir / (log_name + '.stdout')).write_text(p.stdout)
    (log_dir / (log_name + '.stderr')).write_text(p.stderr)
    if p.returncode:
        sys.stderr.write(p.stderr[-4000:])
        raise SystemExit(f'{log_name} failed rc={p.returncode}')
    return p


def compose_phase(args, logs: Path) -> Path:
    """Phase A: produce the composed runtime object (all units unchanged)."""
    if args.reuse_composition_run:
        run = Path(args.composition_run)
        rec = json.loads((run / 'receipt.json').read_text())
        if rec['status'] != 'NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED':
            raise SystemExit('reused run status drift: ' + rec['status'])
        obj = Path(rec['runtime_object'])
        got = sha256_file(obj)
        if got != rec['runtime_sha256']:
            raise SystemExit(f'reused composed object drift: {got} != {rec["runtime_sha256"]}')
        print(f'[gate] reused composed object OK: {got[:16]}... ({rec["runtime_bytes"]} bytes)')
        return run

    run = Path(args.composition_run)
    if run.exists():
        raise SystemExit(f'composition run dir already exists (script refuses it): {run}')
    argv = [sys.executable, COMPOSE4_FIXED, '--output', run] + COMPOSE_ARGV_TAIL
    # The composition script resolves receipt/staging paths relative to CWD.
    p = subprocess.run([str(a) for a in argv], cwd=str(ROOT),
                       capture_output=True, text=True)
    (logs / 'compose5.stdout').write_text(p.stdout)
    (logs / 'compose5.stderr').write_text(p.stderr)
    if p.returncode:
        sys.stderr.write(p.stderr[-5000:])
        raise SystemExit(f'composition-5 compose failed rc={p.returncode}')
    print(p.stdout.strip())
    return run


def entry_preflight(entry: Path, attested_sha: str, logs: Path) -> dict:
    """Phase B pre-flight on the delivered wiring source."""
    if not entry.is_file():
        raise SystemExit(f'wiring delivery not found: {entry} — worker-module-start-wiring '
                         'has not delivered yet; nothing to rebuild')
    got = sha256_file(entry)
    if got != attested_sha:
        raise SystemExit(f'entry attestation mismatch: {got} != {attested_sha} '
                         f'({entry}); get the digest from the wiring worker REPORT')
    text = entry.read_text(errors='replace')
    # Alignment attribute may be applied directly or via a macro (the delivered
    # entry defines MODBRIDGE_ENTRY_ATTRS once and annotates both entries).
    direct = len(re.findall(r'force_align_arg_pointer', text))
    macro_names = set(re.findall(r'#define\s+(\w+)\s+[^\n]*force_align_arg_pointer', text))
    macro_uses = sum(len(re.findall(r'\b%s\b' % re.escape(name), text)) - 1
                     for name in macro_names)
    evidence = {
        'entry_source': str(entry),
        'entry_sha256': got,
        'defines_module_start': bool(re.search(r'\bint\s+module_start\s*\(', text)),
        'defines_module_stop': bool(re.search(r'\bint\s+module_stop\s*\(', text)),
        'force_align_arg_pointer_count': direct,
        'alignment_macros': sorted(macro_names),
        'alignment_annotations': direct + macro_uses,
        'entry_guard_serialized': '__atomic_test_and_set(&entry_guard' in text,
        'fail_open_log_line': 'UE4SS.log' in text and 'module_start failed' in text,
        'chain_symbol_references': {s: (s in text) for s in
                                    ['mfa_activate', 'modbridge_runtime_start_with_activation',
                                     'modbridge_runtime_stop', 'MRS_OK']},
        'drops_modbridge_ctor_markers': 'modbridge_ctor_phase' not in text,
    }
    if not (evidence['defines_module_start'] and evidence['defines_module_stop']):
        raise SystemExit(f'entry does not define both module_start and module_stop: {entry}')
    if evidence['alignment_annotations'] < 2:
        raise SystemExit('entry lacks force_align_arg_pointer (directly or via a macro '
                         'used on BOTH entries); the packaging static check measures the '
                         'realignment prologue and would fail closed — refuse early')
    if evidence['drops_modbridge_ctor_markers']:
        print('[note] entry drops modbridge_ctor_phase/modbridge_ctor_value; verified safe: '
              'the composition-4 composed object does not reference them (run-3 '
              'undefined.stdout has no modbridge_ctor*), so no new import can appear.')
    (logs / 'entry-preflight.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('[gate] entry attestation + export surface OK:', got[:16], '...')
    return evidence


def chain_symbols_gate(run_dir: Path, require: list) -> dict:
    rec = json.loads((run_dir / 'receipt.json').read_text())
    obj = Path(rec['runtime_object'])
    p = subprocess.run([str(LLVM_BIN / 'llvm-nm'), '--extern-only', '--defined-only',
                        '--format=posix', str(obj)], capture_output=True, text=True)
    if p.returncode:
        raise SystemExit('llvm-nm failed on composed object: ' + p.stderr[-500:])
    defined = {line.split()[0] for line in p.stdout.splitlines() if line.strip()}
    result = {}
    for sym in require:
        hit = sym in defined or any(d.startswith(sym) for d in defined)
        result[sym] = bool(hit)
        if not hit:
            raise SystemExit(f'startup-chain symbol NOT defined by composed runtime: {sym}; '
                             'the wired entry would import it and the packaging import '
                             'pre-check would refuse — reconcile the composition first')
    (MY / 'staging/chain-symbols.json').write_text(json.dumps(result, indent=2) + '\n')
    print('[gate] startup-chain symbols defined in composed object:', len(result), 'of', len(result))
    return result


def build_overlay(entry: Path, stage_headers: list, stage_units: list,
                  bridge: Path, overlay: Path = None) -> tuple:
    """Symlink-overlay lineage mirror with the wired entry + bridge TU as
    lifecycle units and all extra headers staged flat (hash-pinned)."""
    if overlay is None:
        overlay = MY / 'staging/lineage-overlay-1'
    if overlay.exists():
        raise SystemExit(f'overlay already exists (delete it consciously first): {overlay}')
    overlay.mkdir(parents=True)
    for child in LINEAGE.iterdir():
        if child.name == 'lifecycle':
            continue
        (overlay / child.name).symlink_to(child)
    lc = overlay / 'lifecycle'
    lc.mkdir()
    for child in (LINEAGE / 'lifecycle').iterdir():
        (lc / child.name).symlink_to(child)
    manifest = {'headers': {}, 'units': {}}
    # hash-pinned default headers (flat at overlay root, which is on -I path)
    staged = {(Path(rel).name): (rel, src, sha) for rel, src, sha in DEFAULT_STAGE_HEADERS}
    for spec in stage_headers:
        rel, sep, src = spec.partition('=')
        if not sep or not rel or not Path(src).is_file():
            raise SystemExit('bad --stage-header spec (REL=SRC): ' + spec)
        staged[rel] = (rel, Path(src), None)  # explicit override, no pin (controller fix: key by full rel so same-basename headers in different subdirs both stage)
    for name, (rel, src, sha) in staged.items():
        dest = overlay / rel
        if dest.exists() or dest.is_symlink():
            raise SystemExit('--stage-header would shadow an overlay path: ' + rel)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        manifest['headers'][rel] = {'source': str(src), 'sha256': sha256_file(src),
                                    'attested_sha256': sha}
        if sha and manifest['headers'][rel]['sha256'] != sha:
            raise SystemExit(f'staged header drift: {src}\n  measured '
                             f"{manifest['headers'][rel]['sha256']}\n  pinned   {sha}")
    # entry + bridge TU as packaging lifecycle units
    shutil.copy2(entry, lc / 'runtime_module_entry.c')
    (lc / 'runtime_module_entry.c.sha256').write_text(sha256_file(entry) + '\n')
    manifest['units'][NEW_ENTRY_UNIT] = {'source': str(entry), 'sha256': sha256_file(entry)}
    if bridge is not None:
        shutil.copy2(bridge, lc / 'runtime_start_bridge.cpp')
        manifest['units'][BRIDGE_UNIT] = {'source': str(bridge), 'sha256': sha256_file(bridge)}
    for spec in stage_units:
        rel, sep, src = spec.partition('=')
        if not sep or not rel or not Path(src).is_file():
            raise SystemExit('bad --stage-unit spec (REL=SRC): ' + spec)
        dest = overlay / rel
        if dest.exists() or dest.is_symlink():
            raise SystemExit('--stage-unit would shadow an overlay path: ' + rel)
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        manifest['units'][rel] = {'source': str(src), 'sha256': sha256_file(src)}
    (MY / 'staging/overlay-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('[gate] overlay lineage ready:', overlay,
          f"({len(manifest['headers'])} headers, {len(manifest['units'])} staged units)")
    return overlay, manifest


def package_phase(run_dir: Path, overlay: Path, out: Path,
                  extra_units: list) -> None:
    """Execute the unmodified fixed packaging script with adjusted PIN block."""
    check_sha(PKG_FIXED, PKG_FIXED_SHA, 'fixed package_prx.py')
    rec = json.loads((run_dir / 'receipt.json').read_text())
    if rec['status'] != 'NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED':
        raise SystemExit('composition run status drift: ' + rec['status'])
    if sha256_file(Path(rec['runtime_object'])) != rec['runtime_sha256']:
        raise SystemExit('composed object hash drift vs run receipt')

    real_ledger = ROOT / '.superpowers/native-observations/transport-experiments-progress.json'
    led = json.loads(real_ledger.read_text())
    led['zcode_supplier_composition_6_dry1'] = {
        'utc': __import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat(),
        'actor': 'worker-composition-5-rebuild (PROVISIONAL mirror entry; real ledger '
                 'owned by controller)',
        'status': rec['status'],
        'composition_run': str(run_dir),
        'runtime_object': rec['runtime_object'],
        'runtime_sha256': rec['runtime_sha256'],
        'runtime_bytes': rec['runtime_bytes'],
        'wiring': 'module_start wiring: packaging-time lifecycle units replaced/extended '
                  'by the hash-attested runtime_module_entry.c + runtime_start_bridge.cpp '
                  '(worker-module-start-wiring) via symlink-overlay lineage mirror; all '
                  'composition units unchanged',
    }
    mirror = MY / 'transport-ledger.composition5-mirror.json'
    mirror.write_text(json.dumps(led, indent=1) + '\n')

    spec = importlib.util.spec_from_file_location('package_prx_fixed', PKG_FIXED)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)

    # Controller addition 2026-10-04: the frozen packaging CLANG_FLAGS carry no
    # -std (C-oriented); the bridge is the first C++ bootstrap unit and its
    # headers need C++20 (std::span). Inject -std=c++20 into C++ compile
    # invocations only, in-memory; the pinned packaging file stays byte-identical
    # and b.run's logged argv/receipts show the real command used.
    _orig_run = m.Build.run
    def _cpp20_run(self, argv, **kw):
        argv = [str(a) for a in argv]
        if argv and argv[0].endswith(('prospero-clang++', 'clang++')) \
                and '-c' in argv and '-std=c++20' not in argv:
            argv = [argv[0], '-std=c++20'] + argv[1:]
        return _orig_run(self, argv, **kw)
    m.Build.run = _cpp20_run

    units = list(m.BOOTSTRAP_UNITS)
    if OLD_ENTRY_UNIT not in units:
        raise SystemExit('packaging script BOOTSTRAP_UNITS drifted; expected ' + OLD_ENTRY_UNIT)
    if NEW_ENTRY_UNIT in units or BRIDGE_UNIT in units:
        raise SystemExit('packaging script already references a composition-5 unit')
    units[units.index(OLD_ENTRY_UNIT)] = NEW_ENTRY_UNIT
    units.extend(extra_units)
    m.BOOTSTRAP_UNITS[:] = units  # in-place: same pinned-values adjustment class as PIN

    m.PIN['lineage'] = overlay
    m.PIN['composition_dir'] = run_dir
    m.PIN['composition_object_name'] = 'coherent-tick-file-witness-runtime.o'
    m.PIN['composition_object_sha256'] = rec['runtime_sha256']
    m.PIN['composition_object_bytes'] = rec['runtime_bytes']
    m.PIN['composition_checkpoint'] = 'zcode_supplier_composition_6_dry1'
    m.PIN['ledger'] = mirror

    if out.exists():
        raise SystemExit(f'output dir exists: {out}')
    sys.argv = ['package_prx.py', '--link', '--out', str(out)]
    print('driver: PIN adjustments applied (lineage->overlay with wired entry%s, '
          'checkpoint zcode_supplier_composition_5); running fixed package_prx.py --link'
          % (' + bridge' if extra_units else ''), flush=True)
    raise SystemExit(m.main())


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--entry-source', type=Path, default=ENTRY_DEFAULT,
                    help='delivered wired entry (default: worker-module-start-wiring/'
                         'runtime_module_entry.c)')
    ap.add_argument('--entry-sha256', default=ENTRY_MEASURED_SHA,
                    help='attested sha256 of the delivered entry (default: the pin this '
                         'driver was prepared with, measured from the delivered file; '
                         'reconcile with the wiring worker REPORT when it lands)')
    ap.add_argument('--bridge-source', type=Path, default=BRIDGE_DEFAULT,
                    help='delivered bridge TU (default: worker-module-start-wiring/'
                         'runtime_start_bridge.cpp; compiled as an extra packaging unit)')
    ap.add_argument('--no-bridge-unit', action='store_true',
                    help='do NOT add the bridge TU to the packaging link (only if the '
                         'bridge ships some other way — the wired entry calls '
                         'modbridge_runtime_start_with_activation, so without the bridge '
                         'the combined-object pre-check WILL refuse)')
    ap.add_argument('--stage-unit', action='append', default=[],
                    help='REL=SRC; additional staging file compiled as an extra '
                         'packaging unit (relative to the overlay lineage root)')
    ap.add_argument('--composition-run', type=Path, default=None,
                    help='composition run dir (default: fresh composition-5 run-1 for '
                         'Phase A, composition-4 run-3 with --reuse-composition-run)')
    ap.add_argument('--reuse-composition-run', action='store_true',
                    help='skip Phase A; re-verify and re-use an existing composed run '
                         '(default target: composition-4 run-3, pass --composition-run '
                         'to override)')
    ap.add_argument('--verify-only', action='store_true',
                    help='run the cheap gates only (entry pre-flight + chain-symbol '
                         'check against the existing composed run) and exit; no '
                         'composition, no overlay, no packaging')
    ap.add_argument('--require-symbol', action='append', default=[],
                    help='extra symbol (plain or mangled prefix) that must be DEFINED by '
                         'the composed runtime before packaging')
    ap.add_argument('--stage-header', action='append', default=[],
                    help='REL=SRC; copy SRC into the overlay lineage at REL so the new '
                         'entry can include headers that live outside the lineage')
    ap.add_argument('--overlay-dir', type=Path, default=None,
                    help='comp6-drybuild: lineage overlay dir (default: MY/staging/lineage-overlay-dry1; distinct per packaging run)')
    ap.add_argument('--out', type=Path, default=MY / 'prx-candidate-composition5-1',
                    help='packaging output dir (must not exist)')
    args = ap.parse_args()
    if args.composition_run is None:
        args.composition_run = RUN4 if args.reuse_composition_run else RUN5

    check_sha(COMPOSE4_FIXED, COMPOSE4_FIXED_SHA, 'fixed build_runtime_composition4_fixed.py')
    logs = MY / 'staging'
    logs.mkdir(parents=True, exist_ok=True)

    entry_preflight(Path(args.entry_source), args.entry_sha256, logs)
    if not args.no_bridge_unit:
        bridge = Path(args.bridge_source)
        if not bridge.is_file():
            raise SystemExit(f'bridge TU not found: {bridge} — the wired entry calls '
                             'modbridge_runtime_start_with_activation; refusing to build '
                             'without it (--no-bridge-unit overrides consciously)')
        bridge_got = sha256_file(bridge)
        if bridge_got != BRIDGE_MEASURED_SHA:
            raise SystemExit(f'bridge source drift vs prepared pin: {bridge_got} != '
                             f'{BRIDGE_MEASURED_SHA} ({bridge}) — reconcile with the '
                             'wiring worker before building')
        extra_units = [BRIDGE_UNIT]
    else:
        bridge, extra_units = None, []
    extra_units += [spec.partition('=')[0] for spec in args.stage_unit]

    if args.verify_only:
        if not args.reuse_composition_run and args.composition_run == RUN5:
            verify_run = RUN4  # the existing composed object, for the symbol gate
        else:
            verify_run = args.composition_run
        chain_symbols_gate(verify_run, DEFAULT_REQUIRE_SYMBOLS + args.require_symbol)
        print('VERIFY-ONLY PASS: entry pre-flight OK; all',
              len(DEFAULT_REQUIRE_SYMBOLS) + len(args.require_symbol),
              'required symbols defined in', str(verify_run))
        return

    run_dir = compose_phase(args, logs)
    chain_symbols_gate(run_dir, DEFAULT_REQUIRE_SYMBOLS + args.require_symbol)
    overlay, _manifest = build_overlay(Path(args.entry_source), args.stage_header,
                                       args.stage_unit, bridge,
                                       args.overlay_dir or (MY / 'staging/lineage-overlay-dry1'))
    package_phase(run_dir, overlay, Path(args.out), extra_units)


if __name__ == '__main__':
    main()
