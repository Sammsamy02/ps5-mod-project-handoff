# NO-EXEC-GATE-DESIGN.md — worker-no-exec-mode-gate, 2026-10-04

PROPOSALS ONLY. Closes LAUNCH10-MATRIX gap **U-SWAP-5** (the matrix's "GAP: no worker owns
this yet" cell). The merged staged initializer
(`TEAM:worker-initializer-integration/staging/UnrealInitializer.cpp`, sha256
`c85c42f7aaab0630569d65db599fbf8cfcbd0b79b8e99c6e27ea709aee5a8b7b`, 2044 lines) is FROZEN
post-delivery; this document + `NO-EXEC-GATE.diff` are the NEXT revision's diff set.
Deliverable file set in this directory:

- `CLAIM.json` — claims/non-claims (written first)
- `NO-EXEC-GATE.diff` — the exact unified diff vs the staging baseline (11 hunks, 374 lines)
- `UnrealInitializer.staging-baseline.cpp` — byte-identical copy of the frozen unit (hash above)
- `UnrealInitializer.gated.cpp` — the gated candidate the diff was generated from
  (sha256 `40d6cd4974722e0db2af77dc0ca9b648a9d2c8ba3d8bc2143c01d92da5431531`)
- `apply_gate.py` — the anchored replayer (28 exact-anchored replacements, aborts on drift;
  re-runnable against a new staging revision)
- `host-test/gate_pattern_mock.cpp` — compile/warning evidence for the inserted hunks

---

## 1. Task 1 — every exec-touching site the armed fold leaves live

Cite base: **current staging numbering** (this file's line numbers). The matrix U-SWAP-5
row and LAUNCH9-PREDICTION §2 cite the pre-S2-wrapper merged numbering (their `:1749-1764`
registration = `:1787-1802` here; their `:1876` PI gate = `:1914` here; their
`:1385-1397` FName skip = `:1424-1434` here; the SCO `:480` and arming-region cites are
within a few lines). Integration must use THIS table's numbers against
`UnrealInitializer.gated.cpp` (whose offsets differ again by the inserted hunks — the diff
hunk headers are authoritative for the gated file).

Exec classes per LAUNCH9-PREDICTION §1/§2 and its sources: **FLAG-2** = vtable walk ending
in `ASM::resolve_function_address_from_potential_jmp` →
`get_first_instruction_at_address` reading text (`UnrealVirtualBaseVC.hpp:115,133` →
`ASMHelper.cpp:57-69`, **no `__PROSPERO__` branch** — verified in the
`ue4ss-unreal-native-build-1` spike snapshot); **FLAG-1** = PLH x64Detour install
(100-byte disasm READ + RWX protect + jmp WRITE, `x64Detour.cpp:322-397`); **FLAG-3** =
Zydis code derivation reads.

| id | hop/class | site (current staging lines) | gate today |
|----|-----------|------------------------------|------------|
| E-1 | I6 / FLAG-2 read | LoadMap vtable walk `:1690-1698` (macro `:1693`) | `bHookLoadMap` ini key only (default true, scaffold `comp6-integration/UE4SSProgram.cpp:1240-1252`) |
| E-2 | I7 / FLAG-2 read | Tick vtable walk `:1702` | `bHookEngineTick` only |
| E-3 | I7 / FLAG-2 read (latent when armed) | vtable-mismatch DISPLAY walk `:1707-1737` — up to 17 `resolve_function_address_from_potential_jmp` text reads (`:1725`, loop `:1720-1735`) | requires non-null `scan_address` (`:1707`); **unreachable when seeds armed** (TickInternal never assigned → `:1759` null), live in the legacy path |
| E-4 | **I7 / FLAG-1 write** | `RegisterEngineTickPreCallback(HookedEngineTick, …)` `:1787` + ERROR_ID throw `:1788-1791`; twin `RegisterEngineTickPreCallback(QueryHarnessEngineTickDispatch, …)` `:1798-1802` | `bHookEngineTick` only — **no exec-verdict gate** (the matrix's named I7 gap) |
| E-5 | I8 / FLAG-2 read | eight further virtual resolves: InitGameState `:1810`, BeginPlay `:1819`, EndPlay `:1828`, AActor::Tick `:1837`, GameViewportClient::Tick `:1846`, ProcessEvent `:1855`, ProcessConsoleExec `:1864` (GET_ variant, same resolve tail), UStruct::Link `:1873` | individual `bHook*` ini keys only |
| E-6 | **I12 / FLAG-1 write** | `RegisterStaticConstructObjectPostCallback(…)` `:480-507` — detour at the slot-4 seed constant whose identity is **UNVERIFIED by design** (`:847-855`: "no data-side validation exists") | **NONE — unconditional** (the matrix's named I12 gap; L9 I12: wrong-constant detour = write to a wrong text address, a hazard *beyond* writability) |
| E-7 | FLAG-3 read | ProcessInternal/PLSF derivation `:1914-1975`: `GetFuncPtr()` `:1922` + jmp resolve `:1923` + 164-byte Zydis decode `:1944-1972` | v3 ini keys (both REQUIRED 0, SETTINGS-V3-DECISION §1) — **settings-only closure**; a stale/typo'd ini re-opens it silently |
| E-8 | FLAG-1 read+write | FName ctor detour verifier: `RegisterFNameConstructorPostCallback` `:1317`, reached from `:1424-1434` | skipped only when `SeedArming::bSeedBoundFNameCtor` (`:1425`); LIVE whenever seeds are not armed |
| E-9 | FLAG-2-class read (scan) | legacy `ps_scan` loop `:957-981`; Lua AOB `DoScan` arms `:1186` / `:1286` | structurally dead when armed (`:944-953`, gates `:1181`/`:1276`); **the only address source when the table is ABSENT** |

Deliberately **not** gated (disclosed, not silent): the seed chain's behavioral probe
`:835` (`FName ProbeName{STR("ByteProperty"), FNAME_Find}`) *calls* the seed-bound ctor —
a call into the engine's own live function, neither a text read nor a write; it is the
data-side tier's own verification design (FNAME-VERDICT §5) and stays. Likewise
`Conv_NameToString` retrieval `:1484` and later Lua-driven engine calls are call-class.
Data-side keep-alives (never gated): slot bindings `:753-855`, pool chain `:785-828`,
`Hook::StartCallbackGarbageCollector` `:476`, GUObjectArray listeners `:455-461`,
required-object waits `:1513-1628`, Kismet/reflection/TypeChecker `:1462-1511`,
`:1880-1898`, S2 layout gate `:1220-1234`/`:1291-1298`/`:1406-1414`/`:1979-1989`.

**Summary answer to the matrix's premise:** under a complete 4-slot table +
BOTH_FAIL/FATAL, an unmodified armed launch re-opens FLAG-2 at E-1/E-2/(E-5) and FLAG-1 at
E-4/E-6 — exactly the matrix's I6/I7/I8/I12 set, plus the three latent/settings-only sites
E-3/E-7/E-8 the row did not name.

## 2. Task 2 — the gate: source, mechanism, loud-failure policy

### 2.1 Source recommendation: COMPILE-TIME (define), not a settings key

`UE4SS_NO_EXEC_MODE` — build define with an in-file `#ifndef` fallback default `0`,
folded into `static constexpr bool NoExecGate::kNoExecMode` (diff hunks 1-2, gated
`:72-84` / `:126-168`). Reasons, tied to precedent:

1. **v3 precedent trap.** SETTINGS-V3-DECISION §1 trap 2: a missing/invalid key silently
   keeps the `true` default (parser swallows exceptions). For a safety gate that means a
   typo'd key *silently re-arms exec* — precisely the "silently half-armed" failure this
   design forbids. v3 had to add a readback oracle to compensate; a compile-time constant
   needs no runtime trust at all.
2. **Constant folding = no re-arm path.** With `kNoExecMode=true` every refused site is
   compiled OUT (`if constexpr`); no ini key, Lua call, or probe file can reach an exec
   page through this unit. That is a strictly stronger guarantee than any runtime flag.
3. **Unit-local diff.** A settings key touches SettingsManager.hpp/.cpp +
   UE4SSProgram.cpp (three more units into the replace-receipt walk); the compile-time
   gate keeps the merged-unit change self-contained — matching the matrix R5 note
   "no new key REQUIRED if the gate is compile-time" and the unit's own
   designed-not-armed constexpr precedent (`SeedArming::kExecWritabilityVerdictGating`,
   staging `:623-638`) and SHAPE-A fold (`:883-930`).
4. **FATAL-row robustness (R9).** Console/settings recovery precedes any staging there;
   a build that cannot be talked into exec by any parsed input is the right shape for the
   post-FATAL composition.

The v3 ini keys (PI/PLSF=0) remain shipped and complementary; they are necessary context
for E-7 but no longer its only closure.

### 2.2 Mechanism (what the diff does)

Wrap pattern is the unit's own S2 precedent (`:1409-1414` "byte-minimal reviewable diff…
not re-indented"): every refused region keeps its original bytes inside a `constexpr`-dead
`else { … }`; nothing in the armed-and-un-gated behavior changes. 15 `if constexpr`
gate sites = 12 `refuse_exec_site` refusals + 3 throws/guards:

| gated site | diff hunk | gated-file behavior |
|---|---|---|
| E-6 SCO callback `:479-507` | hunk 4/5 (`:534-554` region) | refusal line + consequence line; slot-4 address stays bound (inert) |
| arming decision `:897-929` | hunk 6 (`:980-999`) | **table absent + gate ON → THROW `[NoExec] REFUSED: …legacy exec-page scan path…`**; table partial → the UNCHANGED `[Seeds] Required-slot seed table incomplete` throw fires FIRST (see §4); armed → one `[NoExec] mode ON` line |
| E-8 verifier head `:1301` | hunk 7 (`:1388-1403`) | defensive THROW (unreachable when armed; keeps the invariant named against future call-site edits) |
| E-1 LoadMap `:1690-1698` | hunk 8 (`:1787-1827`) | refusal + consequence (RegisterLoadMapPreHook pcall-guards per L9 L1) |
| E-2/E-3/E-4 EngineTick block `:1699-1806` | hunk 8/9 (`:1787-1827`, `:1927-1930`) | whole block compiled out: no walk, no display walk, no assign, **no detour install**; refusal names identity/ExecuteInGameThread/QueryHarness consequences |
| E-5 eight resolves `:1807-1878` | hunk 9 (`:1927-2070`) | per-feature refusal (one named line each); ProcessEvent adds the cross-fallback consequence |
| E-7 PI/PLSF `:1914-1975` | hunk 10 (`:2101-2119`) | skip unconditional regardless of ini; refusal only if the ini still asked |
| summary | hunk 11 (`:2189-2202`) | after `PostInitialize`: `[NoExec] summary: mode=on exec_sites_refused=<N> engine_ready_data_side=1` |

Support hunks: define block (hunk 1), `NoExecGate` namespace with counter + helper (hunk
2), `[[maybe_unused]]` on `QueryHarnessEngineTickDispatch` (hunk 3) — the one file-static
that becomes unreferenced under gate ON (warning discipline).

### 2.3 Loud-failure policy (never silently half-armed)

- **Refuse-and-continue** (named `[NoExec] refused:` Warning line + consequence, counter
  incremented) for exec *features* whose absence must degrade the session but not kill the
  data-side engine-ready attempt — the entire point of rows R5/R9.
- **THROW** where no data-side fallback exists: seed-table-absent under gate ON (E-9 would
  be the only address source), and the defensive FName-verifier guard. Surfaces as the
  standard loud `failed(1,<msg>)` ack (BACKEND `:59-71` → OWNER `:111-118` chain).
- **No silent half-arming:** no site binds an address while skipping its registration
  (Tick/LoadMap assigns are inside the skipped regions), and the half-state that *does*
  exist by design (slot-4 bound, hook dead) is stated in its refusal line.

## 3. Task 3 — the degraded path (no-exec mode ON, seeds armed)

### 3.1 What still works (data-side)

- All four seed bindings + validation: base check, GUObjectArray pointer probe + bind,
  gmalloc live-pointer probe + bind, FName pool chain (shape/ASCII/ids), ctor bind +
  behavioral ByteProperty probe, slot-4 bind (`:741-856`).
- Object waits ≥10000, Kismet discovery chain, Conv_NameToString via the Kismet fallback
  (fname_tostring unbound-by-design degradation `:918-928`), required-object machinery,
  reflection (`FindFirstOf`/`StaticFindObject*`), TypeChecker, FName globals.
- PostInitialize data-side: GMalloc check passes (slot 3), searcher pools populate from
  `ForEachUObject`, GUObjectArray create/delete listeners remain the post-init pool feed,
  shutdown listener, `IsInitialized()`.
- S2 QueryHarness **layout gate** arms (PublishStagedNativeLayoutMaps is data-side); its
  per-tick *dispatch* is dead (rode E-4) — the harness's own design counts refusals and
  never throws from a tick (`:1792-1797` comment).
- Engine version via the `[EngineVersionOverride]` settings route (SLOTS-FEASIBILITY §4b;
  SettingsManager.cpp:171-173) — unchanged.
- **R3 tier compatibility:** the gate keeps every path the R3 tier needs (heap/data
  writes, UFunction objects, reflection) alive and adds no gate of its own in their way.
  Per VT §5 R3 (via MATRIX U-SWAP-3): `UFunction::SetFuncPtr` is an in-object heap FuncPtr
  write (prot=3 class, igf4-proven; no vtable, no mprotect) and therefore **survives
  BOTH_FAIL and FATAL** (MATRIX R9 cell says exactly this). **PENDING-R3:**
  worker-r3-zerosyscall-tier was CLAIM-only at design time (`CLAIM.json` R3-1..R3-5,
  `R3-TIER-DESIGN.md` absent); its spawner-dependency map — which Lua traffic
  (ExecuteInGameThread alternatives, RegisterHook wrappers) survives under no-exec — is
  cited here as pending reconciliation, per MATRIX checklist box 8. Its non-claims stand:
  R3 can never carry EngineTick (C++ virtual, not a UFunction).

### 3.2 What is dead (each loudly named at startup)

- Game-thread identity publication (`NativeGameThreadIdentity` publishes only from the
  genuine tick callback, `:170-177`) → `IsInGameThread()`/`GetGameThreadId()` unusable,
  `IsGameThreadInitialized()` false — named in the EngineTick refusal consequence.
- `ExecuteInGameThread` closure drain (F7-2, L9 §3: rides I7's detour) — **dead**; and the
  ProcessEvent-mode cross-fallback is dead too (needs the E-5 ProcessEvent walk,
  SETTINGS-V3-DECISION §2). Menu acceptance is honestly degraded per EW §7.3 / MATRIX R5
  unless the EW §7.3 polling replacement lands (composition-level work, NOT in this
  unit's diff — explicit dependency for the R5/R9 integrator).
- All ten virtual hooks (LoadMap, EngineTick, InitGameState, BeginPlay, EndPlay,
  AActor::Tick, GameViewportClient::Tick, ProcessEvent, ProcessConsoleExec, UStruct::Link)
  and their Lua RegisterHook equivalents on C++ virtuals.
- SCO searcher hook post-init adds (pools fed by listeners only).
- PI/PLSF detours → custom events and pre-R3 script-function RegisterHook stay dead
  (SETTINGS-V3-DECISION §2 "what is actually lost").

### 3.3 Signatures for the matrix lookup (rows R8/R9 — and R5)

Truth table (gate ON unless noted):

| seed table | terminal | UE4SS.log signature | status/ack |
|---|---|---|---|
| absent | NEW named refusal | `[NoExec] REFUSED: no-exec mode forbids the legacy exec-page scan path (ps_scan / Lua AOB); a complete REQUIRED-slot seed table is the only data-side address source. …` | loud `failed(1,<msg>)` |
| partial (R8's HOLD composition) | **UNCHANGED T-A** — the `[Seeds]` throw at `:899-909` precedes all `[NoExec]` logic | `[Seeds] Required-slot seed table incomplete: present […] missing [gmalloc] …` byte-identical to launch-9 evidence | loud `failed(1,<msg>)` |
| armed, healthy | **data-side engine-ready** | ordered: `[Seeds] Required-slot seed table armed (slots 0/2/3/4)…` → `[NoExec] mode ON (UE4SS_NO_EXEC_MODE=1)…` → the existing `[Seeds] slot …`/pool/probe lines → twelve `[NoExec] refused:` lines (LoadMap; Tick; SCO; the eight resolves; PI only if ini asked) → `Post-initialization: GMalloc: …` → size logs → `[NoExec] summary: mode=on exec_sites_refused=<N> engine_ready_data_side=1` | runtime-status.txt UNTOUCHED formats: entry-r4 freshness-prefixed lines, last line `activated`, then the unchanged I13 ack chain (`engine-ready` + `mods-started` back-to-back) |
| armed, seed refuted | UNCHANGED T-REF | `[Seeds] REFUTED: <detail>` (gate silent) | loud `failed(1,<msg>)` |

Matrix-consumption notes:

- **R8** (fc1 pool-side REFUSED, U-SWAP-1 held): box-2 D1 lookup sees the documented
  T-A/T-REF signatures because the partial-table throw strictly precedes the no-exec
  precondition, and with gate OFF the unit is behavior-identical to the frozen one. Either
  way the R8 lookup reads cleanly.
- **R9** (FATAL): the gate build contains no mprotect/jmp-write code path at all
  (constexpr-dead), so the permanently-closed syscall route cannot re-open through this
  unit; IGW/fc1 probe lines `= 0`/removed are ini/composition work outside this diff
  (console recovery still precedes staging per the R9 row).
- The `exec_sites_refused=<N>` count and the one-line summary are the machine-checkable
  handles for the log-parser workers; N=12 is the expected healthy value (11 without the
  PI ask when v3 keys are honored as shipped — PI refusal fires only if the ini asked).
- The `[NoExec] REFUSED: …` string is NEW evidence vocabulary: the next matrix revision
  should add it to §4 box 2's D1 match list (table-absent-under-gate).

## 4. Task 4 — arm / disarm procedure (per-launch, composition-owned)

**ARM (rows R5/R9):**

1. Copy the frozen staging unit to the revision staging dir and apply
   `NO-EXEC-GATE.diff` (or run `apply_gate.py`, which aborts on any anchor drift against
   a changed base). Result hash-pins at `UnrealInitializer.gated.cpp`
   `40d6cd49…` with fallback default 0 (behavior-identical to frozen).
2. Choose ONE sourcing, both covered by receipts:
   - **Recommended:** flip the in-file fallback — `#define UE4SS_NO_EXEC_MODE 1` in the
     staged copy. The armed state is then inside the hashed source itself (self-evident in
     the replace receipt), zero driver changes.
   - Alternative: pass `-DUE4SS_NO_EXEC_MODE=1` on the initializer TU's compile line in
     the comp6 overlay and record the flag in the run manifest (the source alone no longer
     proves the armed state — the manifest line is then load-bearing).
3. Regenerate BOTH receipts (OPC §1/§3.1): `regen_extra_receipt.py`, then
   `regen_replace_receipt.py --program-source <NEW staging dir>` — this is a merged-unit
   change, so the replace walk covers it exactly like the comp6 initializer fold
   (MATRIX R5 delta cell). The staged `SeedTable.h`/`SeedValidation.hpp` quoted includes
   stay in the file set.
4. `--verify-only` preflight, then the one pass with fresh run/candidate dirs (OPC
   P8/P9). Ini delta: none REQUIRED (v3 stands); R9 additionally strips the probe lines at
   composition level.
5. Expected healthy-run markers to check post-launch: §3.3 armed row.

**DISARM:** flip the define back to `0` (or drop the build define) → the unit is
behavior-identical to the frozen merged initializer (not byte-identical source: the
comment/gate scaffolding remains, constant-folded away — regenerating the receipts and
re-pinning the pre-image sha is mandatory after ANY flip, per the standing
"receipts are stale after ANY source change" law). There is deliberately no runtime
disarm: that is the design's guarantee, not a limitation to fix.

## 5. Verification evidence and honesty bounds

Verified here (host-side only, no engine, no console contact):

- The gated candidate is a mechanical, replayable transform of the byte-identical staging
  baseline: 28 exact-anchored replacements, all matched exactly once (`apply_gate.py`
  aborts otherwise); brace-depth landmarks identical to baseline at every function
  boundary checked, final depth 0/0; ASCII-only string literals; zero unescaped-newline
  accidents.
- `host-test/gate_pattern_mock.cpp` — the inserted statement structures compile
  `-std=c++20 -Wall -Wextra -Werror` **PASS under gate=0, gate=1, and undef**; runtime
  probes: gate=0 silent passthrough (exit 0), gate=1 surfaces the FName-verifier REFUSED
  throw (exit 2). The mock also reproduces the `QueryHarnessEngineTickDispatch`
  file-static case: `[[maybe_unused]]` suppresses `-Wunused-function` and
  `-Wunneeded-internal-declaration` in the discarded regime.

NOT verified here (owned downstream):

- No compile of the real TU against the UE4SS include tree; no in-engine run; no claim
  any BOTH_FAIL/FATAL verdict exists. The gated file's zero-warning status on the real
  toolchain (including the discarded-branch checking of `if constexpr` in non-template
  code) must be confirmed by the comp pass before staging.
- The `CharType`/`STR`/`Output::send` call shapes mirror existing usage in the same file
  (`:491-492`, `:1455`) but the Prospero wide-char format path is not exercised here.
- R3 spawner-dependency map: **PENDING-R3** (§3.1).
- Line numbers in §1 are the current staging numbering; the matrix/L9 cite base differs
  (mapping in §1 preamble). If the staging copy moves again, re-run `apply_gate.py` — its
  anchor failures ARE the drift detector.
