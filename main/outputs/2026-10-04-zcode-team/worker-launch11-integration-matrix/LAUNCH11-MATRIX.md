# LAUNCH11-MATRIX — the post-launch-10 integration decision matrix, pre-built

Worker: worker-launch11-integration-matrix · 2026-10-04. Read-only synthesis; no console
contact, no nested subagents, nothing executed (one sha256 readback, itself recorded as
evidence in L11M-2d). Purpose: make the post-launch-10 integration cycle ONE LOOKUP, the
way LAUNCH10-MATRIX did for launch 10 — read launch 10's evidence (§5 checklist), find
the row (§2), apply the deltas (§3), honoring the integration-decision ledger (§4) before
any staging.

Path abbreviations (reused from cited workers):
- `TEAM=` `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/`
- `L10M=` `TEAM:worker-launch10-matrix-prep/LAUNCH10-MATRIX.md` (dimension law §0, rows, §3 skeleton, §4 checklist — the base this matrix updates)
- `L10PB=` `TEAM:worker-launch10-prebuild/` (the launch-10 armed candidate: `prx-candidate-composition6-l10-a`, armed table `59effe6e…`, wrapped fself `d8cb4a76…`)
- `L11PB=` `TEAM:worker-launch11-vtable-prebuild/` (PREPARED-UNARMED candidate `prx-candidate-composition6-l11-a`(+`-b`), seam + backend header, R2 build-ready diff, gate GO 15/15)
- `WST=` `TEAM:worker-walk-site-transitions/` (`ASMHELPER-WALK-GUARD.diff`, fold law, PREBUILT-.o stale-link fact)
- `RDA=` `TEAM:worker-r3-diffs-authoring/` (DIFF 1 `r3-luamod-funcptr-marshal.diff` `a8d1d6b4…`, DIFF 2 `r3-initializer-r3-marshal-mode.diff` `d556c4e7…`, ARM-DISARM.md)
- `NE=` `TEAM:worker-no-exec-mode-gate/` (`NO-EXEC-GATE.diff` `339ef3c4…`, `apply_gate.py`, baseline `c85c42f7…`)
- `PLH=` `TEAM:worker-polyhook-prospero-layer/` (CLAIM-1..6 + compile-gate receipts; REPORT pending — IN FLIGHT)
- `CC1=` `TEAM:worker-carrier-probe-build/` (`modbridge_carrier.prx` fself `d83410aa…`, STAGE-PLAN, constants brief)
- `R3D=` `TEAM:worker-r3-zerosyscall-tier/R3-TIER-DESIGN.md` (the R3 design whose BOTH_FAIL branch RDA authored)
- `OPC=` `TEAM:worker-comp6-driver/ONE-PASS-CHECKLIST.md` (receipt/preflight law; `regen_extra_receipt.py` / `regen_replace_receipt.py`)

---

## 0. What changed since LAUNCH10-MATRIX (the successor artifacts, and their exact maturity)

L10M §1's unit-swap catalog said what launch 10 COULD change. Launch 10 shipped exactly
one change (the armed table; L10PB REPORT §1) — everything else was unbuilt. Since then:

| L10M unit | status THEN | status NOW (evidence) |
|---|---|---|
| U-SWAP-1 initializer re-seed (armed table) | generator + consumer landed | **SHIPPED in launch 10** (L10PB: armed `SeedTable.launch10.h` `59effe6e…` → initializer object `983b18e2…`; composed runtime `d6214abd…` identical to L11PB's disarmed vehicle) |
| U-SWAP-2 vtable backend R1+R2 | DESIGN ONLY | **R1 staged PREPARED-UNARMED** (`L11PB`: 5-site seam, `DispatchDataDetour.hpp` with REFUSING D-1 seam, gate GO 15/15 `ee753d48…`, disarmed object byte-identical `983b18e2…`, armed `0180d374…` +360 B); **R2 capture BUILD-READY** (`build-ready/VT-R2-UNREALVIRTUALBASEVC.diff` + `apply_vt_r2.py`, anchors 1/1+2/2); **D-1 still NOT implemented** (refusing seam, zero `0F 05` bytes) — the row's gating unverified fact is UNCHANGED |
| U-SWAP-3 R3 zero-syscall tier | worker in flight | **R3D landed** (verdict: acceptance ladder cannot complete as composed today — init fatal at `RegisterEngineTickPreCallback` `SINI:1383-1397`; CAN complete visible-menu+spawn rungs IF the marshal calibration finds a carrier; A10 + global-visibility stay dead) and its **BOTH_FAIL-branch diffs are AUTHORED + host-verified** (RDA: both compile exit 0, armed variant `-DUE4SS_R3_MARSHAL_MODE=1` object `686a573a…`, werror kind-parity, 24/24 mock checks; link + composition UNVERIFIED) |
| U-SWAP-4 PLH Prospero layer | DESIGN ONLY | **PLH in flight, compile-gated** (CLAIM-1..6: one new TU, zero edits to canonical PLH sources, no ODR conflict; four platform TUs compile under the exact polyhook unit argv; verdict dependencies documented, nothing linked/executed; REPORT.md pending) |
| U-SWAP-5 no-exec gate | GAP — no worker owned it | **DELIVERED** (`NE`: `NO-EXEC-GATE.diff` 11 hunks/374 lines, `UE4SS_NO_EXEC_MODE` constexpr, 15 if-constexpr sites, 12 counted loud refusals, E-1..E-9 enumeration; fallback 0 = frozen behavior; host mock gate=0/1/undef PASS; real-TU zero-warning status still owed to the comp pass) |
| U-SWAP-6 GMalloc slot-3 validator ridealong | pattern only | **OBSOLETE AS SEPARATE BUILD**: launch 10 armed slot 3 directly and its bound line (`slot 3 gmalloc bound at 0x930add8`) is the in-object validator evidence; D4's remaining instrument is `[Seeds] REFUTED` + the fc1 crosscheck |
| — (new) walk-site RX guard | — | **NEW U-GUARD**: `WST` delivered `ASMHELPER-WALK-GUARD.diff` (single innermost read `get_first_instruction_at_address` covers ALL FLAG-2 walks incl. `UnrealVirtualBaseVC.hpp:115/:133` macro and `DetourSubclasses.hpp:95`, zero call-site edits); fold define `RC_ASM_WALK_RX_GUARD`; host logic 144/144; real-argv compile gate both folds (fold0 `81dd832b…` **0 syscall instructions**, fold1 `6604ce2d…` exactly 1); **PREBUILT-.o stale-link fact** (WST-1) |
| — (new) cc1 carrier census probe | — | **NEW U-CC1**: `CC1` built + deterministic (fself `d83410aa…`, run1==run2), artifact checks PASS, host tests 24/24 incl. every refusal class, 9th-ini STAGE-PLAN under the final-ini law (8 of MAX_PLUGINS=10; adoption = layout owner's NEW pinned-bytes decision — `verify_final_ini_layout.py` REJECTS 9-entry bytes today) |

**Two measured facts that restructure every row** (both recorded by their workers, both
load-bearing here):
1. **The asmhelper unit is NOT recompiled per pass** — the link consumes the PREBUILT
   `coherent-full-1/asmhelper-asmhelper.o` (comp-4 run-3 `commands.json` CMD 15; WST-1).
   Any guard-armed row MUST explicitly rebuild the unit + attest receipts, or the link
   silently keeps the stale object.
2. **R1 arming is NOT expressible through the frozen comp6 fixed script** — the
   `TDetourInstance::PLHDetour` member type change is a genuine cross-TU ODR question and
   the frozen script's replaceable set + pinned `-I` order cannot answer it (L11PB-6,
   INTEGRATION-NOTE §3). Arming = an integration-time composition decision: a NEW
   fixed-script edition admitting `unreal-hooks` replacement OR a shadow `-I` slot, with
   its own receipts/ODR walk. Never bypass, never fork silently.

---

## 1. The four decision dimensions (UNCHANGED law, updated sources)

Same D1–D4 as L10M §0, now read from **launch 10's** evidence:

| dim | values (launch-10 evidence source) |
|---|---|
| **D1 terminal point** | Same six families as L10M (T-A / T-REF / T-RD / T-WR / T-GM / T-OTHER), PLUS the armed-lineage strings: `[Seeds] Required-slot seed table armed`, `slot 3 gmalloc bound at 0x930add8`, `[Seeds] REFUTED: <detail>` (L10PB §2 — the armed object exports all three; T-A should be UNREACHABLE with the armed table ⇒ a bare T-A is a **staging anomaly**, reconcile box 1 first, no row). |
| **D2 IGW verdict** | `W_WORKS` / `W_FAILS_R_WORKS` / `BOTH_FAIL` / `FATAL` / `ABSENT` — same contiguous-seq law, `/data/modbridge-exec-writability.jsonl` seq5/seq10/seq12 (EW §5). |
| **D3 fc1 pool verdict** | `VALIDATED` / `REFUSED(<reason>)` / `GRACE` / `ABSENT` — same shapes, FC §7. |
| **D4 GMalloc** | `GM✓` (slot 3 armed AND bound in-session — launch 10's new baseline) / `GM✗` (T-REF or unbound — re-derivation family). |

**Consistency rules: ALL FOUR L10M §0 rules carry over verbatim** (T-GM ⇔ exec chain
mechanically worked ⇒ W_WORKS-class only; T-WR + W_WORKS localizes to PLH plumbing;
ridealong verdicts available under every D1; W_WORKS ≠ safe untransitioned walks —
the per-page-transition rule, now concretely U-GUARD). **New rule 5:** with the armed
table shipped, `T-A` is an anomaly (the Shape-A fold is measured GONE from the shipped
object, L10PB §2) — treat as box-1 reconciliation, not a row. **New rule 6:** if any
successor unit WAS staged into launch 10 beyond the table (not the case per L10PB §1,
"exactly one change"), the successor vocabularies of §6 become live D1 strings.

---

## 2. The matrix — launch-10 outcome → launch-11 composition (one lookup)

Master table. Every row's composition STARTS from the L11PB disarmed vehicle
(`prx-candidate-composition6-l11-a`: initializer staging = `c85c42f7…` + exactly the 2
seam include lines, staging sha `39fe33f3…`, carrying the launch-10 ARMED SeedTable) and
adds ONLY what the row names. Command deltas are §3. Ledger constraints (§4) bind ALL rows.

| row | launch-10 outcome (D1 × D2 × D3 × D4) | launch-11 build composition (candidate + diffs armed + rebuild/receipt steps) | expected launch-11 terminal | command delta |
|---|---|---|---|---|
| **L11-1 CONSOLIDATE** | `T-GM` × `W_WORKS` × `VALIDATED` × `GM✓` — the whole chain mechanically worked; the named hazard is I12 SCO (write permitted, constant identity-unverified) | L11PB disarmed vehicle as staging base (byte-identical — the pass exists to re-pin receipts + carry the guard) + **U-GUARD ACTIVE** (`RC_ASM_WALK_RX_GUARD=1`): apply WST diff to a staging copy of the port ASMHelper.cpp, **explicit asmhelper unit rebuild** (§3.2 law) + receipt `--file` admission of BOTH the .cpp and the new .o | SCO detour verification session: `slot 3 gmalloc bound` repeats, `[WalkRxGuard]` ops appear, I12 either verifies (first engine-ready + mods-started + F7-2 live) or refuses loud — never silent | §3.1 + §3.2 |
| **L11-2 WALK-GUARD** | `T-RD` × `W_WORKS` × `VALIDATED` × `GM✓` — walk died despite probe W_WORKS: exactly L10M §0 rule 4 (probe's far page ≠ initializer's untransitioned game-text reads; igf6 class) | L11-1's composition **+ PLH Prospero layer absorbed** (U-SWAP-4 now exists: PLH's new TU + patched-include under the polyhook unit argv, receipt `--file` admissions) + **U-GUARD ACTIVE**; backend/R2 stays OPTIONAL and only with the §4-g D-1 step (do not pre-judge D-1) | walks pass behind per-page RX transitions (`[WalkRxGuard]` lines); I7 registration behind translated protection; next terminal = T-GM family (then I12 SCO) or loud PLH refusal — new evidence, never a replay | §3.1 + §3.2 + §3.4 (PLH TU) |
| **L11-3 PLH-REG** | `T-WR` × `W_WORKS` × `VALIDATED` × `GM✓` — walks passed (I6 ok), registration failed (I7) = the PLH-plumbing localization (L10M §0 rule 2, EW-5) | L11-2's composition (guard + PLH layer). The guard is honest hardening even though I6 passed: launch-10's walk survival was untransitioned luck, not a transition | I7 registration succeeds → T-GM family next (SCO hazard inherited); else the PLH refusal path is the new loud terminal with recorded errno | §3.1 + §3.2 + §3.4 |
| **L11-4 BACKEND** | `T-RD` (or `T-WR`) × `W_FAILS_R_WORKS` × `VALIDATED` × `GM✓` — RX works, writes fail; the L10M R3/R4 family whose U-SWAP-2 cell is now staged | L11PB vehicle + **R2 capture applied** (`apply_vt_r2.py`, anchors re-verified; header consumed across the unit set ⇒ receipt admission decision per §4-d family) + **backend ARMED** (`UE4SS_DISPATCH_DATA_BACKEND=1`) — WHICH REQUIRES the **frozen-script arming decision** (§4-g: NEW fixed-script edition admitting `unreal-hooks` replacement or shadow `-I` slot, own receipts/ODR walk) + **D-1 step ONLY via its own controller-approved design** (until it lands the armed backend refuses every hook loudly — the honest default) + **U-GUARD ACTIVE** (RX is exactly what the guard adds) | first real Tick swap attempt: query-harness counter is the falsifier (VT §6.5; first target EngineTick), or the loud per-hook `[DispatchData] refused:` family with recorded reasons — both are evidence, neither is a crash | §3.1 + §3.2 + §3.3 (R2 + arm + script edition) |
| **L11-5 DATASIDE+R3** | any D1 × `BOTH_FAIL` × `VALIDATED` × `GM✓` — syscall-74 closed for both directions; the L10M R5 family, now fully staged | **Fresh staging copy of the `c85c42f7…` initializer** → `apply_gate.py` (NE) → **gate ARMED** (`UE4SS_NO_EXEC_MODE=1`) + **DIFF 2 REBASED onto this staging and ARMED** (`UE4SS_R3_MARSHAL_MODE=1`; REBASE IS MANDATORY — §4-d measured base skew `933d08a2…` ≠ `c85c42f7…`; never hand-patch, new revision = new ids) + **DIFF 1 applied** to staging copies of LuaMod.cpp + SettingsManager.hpp (recompile set: luamod + settingsmanager + ue4ssprogram + luamodref + includers — RDA §4) + **U-GUARD fold ABSENT** (zero-syscall backstop; walks compiled out upstream) + **U-CC1 staged** (9th ini entry, §4-i adoption; the calibration instrument) | the data-side engine-ready attempt: `[NoExec] summary: mode=on exec_sites_refused=<N> engine_ready_data_side=1` + `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` + marshal pump on the calibrated carrier; F-1 is the falsifier; menu acceptance honestly degraded (EW §7.3 — no polling replacement landed) | §3.1 + §3.5 (gate + rebase + R3 arm + cc1) |
| **L11-6 REFUTED** | `T-REF(<slot>)` × any × any × `GM✗` — a seeded slot failed in-object validation (launch 10's new early terminal, L10PB §2) | **NO re-seed, NO arming recomposition** (L10M R7 law unchanged). Offline re-derivation with NEW ids. Any ridealong relaunch reverts the initializer staging to the launch-9 table (`2cab0c52…`, both tables preserved in `worker-initializer-integration/staging`); U-CC1 may stage (data-side, no syscalls) | same `T-REF` family on any relaunch until the revised table lands; the session's product is the refutation record | **command-free** (driver refuses no-op rebuild, OPC §0) |
| **L11-7 POOL-REFUSAL** | any × any × `REFUSED(<pool-side reason>)` × any — hard fc1 refusal (`none_ascii_mismatch` w/ recorded bytes16, `pool_window_uncovered`, `current_block_insane`) | **HOLD the arming lineage**: the pool label/base facts are load-bearing for the armed slot's derivation (L10M R8 law). Reconcile slot-3 derivation vs the refusal BEFORE any recomposition; revised constants first, then the applicable row | reconcile-first; no arming progress this cycle | **command-free** until revised |
| **L11-8 FATAL** | any × `FATAL` (missing IGW seq / console loss) × any × any — **overrides everything** (EW §7.4) | **L11-5's composition MINUS every syscall-bearing unit**: gate ARMED (constexpr compile-outs, zero syscalls), DIFF 2 ARMED (heap SetFuncPtr — zero syscalls, R3D §0.1), DIFF 1, U-GUARD fold ABSENT **MANDATORY** (constexpr-dead transition; R9 property measured: fold0 = 0 syscall text), U-CC1 (zero raw syscall instructions, build-gated) survives; NO backend, NO D-1, NO PLH layer syscalls; console recovery + preservation precede any staging | L11-5's expected terminal (data-side + marshal pump), reached without any syscall route | §3.5 (same steps; §3.2/§3.3/§3.4 FORBIDDEN) |
| **L11-9 RECONCILE** | `T-OTHER` (lease/D2/layout/required-objects refusals), bare `T-A` (rule 5 anomaly), any contradiction (incl. `T-GM` + non-W_WORKS D2), or `ABSENT` collapses | **NO row applies.** Reconcile with the controller first; only after reconciliation does the conservative data-side plan apply (L11-5 minus the R3 arm, i.e. gate disarmed-or-armed per the reconciled verdict) plus a dedicated probe launch — never same-session improvisation (L10M collapse rules) | reconcile-first | **command-free** until reconciled |
| **GRACE note** | D3 = `refused_after_grace` (`pool_uninitialized_after_grace`) | NO composition change (timing, never re-rating): re-stage the SAME composition in a NEW launch window; cc1's bounded 30×4 s window is itself the new instrument if adopted | unchanged row family, fresh window | §3.1 receipts re-pin only |

**Row collapse / precedence:** L11-8 (FATAL) overrides all rows. L11-9 (reconcile)
precedes everything else. L11-6/L11-7 are pre-composition holds. Among evidence rows,
D2 selects: `W_WORKS` → L11-1/2/3 by D1; `W_FAILS_R_WORKS` → L11-4; `BOTH_FAIL` → L11-5.

---

## 3. The one-pass command delta (skeleton law: OPC + L10M §3 + L11PB driver)

### 3.1 Common skeleton (any row that recomposes: L11-1..L11-5)

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
L11PB=outputs/2026-10-04-zcode-team/worker-launch11-vtable-prebuild
MYD=outputs/2026-10-04-zcode-team/worker-launch11-integration-driver   # NEW driver home, one per row instance (NEW ids)

# staging base: copy L11PB's initializer vehicle (do NOT mutate L11PB):
#   $L11PB/staging/initializer-vtable-pre/ -> <row-staging>/   (sha 39fe33f3… = c85c42f7… + 2 seam includes + armed SeedTable)

# receipts are stale after ANY source change — regen BOTH, admitting every new/edited
# file explicitly (--file), then verify-only, then the ONE pass with FRESH run+candidate dirs (OPC P8/P9):
python3 outputs/2026-10-04-zcode-team/worker-comp6-driver/regen_extra_receipt.py
python3 outputs/2026-10-04-zcode-team/worker-comp6-driver/regen_replace_receipt.py \
  --program-source <row-staging-dir>          # + --file for every admitted unit/header below
python3 $MYD/<driver-copy>.py --verify-only   # driver copy = L11PB's build_runtime_composition6_l11pre.py (ba0d899e…) + row deltas, diff-verified to contain nothing else (the L10PB/L11PB pattern)
python3 $MYD/<driver-copy>.py
```

All-rows deltas carried over from L10M §3.1: ploader.ini Phase-0 pre-image re-pin BEFORE
any ini edit; fc1 STAGE-PLAN §2 duplicate-line guard re-run; every ridealong addition
keeps the standing ritual (hash-pin, SHT-strip readback law, append-only, count-verified).

### 3.2 The asmhelper prebuilt-object rebuild law (rows with U-GUARD ACTIVE: L11-1..L11-4)

The composition does NOT recompile ASMHelper per pass; the link consumes the PREBUILT
`coherent-full-1/asmhelper-asmhelper.o` (WST-1). Skipping this step = silent stale link.

```bash
# 1) apply the guard to a STAGING COPY of the port unit (never edit the tree):
#    ASMHELPER-WALK-GUARD.diff  (WST, sha 42baeebd…)
# 2) explicit unit rebuild under the REAL unit argv (asmhelper-asmhelper.command.json, 148 args)
#    with ONLY source/output redirected, + the fold define:
#    prospero-clang++ … -DRC_ASM_HELPER_BUILD_STATIC -DOVERRIDE_PLATFORM_HEADER_NAME=Prospero \
#      -DRC_ASM_WALK_RX_GUARD=1 -c <staged ASMHelper.cpp> -o <staging>/asmhelper-asmhelper.o
#    (env: LLVM_CONFIG -> the retained LLVM-21.1.8-macOS-ARM64, per WST compile_gate.sh)
# 3) receipt attestation of BOTH artifacts (else the stale object ships):
#    regen … --file <staged ASMHelper.cpp> --file <staging>/asmhelper-asmhelper.o
```

Fold discipline (WST-5): `=1` admissible ONLY when D2 shows seq5 `rx_result.mprotect_rc==0`
(W_WORKS / W_FAILS_R_WORKS). `BOTH_FAIL`/`FATAL`/`ABSENT` compile with the define ABSENT
(fold0: loud refusal, zero syscalls — measured). Runtime fail-closure on rc!=0 makes a
mis-fold degrade to the loud refusal, never a silent crash.

### 3.3 Backend arm steps (L11-4 ONLY)

1. `python3 $L11PB/build-ready/apply_vt_r2.py <port UnrealVirtualBaseVC.hpp> <staged-copy>` —
   anchors re-verified at integration time (drift aborts nonzero); admit the patched
   header via receipt `--file` (it is consumed across the composed unit set — the same
   receipt-admission family L10M §3.1 R3/R4 already mandated).
2. Flip `UE4SS_DISPATCH_DATA_BACKEND 1` in the staged `DetourInstance.hpp` sibling.
3. **THE FROZEN-SCRIPT ARMING DECISION (§4-g):** author the NEW fixed-script edition
   (admit `unreal-hooks` replacement OR add a shadow `-I` slot ahead of the port include)
   as a NEW driver copy under NEW ids, diff-verified like `l11pre` was; its own receipts +
   ODR walk. If this decision is NOT taken, L11-4 cannot launch — the row is blocked, not
   bypassed.
4. D-1: the refusing `DispatchDataTransition` seam stays refusing until a
   controller-approved, separately-designed D-1 step lands (new file identities; no replay
   of L11PB artifacts). Without it, expect the loud `[DispatchData] refused:` family —
   that is the honest terminal, not a failure of the matrix.

### 3.4 PLH layer absorption (L11-2, L11-3)

Compile `PLH/integration/files/sources/PolyHookProsperoPlatform.cpp` as a NEW TU under the
exact polyhook unit argv (PLH CLAIM-6 receipts show the argv class), place
`patched-include/polyhook2/PolyHookProsperoPlatform.hpp` via the row's include decision,
admit BOTH via receipt `--file`. Additive by construction (CLAIM-1: zero edits to
canonical/port-overlay PLH sources, no ODR conflict). Verdict dependencies stay honest:
the allocator's exec publication rides the same syscall-74 verdict (CLAIM-5) — under
L11-2/L11-3 D2=W_WORKS is the row's evidence, not a guarantee.

### 3.5 Gate + R3 arm steps (L11-5, L11-8)

```bash
NE=outputs/2026-10-04-zcode-team/worker-no-exec-mode-gate
RDA=outputs/2026-10-04-zcode-team/worker-r3-diffs-authoring
# 1) fresh staging copy of the c85c42f7… initializer, then:
python3 $NE/apply_gate.py <staging>/UnrealInitializer.cpp     # 28 anchors, abort on drift
# 2) DIFF 2 REBASE onto THIS staging (measured base skew 933d08a2… != c85c42f7…, §4-d):
#    a NEW RDA-revision authored against the gated bytes (new ids; anchored replayer pattern;
#    the old anchors failing is the EXPECTED drift detector — never hand-patch)
# 3) arm: UE4SS_NO_EXEC_MODE=1 AND UE4SS_R3_MARSHAL_MODE=1 (build defines recorded in the run
#    manifest, or in-file flips — either way the chosen source must be receipt-covered;
#    RDA receipts prove both variants compile clean)
# 4) DIFF 1 onto staging copies of LuaMod.cpp + SettingsManager.hpp (RECIPE-pinned bases
#    a2d07f7c… / a9b4abbe…); recompile set: luamod + settingsmanager + ue4ssprogram + luamodref (+ includers)
# 5) receipts: regen BOTH with --program-source <new staging> + --file for every admitted
#    unit/header; --verify-only; one pass (§3.1)
# 6) manifest records: both mode defines, diff hashes (RDA receipts/diff-hashes.json:
#    DIFF1 a8d1d6b4…, DIFF2 d556c4e7…), the §4-e site-ownership decision, and later the
#    carrier-calibration outcome (F-1 falsifier, R3D §4.2 step 6)
```

Pairing law (ARM-DISARM §2.3): an ARMED initializer must NOT be paired with a Lua world
that assumes ticks (staged ini default is EngineTick); the DIFF 1 ladders degrade
EngineTick→ProcessEvent→FuncPtrMarshal, but tick-cadence instruments are relabeled
`marshal-observation` (MATRIX-ROW-NOTE point 2).

### 3.6 Command-free rows

L11-6, L11-7, L11-9 (pre-decision): no composition pass — the driver refuses a no-op
rebuild by design (OPC §0). Staging/evidence only; cc1's data-side staging is the only
candidate addition and it rides the §4-i adoption decisions.

---

## 4. The integration-decision ledger (every point where two successor artifacts touch)

Honest verdicts with evidence. "ARMED" always means the compile-time flip; no artifact
here has a runtime arm path.

| # | touching pair | verdict | evidence |
|---|---|---|---|
| **a** | vtable backend seam × walk-site guard — can both be ARMED together? | **YES — mechanically compatible, disjoint by construction.** Guard = READ tier: edits ONLY `ASMHelper.cpp` (no header changes → no cross-TU ODR question; single-unit rebuild suffices), adds PROT_READ (0x4→0x5) on text pages, restores, never requests WRITE. Backend = WRITE tier: `DispatchDataDetour.hpp` + seamed siblings (initializer TU via quoted includes), D-1 R→RW on the RELRO data page — a different page class. Resolver coverage overlaps BENIGNLY: the guard's single innermost read covers `DetourSubclasses.hpp:95` (PLH-side resolve) as a side effect, exactly the reads the armed backend's construction still performs. Both fail closed (guard rc!=0 → loud throw, no read; backend refusing seam → hook() false → existing ERROR_ID loud path). | WST-1/WST-2/WST-3/WST-6; L11PB-1 (quoted siblings, initializer TU only); INTEGRATION-NOTE §1/§3; WST compile gate §2.2 |
| **b** | R3 mode (DIFF 2 ARMED) × armed vtable backend — can two be armed together? | **NO — alternative pump routes; co-arming is a configuration error.** DIFF 2 ARMED compiles OUT the two `RegisterEngineTickPreCallback` registrations and the FName-ctor detour and sets `pump_pending` (marshal becomes the pump); the backend's armed path exists to swap EngineTick (INTEGRATION-NOTE §2.4 "first target stays EngineTick"). Arming both removes the registration the backend targets and leaves two owners for one pump. DIFF 2 **DISARMED** (default 0) is byte-inert (constexpr-folded; measured diagnostic parity) and rides safely in ANY composition — including L11-4, where DIFF 1's ladder (EngineTick→ProcessEvent→FuncPtrMarshal) tolerates a working EngineTick and keeps the marshal as dormant fallback. Rule: **R3-mode ARMED belongs to BOTH_FAIL/FATAL rows (L11-5/L11-8) only.** | ARM-DISARM §0/§2; RDA-2 (compiled-out sites); MATRIX-ROW-NOTE R9; INTEGRATION-NOTE §2.4; R3D §0.4 (init fatal at EngineTick registration is the R3 blocker) |
| **c** | no-exec gate × walk-guard folds — can the gate's compile-outs and the guard's fold co-exist? | **YES — complementary layers of the same no-exec policy.** The gate diff touches ONLY the initializer TU (E-1..E-9 compile-outs); the E-1/E-2/E-5 macro walks live in `UnrealVirtualBaseVC.hpp` — a header consumed ACROSS the composed unit set, which an initializer-only diff cannot compile out. The guard's fold-ABSENT refusal at the helper layer is the named backstop for exactly those shared-header reads ("the trap-catcher if a stale ini re-opens one", WST §3.4). R5/R9 posture holds: gate ARMED + guard fold ABSENT = measured zero syscall text in the guard object (fold0 `81dd832b…`, 0 `syscall` instructions). Active-fold rows (L11-1..4) run gate DISARMED + guard `=1`. | NE REPORT §2 (E-sites enumeration); WST REPORT §1.1/§3.4 + WST-5 fold table; WST §2.2 measured syscall counts |
| **d** | DIFF 2 base × current initializer staging lineage — do the diffs apply to the same file the gate/seam build on? | **NO — MEASURED BASE SKEW; REBASE REQUIRED before any co-staging.** DIFF 2 is authored on `worker-query-s2-arm/staging/UnrealInitializer.cpp` = `933d08a2f3e4dea1…` (RDA base pin; R3D's SINI cites agree, e.g. `:1383-1397`). The merged-staging lineage every other initializer artifact builds on is `c85c42f7aaab0630…` (NE baseline == initializer-integration staging, byte-identical), and the L11PB vehicle is `39fe33f3…` (= `c85c42f7…` + 2 include lines). These are DIFFERENT files; DIFF 2's hunks do not apply to the gate/seam base. Verdict: a NEW DIFF-2 revision re-anchored onto the row's staging is a MANDATORY authoring step in L11-5/L11-8 (§3.5 step 2); anchored-replayer drift aborts are the detector; hand-patching is forbidden (NE §4 handoff note). | sha256 readback recorded in L11M-2d (this session); RDA base pins; NE baseline pin; L11PB REPORT §3; R3D path abbreviations |
| **e** | no-exec gate × DIFF 2 at the shared sites (EngineTick registrations, FName-ctor verifier) — who owns the refusal? | **COMPATIBLE IN INTENT; SITE OWNERSHIP MUST BE DECIDED AT MERGE TIME.** Both gate the same logical sites (NE: I7 `:1787-1802` + FName verifier `:1424-1434` on the `c85c42f7…` base; DIFF 2: `SINI:954-958` + `:1383-1397` on its own base). If the gate folds a site to a plain refusal where DIFF 2 expects to place its DEFFERAL semantics (`pump_pending` + the literal deferral line), the pump route dies silently — a dead `[R3]` state inside an otherwise "successful" `[NoExec]` summary. Verdict: in the rebased unit, DIFF 2 owns those two sites (deferral + bridges), the gate owns every other E-site; record the ownership in the run manifest (§3.5 step 6). The disarm pairing law (ARM-DISARM §2.3) is the runtime-facing half of the same decision. | NE REPORT §2; RDA-2(b)(c); MATRIX-ROW-NOTE points 1/3; ARM-DISARM §2 |
| **f** | walk guard × the PREBUILT `.o` stale-link fact (every guard-armed row) | **MANDATORY STEP, NOT OPTIONAL.** The comp pass never recompiles ASMHelper; the link consumes `coherent-full-1/asmhelper-asmhelper.o` (comp-4 run-3 CMD 15). A patched source without an explicit unit rebuild + `--file` receipt attestation ships the STALE object — a silent half-state, "the exact class the no-exec gate forbids" (WST §1.2). §3.2 is therefore part of L11-1..L11-4's definition of done. | WST-1; WST REPORT §1.2 |
| **g** | backend arm × the frozen comp6 fixed script (the arming decision) | **BLOCKED THROUGH THE FROZEN SCRIPT — INTEGRATION-TIME DECISION REQUIRED.** Arming changes `TDetourInstance::PLHDetour`'s type; both src consumers (`unreal-unrealinitializer` staged/replaceable AND `unreal-hooks` port/RETAINED) instantiate shared vague-linkage members ⇒ one-TU arming is a genuine ODR violation; the frozen script's replaceable set (6 affected + retained `{program}`) and pinned `-I` order cannot express two-TU arming. Decision: NEW fixed-script edition (admit `unreal-hooks` replacement OR shadow `-I` slot) + own receipts/ODR walk, NEW ids. Until taken, L11-4 cannot launch; the disarmed vehicle remains exact and inert (proven byte-identical). | L11PB-6; INTEGRATION-NOTE §3; L10M §3.1 R3/R4 delta ("decide at integration time, never bypass") |
| **h** | PLH Prospero layer × backend seam × guard | **COMPOSABLE IN ALL ROWS; REQUIRED exactly where PLH stays the selected backend.** Layer is additive (one new TU + patched include; CLAIM-1: zero edits to canonical sources, no ODR conflict) — inert-but-harmless under an ARMED backend (the seam selects DispatchDataDetour INSTEAD of `PLH::x64Detour` at all FIVE sites). Required in L11-2/L11-3 where the PLH detour path must function (protection translate/restore, old-prot via the proven mapscan, near-allocator). Guard overlap is PARTIAL and honestly scoped: the guard covers the resolver read at `DetourSubclasses.hpp:95`, but PLH's own 100-byte disasm + RWX write phases are the LAYER's deliverable, not the guard's (WST-6). | PLH CLAIM-1/5/6; L11PB-1 (five sites); WST-6 |
| **i** | cc1 ini entry × the final-ini law (and × every row) | **STAGEABLE EVERYWHERE, ADOPTION IS A SEPARATE OWNER DECISION.** cc1 is data-side-only, zero raw syscall instructions (build-gated), own single-writer `/data/modbridge-carrier-facts.jsonl` — it rides L11-5/L11-8 (calibration instrument) and may ride any other row. The 9-entry ini (`fd9c7393…`, 8 of MAX_PLUGINS=10) REQUIRES the layout owner's NEW pinned-bytes decision (`verify_final_ini_layout.py` rejects 9-entry bytes today — correct behavior for the law it pins) + the batch tool owner under a NEW id (constants brief only; no tool edits). | CC1 REPORT §3/§5; CC1 CLAIM bounds; STAGE-PLAN §7 |
| **j** | armed SeedTable lineage × rows that must disarm (L11-6/L11-7) | **REVERT PATH EXISTS AND IS PINNED.** The L11PB vehicle carries the launch-10 ARMED table; the launch-9 table (`2cab0c52…`) is preserved untouched in `worker-initializer-integration/staging` (both seed tables; L10PB §5). An L11-6/L11-7 relaunch stages the launch-9 table copy explicitly; never edit the armed header in place. | L10PB §1/§5; L11PB REPORT §3 |

---

## 5. Pre-conditions checklist — what the post-launch-10 controller reads FIRST

Ordered; every row lookup requires boxes 1–7; box 8 verifies the successor artifacts
themselves; boxes 9–10 are the citation base and the standing ritual.

1. **Which candidate actually shipped launch 10** — `L10PB:prx-candidate-composition6-l10-a`
   receipts + `staging/initializer-launch10/` (armed table `59effe6e…`). Decides whether a
   bare `T-A` line is a staging anomaly (rule 5) rather than a row. If anything beyond the
   table was staged, inventory it before box 2.
2. **`/data/plugins/UE4SS.log` tail** (host copy under the controller's console dir) → D1.
   Match against the EXPANDED list (§6): `[Seeds] Required-slot seed table armed` /
   `slot 3 gmalloc bound at 0x930add8` (armed lineage, expected) / `[Seeds] REFUTED:
   <detail>` (→ L11-6) / last-line-near LoadMap with NO failed line (→ T-RD) /
   `Native game-thread initializer callback registration failed` (→ T-WR) /
   `GMalloc is uninitialized.` (→ T-GM) / any lease/D2/layout refusal (→ L11-9). If any
   successor unit WAS staged, also match the §6 successor vocabularies.
3. **`runtime-status.txt`** — freshness-prefixed lines, last line `activated`, any
   `failed(1,<msg>)` ack; mid-init end corroborates T-RD (L10M box 3 unchanged).
4. **`/data/modbridge-exec-writability.jsonl`** → D2. Contiguous seq law: seq5
   `rx_result.mprotect_rc`, seq10 `rwx_result.mprotect_rc`, seq12
   `summary.decision_input`. A MISSING seq = `FATAL` → L11-8 overrides everything;
   preserve first, never replay.
5. **`/data/modbridge-fc1pool-facts.jsonl` + the one `FC1POOL:` log line** → D3.
   `VALIDATED` (11/11 + crosscheck) / `REFUSED(<reason>)` (→ L11-7 if pool-side) /
   `refused_after_grace` (→ GRACE note) / `ABSENT` (collapse rule).
6. **observer + qdrv1 records** (`/data/modbridge-ingame-facts.jsonl`) — survival/crash
   localization and ridealong non-interference (L10M box 6 unchanged).
7. **Slot-3 evidence** → D4: the bound line + absence of REFUTED ⇒ `GM✓` (launch 10's new
   baseline); REFUTED or unbound ⇒ `GM✗` (L11-6). Apply the consistency rules (§1) BEFORE
   the row lookup — `T-GM` + non-W_WORKS D2 = contradiction → L11-9.
8. **Successor-artifact receipt verification (hash-check before staging anything):**
   - `L11PB`: gate receipt `ee753d48…` GO 15/15; candidate `l11-a`; disarmed initializer
     object `983b18e2…` == launch-10's; composed runtime `d6214abd…`; wrapped `d8cb4a76…`.
   - `WST`: diff `42baeebd…`; fold objects `81dd832b…` (0 syscalls) / `6604ce2d…` (1).
   - `RDA`: DIFF1 `a8d1d6b4…` / DIFF2 `d556c4e7…`; compile-gate receipts; base pins
     (`a2d07f7c…`, `a9b4abbe…`, `933d08a2…` — the base-skew fact, §4-d).
   - `NE`: diff `339ef3c4…`; baseline `c85c42f7…`; gated candidate `40d6cd49…`.
   - `PLH`: compile-gate receipts (CLAIM-6); REPORT still pending — treat the layer's
     maturity as compile-gated-proposal until its REPORT lands.
   - `CC1`: fself `d83410aa…`; inner `e72366c4…`; 9-entry ini bytes `fd9c7393…`.
   Any mismatch = reconcile that artifact first; never restage from a divergent copy.
9. **Design citations already read** (no action; the rows' ancestry): `VT` (R1/R2/D-1),
   `EW` §7/§8, `STF`, `FC STAGE-PLAN`, `OPC`, plus the successor set: R3D (verdict §0.5),
   INTEGRATION-NOTE, ARM-DISARM, MATRIX-ROW-NOTE, WST §3.4 verdict table, NE design §2-§4.
10. **Standing ritual unchanged:** only the controller stages and launches; every
    ridealong addition (cc1 included) keeps the standing go/no-go coordination for
    syscall-bearing work; new-revision-new-ids everywhere; no replay of failed/reserved
    work; no verdict re-rating on unchanged evidence.

---

## 6. New evidence vocabulary for box 2 (the D1 match list grows)

Recorded by the successor workers for exactly this purpose; a launch 10 that staged any
of these units would emit them, and launch 11 rows WILL:

| family | strings | source |
|---|---|---|
| walk guard | `[WalkRxGuard] REFUSED …` / `[WalkRxGuard] ANOMALY …` | WST §4 (named for this matrix) |
| no-exec gate | `[NoExec] REFUSED: …` / `[NoExec] summary: mode=on exec_sites_refused=<N> engine_ready_data_side=1` | NE REPORT §2/§3 (flagged for this revision) |
| R3 marshal | `[R3] refused: FName constructor verification detour …` / `[R3] refused: EngineTick pre-callback registrations …` / `[R3] EngineTick detour unavailable; deferring game-thread pump to R3 marshal` / `[R3] FuncPtr-marshal armed on carrier <full name> (callback id <n>)` / `[R3] marshal is the game-thread pump …`; a MISSING deferral line in an ARMED run is itself a box-2 anomaly | RDA-5 + MATRIX-ROW-NOTE point 1 |
| backend | `[DispatchData] refused: …` (per hook; the expected loud path) | INTEGRATION-NOTE §2.4 |
| carrier probe | exactly one `CC1:` UE4SS.log line per launch + `/data/modbridge-carrier-facts.jsonl` verdicts `CENSUS:VALIDATED` / `CENSUS:REFUSED` + reason_code | CC1 REPORT §1 |
| armed lineage (launch 10 baseline) | `[Seeds] Required-slot seed table armed` / `slot 3 gmalloc bound at 0x930add8` / `[Seeds] REFUTED: <detail>` | L10PB §2 |

Instrument honesty under an armed marshal: query-harness "two consecutive accepted reads"
counts CARRIER INVOCATIONS, not ticks — prior "tick"-phrased rungs are NOT comparable
(MATRIX-ROW-NOTE point 2).

---

## 7. Honesty bounds

- This is a PREDICTION ROUTER, not an outcome promise: it routes staged designs and named
  diffs per launch-10 evidence classes; it claims no launch-10 result exists yet and no
  launch-11 outcome.
- NOTHING is drop-in: every consuming row names its authoring step (guard rebuild + R2
  application + DIFF-2 rebase + fixed-script edition + cc1 adoption). "Armed" is a
  receipt-covered compile-time act, never a flag flip at composition level (§4-g).
- No claim any swap, thunk, mprotect-on-data, marshal, gate refusal, guard transition, or
  census has executed in-game; every cited worker's non-claims carry over unchanged.
- D-1 remains the gating unverified fact of the backend row (L11-4); F-1 remains the
  falsifier of the marshal rows (L11-5/L11-8); the SCO constant (I12) remains the named
  hazard of the T-GM family (L11-1..L11-3).
- Rows L11-2/L11-3 depend on a worker still IN FLIGHT (PLH: CLAIM + compile-gate receipts
  landed, REPORT pending) — re-verify box 8 at integration time.
- No quality score, no completion claim, no kernel-behavior claim is made or inherited
  here; launch 10's own records remain the only evidence source once they exist.
