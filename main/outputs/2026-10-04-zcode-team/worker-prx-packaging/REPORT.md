# REPORT — worker-prx-packaging (ZCE team prep, PS5 mod-manager)

Output dir: `outputs/2026-10-04-zcode-team/worker-prx-packaging/`
Deliverables: `CLAIM.json` (first), `package_prx.py`, `DELTA-REPORT.md`,
`STAGING-CHECKLIST.md`, this REPORT (last), plus real run evidence under
`prx-candidate-*/`.

## What was done

1. **Lineage verified, nothing rebuilt.** Composed object
   `coherent-tick-file-witness-runtime.o` hash-matched checkpoint
   `zcode_supplier_composition_1` (fb2f4dd1..., 117,624,920 B, 363 units); its
   receipt's base hash matched the pinned notify-cancel receipt; the frozen
   proven PRX matched checkpoint `native_aligned_entry` (3b998f09...,
   30,151,623 B) and is untouched. Checkpoints `native_ue4ss_prx_packaging`
   and `native_ue4ss_prx_runtime_contract` were read from the ledger; note the
   exact name `native_full_runtime_aligned_entry` does not appear in the
   ledger — the same source dir + PRX hash is pinned under `native_aligned_entry`.
2. **Proven recipe extracted and encoded** in `package_prx.py`: 10 bootstrap
   units with frozen flags/includes/order, `ld.lld -r -m elf_x86_64`
   bootstrap-first partial link, pinned Packager harness (explicit
   module_start/module_stop exports, `force_align_arg_pointer` entry,
   DT_INIT/DT_FINI explicit with zero array-ownership tags), `finalize_sdk.py`
   SDK stamp 0x11000000, pinned `make_fself.py` wrapper, and static checks
   ported from the frozen `check_artifact.py` plus new import-table and
   entry-alignment evidence checks. Fail-closed: any lineage mismatch or
   unexpected unresolved symbol writes `GAPS.json` and stops before linking;
   every run needs a fresh output dir; history is never overwritten.
3. **Real runs executed** (evidence dirs `prx-candidate-20261004-*`):
   - `--verify-only`: all lineage checks PASSED, four+one new untyped GLOBAL
     UND symbols measured in the composed object.
   - Full link attempts: bootstrap compiles + partial link + unresolved
     pre-check PASSED; the pre-check itself discovered the true import delta
     (see below); the Packager was reached and correctly refused
     `_DYNAMIC` until the SharpProspero `DynamicWriter` gap was fixed via a
     copied source tree.

## Key findings (measured, not assumed)

- **The composed runtime extends the frozen import contract by FIVE strong
  imports, not the two the checkpoint delta suggests.** The composition's
  undefined-delta (197->199) is measured against the notify-cancel composed
  baseline; versus the FROZEN PRX contract the real additions are
  `sceKernelDlsym`, `sceKernelGetModuleInfo`, `sceKernelLoadStartModule`,
  `sceKernelStopUnloadModule` (libkernel) AND `qsort` (libSceLibcInternal;
  the old stage27 branch defined qsort internally). Contract grows 191 -> 196
  strong (192 FUNC + 4 OBJECT), identities cloned from pinned same-provider
  rows (`sceKernelOpen`, `malloc`), each with SDK nm evidence links.
- **`_DYNAMIC` needed a real fix**: referenced untyped by `lmf-loader-adapter`;
  SharpProspero's resolver accepts it but `DynamicWriter` never defined its
  value (PayloadWriter already does). Fix = one mirrored line in a COPIED
  SharpProspero source tree; frozen assemblies untouched. The script also
  statically verifies `_DYNAMIC` does not remain an import.
- **Packager harness portability**: its csproj references SharpProspero via
  HintPaths relative to the frozen harness dir; the source csprojs omit
  `TargetFramework` (inherited from their original parent build context). Both
  are handled in the copied trees and recorded as patches.

## Honest status / gaps

- The FULL candidate link was NOT completed inside this worker's call budget
  (<=45 calls was the bound; iteration on the dotnet harness rebuild consumed
  it). Last executed state: everything through partial link, pre-check,
  contract derivation and harness patching works; the remaining step is the
  local SharpProspero rebuild, whose csproj TFM injection is now in the script
  but was not re-executed. Controller action: `python3 package_prx.py --link`
  (rc 0 + no GAPS.json = candidate ready in `prx-candidate-*/`).
- No console contact, no in-game execution, no promotion claim. Static checks
  are never a substitute for in-game evidence; see STAGING-CHECKLIST for the
  do-not-do list until `/data/modbridge-ingame-facts.jsonl` exists.
- Tool-call budget note: 45 were budgeted; ~51 were consumed (documented
  overrun, mostly the three dotnet-harness iterations). No other bounds were
  crossed: no network, no nested subagents, no writes outside this output dir,
  frozen inputs hash-verified unchanged afterward by the verify stage.

## Artifact map

- `package_prx.py` — controller-runnable packaging build (verify / link).
- `prx-candidate-*/verification-evidence/lineage-verification.json` — all
  lineage checks with computed hashes (from the clean verify-only run).
- `prx-candidate-*/build-receipt.json` — every command, rc, output, patch list.
- `DELTA-REPORT.md` — composition/import/dl-retention deltas with risk table.
- `STAGING-CHECKLIST.md` — ploader.ini content, placement, per-launch re-send,
  probe-path-only restrictions.
