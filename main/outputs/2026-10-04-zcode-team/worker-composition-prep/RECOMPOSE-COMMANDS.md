# RECOMPOSE-COMMANDS.md — controller recomposition of the UE runtime with worker-supplier-ingame-update

Workspace root for every relative path: `/WORKSPACE/.codex/worktrees/translation-core/what`
Prepared 2026-10-04 by worker-composition-prep. Everything below was rehearsed against a
patched VENDORED tree; the real spike tree is only touched by these commands, by the controller.

Evidence base (this worker's area, `outputs/2026-10-04-zcode-team/worker-composition-prep/`):
- `controller-apply/bundle/` — 9 ready-to-apply files in destination-relative layout
- `controller-apply/COPY-MANIFEST.json` — per-file sha256, bytes, current-spike sha, provenance
- `build-receipts/vendored-run-1/receipt.json` — status
  `VENDORED_TREE_HOST_TESTS_PASSED_SDK_OBJECTS_VERIFIED`; host run:
  **ALL S1 ADAPTER HOST TESTS PASSED (169 checks)** against the patched tree
- `build-receipts/template-run-1/` — the four extra units recompiled with the EXACT
  composition-1 argvs against the patched sources + `predicted-undefined-delta.json`

---

## Step 0 — assert current state (all must hold before touching anything)

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 - <<'EOF'
import hashlib, json
from pathlib import Path
W = Path('.')
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
man = json.load(open('outputs/2026-10-04-zcode-team/worker-composition-prep/controller-apply/COPY-MANIFEST.json'))
# pinned build_runtime.py BASE receipt must still match the script's hard-coded sha
assert sha('.superpowers/native-dependency-spike/ue4ss-native-notify-cancel-1/build-1/receipt.json') \
    == 'bcd23a2a4abef518d8f220786e80cb131036e351b2218a322d26aee6adca8384'
assert json.load(open('.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/host-final-1/receipt.json'))['status'] \
    == 'OWNED_CPP_FILE_FALLBACK_HOST_PASS'
for f in man['files']:
    if 'current_spike_sha256' in f:
        cur = sha('.superpowers/native-dependency-spike/' + f['dest'])
        assert cur == f['current_spike_sha256'], f"spike drift: {f['dest']}"
    assert sha('outputs/2026-10-04-zcode-team/worker-composition-prep/controller-apply/bundle/.superpowers/native-dependency-spike/' + f['dest']) == f['bundle_sha256']
print('PRECONDITIONS OK')
EOF
```

Verified by the preparer on 2026-10-04: BASE sha matches the pin; host receipt status is
`OWNED_CPP_FILE_FALLBACK_HOST_PASS`; the 7 replaced spike files match the exact bytes the
patches were validated against (no drift since composition-1).

## Step 1 — apply the 9-file bundle into the spike tree

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
cp outputs/2026-10-04-zcode-team/worker-composition-prep/controller-apply/bundle/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/* \
   .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/include/
cp outputs/2026-10-04-zcode-team/worker-composition-prep/controller-apply/bundle/.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/* \
   .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/
```

Then re-verify: rerun the Step-0 script — the `current_spike_sha256` assertions will now
FAIL for the 5 replaced files (expected); instead assert the post state:

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 - <<'EOF'
import hashlib, json
from pathlib import Path
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
man = json.load(open('outputs/2026-10-04-zcode-team/worker-composition-prep/controller-apply/COPY-MANIFEST.json'))
for f in man['files']:
    got = sha('.superpowers/native-dependency-spike/' + f['dest'])
    assert got == f['bundle_sha256'], f"BAD {f['dest']}: {got}"
print('POST-APPLY OK: all 9 files match bundle hashes')
EOF
```

Alternative to Step 1: `patch -p1` the 8 originals from
`outputs/2026-10-04-zcode-team/worker-supplier-ingame-update/patches/` inside a
workspace-mirror staging tree (they apply cleanly — all hunks verified); the bundle files
are byte-identical to that result (checked per file, see COPY-MANIFEST).

## Step 2 — recompose (single invocation)

`--output` MUST NOT pre-exist (the script refuses). The three lm* unit names are kept from
composition-1 for lineage; `lmf-base-scan` is the ONE new unit (the count grows 3 -> 4).

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
python3 .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/build_runtime.py \
  --output .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-2/run-1 \
  --rust-receipt .superpowers/native-dependency-spike/ue4ss-native-tick-witness-1/native-1/receipt.json \
  --host-receipt .superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/host-final-1/receipt.json \
  --extra-unit lmf-live-module-facts=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_facts.cpp \
  --extra-unit lmf-sce-ops=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_os_ops_sce.cpp \
  --extra-unit lmf-loader-adapter=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/lmf_to_native_loader_facts.cpp \
  --extra-unit lmf-base-scan=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_base_scan.cpp \
  --extra-define LMF_DISABLE_DL_RETENTION \
  --extra-receipt outputs/2026-10-04-zcode-team/worker-supplier-ingame-update/build-receipts/run-1/receipt.json \
  --extend-undefined-baseline
```

Notes (all from build_runtime.py source + composition-1 evidence):
- `--extra-define LMF_DISABLE_DL_RETENTION` is inserted into EVERY extra-unit compile,
  exactly as in composition-1 (its `extra-lmf-*` argvs show `-DLMF_DISABLE_DL_RETENTION`
  first, then the loader-context template defines). The extra-unit compile template is the
  recorded `native-loadercontext` argv; headers are picked up with `-I <spike>/include`
  FIRST, so the patched headers shadow everything else once Step 1 is applied.
- The extra receipt is hash-pinned into the output receipt (path + sha256) — mechanism
  review hardening: mandatory with extras.
- No env needed; the script takes `LLVM_CONFIG` from the recorded base units.

## Step 3 — predicted added-symbol set (nm-verified)

Prediction method: the four extra units were rebuilt with the EXACT composition-1 argvs
against the patched sources (ELF objects, `build-receipts/template-run-1/`), then compared
against the hard-coded baseline (notify-cancel `undefined_delta.current`, 197 keys) and the
composition-1 composed object (199 keys). Composition-1 is the empirical anchor: the `-r`
link resolves ALL lm-unit C++ ABI references via retained inputs (it added only
`_DYNAMIC` + `sceKernelGetModuleInfo` vs the baseline).

- **PREDICTED ADDED vs baseline: exactly `{ sceKernelGetModuleInfo }`**
  (libkernel.so family export, nm-verified by the composition-1 independent review;
  the gate will re-nm all 34 pinned SDK .so files and re-pin provider hashes).
- PREDICTED REMOVED vs baseline: none. (`_DYNAMIC` disappears vs the composition-1
  OBJECT because the patched dl-disabled SCE TU compiles the DT_DEBUG walk out, but
  `_DYNAMIC` was never in the baseline — composition-1 added it — so the gate sees no
  removal. `dlopen`/`dlclose` likewise never in baseline under the disable define.)
- The new reference `_ZN3lmf20lmf_scan_image_basesEPKNS_10VmScanSpanEmPFmPvmS3_mES3_PNS_15VmScanCandidateEm`
  (undefined in `lmf-sce-ops`) is DEFINED byte-identically (same mangled name, nm-checked)
  by the new `lmf-base-scan` unit — resolved inside the relocatable link, never undefined.
- Continuity: the new units' extern DEFINED set is a strict superset of the old lm units'
  (zero lost definitions; none referenced). Zero references unseen in the old units or
  baseline (risk set empty). Full numbers: `build-receipts/template-run-1/predicted-undefined-delta.json`.
- SCE TU (dl-disabled) undefined, from the vendored SDK build: `lmf_scan_image_bases`
  (resolved by the new TU), `getpid`, `memcpy`, `memset`, `sceKernelGetModuleInfo`,
  `sysctl` — matches the worker's pinned receipt exactly.

## Step 4 — post-composition verification

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
R=.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-2/run-1
LLVM=outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin
# 1. receipt status + shape
python3 -c "
import json; r=json.load(open('$R/receipt.json'))
assert r['status']=='NATIVE_SELECTED_TICK_FILE_WITNESS_RELOCATABLE_COMPOSED'
assert len(r['freshly_compiled_units'])==11, r['freshly_compiled_units']   # 7 native + 4 lm*
assert r['retained_runtime_units']==353
assert r['runtime_compilation_units']==364
assert r['extra_receipt']['sha256']
assert r['undefined_delta']['baseline_extended'] is True
assert r['undefined_delta']['added']==['sceKernelGetModuleInfo'], r['undefined_delta']['added']
assert r['undefined_delta']['removed']==[]
assert r['undefined_delta']['changed_bindings']=={}
print('RECEIPT OK', r['runtime_sha256'][:16], r['runtime_bytes'])
"
# 2. no dl symbols, no _DYNAMIC, no unresolved scan symbol in the composed object
"$LLVM/llvm-nm" --extern-only --undefined-only --format=posix "$R/coherent-tick-file-witness-runtime.o" \
  | grep -E 'dlopen|dlclose|_DYNAMIC|lmf_scan_image_bases' && echo 'FAIL: forbidden/unresolved symbol' || echo 'SYMBOL GATE OK'
# 3. Rust witness exports preserved (the script also asserts this; re-check by eye in exports.stdout)
# 4. record runtime sha/bytes into the checkpoint per studio contract; queue package_prx.py --link
```

Expected: 353 retained + 11 freshly compiled = 364 units; runtime bytes in the same
~117.6 MB class as composition-1 (1,176,249,20 bytes then; new TU adds a small delta).

## Runtime-wiring facts (no further source change required for composition)

- `make_sce_live_module_os_ops()` (patched SCE TU) binds `scan_image_base` by default as
  the primary in-game identity route, plus the two distinct refusal slots
  (`kRefusalDtDebugUnfilledIngame` = -4002, `kRefusalExecHandleRawZeroIngame` = -4001).
- VmScan-fed conversion stays REFUSED unless the caller passes
  `LmfConversionOptions{.allow_vm_scan_base_as_module_image_base = true}` — that call site
  is external to the composed runtime (no reference from NativeProgramFactory or any
  retained unit; grep-verified). Nothing in the spike tree needs editing for the opt-in;
  enabling it is a probe/caller decision, per the honesty rules (candidate != validated).
- Honest limits carried over from the worker (not papered over): in-game
  KERN_PROC_PATHNAME unobserved (adapter refuses empty executable_path); `text_base`
  still unresolved; module-list/GetModuleInfo domain mismatch (g4) unresolved.

## Failure modes to expect, not to paper over

- `ValueError: Unpinned extra project dependency` -> a patched header includes a project
  file outside the spike pins; STOP, reconcile with the controller, do not edit the script.
- `Unexpected native import change` with non-empty removed/changed -> real symbol drift
  vs the prediction above; compare against `predicted-undefined-delta.json` and the
  template-run objects before any retry.
- `Prior combined object input refused` / changed-input errors -> state drift since this
  prep; re-run Step 0 assertions and reconcile; never blind-replay.
