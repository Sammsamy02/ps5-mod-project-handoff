# REPORT — worker-walkguard-gate

Worker: worker-walkguard-gate · 2026-10-05 · local only. Read-and-derive, proposal only:
no console contact, no engine/provider execution, no edits outside this directory, no launch
claims. Current-tree basis: the controller-edited launch11-combined initializer
(`outputs/2026-10-04-zcode-controller/launch11-combined/units/UnrealInitializer.cpp`,
sha256 `e7f349af…`, mtime Oct 5 19:15) — line drift vs worker-engine-pump-chain confirmed
(its `final-units:1877` is `:1979` here). Pinned walkguard unit:
`worker-walk-site-transitions/ASMHelper.guarded.cpp` (== the fold-0 form composed into the
launch11 candidate, `worker-launch11-dry-compose/REPORT.md:21`, object `68d22a1c…`).

---

## 1. The traced GATE-1 path (current file:line)

Aliases: `UI` = launch11-combined/units/UnrealInitializer.cpp (sha `e7f349af…`); `VCM` =
`worker-launch11-vtable-prebuild/build-ready/build-ready/preview/UnrealVirtualBaseVC.hpp`;
`ASM` = the pinned ASMHelper.guarded.cpp; `SWAP` = worker-vtable-swap-design.

1. `UI:1699` — the post-arm startup `try` (serialized init; disarm-on-throw).
2. `UI:1977` — `if (UnrealConfig.bHookLoadMap)` (settings-fed: source-reconstruction
   UE4SSProgram.cpp:1224 `config.bHookLoadMap = settings_manager.Hooks.HookLoadMap`).
3. `UI:1979` — `Output::send(STR("Finding GameEngine::LoadMap\n"))` — the last progress
   line before the gate (the W-oracle anchor).
4. `UI:1980` — `OPTIONAL_GET_ADDRESS_OF_UNREAL_VIRTUAL(UEngine, LoadMap, GameEngine)`.
5. Macro body (VCM:141-160):
   - VCM:143-150 — static offset lookup `UEngine::VTableLayoutMap[STR("LoadMap")]`;
     a missing name returns 0 ⇒ nullptr with NO walk (not our world: W1/W2 prove the walk ran).
   - VCM:155 — `vtable = *(std::byte**)GameEngine` — **READ-A (data)**: the CDO instance's
     vptr. Address class: engine object/heap data — the class igf4 proved readable
     (same-value writeback at 0x97ba110 prot=3, SWAP §1.2). Works today.
   - VCM:156-157 — `vt_slot_address = (void**)(vtable + offset); vt_entry_value =
     *vt_slot_address` — **READ-B (data)**: the LoadMap vtable slot word. Address class:
     vtable storage, RELRO-covered span `[0x82bc000,0x953c000)` kernel-observed prot=1
     READABLE (SWAP §1.1). Works today.
   - VCM:159 — `return ASM::resolve_function_address_from_potential_jmp(vt_entry_value)`
     — enters exec text.
6. Walkguard (ASM, the single innermost text read of every resolve path, ASM:27-34):
   - ASM:325-345 `resolve_function_address_from_potential_jmp` → ASM:328
     `get_first_instruction_at_address(function_ptr, operands)`.
   - ASM:286 — `ScopedRxGuard rx_guard{in_instruction_ptr, kWalkReadLen=16}` —
     **READ-C (THE GATE)**: the first ≤16 bytes at the vtable ENTRY VALUE, i.e. LoadMap's
     own first instruction bytes. Address class: the exec span `[0x400000,0x787c000)`,
     kernel-observed prot=4 EXEC-ONLY, no read bit (SWAP §1.1; EXEC-WRITABILITY-EVIDENCE
     §1). An untransitioned read here is the igf6 crash class (twice, module-contained).
   - `Core::enter` ASM:116-167 — refusal decision:
     - ASM:132-136 — `if (!rx_guard_folded_on) return "guard_off_no_rx_verdict"`.
     - Fold input: `RC_ASM_WALK_RX_GUARD != 0` (ASM:249); default 0/absent (ASM:57-59).
       **That is the whole refusal: guard_off means NO rx verdict exists, so the guard
       refuses loudly with ZERO syscalls** — the mprotect text is preprocessor-dead
       (ASM:215-223), by design (the no-exec-gate backstop, U-SWAP-5).
   - ASM:249-253 — refused ⇒ `refuse_and_throw` ⇒ **W1** ASM:234 (UE4SS.log Error:
     `[WalkRxGuard] REFUSED: NO read attempted; walk text reads require the IGW RX verdict
     (LAUNCH10-MATRIX D2)…`) + **W2** ASM:235-237 throw (`[WalkRxGuard] refused:
     guard_off_no_rx_verdict (walk text read not permitted; no read attempted; detail=0)`)
     — the `state N failed 1 [WalkRxGuard] refused: guard_off_no_rx_verdict` line.
7. Fail-closed unwind: `UI:2286` `catch (...)` → `UI:2291`
   `QueryHarness::Global().DisarmLayoutGate()` (closed gate refuses and counts every later
   dispatch — honest negative evidence) → `UI:2292` rethrow → UE4SSProgram catch → native
   shutdown. The event loop never starts (PROG:619→673, pump-chain CHAIN.md) — mods, pump,
   input, menu, spawn all blocked. GATE-1 alone is the simultaneous blocker.
8. Same-gate sites queued behind it (all hit ASM:286 once LoadMap passes): GameEngine::Tick
   `UI:1989` (+ the 16-entry vtable DISPLAY walk `UI:2000-2023`, per-entry resolve
   `UI:2012` — dead in the seeded world, gated on non-null scan_address `UI:1994`);
   InitGameState `UI:2115`; BeginPlay `UI:2124`; EndPlay `UI:2133`; AActor::Tick `UI:2142`;
   GameViewportClient::Tick `UI:2151`; ProcessEvent `UI:2160`; ProcessConsoleExec `UI:2169`
   (GET_ variant — throws when the layout name is absent, VCM:124-139); UStruct::Link
   `UI:2178`; then ProcessInternal derivation `UI:2219-2282` (resolve `UI:2228` + the
   164-byte Zydis decode — FLAG-3 class, skipped only under the settings-v3 keys,
   `UI:2206-2218`).

### What would make the guard PASS instead of refuse (preconditions, from the unit)

1. **Fold = 1**: `RC_ASM_WALK_RX_GUARD == 1` at compile (ASM:249; static_assert 0/1 only,
   ASM:271-272). Admissibility law (walkguard-prep REPORT §4, WST-5): exactly when the IGW
   D2 seq5 records `rx_result.mprotect_rc == 0` (`W_WORKS`/`W_FAILS_R_WORKS` rows);
   ABSENT/BOTH_FAIL/FATAL keep fold-0. The flip is MEASURED: one-token edit
   (`#define RC_ASM_WALK_RX_GUARD 0`→`1`), object `f5627427…`, 1 syscall instruction,
   export surface unchanged (walkguard-prep §4 probe 2).
2. **No anomaly latch** (ASM:118-122): any earlier restore failure permanently refuses
   every later walk read, loudly.
3. **Valid page range** (ASM:123-128): 16 KiB pages (PAGE_SHIFT 14, ASM:69-71), no
   `addr+len` overflow.
4. **Guard depth**: < 8 in flight (ASM:149-153), or exactly nested-covered by an outer
   scope on the same page pair (ASM:140-148 — inner skips the transition).
5. **The transition succeeds**: `sys_mprotect(first..last, RX=0x5)` — inline syscall 74
   (ASM:203-214; constants SDK-stamped ASM:60-71) — must return 0 (ASM:156-161); rc != 0 ⇒
   `rx_transition_denied`, NO read, loud throw. This is THE unproven kernel fact: syscall-74
   X→RX on the game's own exec pages. The staged armed xwp probe exists to settle exactly
   this (its `sceKernelMprotect` import route is separately "NOT console-proven" — T2 is
   the proving experiment; the composed guard uses the inline syscall, same op).
6. Then read+Zydis-decode ≤16 bytes (ASM:288-297); JMP/CALL ⇒ recursive hop resolution
   (ASM:329-334, each hop re-guarded); `exit()` restores EXEC-only 0x4 (ASM:180); restore
   failure latches the anomaly (ASM:184-188, loud ASM:256-262).

---

## 2. Candidate mechanisms to pass GATE-1 honestly

| id | mechanism | new evidence needed | failure mode | fits pinned import surface? |
|---|---|---|---|---|
| **M-A** | **IGW RX verdict → measured fold flip.** Run the STAGED armed exec-write probe (`worker-igw-row0-rebuild/modbridge_exec_write_probe_armed.prx` `e1541af6…`, row 0 = T2 `0x7878000` X→RX `confirmed=1` only) under a probe-loading ini (Option A `fd9c7393…` recommended, STAGING-PROPOSAL §1-2). T2 rc==0 ⇒ `W_WORKS` ⇒ controller flips `RC_ASM_WALK_RX_GUARD 0→1` (measured object `f5627427…`) and recomposes. Guard then transitions RX per read, decodes, restores. | The T2 record chain: `vmmap_persist span_prot=0x4` pre-proof; `attempt mprotect_route=sceKernelMprotect_import rc` (THE number); `rw_test read_done=1`; `restore` outcome=10; `post_restore restored_matches_from_prot=1` (STAGING-PROPOSAL §3). | T2 rc<0 ⇒ `W_FAILS_R_WORKS` (fold stays 0; D-1-only world). Refusals 5/6 (map drift) reconcile-only. ABSENT = probe didn't load. Residual at runtime: a walk page deep in text could still refuse its transition ⇒ `rx_transition_denied` — loud, no read, fail-closed (ASM:156-161), never a crash. | **Yes — this IS the designed experiment.** Probe: single typed `sceKernelMprotect` import (loud provenance flag carried). Composed guard: inline syscall 74, zero imports (fold-1 measured). No initializer change at all (`e7f349af…` stays intact). |
| **M-B** | **Bounded first-instruction decode WITHOUT reading** — answer the thunk question (first insn JMP/CALL ⇒ target) from the file-backed image bytes instead of runtime exec bytes: bridge r7 already observes the exec span's `kve_path` (spanpub:315, :354-409, :1007-1011; tags `bridge_kve_distinct0-3`/`bridge_kve_eboot`); open the backing file in-process, map VA→file offset via ELF phdrs (runtime = base + vaddr; uniform-bias model confirmed L0/L1/L3/L7, SWAP §1.1), Zydis the FILE bytes, derive the resolve result. | (1) `kve_path` actually observed in-game for the anchored image; (2) in-process read capability for the app0 backing file from the composed PRX (ps5io rdwr covers /data; app0 read unproven); (3) ELF program headers of the file. | **Identity is unverifiable by construction**: proving file bytes == runtime bytes requires the very exec read being avoided. Any divergence (loader-applied relocation/patching at a thunk entry) silently mis-resolves — a silent-wrong class this project's loud-only discipline refuses; would have to ship labeled DERIVED, never READ. Plus a new resolve path in the initializer (engine-adjacent edit, new revision). | Mechanically yes (file IO + Zydis already in surface; no new privileged ops). Fails the evidence discipline at the identity step; heavier initializer surgery than M-A. |
| **M-C** | **Raw-entry resolve skip** — PROSPERO macro variant returning `vt_entry_value` without `resolve_function_address_from_potential_jmp` (zero exec reads at resolve time). | None kernel-side; needs a new distinct-ID initializer/macro revision. | Only moves the read: any later PLH detour arm on the raw (possibly JMP-thunk) address does its OWN naked disasm+write — the unguarded GATE-2 crash class (ASM:31-34 explicitly does NOT cover PLH phases). Thunk-vs-real semantics shift for address consumers. | Yes (pure code change) but consumes none of the bridge evidence and leaves the crash class armed. |
| **M-D** | **Alternate hook shape — data-side dispatch, no exec-derived addresses at all.** R3 SetFuncPtr marshal (heap tier, in-tree UFunction.cpp:78-90) for the pump; vtable-swap backend (SWAP §5 R1/R2) for virtual targets = one aligned 8-byte store into the RELRO vtable slot after **D-1** (mprotect R→RW on a prot=1 DATA page — a different syscall-74 page class, its own unproven fact, SWAP §1.2/§3). Exec pages never read or written by this architecture (SWAP §0.3). | D-1 same-value-writeback probe verdict on a far RELRO page (design exists, SWAP §3; "meaningful ONLY after IGW-1's outcome is reconciled", SWAP §3). | D-1 refusal ⇒ swap backend dies ⇒ heap tier (R3) + polling only (SWAP §5.1). ProcessEvent parity needs the WHOLE UClass family (partial = silent per-class holes; R4 loud coverage required). FName ctor not swap-reachable (direct E8; FC-1 replaces it — and FC-1's reads need RX, i.e. M-A anyway). | Yes — swap thunk = PRX's own .text; no new imports; syscall 74 same number. The pre-built hedge for W_FAILS_R_WORKS (STAGING-PROPOSAL §4 decision table). |
| **M-E** | **Settings avoidance** — `HookLoadMap=false` etc. so the resolve blocks never run (the ini keys exist; `UI:1977/:1986/:2113-2191` are all flag-gated; ProcessInternal derivation already honors settings-v3 keys, UI:2219). | None. | Does NOT pass GATE-1 — relocates it to the next enabled flag (EngineTick `UI:1989`, then the family). Disabling the whole family lobotomizes UE4SS features (reload, RegisterHook) without producing the RX verdict; honest but insufficient alone. | Yes, trivially — config-only. |

---

## 3. Recommended primary route + unit-level design

**PRIMARY: M-A (IGW RX verdict → measured fold flip).** It is the project's own designed
GATE-1 pass — staged, measured, decision-tabled — and the ONLY candidate that produces the
kernel-evidence verdict the rest of the ladder also consumes (FC-1 validator reads,
FLAG-3 ProcessInternal reads, any future U-SWAP-4 PLH layer all require RX; SWAP §5 C1,
SIGNATURES-INVENTORY). M-D stays the pre-built hedge if T2 refuses. M-B rejected as primary
(unverifiable identity = silent-wrong class); M-C rejected (punts the read into unguarded
PLH disasm); M-E rejected (relocates, not passes).

Concrete design (proposal; every step controller/user-owned, none executable by this
worker):

1. **Verdict acquisition** (console, user-coordinated; per worker-igw-row0-rebuild
   STAGING-PROPOSAL §2): files-first-config-last — STOR `e1541af6…` armed probe to
   `/data/plugins/modbridge_exec_write_probe_armed.prx`, readback-verify, presence guard,
   then Option A ini (`fd9c7393…`, 9-entry canonical, xwp-armed FIRST), `.bak` + byte-exact
   RETR, bounded restore law. USER launches. Expected grammar: seq1 entry armed=1
   `igd1-e030a09eb4d1` → seq2 startup budget=8 → row0 `confirmed:1 addr 0x7878000 from 0x4
   to 0x5` + `vmmap_persist span_prot=0x4` → `attempt … rc` → `rw_test read_done=1
   write_tested=0` → `restore outcome=10` → `post_restore restored_matches_from_prot=1`.
   **rc==0 ⇒ W_WORKS.** Honest caveat carried: T2's page is the exec span's tail page past
   the file-content end (design-pinned crash safety), same kernel VMMAP record class
   (prot=0x4, span `[0x400000,0x787c000)`) as the code pages — a class-exact, byte-remote
   proxy; runtime residual refusals on real code pages remain possible and are
   fail-closed-loud by the guard itself.
2. **Activation** (controller-owned, distinct receipt; walkguard-prep REPORT §4 PRIMARY
   flip, zero new code): in the staged `ASMHelper.guarded.cpp` change
   `#define RC_ASM_WALK_RX_GUARD 0` → `1` (keep the basename `__FILE_NAME__` pin), admit
   via `--file`, declare expected asmhelper object `f5627427131f53ac…` in the manifest,
   recompose through the same edition/driver. Never fold without the verdict (the WST-5
   gate).
3. **Runtime pass path** (already implemented in the pinned unit — no edits): per walk
   read, `ScopedRxGuard` ctor → `enter` → inline syscall 74 X→RX over the exact page span
   `[addr & ~0x3fff, …]` → 16-byte Zydis decode → dtor → restore EXEC-only 0x4 → fresh
   reads re-guard. Recursive JMP hops re-guard per hop (ASM:329-334). All refusals stay
   loud, zero-read, fail-closed (ASM:156-161, :228-238); restore anomaly latches loudly
   (ASM:180-188, :256-262).
4. **Expected oracle lines in the W_WORKS launch** (evidence shapes, not predictions):
   `GameEngine::LoadMap address 0x…` (`UI:1982`) replacing the W1/W2 pair; `GameEngine::Tick
   address (vtable: 0x…; scan: 0)` (`UI:1992`); then the resolve-family lines
   (`UI:2114-2190`); runtime-status no longer shows
   `state N failed 1 [WalkRxGuard] refused: guard_off_no_rx_verdict`.
5. **Diagnostics**: none NEW required — the guard's own W1/W2/refusal lines and the
   initializer's per-site address lines ARE the evidence (adding tags would touch the
   pinned unit for no decision value). Optional polish only: a bridge-side
   `diagnostic_event("walkguard_first_rc", rc)` echo wired from the guard's first
   transition — proposed, not needed.

---

## 4. R3 interaction verdict (RUNBOOK §5.1 cross-check)

- The RUNBOOK's trap (§5.1): DIFF-1's ladders keep `is_engine_tick_hook_available()`
  (address availability, LuaMod-l11-merged :4673-4675) as the PRIMARY predicate; in any
  walk-permitted world the vtable fallback assigns Tick, so availability is true,
  `ensure_engine_tick_hooked()` runs, and the first default-method ExecuteInGameThread
  throws `EngineTick callback registration failed` — the marshal sits armed and unused
  (ladder sites :5015/:5058/:5218-5223).
- **M-A produces exactly that walk-permitted world. Therefore the trap APPLIES unchanged:
  my route does not change, defuse, or substitute for the §5.1 re-order.** The
  marshal-first-when-armed ladder re-order (ExecuteInGameThread + all three WithDelay
  ladders, distinct-ID revision, DIFF-1 author/controller) remains BLOCKING for the
  spawner's default path, exactly as the RUNBOOK states.
- Hazard note (recorded, not relied on): the rejected M-C/M-E routes would leave Tick
  UNASSIGNED, which would ACCIDENTALLY flip the availability predicate to false and fall
  through to the marshal — a silent behavior coupling. RUNBOOK §5 forbids silent patching;
  if any such revision ever lands, §5.1 must still land as its own distinct-ID fix.
- Also unchanged by M-A: **GATE-2** (fold=1 permits walk READS only; PLH detour
  disasm+write phases are explicitly NOT covered, ASM:31-34 — detour arms stay compiled
  out / rerouted); **GATE-5** (LuaMod's eager LoadMap registration still needs its scoped
  guard, RUNBOOK §5.2, unowned); **GATE-3** (pad ownership, wpd2 retry, independent).
  First-menu shape stays the pump-chain conjunction: walk reads permitted + R3 posture
  (DIFF-2 + §5.1 + §5.2) + a draining carrier + a delivered pad edge.

## 5. Honest bounds

Source-and-report derived only; zero console contact; zero engine edits; nothing staged,
flipped, or launched; no outcome claims for any launch. Line numbers are from the current
`e7f349af…` initializer and the pinned units as read today; any further controller edits
re-anchor them. The W1/W2 shapes are the pump-chain worker's pinned expectation, not a
captured result of mine.
