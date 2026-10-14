# LAUNCH10-MATRIX — the post-launch-9 integration decision matrix, pre-built

Worker: worker-launch10-matrix-prep · 2026-10-04. Read-only synthesis; no console contact,
no nested subagents, nothing executed, no design modified. Purpose: make the post-launch-9
integration cycle ONE LOOKUP — read launch 9's evidence (§4 checklist), find the row (§2),
apply the deltas (§3 catalog), run the one-pass command delta (per-row).

Path abbreviations (reused from cited workers):
- `TEAM=` `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/`
- `CON=` `.../outputs/2026-10-04-zcode-controller/`
- `L9=` `TEAM:worker-launch9-prediction/LAUNCH9-PREDICTION.md` (terminal shapes, hop table I1–I13)
- `EW=` `TEAM:worker-exec-writability/EXEC-WRITABILITY-PROBE-DESIGN.md` (IGW-1 verdicts, records)
- `EWV=` `TEAM:worker-exec-writability/EXEC-WRITABILITY-EVIDENCE.md`
- `VT=` `TEAM:worker-vtable-swap-design/VTABLE-SWAP-DESIGN.md` (R1/R2/R3/R4, D-1)
- `FC=` `TEAM:worker-fc1-validator-probe/` (STAGE-PLAN.md record shapes)
- `STF=` `TEAM:worker-initializer-integration/SEED-TABLE-FORMAT.md` (arming semantics, re-seed mechanics)
- `OPC=` `TEAM:worker-comp6-driver/ONE-PASS-CHECKLIST.md` (the one-pass command skeleton)

---

## 0. The four launch-9 decision dimensions

| dim | values (evidence source) |
|---|---|
| **D1 terminal point** (L9 §2/§4) | `T-A` Shape-A loud refusal `[Seeds] Required-slot seed table incomplete … missing [gmalloc]` (MERGED `:876`; nothing binds, zero exec touch — the compiled default, STF §4/§7) · `T-REF` `[Seeds] REFUTED: …` (L9 §4-#5, terminal per launch) · `T-RD` igf6-class crash at first vtable walk (L9 I6/§4-#3) · `T-WR` loud `Native game-thread initializer callback registration failed` (L9 I7/§4-#4) · `T-GM` loud `UnrealInitializer::PostInitialize: GMalloc is uninitialized.` (L9 I11/§4-#1; reachable only if exec reads AND the tick-detour write both mechanically succeeded — i.e. W-works-class session) · `T-OTHER` lease/D2/layout/required-objects refusals (L9 §4-#6/7/8; these gate launch 10 entirely — reconcile first, no row applies) |
| **D2 IGW verdict** (EW §5/§7, records in `/data/modbridge-exec-writability.jsonl`) | `W_WORKS` (seq10 rc==0 + seq11 readback/restore ok) · `W_FAILS_R_WORKS` (seq5 rc==0, seq10 rc!=0, errno recorded) · `BOTH_FAIL` (seq5 rc!=0 → refused=rx_denied, or summary) · `FATAL` (a seq missing = death at the preceding action; syscall route permanently closed, EW §7.4) · `ABSENT` (not staged / not reconciled) |
| **D3 fc1 pool verdict** (FC §7, records in `/data/modbridge-fc1pool-facts.jsonl` + one `FC1POOL:` line in `/data/plugins/UE4SS.log`) | `VALIDATED` (`fc1pool_result verdict:"validated"`, checks 11/11, `base_matches_observed_bias:1`, `byteprop_id_for_find_crosscheck:<u32>`) · `REFUSED(<reason_code>)` (hard: `pool_window_uncovered`, `none_ascii_mismatch`, … — terminal evidence, FC §7) · `GRACE` (`refused_after_grace`: `pool_uninitialized_after_grace` = launch-window timing, NEW window remedy, never re-rating) · `ABSENT` |
| **D4 GMalloc hunt** (TEAM:worker-gmalloc-derivation, IN FLIGHT at matrix time) | `GM✓` derived candidate + shipped in-game DATA-side slot-3 validator (its own constraint: no candidate is seedable without one) · `GM✗` no candidate (slot 3 stays unseeded → arming impossible, STF §4) |

**Consistency rules (apply before lookup; contradictions mean reconcile-first, do not proceed):**
1. `T-GM` requires the whole exec chain (walk reads + tick detour write) to have mechanically
   worked in that session ⇒ `T-GM` pairs only with `W_WORKS`-class sessions. `T-GM` +
   `W_FAILS_R_WORKS`/`BOTH_FAIL` = contradiction (L9 hop order I6 < I7 < I11).
2. `T-WR` + `W_WORKS` is NOT a contradiction — it localizes the I7 failure to the missing
   PolyHook Prospero plumbing, not the kernel (EW-5: MemProtector/MemAccessor/Misc have no
   Prospero branches; PLH is doubly unproven, EWV §1).
3. IGW/fc1 verdicts come from INDEPENDENT ridealong modules — they are available under
   EVERY D1 value including `T-A` (L9 §0 ridealongs row).
4. **Load-bearing nuance:** `W_WORKS` means the PROBE's mprotect-then-read/write ladder
   succeeded on one far page. It does NOT make the initializer's own un-transitioned
   vtable-walk read safe — igf6's death was a plain read of a prot=4 page with no
   transition (EWV §1, L9 I6). Any launch-10 row that lets walks run must add a per-page
   transition (RX or RWX) + fresh mapscan before the first walk — this is the PLH-layer
   integration delta, present in R1/R2 regardless of verdict.

---

## 1. What launch 10 can change (unit-swap catalog; details in §3)

| unit swap | mechanical content | status at matrix time |
|---|---|---|
| **U-SWAP-1 initializer re-seed** | one more `present:true` slot (gmalloc) in `SEED-TABLE.json` (canonical or derivation dialect, derivation triples per STF §3) → `gen_seed_table.py <SEED-TABLE.json> <SeedTable.h>` (argv verified: `sys.argv` 2-arg usage, image sha pinned `3b6cfad4…`) → recomposition; **merged initializer unit UNCHANGED** (STF §8). Compile-time arming flips: `ScanGame` folds from the Shape-A refusal to the full armed path (STF §7) — which re-opens FLAG-2 (walk read) and FLAG-1 (detour write) as launch-10 verdicts | pure table update; generator + consumer landed and host-tested (STF §5 ALL PASS) |
| **U-SWAP-2 vtable backend R1+R2** | `DispatchDataDetour` exposing the exact PLH surface, PROSPERO typedef at the single construction site (`DetourInstance.hpp:164-166`), slot capture at the existing walks (VT §5 R1/R2); every swap gated on identity pre-check + D-1 `mprotect(R->RW)` on the prot=1 RELRO data page (VT §1.2/§3) | DESIGN ONLY — no code exists; a build worker must author it post-9; adds new composed units → receipts/ODR walk must cover them (OPC §5 failure rows) |
| **U-SWAP-3 R3 zero-syscall tier** | route Lua `RegisterHook` to `UFunction::SetFuncPtr` (in-tree, `UFunction.cpp:78-90`, heap FuncPtr write, proven prot=3 write class igf4); composition change confined to LuaMod + registration wrappers (VT §5 R3); CANNOT carry EngineTick (VT §0.5) | worker-r3-zerosyscall-tier IN FLIGHT — its R3-TIER-DESIGN.md will state exact unit diffs + argv + the acceptance-ladder verdict (its CLAIM R3-5/R3-4). Reconcile before any BOTH_FAIL/W_FAILS row executes |
| **U-SWAP-4 PLH Prospero layer** | `MemProtector.cpp` prot translate+restore, `MemAccessor.cpp` old-prot via the proven sysctl mapscan, `Misc.cpp` executable-near allocator (EW §7.1 item 1; EW-5) + per-page transition before walks (§0 rule 4) | DESIGN ONLY — implementer worker required |
| **U-SWAP-5 no-exec-mode gate** (BOTH_FAIL/FATAL rows) | initializer diff that compiles OUT the walk arms and the EngineTick/SCO detour registrations (today unconditional per L9 I6/I7/I12) — the data-side-only configuration | **GAP: no worker owns this yet.** The merged unit has partial gates (A-PI honoring `:1876`, seed-bound verifier skip `:1385-1397`, tick vtable fallback comment) but the Tick registration (`:1749-1764`) and SCO callback (`:480`) arm unconditionally. Must be commissioned if the verdict is BOTH_FAIL/FATAL |
| **U-SWAP-6 GMalloc slot-3 validator ridealong** | fc1pool-pattern standalone data-side probe validating the derived slot-3 constant in the SAME session that first arms it (gmalloc-derivation's own seeding constraint) | pattern proven (FC build), module NOT built — candidate-dependent, post-gmalloc-derivation |

---

## 2. The matrix

Master table — one row per realistic launch-9 outcome combination. Per-row deltas follow.

| row | launch-9 outcome (D1 × D2 × D3 × D4) | launch-10 unit swaps | settings delta | ini delta | worker diffs that apply | expected launch-10 terminal | one-pass command delta |
|---|---|---|---|---|---|---|---|
| **R1** | `T-GM` × `W_WORKS` × `VALIDATED` × `GM✓` | U-SWAP-1 + U-SWAP-4 (+ U-SWAP-6 rides) | none (v3 stands) | IGW line `→ 0`; fc1 `→ 0` or keep; append slot-3 validator `= 2` LAST | gmalloc-derivation (table+validator); PLH-layer implementer (NEW); comp6-driver receipts | walks pass behind per-page transitions; GMalloc binds; **I12 SCO detour = the named hazard** (write permitted but constant identity-unverified, L9 I12) → first engine-ready + mods-started + F7-2 live via tick hook; else loud/crash at I12 | skeleton §3.1 + PLH-layer units in receipts + fresh run/candidate dirs |
| **R2** | `T-A` × `W_WORKS` × `VALIDATED` × `GM✓` | U-SWAP-1 + U-SWAP-4 (+ U-SWAP-6) | none | same as R1 | same as R1 | same family as R1 PLUS: seed slots 0/2/4 get their FIRST behavioral test in launch 10 (Shape A meant launch 9 never bound them) — `[Seeds] REFUTED` is the added early terminal (L9 §4-#5) | same as R1 |
| **R3** | `T-RD` × `W_FAILS_R_WORKS` × `VALIDATED` × `GM✓` | U-SWAP-1 + U-SWAP-2 + per-page RX for walks; SCO detour dropped/replaced (direct-call target, VT §6.1); optionally U-SWAP-3 | none required (backend is a typedef; v3 keys stand) | same as R1 | vtable-swap implementer (NEW, from VT §5); gmalloc-derivation; fc1 (crosscheck constant already in hand) | **D-1 is the gating unverified fact** (VT §6.6): refused ⇒ loud per-hook refusal at tick arm (same stop family as launch-9 I7, now with recorded errno); accepted ⇒ first real Tick swap, query-harness counter is the falsifier (VT §6.5), then PostInitialize passes → engine-ready possible; ProcessEvent parity NOT claimed (R4 family unbudgeted, VT §6.3) | skeleton §3.1 + NEW backend units in the overlay/receipt walk (OPC §5 "Unpinned retained-replacement dependency" row applies) |
| **R4** | `T-A` × `W_FAILS_R_WORKS` × `VALIDATED` × `GM✓` | same as R3 | same | same | same as R3 | same as R3 PLUS first-behavioral-test caveat from R2 (`[Seeds] REFUTED` early terminal) | same as R3 |
| **R5** | `T-A` or `T-RD` × `BOTH_FAIL` × `VALIDATED` × `GM✓` | U-SWAP-1 + **U-SWAP-5 (no-exec gate)** + U-SWAP-3 (Lua hooks if R3-tier verdict allows) + polling replacement for `ExecuteInGameThread` (EW §7.3) | v3 stands (PI/PLSF=0 already); no new key REQUIRED if the gate is compile-time | same as R1 | gmalloc-derivation; r3-zerosyscall-tier (in-flight verdict decides what Lua traffic survives); NEW no-exec-gate author (U-SWAP-5 gap) | **the data-side engine-ready attempt**: seeds + GMalloc bind data-side, walks/detours compiled out, `PostInitialize` passes → I13 acks → engine-ready/mods-started possible for the first time; F7-2 dead (no closure drain) unless the polling replacement lands; menu acceptance honestly degraded (EW §7.3) | skeleton §3.1 + gate diff inside the initializer staging copy (merged-unit change this time — replace-receipt `--program-source` points at the NEW staging) |
| **R6** | `T-A` × any/`ABSENT` × any × `GM✗` | **NONE — no arming recomposition.** Launch 10 is a ridealong/research session: slot-3 hunt continues offline; optionally D-1 probe design (ONLY if D2=`W_FAILS_R_WORKS` opened it AND controller+user approve a NEW falsifiable design, VT §3) | none | no composition line change; optional appended probe line `= 2` per its own stage-plan | gmalloc-derivation (continued); vtable-swap (D-1 probe design only) | same `T-A` family on any composition relaunch — GMalloc gate is structural until slot 3 lands (L9 §4-#1); the session's product is evidence, not progress past I11 | **no comp pass** (composition unchanged ⇒ driver would refuse no-op rebuild, OPC P-gates); ridealong staging only |
| **R7** | `T-REF(<slot>)` × any × (localizes) × any | **NO re-seed of the refuted slot.** Refusal is terminal evidence — never replay identical, never re-rate unchanged evidence. Offline re-derivation with NEW ids; arming stays OFF (all-4 rule, STF §4) | none | no arming composition; optional validator ridealong for the revised candidate | seed-table-derivation (revision, new ids); fc1 reason codes as the localizer (`pool`-side ⇒ fname_remedy/base wrong; initializer-side probe divergence ⇒ ctor label) | revised-constants launch inherits the applicable R1–R5 row once the table is re-derived; until then `T-A` persists | none until the revised table lands, then the applicable row's skeleton |
| **R8** | any × any × `REFUSED(<pool-side reason>)` × `GM✓` | **HOLD U-SWAP-1** — `fname_remedy` pool constants are load-bearing for arming (STF §3 mandatory field); a hard fc1 refusal (`none_ascii_mismatch` with recorded `bytes16` hex, `pool_window_uncovered`, `current_block_insane`, FC §7) refutes the pool label/base BEFORE the initializer ever consumes it | none | validator-revision ridealong only | fc1 revision worker (new ids, new falsifiable design); seed-table-derivation reconciles | same as R7: constants revised first, then the applicable row | none until revised |
| **R9** | any × `FATAL` (missing IGW seq / console loss) × any × any | **Overrides everything:** syscall-74 route permanently closed (EW §7.4; zcode_igf5_diagnosis action rows) — no replay, no variant, **no D-1, no PLH layer, no IGW-2 unless a distinct falsifiable design** (EW §8). Composition falls back to R5's data-side configuration; U-SWAP-3 subject to the R3 worker's verdict (heap tier needs NO syscalls, so it survives FATAL) | v3 stands | probe lines `= 0`/removed; composition as R5 | U-SWAP-5 author; r3-zerosyscall-tier; gmalloc-derivation if GM✓ | R5's expected terminal (data-side engine-ready attempt) | R5's skeleton minus every syscall-bearing unit; console recovery + preservation precedes any staging (console-safety procedure) |

**Row collapse rules:** if launch 9 launched WITHOUT IGW/fc1 staged (`ABSENT`), the row
collapses to the conservative data-side plan (R5's composition, R6's restraint) plus a
dedicated probe launch — never a same-session improvisation. `T-OTHER` (lease/D2/layout
refusals) precedes everything: reconcile with the controller first; no row applies.

---

## 3. The one-pass command delta (skeleton, from OPC)

### 3.1 Common skeleton (rows R1–R5, R9 — any row that recomposes)

```bash
cd /WORKSPACE/.codex/worktrees/translation-core/what
MY=outputs/2026-10-04-zcode-team/worker-comp6-driver

# (rows with U-SWAP-1) regenerate table -> header; staging-local quoted include:
python3 outputs/2026-10-04-zcode-team/worker-initializer-integration/gen_seed_table.py \
  <updated-SEED-TABLE.json> <initializer-staging-dir>/SeedTable.h

# receipts are stale after ANY source change — re-run BOTH (OPC §1):
python3 $MY/regen_extra_receipt.py
python3 $MY/regen_replace_receipt.py \
  --program-source <staged UE4SSProgram.cpp>        # add --file <port path> if the port tree was also edited (OPC F3)

# preflight, then the ONE pass — FRESH run + candidate dirs (OPC P8/P9):
python3 $MY/build_runtime_composition6.py --verify-only \
  --program-source <staged UE4SSProgram.cpp> --bridge-sha256 <r12-sha-from-REPORT>
python3 $MY/build_runtime_composition6.py \
  --program-source <staged UE4SSProgram.cpp> --bridge-sha256 <r12-sha-from-REPORT>
```

Row-specific deltas:
- **R3/R4 (new backend units):** the driver REFUSES unpinned new dependencies (OPC §5) —
  the R1/R2 implementer's files must enter the receipts (regen with `--file`) or a
  comp-7 driver clone; decide at integration time, never bypass.
- **R5/R9 (initializer staging copy changes):** `--program-source` points at the NEW
  staging dir; U-SWAP-5's gate is a merged-unit change, so the replace-receipt walk covers
  it the same way the comp6 initializer fold did.
- **All rows:** the ploader.ini pre-image sha changed with the launch-9 ridealong appends —
  the master plan's Phase-0 pre-image MUST be re-pinned explicitly before launch-10 ini
  edits (EW §6.2 coordination note); fc1 STAGE-PLAN §2 duplicate-line guard re-runs.
- **Ridealong staging** (slot-3 validator, D-1 probe if approved): fc1 STAGE-PLAN §0–§4
  pattern — hash-pin, SHT-strip readback law, append-only ini, count-verified.

### 3.2 Command-free rows

R6, R7, R8 (pre-revision): no composition pass — the driver's edited-landed gates refuse a
no-op rebuild by design (OPC §0). The session is staging/evidence only.

---

## 4. Pre-conditions checklist — what the post-9 controller reads FIRST

Ordered; every row lookup requires ALL of boxes 1–6; boxes 7–9 reconcile the in-flight workers.

1. **Seed-arming shape actually shipped** — which `SeedTable.h`/initializer went out the door:
   `CON:comp6-integration/change-manifest.json` fold_ins status + the merge owner's final
   staging (`TEAM:worker-initializer-integration/staging/`). Decides whether a `missing [gmalloc]`
   line in the log is `T-A` (expected) or a Shape-B anomaly.
2. **`/data/plugins/UE4SS.log` tail** (host copy under `CON:console/`): the exact terminal
   string → D1. Match against L9 §4: `[Seeds] Required-slot seed table incomplete` (`T-A`) /
   `[Seeds] REFUTED: <detail>` (`T-REF`) / last-line-near "Finding GameEngine::LoadMap" with
   NO failed line (`T-RD`) / `Native game-thread initializer callback registration failed`
   (`T-WR`) / `UnrealInitializer::PostInitialize: GMalloc is uninitialized.` (`T-GM`).
3. **`runtime-status.txt`** — entry-r4 freshness-prefixed lines (`[t=<ms>ms pid=<pid>]`),
   last line `activated` + any `failed(1,<msg>)` ack; NO failed line + mid-init log end
   corroborates the crash-shaped `T-RD`.
4. **`/data/modbridge-exec-writability.jsonl`** → D2. Contiguous seq 1..12 law: read
   `seq5 rx_result.mprotect_rc`, `seq10 rwx_result.mprotect_rc`, `seq12
   summary.decision_input ∈ {W_WORKS, W_FAILS_R_WORKS, BOTH_FAIL}` (EW §5). A MISSING seq =
   `FATAL` — stop, preserve, reconcile; the row is R9 regardless of everything else.
5. **`/data/modbridge-fc1pool-facts.jsonl` + the one `FC1POOL:` UE4SS.log line** → D3.
   Match against FC §7 shapes: `verdict:"validated"` + `checks_passed:11` +
   `byteprop_id_for_find_crosscheck`; or `verdict:"refused"` + `reason_code`; or
   `refused_after_grace` (timing — a NEW launch window is the remedy, never a re-rating).
6. **observer + qdrv1 records** (`/data/modbridge-ingame-facts.jsonl`, ridealongs REPORT §1)
   — session survival/crash localization; confirms which module died and that ridealong
   non-interference held.
7. **`TEAM:worker-gmalloc-derivation/` post-flight artifacts** (`GMALLOC-CANDIDATES.json`,
   REPORT.md — IN FLIGHT at matrix time) → D4, including whether the mandated in-game
   data-side slot-3 validator exists (its seeding constraint) and at what maturity.
8. **`TEAM:worker-r3-zerosyscall-tier/R3-TIER-DESIGN.md`** (IN FLIGHT) — the exact R3 unit
   diffs + argv and the honest acceptance-ladder verdict (its CLAIM R3-5) that rows R3/R5/R9
   depend on for Lua-hook survival.
9. **Design-side inputs, already read** (no action, citation base): `VT` (R1/R2/D-1, §1.2
   premise correction + §6 limits), `EW §7` (branch engineering), `STF §4/§7/§8` (arming +
   re-seed mechanics), `FC STAGE-PLAN` (ridealong pattern), `OPC` (command skeleton + gates).

**Then:** apply §0 consistency rules → look up the row in §2 → execute §3. Only the
controller stages and launches; every ridealong addition still needs the standing ritual
and user coordination (EW §8 go/no-go posture unchanged for any syscall-bearing follow-up).

---

## 5. Honesty bounds

- Every row is a PREDICTION ROUTER, not an outcome promise: it routes pre-staged designs
  and named diffs per evidence class; it claims no launch-10 result.
- U-SWAP-2/4/5 and U-SWAP-6 have NO implemented code at matrix time; rows that need them
  therefore include a mandatory author/integrate step before §3.1 can run. Nothing is
  drop-in.
- worker-r3-zerosyscall-tier and worker-gmalloc-derivation were CLAIM-only when this matrix
  was written; rows citing them are marked pending reconciliation (checklist boxes 7–8).
- No claim about kernel behavior, seed validity, or the SCO constant is made or inherited
  here; launch 9's own records remain the only evidence source once they exist.

## SESSION-EVE UPDATE (controller): the "design-only, no code" gaps are now BUILT — polyhook-prospero-layer (all 14 symbol families, compile green, host 71/71), launch11-vtable-prebuild (prepared-unarmed, gate 15/15), r3-diffs (both compiled incl armed), walk-site-transitions (single-point guard, verdict-gated). The R1-R9 rows' integration steps reference REAL artifacts; see worker-launch11-integration-matrix for the consolidated router.
