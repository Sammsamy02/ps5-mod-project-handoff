# VERDICT — worker-luamod-l11-dryrun (2026-10-05)

## GO — for a launch-11 luamod-integrated candidate build attempt.

The DIFF-1 luamod admission path EXECUTES end-to-end on the real toolchain:
the L10LM E0 fixed-script edition admitted the retained `luamod` replace-unit
under the exact pinned 172-token argv (drift = source/-o/-MF only), the
rebased variant-B source (`4540593ac9e66cdc...`) compiled clean inside the
compose to the byte-identical gate object (`5595feb7...`), the full compose
finished 52/52 commands exit 0 with `retained_units_replaced =
["luamod","program"]`, 14/14 startup-chain symbols defined, an import contract
identical to the r3-armed baseline, and two passes produced BYTE-IDENTICAL
composed objects. All of INTEGRATION.md's fail-fast traps stayed silent.

The dry run STOPPED before `build_overlay`/`package_phase` by mission bound
(shared packaging serialization); packaging therefore remains UNPROVEN for
launch-11 and is the controller's first exclusive step.

## Exact remaining steps for the controller (in order)

1. **Re-run the §1 sha gates at compose time** (INTEGRATION.md §1): the 8
   pinned shas + the 2 lineage diffs. If ANY drifts → STOP, reconcile (the
   fail-fast map of INTEGRATION.md §3 governs; do not edit pins).
2. **Two full driver passes** (the delivered `9b332fc5...` driver, UNMODIFIED,
   from `LM=outputs/2026-10-04-zcode-team/worker-luamod-rebase`), fresh dirs,
   controller-owned, serialized (one heavy build at a time; exclusive of any
   launch-10 packaging):
   ```bash
   cd /WORKSPACE/.codex/worktrees/translation-core/what
   python3 $LM/build_runtime_composition6_l10lm.py \
     --composition-run $LM/ue4ss-native-supplier-composition-6-l10lm/run-a \
     --out $LM/prx-candidate-composition6-l10lm-a \
     --program-source outputs/2026-10-04-zcode-team/worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp
   # then run-b / prx-candidate-composition6-l10lm-b
   ```
   **`--program-source` is REQUIRED (finding L11D-F1):** the delivered default
   points at a non-existent rebase-staging path; the rr-covered canonical copy
   is `worker-comp6-driver/staging/comp6-program/UE4SSProgram.cpp`
   (`75abf2ab77791f4e...`) — the r3-armed precedent. (Alternatively re-stage
   under the controller dir and re-run `regen_receipt_l10lm.py`; do NOT edit
   pins.)
   These passes INCLUDE packaging (`package_prx.py e6fbe6c5...` via L10LM E4
   in-memory injection) — that is the controller-owned serialized step the dry
   run deliberately did not execute.
3. **Accept the compose only if** (durable comparators, path-independent):
   - exit 0; `retained_units_replaced == ["luamod","program"]`; 368 units;
   - composed object sha `633a8c82d71b6233989bc5ada83bc06ac6b65e1cf11ef769e
     00601b6ea2ccda4`, 117,742,096 B (as measured twice here);
   - run-a == run-b byte-identical; `luamod.o == 5595feb785206c0057eb29af0ccd
     cd53a8898d86629db0b10debc4a17aa08dea`;
   - undefined_delta == added `["sceKernelGetModuleInfo"]`, removed 0,
     changed 0 (SDK-extended baseline, same as r3-armed);
   - chain-symbols 14/14.
   Note: controller receipts will carry controller-dir paths, so the
   controllers' `receipt.json` shas will differ from the scratch ones
   (`73faa059...`/`f4e0188e...`) by design — compare OBJECT shas, not receipt
   shas.
4. **Manifest** per INTEGRATION.md §5: derive
   `composition-change-manifest-launch10-l10lm.json` from the r3-armed
   manifest (new `manifest_id`, initializer entry re-pointed to
   `LM/staging/initializer-l10-r3/`, second entry
   `{"unit":"luamod","role":"replace","source_paths":[LM/staging/luamod-l10rebase/LuaMod.cpp],
   "sha256_after":{"...":"4540593a..."}}`, `import_delta` empty both
   directions — now backed by this dry run's measured identical UND deltas —
   `resolution_b`/`d2_text_base` verbatim).
5. **Gate v2** (`worker-gate-evolution/gate_check_composition5_v2.py`,
   manifest mode vs the frozen r11 baseline): expect the same 15/15 families.
6. **Before any launch-grade claim** — the INTEGRATION.md §6 design decision:
   the drain is link-INERT as authored (this dry run verified the STRONGER
   form: `arm_funcptr_marshal_drain` / `funcptr_marshal_hook` /
   `r3_marshal_carrier_precheck` are dead-code-ELIMINATED — absent from both
   the unit object and the composed object; there is no symbol to link). The
   composed candidate carries readiness/ladder/table surface only
   (`m_funcptr_marshal_armed`, `is_funcptr_marshal_armed`, `FuncPtrMarshal`
   key). Arming + pump drain need a controller/authoring-worker linkage
   decision and a NEW revision with distinct IDs; it must not be silently
   patched.

## Conditions / honest limits

- GO covers the ADMISSION + COMPOSE path only, host-side, on this host as of
  2026-10-05 tree state. Packaging, gate v2, determinism at the controller's
  canonical paths, I1/I1+ import measurement, and anything in-game remain
  open controller steps.
- If the controller's canonical-path compose does NOT reproduce object sha
  `633a8c82...` (e.g. because a lineage input drifted), STOP and reconcile —
  the dry-run comparator then no longer applies.
- Packaging contention: launch-11's packaging must be serialized against
  launch-10's (one `package_prx.py` at a time; the dry run left it untouched).
