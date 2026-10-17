# REPORT — worker-vtable-swap-design (2026-10-04)

Mission: pre-build the dispatch-data (vtable/table-swap) hook architecture as the hedge for
the exec-writability `{W_FAILS_R_WORKS}` decision branch, so a W-fails probe verdict does not
start design from zero. Evidence synthesis + design only. Deliverable order honored:
CLAIM.json first, VTABLE-SWAP-DESIGN.md, this report last. 30 tool calls used. No console
contact, no nested subagents, nothing executed, no frozen or wiring file modified.

## 1. Bottom line

The swap architecture is designable today in concrete detail because the hard part —
resolving the dispatch tables — is already the port's own data-side code path: the
initializer's ten vtable walks perform exactly the reads a swap needs
(`UnrealVirtualBaseVC.hpp:104-134`), and the UFunction FuncPtr hook mechanism already exists
in-tree (`SRC:port/unreal/src/UFunction.cpp:78-90`). The thunk needs no allocation, no new
imports, and no code writes anywhere: it is ordinary module code, and the saved original
entry replaces the PLH trampoline word (`DetourInstance.hpp:487,499`), which preserves the
entire callback machinery unchanged.

## 2. The one load-bearing correction to the task premise

"Vtables live in WRITABLE data (RW spans), swaps need NO mprotect" is FALSE on this target.
The live VMMAP census shows the RELRO-covered segment L3 mapped READ-ONLY at runtime:
`[0x82bc000,0x953c000)` prot=1 (`CON:igf6-results.jsonl` igf3 seq 4), which is exactly L3
under the now-validated uniform bias 0x400000 (L0/L1/L3/L7 all match the PT_LOAD table in
`TEAM:worker-vaddr-semantics/SEGMENT-CORRESPONDENCE.md:21-28`; L7's bias is confirmed by
igf4's GUA read at `rt_addr 0x97ba110 = 0x400000+0x93ba110`, previously "UNVALIDATED" at
`SEGMENT-CORRESPONDENCE.md:92`). UE vtables live in RELRO-covered `.data.rel.ro` by
toolchain convention, so a slot swap needs `mprotect R->RW` on a 16-KiB DATA page — open
fact **D-1**, a different page class from IGW-1's exec-page questions, carrying igf5-class
residual risk if syscall 74 is ring-0-fatal in general. Data reads AND writes in prot=3
spans are proven in-game (igf4 `gua_read`/`gua_writeback`, same-value 96 B writeback,
`restored:1`), which is what the heap tier (FuncPtr swaps) stands on with zero syscalls.

## 3. What was designed

- **Per-hook swap table** (DESIGN §2): EngineTick is a single-slot single-class swap with a
  decisive safety property PLH lacks — the identity pre-check (a slot is only ever written
  when its current value equals the independently resolved original; wrong offsets/classes
  refuse loudly instead of patching hot code, cf. "nothing refuses a wrong Tick address",
  `TEAM:worker-tick-address-validation/REPORT.md:29-32`). ProcessEvent is a FAMILY of
  swaps across UClass vtable copies; per-function UFunction FuncPtr swaps are the
  zero-syscall tier; GNatives is designed but address-unproven; FName ctor verification is
  not dispatch-table reachable at all and is replaced by the prepared FC-1 validator.
- **Thunk** (DESIGN §4): module-owned static functions, SysV end to end (no ms_abi on PS5),
  saved-original committed before the slot store, release/acquire ordering, no prologue-patch
  race (no instruction byte ever changes), process-lifetime hooks matching the port's own
  PROSPERO no-unhook precedent (`DetourInstance.hpp:191-205,216-225`).
- **Integration ranked by blast radius** (DESIGN §5): R1 backend typedef at the single
  construction site (`DetourInstance.hpp:164-166`) → R2 slot capture at the existing walks
  → R3 zero-syscall Lua routing → R4 full ProcessEvent family; C1 (FC-1) independent.
  EngineTick stays the first real target under every branch, loud-throw intact.
- **Honest limits** (DESIGN §6): global ProcessInternal/ProcessLocalScriptFunction
  interception lost (per-function substitutes); partial ProcessEvent swaps create silent
  per-class holes (R4's registration-time coverage check is an acceptance requirement);
  direct/static call targets (FName ctor, StaticConstructObject) unswappable; GNatives
  unproven; D-1 unverified and risky; the query harness (`SINI:1394-1397`) is the only
  admissible in-game tick-hook evidence.

## 4. Key files

- Deliverables: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-vtable-swap-design/{CLAIM.json,VTABLE-SWAP-DESIGN.md,REPORT.md}`
- Consumed (most load-bearing): `worker-exec-writability/{EXEC-WRITABILITY-EVIDENCE.md,EXEC-WRITABILITY-PROBE-DESIGN.md}`,
  `worker-vaddr-semantics/SEGMENT-CORRESPONDENCE.md`, `CON:igf6-results.jsonl` (igf3 seq 4
  vmmap; igf4 seq 6-7 GUA), `worker-all-signatures-inventory/SIGNATURES-INVENTORY.md`,
  `SRC:.../DetourInstance.hpp`, `SRC:.../UnrealVirtualBaseVC.hpp`,
  `worker-query-s2-arm/staging/UnrealInitializer.cpp`,
  `ue4ss-native-owned-default-detour-1/port/x64Detour.cpp`,
  `SRC:port/unreal/src/{UFunction.cpp,UFunctionStructs.cpp,FFrame.cpp}`,
  `SRC:port/ue4ss/src/Mod/LuaMod.cpp`, `worker-fname-fc1-materialize/FC1-CHECKS.md`,
  `worker-tick-address-validation/{TICK-VALIDATION-PATH.md,REPORT.md}`,
  `worker-tick-witness-design/TICK-WITNESS-DESIGN.md`.

## 5. Honesty bounds

Every in-game fact is quoted from retained raw records or retained worker evidence with
file:line; the `.data.rel.ro`-in-L3 inference, the RELRO re-protect plausibility argument,
and all D-1 outcomes are explicitly marked as convention/speculation/unverified in the
design. This is a hedge document, not a promotion: nothing is built or staged, the composed
runtime still cannot reach `PostInitialize` until the seed/settings remedies land
(`SIGNATURES-INVENTORY.md:243-246`), and no acceptance or quality claim is made. If IGW-1
never runs or returns `{W_WORKS}`, this design costs nothing; if `{W_FAILS_R_WORKS}`, the
controller inherits R1/R2 + D-1 + C1 instead of a blank page.
