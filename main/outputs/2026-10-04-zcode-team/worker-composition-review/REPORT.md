# REPORT — worker-composition-review (2026-10-04)

Independent read-only review. Workspace:
`/WORKSPACE/.codex/worktrees/translation-core/what`.
Budget: 15 tool calls used of ≤35. No network, no console, no subagents, no builds
executed (all verification via hashes, `llvm-nm`, `.d` files, receipts, and diffs
against existing artifacts).

## Artifacts reviewed

| Target | Path |
| --- | --- |
| Extended script | `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/build_runtime.py` (sha256 `54c9b646...`, current) |
| Composition run | `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/ue4ss-native-supplier-composition-1/run-1/` (receipt.json, undefined-delta.json, object, commands.json, sdknm-* evidence) |
| Vendored SCE TU | `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/src/live_module_os_ops_sce.cpp` (193 lines) |
| Worker-1 original TU | `outputs/2026-10-04-zcode-team/worker-1-live-supplier/src/live_module_os_ops_sce.cpp` (172 lines) |
| Pre-extension reference run | `.superpowers/native-dependency-spike/ue4ss-native-tick-file-owner-1/native-1/receipt.json` (pinned script sha `aaf8f7d2cfc7a6e814e3...`) |
| Pinned toolchain | `outputs/ps5-mod-bridge/native-probe/build/deps/llvm/LLVM-21.1.8-macOS-ARM64/bin/llvm-nm` |
| SDK oracle libs | `outputs/ps5-mod-bridge/native-host/build/sdk-v0.43/ps5-payload-sdk/target/lib/*.so` |

## 1. Extended build_runtime.py — mechanism analysis

### No-extras baseline behavior is unchanged

- `--extra-unit` defaults to `[]`; the extras loop (L67-82) is skipped entirely, so
  `extras=[]`, `extra_pinned=[]`, `inputs.extend([])` is a no-op, and `selected`
  gains no extra entries.
- Undefined-delta refusal (L93-102): the extension branch requires
  `a.extra_unit and a.extend_undefined_baseline and not delta['removed'] and not
  delta['changed_bindings']`. Without extras this is False, so ANY
  added/removed/changed binding raises `'Unexpected native import change'` — the
  original behavior. Empirical cross-check: `native-1` (produced by the
  pre-extension script) shows the original gate tolerated exactly zero delta
  (added/removed/changed = `[]`/`[]`/`{}`, 197 undefined symbols).
- `--extend-undefined-baseline` passed alone (no extras) also cannot extend: same
  condition fails → unconditional raise.
- All original invariants run before any extra handling and are untouched:
  base-receipt sha pin (L23), host/rust receipt hash pins (L25-30), originals
  input-hash re-check (L33-34), prior-composite refusal (L35), exactly-one-old-Rust
  (L36-37), per-unit source/object sha checks (L39-43), affected-header inventory set
  equality (L42), fresh-unit deps must already be in `pins` (L60-62), final re-check
  of all hashes (L106).
- Receipt additions are additive metadata only (`extra_pinned_dependencies`, optional
  `extra_receipt`; `new_live_facts_supplier` already existed in the native-1 receipt).
  The runtime object itself would be byte-identical with no extras: same inputs, same
  order (extras append only at the end, L87).

### Extra-unit dep pinning (L74-80)

Resolved dep paths under the worktree ROOT are handled as:
- under `S` (spike) or `ROOT/.superpowers` and not already pinned → hard
  `ValueError('Unpinned extra project dependency')` — project trees cannot sneak in;
- any other ROOT-local path (e.g. `outputs/` SDK headers) → first-seen pinned
  (`pins[rp]=sha(pd)`), listed in `extra_pinned`, persisted as
  `receipt['extra_pinned_dependencies']` with hashes (L110), and re-checked (L106).
- Already-pinned deps get their hash verified.

Verified empirically for run-1 by bucketing every file in the three lmf `.d` files
(32 + 121 + 850 = 1003 dep files): zero outside ROOT, zero unpinned
spike/`.superpowers` files, zero in other ROOT dirs; exactly the 66 first-seen pins,
all from `lmf-sce-ops`'s FreeBSD kernel-header chain.

### Provider gate (L95-101)

Runs `llvm-nm -D --defined-only` over every
`sdk-v0.43/ps5-payload-sdk/target/lib/*.so` and requires each added symbol to be
exported by at least one (or be in the linker-provided allow-list). run-1 evidence:
34 `sdknm-*` commands, all exit 0, stdout saved in the run dir; `sdknm-libkernel.so.stdout`
contains `sceKernelGetModuleInfo`.

`linker_provided={'_DYNAMIC'}` (L97) is minimal and justified: I verified no SDK
`.so` exports `_DYNAMIC` (nm across all libs → none), and the symbol is the standard
ELF dynamic-section array defined by the final PRX/ELF link; the TU references it via
`extern "C" Elf64_Dyn _DYNAMIC[]` (live_module_os_ops_sce.cpp:60). Without the
allow-list the gate would have rejected the symbol — i.e., the exemption is exactly
as wide as needed and no wider.

### Extra defines confined to extras

`-D` insertion (L71) is inside the extras loop. run-1 receipt argv confirms
`-DLMF_DISABLE_DL_RETENTION` appears only in the 3 lmf compiles and in none of the 7
baseline fresh compiles.

## 2. Composition receipt + undefined-delta verification

| Check | Result |
| --- | --- |
| added set | `['_DYNAMIC','sceKernelGetModuleInfo']` exactly — independently recomputed (my nm set == receipt `current`, 199 symbols; set-diff vs native-1's 197 = exactly these 2) |
| removed / changed_bindings | `[]` / `{}` |
| providers | `_DYNAMIC → <linker-provided>`; `sceKernelGetModuleInfo → libkernel.so, libkernel_stub_weak.so, libkernel_sys.so, libkernel_web.so` — independently enumerated with pinned llvm-nm, exact match; `libkernel.so` entry `sceKernelGetModuleInfo T 1218c 0` |
| runtime hash/size | receipt `fb2f4dd16a20997455b39e6857fd2bdf45842c691bd0124a448948ae5bcb775c` / 117,624,920 bytes == recomputed sha256 / actual stat size |
| fresh units (10) | 6 affected (`unreal-unrealinitializer`, `native-ownedmappedscansource`, `native-psscanbinding`, `native-ownedmappedcandidateselection`, `native-loadercontext`, `native-programfactory`) + `native-ownedserializedelfsource` + `lmf-live-module-facts`, `lmf-sce-ops`, `lmf-loader-adapter` |
| retained | 353 (363 total = native-1's 360 + 3 extras; extras appended at end of `runtime_object_order`) |
| extra_pinned (66) | all `ps5-payload-sdk/target/include/{bsm,machine,sys,vm,x86}` SDK headers; no project paths |
| extra_receipt | worker-s1-adapter run-1, sha `fc4c0854...` matches the referenced file |
| safety flags | `linked_prx`/`target_executed`/`console_contact` all False |

## 3. Vendored SCE TU

`diff` worker-1-original → vendored copy shows only:
1. `#ifdef LMF_DISABLE_DL_RETENTION / <comment> / #else / #include <dlfcn.h> / #endif`
   (lines 40-49);
2. `#ifdef` degraded bodies for `lmf_sce_loader_retain_noload` (returns `nullptr`)
   and `lmf_sce_loader_release` (returns `-1`), `#else` original bodies, `#endif`
   (lines 118-138).

Both `#else` branches are byte-identical to the worker-1 original, so with the
define absent the TU compiles to functionally the original. Degraded values match
the real path's failure conventions (dlopen returns NULL on failure; release returns
-1 for a null handle) and are documented inline with the livefacts-probe E6
justification. The ops-table binding (`make_sce_live_module_os_ops`) is unchanged in
both modes. The TU's two extern declarations (`_DYNAMIC`, `sceKernelGetModuleInfo`)
are precisely the two added undefined symbols.

## 4. dl cross-check on the composed object

Pinned llvm-nm over the 117,624,920-byte object:
`grep -Ec '^(_?_dl|dlfcn|dlopen|dlsym|dlclose)'` → 0. Broad case-insensitive sweep
(`dlfcn|dlopen|dlsym|dlclose|dlvsym|__dl`) finds only `sceKernelDlsym`, a PS5 SDK
module API: present in the pre-extension baseline (native-1) and exported by
`libkernel.so` (verified) — not a libdl/dlfcn import.

## Minor observations (non-blocking)

- **O1** Provider-oracle `.so` files are not hash-pinned in the receipt (fixed path,
  nm'd as oracles; not compiled against).
- **O2** Extra-unit deps resolving outside the worktree ROOT escape pinning (none
  did in run-1 — all 1003 dep files were ROOT-local). Baseline units, by contrast,
  require every dep in the sealed pin set (L61).
- **O3** `--extra-receipt` is optional rather than mandatory when extras are passed;
  run-1 supplied one and its hash verifies.
- **O4** No committed pre-extension script exists (file staged, never committed;
  worktree == index for the extended version), so byte-level script diffing was
  impossible; original behavior established via the native-1 receipt (pre-extension
  script pin `aaf8f7d2...`) plus structural analysis.

## Verdict

**ALL_CLEAR.** No actionable mismatches. See `findings.json` (machine-readable) and
`VERDICT.md` (summary).
