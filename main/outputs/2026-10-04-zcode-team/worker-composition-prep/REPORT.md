# REPORT.md — worker-composition-prep

Date: 2026-10-04. Output area (only place written):
`/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-composition-prep/`
Read-only elsewhere (spike tree, worker deliverables, receipts). No network, no console,
no engine writes, no nested subagents.

## Task completion

1. **Patches applied to a VENDORED tree; 169-check suite re-run against it.**
   All 8 patches (`worker-supplier-ingame-update/patches/`) apply cleanly to the CURRENT
   spike state (`patch -p1`, workspace-root-relative staging mirror that includes the
   worker-s1-adapter tree patch 0008 targets). The patched result is byte-identical to the
   worker's verified vendored copies for all 9 composed files. The worker's 169-check host
   suite was re-run against the patched vendored tree (adapted verifier `verify_vendored.py`,
   worker area untouched, all artifacts under my `build-receipts/vendored-run-1/`):
   **ALL S1 ADAPTER HOST TESTS PASSED (169 checks)** + pinned-SDK cross-compiles verified
   (`VENDORED_TREE_HOST_TESTS_PASSED_SDK_OBJECTS_VERIFIED`).
2. **Ready-to-apply controller artifact.** `controller-apply/bundle/` (9 files in
   destination-relative layout) + `controller-apply/COPY-MANIFEST.json` (per-file
   sha256/bytes, current-spike sha for the 7 replaced files, worker-equality flags, all 8
   patch sha256s, pinned receipt paths + shas). BASE receipt still matches build_runtime.py's
   hard-coded pin (`bcd23a2a…` — re-verified today).
3. **RECOMPOSE-COMMANDS.md** — exact single-invocation recomposition: `--extra-unit` x4
   (lmf-live-module-facts, lmf-sce-ops, lmf-loader-adapter + NEW lmf-base-scan),
   `--extra-define LMF_DISABLE_DL_RETENTION`, `--extra-receipt` = supplier-update receipt
   (sha pinned), `--extend-undefined-baseline`, output dir must not pre-exist; plus
   pre/post state assertions and post-composition verification commands.
4. **Predicted added-symbol set (nm evidence, not guesswork).** The four extra units were
   rebuilt with the EXACT composition-1 argvs against the patched sources
   (`build-receipts/template-run-1/`, ELF objects): predicted delta vs the hard-coded
   notify-cancel baseline is **added = {sceKernelGetModuleInfo} exactly; removed = none;
   changed = none**. `lmf_scan_image_bases` ref/def mangled names match byte-exactly
   (resolved intra-link, never undefined). New units' extern definitions are a strict
   superset of the old ones (zero lost defs; zero unseen references). Empirical anchor:
   composition-1's `-r` link resolved all lm C++ ABI refs via retained inputs (it added
   only `_DYNAMIC` + `sceKernelGetModuleInfo`).
5. **Conflicts resolved in the vendored tree and documented**
   (`CONFLICTS-AND-RESOLUTIONS.md`): C1 LiveLoaderObservedFacts.hpp clean-additive;
   C2 dl-retention gate kept via the same single `--extra-define` (patched TU honors it;
   `dlopen/dlclose/_DYNAMIC` drop out vs the old object but never were in the baseline —
   C3, so the refuse-on-removal gate is not tripped); **C4 the real trap**: spike contains
   composition-1-promoted adapter copies that patch 0008 does not target — resolved by
   replacing both with the patched s1 versions (bundled; without this the composed adapter
   silently lacks the VmScan opt-in). Opt-in stays default-off, caller-side only
   (grep-verified: no composed runtime unit references it).

## Deliverable inventory (this worker's area)

- `CLAIM.json`, `REPORT.md` (this file)
- `RECOMPOSE-COMMANDS.md` — controller runbook
- `CONFLICTS-AND-RESOLUTIONS.md` — 5 documented conflicts/resolutions
- `controller-apply/bundle/...` + `controller-apply/COPY-MANIFEST.json`
- `verify_vendored.py` + `build-receipts/vendored-run-1/` (169-check run vs patched tree)
- `build-receipts/template-run-1/` (composition-argv reproduction + `predicted-undefined-delta.json`)
- `staging/` (workspace-mirror: patched spike tree + patched s1 adapter — the canonical proof tree)

## Key numbers

- 8/8 patches clean; 9/9 bundle files byte-equal to worker-verified copies
- 169/169 host checks on the vendored tree; SDK objects ELF64 LE ET_REL AMD64, 0 errors,
  both dl variants; SCE(dl-disabled) undefined = scan-ref + getpid/memcpy/memset/
  sceKernelGetModuleInfo/sysctl (== worker's pinned receipt)
- Expected composition result: 353 retained + 11 fresh = 364 units (composition-1: 353+10=363)

## Honest limits (unchanged from the worker; nothing papered over)

- Prediction is evidence-based but the authoritative delta is computed at composition time;
  any deviation vs `predicted-undefined-delta.json` must be reconciled, not retried blind.
- In-game KERN_PROC_PATHNAME unobserved; `text_base` unresolved; g4 module-info mismatch
  unresolved; scan-fed conversion stays refused without the explicit caller opt-in.
- No PRX link, no target execution, no console contact performed here; `package_prx.py
  --link` remains queued (controller-owned) after a verified composition.
