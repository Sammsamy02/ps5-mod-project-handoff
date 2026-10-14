#!/usr/bin/env python3
"""L11-VTABLE-PREBUILD COPY (worker-launch11-vtable-prebuild) - composition-6 driver.

Copy of worker-d1-determinism-diagnosis/build_runtime_composition6_d1fix.py
(sha 27d85da3826fdbf0090869af86f7094f0211b498e820c2d003aa4520a452d550, the
py3.9-safe reuse-fixed D1FIX lineage, D1FIX E1-E7 all retained verbatim) with
exactly two additional tagged edits:

  L11PB E1  MY -> this worker dir (scope isolation: logs/odr/receipt/entry
            evidence, chain-symbols.json, overlay-manifest.json and the
            transport-ledger mirror are written under
            outputs/2026-10-04-zcode-team/worker-launch11-vtable-prebuild/;
            the worker-d1-determinism-diagnosis evidence stays untouched)
  L11PB E2  the initializer --replace-runtime-unit path points at THIS
            worker's PREPARED-UNARMED staging copy
            staging/initializer-vtable-pre/UnrealInitializer.cpp — the
            UnrealInitializer.cpp content differs from the launch-9/launch-10
            lineage (c85c42f7...) ONLY in the two staged sibling include lines
            (VT-R1 S5/S6: quoted includes of the seamed DetourInstance.hpp /
            DetourSubclasses.hpp staging copies), and the seamed staged headers
            preprocess to a token stream IDENTICAL to the port headers while
            disarmed (measured), so the initializer object is byte-identical to
            launch-10's (983b18e2..., measured). This is LAUNCH10-MATRIX
            U-SWAP-2 PREPARED-UNARMED: the DispatchDataDetour backend ships
            compile-proven but NOT selected (UE4SS_DISPATCH_DATA_BACKEND=0).
            The armed SeedTable.h (59effe6e...) carries over from the launch-10
            armed staging unchanged (U-SWAP-1 already in this lineage).

Everything else (compose argv, packaging pins, gates, C++20 injection,
ledger-mirror semantics, D1FIX E4 reuse fix) is byte-identical to the d1fix
driver. Fresh compose + package runs are dispatched with explicit
--composition-run/--overlay-dir/--out into this worker's dirs.
"""
# --- original docstring preserved below ---
"""composition-6 one-step rebuild driver (worker-comp6-driver).

Fork of worker-composition-5-rebuild/build_runtime_composition5.py implementing
RESOLUTION-DESIGN.md §3.2/§3.3 (worker-configured-roots-design) for the
Resolution-B (configured roots) change-set:

  * Phase A executes the bounded fixed-script edition
    build_runtime_composition6_fixed.py (sha256-pinned below; lineage documented
    in its docstring — fork of the composition-4-fix script 36cc8199...). The
    edition adds ONLY the §3.2 mechanism: attested retained-unit replacement of
    {"program"} (staged UE4SSProgram.cpp copy, own pinned argv, fresh-.d
    coverage gate) + pre-seeded replace-receipt pins for every file the fresh
    dependency walks touch. All frozen gates unchanged.
  * ODR gate (§3.3.1, new, fail-closed): the port-tree
    Platform/NativeProgramContext.hpp (compiled into the recompiled `program`
    unit) and the staged copy restaged flat into the packaging overlay
    (compiled into the bridge/factory packaging TUs) MUST be byte-identical;
    the driver sha-compares both and refuses pre-flight on drift.
  * Unchanged mechanisms (same pin names as the composition-5 driver):
    extra-unit adapter redirect (--extra-unit lmf-loader-adapter=...),
    hash-pinned staged headers (--stage-header REL=SRC[=SHA256], full
    composition-5-2 31-header overlay set by default), entry r3 /
    bridge packaging pins, --replace-bootstrap-unit pair, extra-receipt +
    replace-receipt attestations, 10-symbol DEFAULT_REQUIRE_SYMBOLS chain gate,
    unmodified fixed package_prx.py (17/17 static checks).

New/changed defaults vs composition-5:
  - bridge default = runtime_start_bridge_r13.cpp (r13 = r12 + the
    GO-WITH-FIXES review diffs; Resolution-B wiring, §2.7, unchanged);
    r13 sha pin + the renumbered runtime_start_bridge.h staging pin are
    2026-10-04 measurements by worker-bridge-r13-polish (reconcile with the
    controller REPORT; pass --bridge-sha256 if it moved).
  - "edited content landed" gates: the driver refuses to run against
    UNEDITED Resolution-B sources (adapter cpp, port program/header,
    loadercontext cpp/hpp, lmf hpp) so a no-op rebuild cannot ship silently.
    --allow-unedited exists ONLY for the dry-form rehearsal with unmodified
    stand-ins.
  - fresh run dir default .../ue4ss-native-supplier-composition-6/run-1;
    overlay lineage-overlay-2; checkpoint zcode_supplier_composition_6;
    candidate prx-candidate-composition6-1.

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
MY = TEAM / 'worker-launch11-vtable-prebuild'  # L11PB E1: scope isolation (was worker-d1-determinism-diagnosis)
RECEIPTS = TEAM / 'worker-comp6-driver/receipts'  # D1FIX E2: attestations live with the original driver

COMPOSE6_FIXED = TEAM / 'worker-comp6-driver/build_runtime_composition6_fixed.py'  # D1FIX E3 (sha-pinned below)
COMPOSE6_FIXED_SHA = '8dd92a6fe4b71cf194aa180adb155e00b98cdae682425b5c825032518fef8a10'
PKG_FIXED = TEAM / 'worker-prx-link-gaps/package_prx.py'
PKG_FIXED_SHA = 'e6fbe6c5011b498e0c26ee88bdc6c84aacab204b6ccad5696a78353afef5c313'

SPIKE = ROOT / '.superpowers/native-dependency-spike'
OWNER = SPIKE / 'ue4ss-native-tick-file-owner-1'
RUN4 = OWNER / 'ue4ss-native-supplier-composition-4/run-3'
RUN6 = OWNER / 'ue4ss-native-supplier-composition-6/run-1'
LINEAGE = SPIKE / 'native-full-runtime-aligned-entry-1'
LLVM_BIN = ROOT / 'outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin'
CR1 = SPIKE / 'native-coherent-runtime-1'
UONB = SPIKE / 'ue4ss-unreal-native-build-1/source-dependencies/ue4ss/deps/first'

OLD_ENTRY_UNIT = 'lifecycle/module_entry.c'
NEW_ENTRY_UNIT = 'lifecycle/runtime_module_entry.c'
BRIDGE_UNIT = 'lifecycle/runtime_start_bridge.cpp'
TEAM_WIRING = TEAM / 'worker-module-start-wiring'
ENTRY_DEFAULT = TEAM_WIRING / 'runtime_module_entry_r4.c'
ENTRY_MEASURED_SHA = 'c97055f40f5a6dd94dd6360c4459538b8130e8b63a806a84d975d7c7604db470'
BRIDGE_DEFAULT = TEAM_WIRING / 'runtime_start_bridge_r13.cpp'
# measured 2026-10-04 by worker-bridge-r13-polish (r13 = r12 feed5254... plus
# the GO-WITH-FIXES review diffs; prospero -c compile verified, import set
# identical to r12); reconcile with the controller comp6-integration REPORT
# before the real pass.
BRIDGE_MEASURED_SHA = '10385e2265af0a12236bc7f713ed343a7f13796644aac0de511f6663f579209a'

# Resolution-B canonical sources + their UNEDITED baselines (measured
# 2026-10-04 pre-landing). The "edited landed" gates refuse these hashes
# unless --allow-unedited (dry-form rehearsal only).
PORT_PROGRAM = CR1 / 'source/port/ue4ss/src/UE4SSProgram.cpp'
PORT_PROGRAM_UNEDITED = 'dfdd5014a0bf74663152fa74a7f5d2b245517252788e6bc30225cdc71e3ee907'
ODR_HEADER_PORT = CR1 / 'source/port/ue4ss/include/Platform/NativeProgramContext.hpp'
ODR_HEADER_UNEDITED = '664996150d3fec6df2471b1bf8c0cb231cf9f7a54d17a9d52595cc60d08b1868'
ADAPTER_DEFAULT = OWNER / 'src/lmf_to_native_loader_facts.cpp'
ADAPTER_UNEDITED = 'b6d13bf9da24384a19edb5bf4cead635e305369b723ff1c70fc684d9b97f4410'
W = OWNER / 'include'
LC_CPP_DEFAULT = OWNER / 'src/NativeLoaderContext.cpp'
LC_CPP_UNEDITED = '2a17b66ca4025f83c5e7bafb030ba098a556d5d366cb34362e8c6d6fdda7e5ca'
LC_HPP_DEFAULT = W / 'NativeLoaderContext.hpp'
LC_HPP_UNEDITED = '18c76cdc16a7427bc056bf079a89e1383578e8374075b7244b6afb19d364dd46'
LMF_HPP_DEFAULT = W / 'lmf_to_native_loader_facts.hpp'
LMF_HPP_UNEDITED = 'a5bc3b70fee03f988553fa99138fb2b77b855c40f3065fb40118c59bfee7a8cd'

DEFAULT_EXTRA_RECEIPT = RECEIPTS / 'composition6-extra-receipt.json'
DEFAULT_REPLACE_RECEIPT = RECEIPTS / 'composition6-replace-receipt.json'

# Full composition-5-2 overlay staging set (31 headers; manifest measured
# 2026-10-04). Pin = comp-5-2 staged sha for every UNCHANGED header (drift
# refuses). The three Resolution-B headers default UNPINNED from their
# canonical sources (the ODR gate pins the port one; --stage-header REL=SRC=SHA
# may pin them explicitly).
DEFAULT_STAGE_HEADERS = [
    # (overlay-relative name, source path, attested sha256 or None)
    ('runtime_start_bridge.h', TEAM_WIRING / 'runtime_start_bridge.h',
     '4bb024134385fd548e31fa736264ffc4014ddf12e3813137f140c4c6be88e930'),
    ('modforge_activation.h', W / 'modforge_activation.h',
     '4d79eb0529ba05f631405c758195e710fb979289ed2010cd9a9ef05a7549d7e6'),
    ('activated_mod_source.hpp', W / 'activated_mod_source.hpp',
     'e7d92aaf70e871ee0d119e4c0644ff9c7280e0e7b5feee2d26e283642b513be0'),
    ('live_module_facts.hpp', W / 'live_module_facts.hpp',
     'f114f876d04efcd83d9f79631da6fccb141f5452285795c25d3b881c62a21f93'),
    ('lmf_to_native_loader_facts.hpp', LMF_HPP_DEFAULT, None),      # Resolution-B edited
    ('NativeLoaderContext.hpp', LC_HPP_DEFAULT, None),              # Resolution-B edited
    ('runtime_ack_supplier.hpp',
     TEAM / 'worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.hpp',
     '509037a2059d67a7d1143f0f0fb7bf8fa21fd5a3ce39ee51d22d6eaa6a59c466'),
    ('runtime_owner.hpp', W / 'runtime_owner.hpp',
     '5428f8c322c3aaf9761188f53255fea6cb5aedabb5f60bd7c26ba89953dcbd2a'),
    ('ue4ss_backend.hpp', W / 'ue4ss_backend.hpp',
     '7f932a587ad58c355c407e44a31434937aa1fd6d2d194ec4a75cd8c3b4ac3b0f'),
    ('Helpers/Common.hpp', UONB / 'Helpers/include/Helpers/Common.hpp',
     'e5286b1eeed8630b979dcb9905a1dc04654f507e019f31c50fd7cc0aed647cf9'),
    ('Helpers/UETargetModules.hpp', CR1 / 'source/port/first/include/Helpers/UETargetModules.hpp',
     '90c41a8545bb783af7aef82459fe4c49c1fb1657df02df9d6059b71d033bce45'),
    ('OwnedMappedCandidateSelection.hpp', W / 'OwnedMappedCandidateSelection.hpp',
     '3156a109626442df555fb0c6b622c78482209c7ea973f599239269d3cd54e0c5'),
    ('OwnedMappedImageCaller.hpp', SPIKE / 'ue4ss-native-ps-scan-provider-1/include/OwnedMappedImageCaller.hpp',
     'fb8aabec09e8fef338489662a196f41b2874693d07e17e95dc63c92e7ff2bacf'),
    ('OwnedMappedResolverCollector.hpp', SPIKE / 'ue4ss-native-mapped-candidate-selection-1/include/OwnedMappedResolverCollector.hpp',
     '11488d860bf271f0267283883c7e29e6b28e3a0ff176c081079c198fcb168fa1'),
    ('OwnedMappedScanSource.hpp', W / 'OwnedMappedScanSource.hpp',
     '55b7a06ab1c7947eecc87d3efa921c54237cac619cf52a59df6fbfc0aba385d8'),
    ('OwnedSerializedElfSource.hpp', W / 'OwnedSerializedElfSource.hpp',
     'cbad47330073a1cc7e4983d43bbd9853de72faf84d0baf1b4cef3d7f44a96820'),
    ('Platform/NativeModSource.hpp', CR1 / 'source/port/ue4ss/include/Platform/NativeModSource.hpp',
     '2b44df14854934470a16fec6ef153501e08a8554ba51748b335fd0733856a450'),
    ('Platform/NativeProgramContext.hpp', None, None),              # ODR-gated staged copy
    ('PsMappedImageCaller.hpp', SPIKE / 'ue4ss-native-ps-scan-abi-1/include/PsMappedImageCaller.hpp',
     'f04ce4aa1dcf0dfcf5395983bc1f3af02a01b70ef7b1ecb4a9708c674af7000f'),
    ('PsScanAbi.h', SPIKE / 'ue4ss-native-ps-scan-abi-1/include/PsScanAbi.h',
     '7647b3ff5cda515bf440e70dcf0870bea310868bc643a160e0ef0ef9183790ee'),
    ('SigScanner/Common.hpp', UONB / 'SinglePassSigScanner/include/SigScanner/Common.hpp',
     '88e708ada07525df4d849c8088ac7b0abeac932159bc0d221308ffcc38d5eed9'),
    ('SigScanner/SinglePassSigScanner.hpp', CR1 / 'source/port/first/include/SigScanner/SinglePassSigScanner.hpp',
     'fd4c706eb980ae7c08efcf27a2d4051e09901b2a70fdca215e90b8c1170e1b8a'),
    ('Unreal/PlatformModuleSnapshot.hpp', CR1 / 'source/port/unreal/include/Unreal/PlatformModuleSnapshot.hpp',
     '3d05b06841a7e3f1bbc12834b47936cd2690c8eb974edaf5cb84a46f13cfa5ec'),
    ('live_module_base_scan.hpp', W / 'live_module_base_scan.hpp',
     '5da45cd31df5b3107b40d1c5818f5d05b693246c3d10388fb2e85b2e90bd39c0'),
    ('live_module_os_ops.hpp', W / 'live_module_os_ops.hpp',
     '41c17270d4c9287c2eb6bb14068e46a8cf3e2770fecbc4dd41f82665a67bd0d3'),
    ('patternsleuth_mapped_candidates.h', SPIKE / 'ue4ss-native-mapped-candidates-1/include/patternsleuth_mapped_candidates.h',
     'f3ac1f9653bc8ee4fa1473985fa69f6db7ec25e129b5e767bd028b8f98f232ef'),
    ('polyhook2/Enums.hpp', SPIKE / 'ue4ss-unreal-native-build-1/source-dependencies/polyhook/polyhook2/Enums.hpp',
     '8a2b2eba9efae2846c67a186f1604c89fca520ff741044f34142b309353ea2f4'),
    ('polyhook2/OwnedMemory.hpp', SPIKE / 'ue4ss-native-read-lease-1/include/polyhook2/OwnedMemory.hpp',
     '6a8fe108af18d70df9c6f3cf6e2403034d0debbd4c229047dafbbb4247c968aa'),
    ('polyhook2/PolyHookOs.hpp', SPIKE / 'ue4ss-polyhook-platform-port-1/patched/include/polyhook2/PolyHookOs.hpp',
     '6e587efeb63509b8bfcffebb2ef32757fcfca3e56545c1df472f6317bdbc3c63'),
    ('sce_module_abi.hpp', W / 'sce_module_abi.hpp',
     'c8a04cab824b11c38e10f62a46169f03dceac05e357a03878e8bb2ff64af50c1'),
]

DEFAULT_REQUIRE_SYMBOLS = [
    'mfa_activate',
    'mfa_status_open',
    'mfa_status_write',
    'mfa_status_close',
    'mfa_result_name',
    'mfa_preloaded_bytes',
    '_ZN9modbridge6native28StartActivatedRuntimeWithAckE',
    '_ZN9modbridge6native18StopRuntimeWithAckE',
    '_ZN2RC8Platform36TryMakeNativeLoaderFactsFromSupplierE',
    '_ZN2RC8Platform24MakeNativeProgramContextE',
    '_ZN3lmf27make_sce_live_module_os_opsE',
    '_ZN9modbridge6native12RuntimeOwner5startEv',
    '_ZN9modbridge6native28PrepareActivatedUE4SSRuntimeE',
    '_ZN2RC8Platform15NativeModSourceC',
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


def odr_gate(port: Path, staged: Path, logs: Path) -> dict:
    """§3.3.1 ODR sha-equality gate: port-tree Platform/NativeProgramContext.hpp
    (compiled into the recompiled program unit) vs the staged copy restaged flat
    into the packaging overlay (compiled into bridge/factory packaging TUs).
    Byte-identical or refuse."""
    if not port.is_file():
        raise SystemExit(f'ODR port header missing: {port}')
    if not staged.is_file():
        raise SystemExit(f'ODR staged header missing: {staged}')
    ps, ss = sha256_file(port), sha256_file(staged)
    evidence = {'port_header': str(port), 'staged_header': str(staged),
                'port_sha256': ps, 'staged_sha256': ss,
                'byte_identical': ps == ss}
    if ps != ss:
        (logs / 'odr-gate.json').write_text(json.dumps(evidence, indent=2) + '\n')
        raise SystemExit(f'ODR GATE REFUSED: port {port} ({ps[:16]}...) != staged '
                         f'{staged} ({ss[:16]}...); the recompiled program unit and the '
                         'packaging bridge/factory TUs would see different '
                         'NativeProgramContext layouts (§3.3.1). Reconcile the staged '
                         'copy with the port tree — never bypass.')
    (logs / 'odr-gate.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('[gate] ODR sha-equality OK:', ps[:16], '...')
    return evidence


def receipt_preflight(replace_receipt: Path, extra_receipt: Path,
                      program_source: Path, adapter_source: Path,
                      odr_port: Path, lc_cpp: Path, logs: Path) -> dict:
    """§3.2 pre-flight: the replace-receipt must cover the staged program copy,
    the (edited) port header and the loadercontext TU; the extra-receipt must
    cover the adapter source. Hashes are re-measured here."""
    for p in (replace_receipt, extra_receipt):
        if not p.is_file():
            raise SystemExit(f'attestation receipt missing: {p} — run '
                             'regen_replace_receipt.py / regen_extra_receipt.py '
                             'AFTER the controller edits land (§3.3 step 2)')
    rr = json.loads(replace_receipt.read_text())
    er = json.loads(extra_receipt.read_text())
    required = {
        'program staged copy': program_source,
        'port NativeProgramContext.hpp': odr_port,
        'NativeLoaderContext.cpp': lc_cpp,
    }
    files = rr.get('files', {})
    evidence = {'replace_receipt': str(replace_receipt),
                'replace_receipt_files': len(files),
                'extra_receipt': str(extra_receipt),
                'extra_receipt_files': len(er.get('files', {})),
                'required_coverage': {}}
    for what, path in required.items():
        rp = str(path.resolve())
        if rp not in files:
            raise SystemExit(f'replace-receipt does not cover the {what}: {path} — '
                             'regenerate with regen_replace_receipt.py')
        got = sha256_file(path)
        if got != files[rp]:
            raise SystemExit(f'{what} drifted after receipt generation: {path} '
                             f'({got[:16]}... != attested {files[rp][:16]}...) — regenerate receipts')
        evidence['required_coverage'][what] = {'path': rp, 'sha256': got}
    ap_ = str(adapter_source.resolve())
    if ap_ not in er.get('files', {}):
        raise SystemExit(f'extra-receipt does not cover the adapter source: {adapter_source} '
                         '— regenerate with regen_extra_receipt.py')
    got = sha256_file(adapter_source)
    if got != er['files'][ap_]:
        raise SystemExit(f'adapter source drifted after receipt generation: {adapter_source} '
                         '— regenerate receipts')
    evidence['required_coverage']['adapter source'] = {'path': ap_, 'sha256': got}
    (logs / 'receipt-preflight.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print('[gate] receipt coverage OK:', len(files), 'replace-receipt files,',
          len(er.get('files', {})), 'extra-receipt files')
    return evidence


def edited_landed_gate(args) -> None:
    """Refuse a no-op rebuild: every Resolution-B source must differ from its
    pre-landing baseline (unless --allow-unedited, dry-form rehearsal only)."""
    checks = [
        ('program staged copy', Path(args.program_source), PORT_PROGRAM_UNEDITED),
        ('port ODR header', Path(args.odr_port_header), ODR_HEADER_UNEDITED),
        ('adapter source', Path(args.adapter_source), ADAPTER_UNEDITED),
        ('NativeLoaderContext.cpp', LC_CPP_DEFAULT, LC_CPP_UNEDITED),
        ('NativeLoaderContext.hpp', LC_HPP_DEFAULT, LC_HPP_UNEDITED),
        ('lmf_to_native_loader_facts.hpp', LMF_HPP_DEFAULT, LMF_HPP_UNEDITED),
    ]
    for what, path, unedited in checks:
        if not path.is_file():
            raise SystemExit(f'{what} missing: {path}')
        if sha256_file(path) == unedited:
            raise SystemExit(f'{what} is UNEDITED (matches pre-Resolution-B baseline '
                             f'{unedited[:12]}...): {path} — the controller\'s §2 edits '
                             'have not landed here; refusing a no-op rebuild. '
                             '(--allow-unedited is for the dry-form rehearsal only)')


def compose_argv(args) -> list:
    """APPLY-ORDER Step 2 argv for composition-6: identical to composition-5
    except the adapter redirect, the §3.2 program replacement, and the fresh
    composition-6 attestation receipts."""
    return [
        '--rust-receipt', '.superpowers/native-dependency-spike/ue4ss-native-tick-witness-1/native-1/receipt.json',
        '--host-receipt', '.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/host-final-1/receipt.json',
        '--extra-unit', 'lmf-live-module-facts=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp',
        '--extra-unit', 'lmf-sce-ops=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_os_ops_sce.cpp',
        '--extra-unit', f'lmf-loader-adapter={args.adapter_source}',
        '--extra-unit', 'lmf-base-scan=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_base_scan.cpp',
        '--extra-unit', 'native-query-harness=outputs/2026-10-04-zcode-team/worker-query-harness/src/NativeQueryHarness.cpp',
        '--extra-unit', 'module-runtime-ack-supplier=outputs/2026-10-04-zcode-team/worker-query-s2-arm/integration-patch/new-units/runtime_ack_supplier.cpp',
        '--extra-define', 'LMF_DISABLE_DL_RETENTION',
        '--extra-receipt', str(args.extra_receipt),
        '--extend-undefined-baseline',
        '--replace-bootstrap-unit', 'runtime_owner=outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-runtime-1/source/bootstrap/runtime_owner.cpp',
        '--replace-bootstrap-unit', 'ue4ss_backend=outputs/2026-10-04-zcode-team/worker-composition-3/bootstrap-stage/native-coherent-pad-runtime-1/build-1/source/ue4ss_backend.cpp',
        '--replace-receipt', str(args.replace_receipt),
        # L11PB E2: initializer merged unit compiled from THIS worker's
        # PREPARED-UNARMED staging copy (VT-R1 seam S5/S6 quoted sibling includes;
        # disarmed object byte-identical to launch-10's 983b18e2..., measured).
        '--replace-runtime-unit', 'unreal-unrealinitializer=outputs/2026-10-04-zcode-team/worker-launch11-vtable-prebuild/staging/initializer-vtable-pre/UnrealInitializer.cpp',
        # §3.2 bounded retained-unit replacement (attested; allow-list {"program"})
        '--replace-runtime-unit', f'program={args.program_source}',
    ]


def compose_phase(args, logs: Path) -> Path:
    """Phase A: the bounded fixed-script edition produces the composed object."""
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
    argv = [sys.executable, COMPOSE6_FIXED, '--output', run] + compose_argv(args)
    # The composition script resolves receipt/staging paths relative to CWD.
    p = subprocess.run([str(a) for a in argv], cwd=str(ROOT),
                       capture_output=True, text=True)
    (logs / 'compose6.stdout').write_text(p.stdout)
    (logs / 'compose6.stderr').write_text(p.stderr)
    if p.returncode:
        sys.stderr.write(p.stderr[-5000:])
        raise SystemExit(f'composition-6 compose failed rc={p.returncode} '
                         f'(run dir kept for forensics: {run}; use a fresh run dir to retry)')
    print(p.stdout.strip())
    return run


def entry_preflight(entry: Path, attested_sha: str, logs: Path) -> dict:
    """Phase B pre-flight on the delivered wiring source (unchanged from comp-5)."""
    if not entry.is_file():
        raise SystemExit(f'wiring delivery not found: {entry}')
    got = sha256_file(entry)
    if got != attested_sha:
        raise SystemExit(f'entry attestation mismatch: {got} != {attested_sha} ({entry}); '
                         'get the digest from the wiring/controller REPORT')
    text = entry.read_text(errors='replace')
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


def parse_stage_spec(spec: str):
    """REL=SRC or REL=SRC=SHA256."""
    rel, sep, rest = spec.partition('=')
    if not sep or not rel:
        raise SystemExit('bad --stage-header spec (REL=SRC[=SHA256]): ' + spec)
    src, sep2, sha = rest.partition('=')
    if sep2 and len(sha) != 64:
        raise SystemExit('bad --stage-header sha (64 hex chars expected): ' + spec)
    return rel, Path(src), (sha if sep2 else None)


def build_overlay(entry: Path, stage_headers: list, stage_units: list,
                  bridge: Path, odr_staged: Path,
                  overlay_dir=None) -> tuple:  # D1FIX E6 (unannotated: host python3 < 3.10)
    """Symlink-overlay lineage mirror with the wired entry + bridge TU as
    lifecycle units and all extra headers staged flat (hash-pinned). The
    Platform/NativeProgramContext.hpp overlay copy is the ODR-gated staged
    source."""
    overlay = Path(overlay_dir) if overlay_dir else MY / 'staging/lineage-overlay-2'  # D1FIX E6
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
    staged = {}
    for rel, src, sha in DEFAULT_STAGE_HEADERS:
        if rel == 'Platform/NativeProgramContext.hpp':
            src = odr_staged  # the ODR-gated staged copy
        staged[rel] = (rel, src, sha)
    for spec in stage_headers:
        rel, src, sha = parse_stage_spec(spec)
        if not src.is_file():
            raise SystemExit('--stage-header source missing: ' + spec)
        staged[rel] = (rel, src, sha)
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
    led['zcode_supplier_composition_6'] = {
        'utc': __import__('datetime').datetime.now(__import__('datetime').timezone.utc).isoformat(),
        'actor': 'worker-comp6-driver (PROVISIONAL mirror entry; real ledger '
                 'owned by controller)',
        'status': rec['status'],
        'composition_run': str(run_dir),
        'runtime_object': rec['runtime_object'],
        'runtime_sha256': rec['runtime_sha256'],
        'runtime_bytes': rec['runtime_bytes'],
        'retained_units_replaced': rec.get('retained_units_replaced', []),
        'wiring': 'Resolution-B configured-roots composition (§3.2 bounded edition: '
                  'attested retained-unit replacement of program + rr pre-seeded pins; '
                  'ODR-gated Platform/NativeProgramContext.hpp restage) plus the '
                  'unchanged module_start wiring lifecycle units',
    }
    mirror = MY / 'transport-ledger.composition6-mirror.json'
    mirror.write_text(json.dumps(led, indent=1) + '\n')

    spec = importlib.util.spec_from_file_location('package_prx_fixed', PKG_FIXED)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)

    # Frozen packaging CLANG_FLAGS carry no -std; the bridge needs C++20.
    # In-memory injection only; the pinned packaging file stays byte-identical.
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
        raise SystemExit('packaging script already references a composition-5/6 unit')
    units[units.index(OLD_ENTRY_UNIT)] = NEW_ENTRY_UNIT
    units.extend(extra_units)
    m.BOOTSTRAP_UNITS[:] = units  # in-place: same pinned-values adjustment class as PIN

    m.PIN['lineage'] = overlay
    m.PIN['composition_dir'] = run_dir
    m.PIN['composition_object_name'] = 'coherent-tick-file-witness-runtime.o'
    m.PIN['composition_object_sha256'] = rec['runtime_sha256']
    m.PIN['composition_object_bytes'] = rec['runtime_bytes']
    m.PIN['composition_checkpoint'] = 'zcode_supplier_composition_6'
    m.PIN['ledger'] = mirror

    if out.exists():
        raise SystemExit(f'output dir exists: {out}')
    sys.argv = ['package_prx.py', '--link', '--out', str(out)]
    print('driver: PIN adjustments applied (lineage->overlay-2 with wired entry + bridge, '
          'checkpoint zcode_supplier_composition_6); running fixed package_prx.py --link',
          flush=True)
    raise SystemExit(m.main())


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--program-source', type=Path,
                    default=MY / 'staging/comp6-program/UE4SSProgram.cpp',
                    help='staged (edited) UE4SSProgram.cpp copy for the §3.2 retained '
                         'replacement of unit "program" (default: this worker\'s staging; '
                         'pass the controller comp6-integration path for the real pass)')
    ap.add_argument('--adapter-source', type=Path, default=ADAPTER_DEFAULT,
                    help='(edited) lmf_to_native_loader_facts.cpp for the lmf-loader-adapter '
                         'extra-unit redirect (default: canonical W/src path, edited in place)')
    ap.add_argument('--odr-port-header', type=Path, default=ODR_HEADER_PORT,
                    help='port-tree Platform/NativeProgramContext.hpp (compiled into the '
                         'recompiled program unit)')
    ap.add_argument('--odr-staged-header', type=Path, default=None,
                    help='staged copy restaged flat into the overlay; MUST be byte-identical '
                         'to the port header (ODR gate). Default: the port header itself')
    ap.add_argument('--replace-receipt', type=Path, default=DEFAULT_REPLACE_RECEIPT,
                    help='composition-6 replace-receipt (regen_replace_receipt.py output)')
    ap.add_argument('--extra-receipt', type=Path, default=DEFAULT_EXTRA_RECEIPT,
                    help='composition-6 extra-receipt (regen_extra_receipt.py output)')
    ap.add_argument('--entry-source', type=Path, default=ENTRY_DEFAULT)
    ap.add_argument('--entry-sha256', default=ENTRY_MEASURED_SHA)
    ap.add_argument('--bridge-source', type=Path, default=BRIDGE_DEFAULT,
                    help='bridge TU (default: runtime_start_bridge_r13.cpp, Resolution-B wiring)')
    ap.add_argument('--bridge-sha256', default=BRIDGE_MEASURED_SHA,
                    help='attested bridge sha (default: r13 measurement 2026-10-04; '
                         'reconcile with the controller REPORT)')
    ap.add_argument('--no-bridge-unit', action='store_true')
    ap.add_argument('--stage-unit', action='append', default=[])
    ap.add_argument('--composition-run', type=Path, default=None,
                    help='composition run dir (default: fresh composition-6 run-1 for '
                         'Phase A; --reuse-composition-run re-verifies the SAME '
                         'composition-6 run-1 object - D1FIX E4/E5)')
    ap.add_argument('--reuse-composition-run', action='store_true')
    ap.add_argument('--overlay-dir', type=Path, default=None,
                    help='D1FIX E7: overlay dir for this run (default '
                         'MY/staging/lineage-overlay-2-d1 semantics: '
                         'staging/lineage-overlay-2 under this worker dir)')
    ap.add_argument('--verify-only', action='store_true',
                    help='cheap gates only (ODR + receipt coverage + edited-landed + entry '
                         'pre-flight + chain symbols vs the existing composed run); no '
                         'composition, no overlay, no packaging')
    ap.add_argument('--allow-unedited', action='store_true',
                    help='REHEARSAL ONLY: skip the edited-landed gates so UNMODIFIED '
                         'stand-in sources can exercise the mechanism')
    ap.add_argument('--require-symbol', action='append', default=[])
    ap.add_argument('--stage-header', action='append', default=[],
                    help='REL=SRC[=SHA256]; overrides/additions to the default 31-header '
                         'overlay staging set')
    ap.add_argument('--out', type=Path, default=MY / 'prx-candidate-composition6-1',
                    help='packaging output dir (must not exist)')
    args = ap.parse_args()
    if args.composition_run is None:
        # D1FIX E4: --reuse-composition-run previously fell back to RUN4
        # (composition-4/run-3, the STALE pre-comp6 object 45dd9901..., 77,368 B
        # larger, missing the program/initializer/Resolution-B replacements).
        # That silently built candidate-2 from the wrong lineage (D1 FAIL).
        # Reuse now re-verifies the composition-6 composed object itself.
        args.composition_run = RUN6
    if args.odr_staged_header is None:
        args.odr_staged_header = args.odr_port_header

    check_sha(COMPOSE6_FIXED, COMPOSE6_FIXED_SHA, 'fixed build_runtime_composition6_fixed.py')
    logs = MY / 'staging'
    logs.mkdir(parents=True, exist_ok=True)

    odr_gate(Path(args.odr_port_header), Path(args.odr_staged_header), logs)
    receipt_preflight(Path(args.replace_receipt), Path(args.extra_receipt),
                      Path(args.program_source), Path(args.adapter_source),
                      Path(args.odr_port_header), LC_CPP_DEFAULT, logs)
    if not args.allow_unedited:
        edited_landed_gate(args)
    else:
        print('[note] --allow-unedited: dry-form rehearsal with unmodified stand-ins; '
              'edited-landed gates skipped')
    entry_preflight(Path(args.entry_source), args.entry_sha256, logs)
    if not args.no_bridge_unit:
        bridge = Path(args.bridge_source)
        if not bridge.is_file():
            raise SystemExit(f'bridge TU not found: {bridge} — the wired entry calls '
                             'modbridge_runtime_start_with_activation; refusing to build '
                             'without it (--no-bridge-unit overrides consciously)')
        bridge_got = sha256_file(bridge)
        if bridge_got != args.bridge_sha256:
            raise SystemExit(f'bridge source drift vs pin: {bridge_got} != {args.bridge_sha256} '
                             f'({bridge}) — reconcile with the controller REPORT and pass the '
                             'attested digest via --bridge-sha256')
        extra_units = [BRIDGE_UNIT]
    else:
        bridge, extra_units = None, []
    extra_units += [spec.partition('=')[0] for spec in args.stage_unit]

    if args.verify_only:
        if not args.reuse_composition_run and args.composition_run == RUN6:
            verify_run = RUN4  # the existing composed object, for the symbol gate
        else:
            verify_run = args.composition_run
        chain_symbols_gate(verify_run, DEFAULT_REQUIRE_SYMBOLS + args.require_symbol)
        print('VERIFY-ONLY PASS: ODR + receipt coverage + entry pre-flight OK; all',
              len(DEFAULT_REQUIRE_SYMBOLS) + len(args.require_symbol),
              'required symbols defined in', str(verify_run))
        return

    run_dir = compose_phase(args, logs)
    chain_symbols_gate(run_dir, DEFAULT_REQUIRE_SYMBOLS + args.require_symbol)
    overlay, _manifest = build_overlay(Path(args.entry_source), args.stage_header,
                                       args.stage_unit, bridge,
                                       Path(args.odr_staged_header),
                                       args.overlay_dir)  # D1FIX E7
    package_phase(run_dir, overlay, Path(args.out), extra_units)


if __name__ == '__main__':
    main()
