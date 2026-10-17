# STAGE-PLAN — worker-composition-5-minimal (composition-5 bisection)

Workspace root: `/WORKSPACE/.codex/worktrees/translation-core/what`
Date: 2026-10-04. Controller-runnable; no console access was used or claimed by this worker.

## 1. Purpose

The composition-4 PRX (packaged `STATIC_CHECKS_PASSED` 20/20) produces zero output in-game,
while the aligned-entry lineage (frozen 191-import contract) works every time. The suspect is
the added import surface. This plan bisects it with two minimal checkpoint modules (built and
delivered) plus a bounded follow-on ladder (not built).

Both delivered modules replace the full UE4SS runtime with a checkpoint writer:
`module_start` opens `/data/plugins/UE4SS.log` (`O_WRONLY|O_CREAT|O_APPEND|O_NOFOLLOW`, 0600),
writes one line, closes, and returns 0 ALWAYS (igd1 directive: an open failure must never look
like a start failure). The full frozen 191-import surface is carried by the lineage's frozen
`import_references.S` typed rows (byte-identical source). No C++ in either binary.

## 2. Delivered payloads (frozen, two-run deterministic)

| rung | module | strong imports | checkpoint line | fself sha256 | bytes |
|---|---|---|---|---|---|
| A (control) | `composition-5-zero/run-1/modbridge_supplier_composition.prx` | 191 (frozen contract, verbatim) | `COMP5Z: 191 imports, module_start reached` | `1cb624817ad7df03b32ceb7bb4c2fe837b7787fdd0b187ea198332e95b6b139e` | 21191 |
| B | `composition-5/run-1/modbridge_supplier_composition.prx` | 192 (191 + sceKernelGetModuleInfo) | `COMP5: 192 imports, module_start reached` + `COMP5: sceKernelGetModuleInfo_ptr=0x… fd=…` | `34fb620bb256a497c157ce548a04a41e112a2ab654ac6d78a66ab71d4e15ad7f` | 21623 |

- Two-run determinism: DETERMINISM-PASSED. run-1 == run-2 byte-identical for combined object,
  contract, bare-original, bare-sdk AND fself (both variants; measured 2026-10-04).
- Inner ELF (bare-sdk): comp5 `1237b8ce3c7e0e1b…` 82776 B; comp5-zero `1127dcf350e235e1…` 82608 B.
- Static verification (composition-3 check set adapted, all gates PASS, zero gaps):
  fself structure + payload digest, ELF64 x86-64 LE, unique PT_SCE_MODULE, SDK stamp
  0x11000000, explicit DT_INIT/DT_FINI, loader owns no arrays, init/fini slots, .eh_frame +
  .eh_frame_hdr present, import table == contract on decoded NID identities (long form
  `<nid>#<library>#<module>`), no `_DYNAMIC` import, entry-alignment bytes
  (`force_align_arg_pointer`) at module_start AND module_stop.
- Recorded by-design delta vs the composition-3 20/20 list: `.gcc_except_table` ABSENT
  (that section requires C++ EH code; the igd1-proven minimal family is C-only by directive).

## 3. Staging runbook (one variant per launch; user-coordinated)

1. Stage ONE module at the PPSA33286 plugin slot `/data/plugins/modbridge_supplier_composition.prx`
   (same slot the composition-3/4 candidates target; no ini change).
2. Delete `/data/plugins/UE4SS.log` before launch (unambiguous single-launch evidence).
3. Launch; after the game reaches a stable state, read back `/data/plugins/UE4SS.log`.
4. Console readback oracle (igd1 size law, prediction not gate): readback = bare-sdk − ~1560 B
   (SHT tail not embedded by make_fself; full-hash equality IMPOSSIBLE via this channel).
   comp5 prediction ≈ 81216 B; comp5-zero ≈ 81048 B; ELF magic required.
5. Stage rung A (control) FIRST, then rung B. Never both in one launch.

## 4. Outcome matrix

| # | control (A) log line | rung B log line | verdict | next action |
|---|---|---|---|---|
| 1 | PRESENT | PRESENT | Imports resolve; sceKernelGetModuleInfo is CLEARED | Run B's second line: `sceKernelGetModuleInfo_ptr` non-zero proves the binding relocated. Bisect the REMAINING composition-4 extensions (ladder §5) — the blocker is a later rung or non-import code |
| 2 | PRESENT | ABSENT, game fine | sceKernelGetModuleInfo is the blocker (load refuses or start never runs) | Verify with rung 5b (+sceKernelDlsym): if 5b works, the failure is specific to GetModuleInfo's provider row; escalate to controller — provider-availability question, do not re-roll the same import |
| 3 | ABSENT, game fine | n/a | The checkpoint approach itself fails (slot, writer, or packaging recipe at minimal scale) | Do NOT stage B. Compare against the in-game-proven igd1 7-import build shape; suspect module_path/log channel, not imports |
| 4 | ABSENT + crash | n/a | Format-level fault at minimal scale (would contradict the igd lineage) | Stop; full evidence capture to controller |
| 5 | Either ABSENT with untouched log | — | Ambiguous open-failure vs never-started (no second channel in this frozen spec) | Check readback size/oracle first; only then interpret |

Log-line distinction: control writes `COMP5Z:` (Z suffix); rung B writes `COMP5:` — a shared log
file can never be misattributed between rungs.

## 5. Bisection ladder (built on evidence, one rung per launch)

Measured composition-4 delta over the frozen 191 (from its delivered 196-row contract,
`worker-composition-4-fix/prx-candidate-composition4-1/supplier-contract.json`): the five
strong extension rows below. (The task's "198 imports" counts the composed object's raw
undefined set; the remaining names are linker-provided boundaries `__dso_handle`,
`__eh_frame_*` and the allowed-weak `kernel_mprotect`, not published imports.)

| rung | add | strong total | driver variant |
|---|---|---|---|
| A (delivered) | — (control) | 191 | `comp5-zero` |
| B (delivered) | sceKernelGetModuleInfo | 192 | `comp5` |
| 5b | + sceKernelDlsym | 193 | new VARIANTS entry, same driver |
| 5c | + sceKernelLoadStartModule | 194 | " |
| 5d | + sceKernelStopUnloadModule | 195 | " |
| 5e | + qsort (libSceLibcInternal) | 196 | " (== packaged composition-4 surface) |

Rules: ladder runs only on matrix outcome 1 or 2; one rung per launch; a rung that fails while
its predecessor passed names that import as the blocker — stop there, no re-rolls of identical
evidence. Extension rows are cloned from the same donor identities as the composition-4
contract (sceKernel* from the `sceKernelOpen` row, qsort from the `malloc` row), so rung 5e is
import-identity-identical to composition-4's table.

## 6. Rollback

No promotion is authorized for any rung (`promotion_allowed: false` in every package profile).
The staged plugin file is replaced per rung; removing it restores the pre-experiment slot.
The delivered payloads are additive artifacts; nothing in the spike tree or canonical outputs
tree was modified by this worker.
