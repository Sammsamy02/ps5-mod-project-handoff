# agent II — round-5 build staging (restore + additions + plan validation)

2026-10-11, battery-safe throughout: /tmp staging + plan checks ONLY, no
builds, no console contact, ZERO live-tree edits (proven below). Everything
lands in `outputs/2026-10-08-l25-prep/round5-staging/ROUND5/` (+ this report).

## Deliverables

| File (prefix `.../round5-staging/ROUND5/`) | What |
|---|---|
| `restore-round5.sh` | Idempotent restore (S1-S5) with `--verify` read-only mode; `--tree/--prep` mirror overrides. Proven: live `--verify` correctly reports every set-aside hole; full apply + verify exit 0 in the /tmp mirror; re-run = 8 SKIPs (idempotent) |
| `additions/fl-wiring-unrealinitializer.patch` | GT's first-launch TU call-site wiring (the exact insertion + 2 includes); dry-run + apply + ordering (try@1703 < consume@1706) proven on a tmp copy |
| `additions/markers-add-round5.json` | The addition marker set: IB `[SHIM] write-area` + 8 `[FL]` + 3 `[FL-MAP]` (resolved from IA's landed 03-map-logger.patch); every literal grep-verified in-tree by the apply script |
| `additions/apply-additions-round5.sh` | Wires the additions: wiring patch + CHANGES row refresh, world-fix patches 02->02b->03 (candidate-relative, syntax-receipted order) + changed-file row refresh, marker adds with literal ground-truth gate |
| `plan-validate-round5.sh` | Rebuilds the FULLY restored+additions state under /tmp and runs the REAL driver `--plan`; asserts census/gates; exit 0 |
| `BUILD-CARD-ROUND5.md` | The AC command sequence (restore -> additions -> checks -> snapshot -> plan -> execute -> package -> P12/P13 -> manifest verify -> archive) + rollback (re-set-aside + frozen 59b0528b) + prerequisites |

## The restore+wire sequence (what ROOT runs at AC power)

1. `restore-round5.sh` — copies `LuaPathShim.{cpp,hpp}` + `FirstLaunchProfile.{cpp,hpp}` from `round4-setaside/` (COPY not move: the stash survives for rollback; **KernelBridgeProbes stays set aside** and the script hard-fails if it ever finds it in the tree); reinserts the 21-line LuaMod.cpp block byte-exact after the unique `setup_lua_require_paths(lua);` anchor (9509 -> 9530 lines, restored sha `9445f77bb132…`), with a conceptual-compile check that the block's 3-arg extern matches the post-IB `RC::LuaPathShim_install` definition (now at stash :1637); re-adds the `lua-pathshim` (verbatim per WAKE-RUNBOOK section 12 step 0.4) + `firstlaunch` (reconstructed to GT's spec — original row text not preserved on disk; honest note) NEW_UNITS rows; restores the 3 [SHIM] manifest entries (utf-16le, literals/lines re-verified against the post-IB stash: :1623/:753/:604) -> 18 markers; reconciles CHANGES.json (LuaMod rows in BOTH `changes[]`+`files[]`, added_headers/added_header_hashes with post-IB shas, round5_row_sync, round4_setaside=false) with a valid-JSON gate.
2. `apply-additions-round5.sh` — applies the FL wiring patch to UnrealInitializer.cpp (before the `consume_required_slot_seeds` admission, GA Rule-10 root law via `UE4SSProgram::get_program().get_mods_directory()` — grounding verified: program constructed + mods dirs pinned before ScanGame; get_program throws, never UB); refreshes the UI CHANGES rows; applies IA/IK's `02-core-fix` -> `02b-getworld-fallback` -> `03-map-logger` (proven clean on the restored tree); refreshes only the rows of files that actually changed (LuaMod.cpp + LuaUObject.hpp); adds the 12 addition markers with a pre-add literal grep against the tree sources (FW phantom-build law).

## Expected final census / markers (and how they were proven)

- **Census 396** = 391 + 5 NEW_UNITS (round-4 was 394). PROVEN: the driver's own `G6-link-input-census planned_inputs = 396` (x4 passes) in the /tmp `--plan` run.
- **Markers 30** = 15 round-4 + 3 restored [SHIM] + 1 IB `[SHIM] write-area ` + 8 [FL] (narrow utf-8, st_send doctrine) + 3 [FL-MAP] (wide utf-16le: `…phase=pre src=detour`, `…phase=loaded src=detour url='{}'`, `…phase=loaded src=marshal-watch`). PROVEN: the mirror manifest reached exactly 30 with every literal grepped in-tree.
- LuaMod.cpp final shape in the validated tree: **9719 lines** (9509 + 21 restore block + 189 map-logger).

## Plan-mode validation verdict (GREEN, exit 0)

`/tmp/r5-plan` mirror (drivers + candidate + CHANGES parts with candidate-side paths repointed; BASE receipts verbatim so the 18k-pin sweep re-proves the LIVE tree read-only), all four patches applied, real driver `--plan`:

- G1-base-selection-pins ok=True (19044 pins, drift=0 — live round-4 tree intact)
- G2-part-change-pins 42 rows, **0 not-ok** (fully-patched tree consistent with refreshed CHANGES)
- **G2q**: Q's copyin-probe patch rebased cleanly onto the WIRED UnrealInitializer.cpp (`patch-rebased-onto-revised-base`, token present) — the one ok=false (`…unrealinitializer.base`) is the driver-documented EXPECTATION pin, accepted only because the rebase+token proof followed
- **G12b**: LuaUObject.hpp -> 33 consumers recompiled, gaps=[] (world-fix header cascade covered); G2t: all four listener units fresh
- G2r/G2s/G4/G4-ctoken/G5/G12/stamp True; G7/G9/G10 PLANNED (execute-enforced); 77 gate records total
- No build was attempted at any point (plan mode performs no toolchain execution; the stamp cleanup happened inside the mirror only)

## Prerequisites / sibling landings (final state at report time)

- **IB (writable-area shim)**: stash edit LANDED 2026-10-11 09:14 (`round4-setaside/LuaPathShim.cpp` sha `2f6d2e6d…`, 1655 ln; `.hpp` `0f5a043d…`, 121 ln; write-area marker at :1547). IB's REPORT is still pending — the card lists it as the one remaining soft prerequisite (marker-budget note confirmation); the staging already consumes the landed bytes.
- **IA/IK (world-fix)**: fully landed (`round5-staging/world-fix/`: diagnosis + 02 + 02b + 03 + apply-order doc + syntax receipts). [FL-MAP] sockets resolved.
- **IC (zzdiag/ueselftest)**: Lua-side, LANDED (`agent-IC-st2-zzdiag.md`); not build content — deploy-side for launch-2 (note IK's doc: zzdiag main.lua is now 3919 B -> loadlist row re-pin).
- Standing AC-queue predecessors per the order law: serial-equivalence proof -> P1 -> P2 (P1/P2 staging cards from the sibling agent are in `round5-staging/P1|P2/`).

## Honest residuals

1. No compile of the wired/restored/patched code ran (battery law). Evidence chain: GT TU + GA's pre-IB shim + IA/IK's patched headers all have prospero `-fsyntax-only` receipts on disk; the FL wiring block is the only new code in a proven TU; IB's upgraded shim TU has NO syntax receipt yet — the card recommends one single-file `-fsyntax-only` (LuaPathShim.cpp + the wired UnrealInitializer.cpp) before `--execute`, lawful per the house battery precedent.
2. The `firstlaunch` NEW_UNITS row text is reconstructed to GT's registration spec (original not preserved anywhere) — functionally equivalent, flagged for provenance.
3. The FL wiring is the call-site only; the PendingVerify probe/report/commit loop + pin-registry retarget are follow-on rungs per GT section 3.
4. Live tree verified untouched at close: LuaMod.cpp 9509 lines, NEW_UNITS rows absent, manifest 15, no restored files present.
