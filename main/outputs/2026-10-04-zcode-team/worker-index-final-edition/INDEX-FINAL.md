# INDEX-FINAL — session-eve final edition (2026-10-05, compiled ~01:45 local)

Authoritative workspace: `/WORKSPACE/.codex/worktrees/translation-core/what`
Ledger: `.superpowers/native-observations/transport-experiments-progress.json` — **268 `zcode_*` entries** of 599 keys at this read (newest `zcode_worker_ue4ss_docs_fidelity`); INDEX-v2 froze at 234. Compiled by `worker-index-final-edition` (read-only; no console contact; no nested subagents). Baseline: **INDEX-v2** (`worker-index-v2-refresh/`, 2026-10-04 23:05). This edition covers the ~30 deliveries that landed after that freeze — the two launch candidates going built+gate-approved, the full closure wave, the app re-pin, the fidelity fixes, and the knowledge base.

Conventions: `W/…` = `outputs/2026-10-04-zcode-team/…`; `C/…` = `outputs/2026-10-04-zcode-controller/…`; `L<key>` = ledger key `zcode_<key>`. "VERIFIED" = this worker recomputed the hash/size on disk this pass; everything else is quoted from the cited worker artifact with citation.

---

## THE one-line status

**Both launch candidates are built, gate-approved, and waiting locally — the console went DOWN at close before anything was staged, so the first command on return is the staging path (reachability check, then the comp6 one-pass/staging).**

- Launch-9 candidate: `W/worker-comp6-driver/prx-candidate-composition6-1/` — fself sha16 `76ee6af3` 30,773,943 B (VERIFIED) = the controller-local staged copy `C/console/comp6-staged-fself.prx` (VERIFIED byte-identical) — FINAL gate receipt **GO, failed_checks []**.
- Launch-10 armed candidate: `W/worker-launch10-prebuild/prx-candidate-composition6-l10-a/` — wrapped fself sha16 `d8cb4a76` 30,818,247 B (VERIFIED) — offline gate **GO 15/15**; its only delta vs launch-9 is the 2-line seed-table swap.
- **Console state at close**: homebrew servers DOWN before comp6 staging (FTP refused / HTTP empty). Slot still holds composition-5 **r11 `0911af0c`** (30,792,151 B), activation root holds the v2 generation, ploader.ini must still match pre-image `8d331b3f` 247 B. Nothing was dispatched, retried, or replayed against the down console (EVE-STATE "CONSOLE-DOWN NOTE"; RECONCILED.md divergence-2).

**Cheat-sheet pointer** — read these two FIRST, they are the operational layer of this index:
- Controller: `W/worker-session-eve-coherence/EVE-STATE.md` — staged-console table (S1–S3), the 12-artifact local-ready table (L1–L12, all hashes recomputed 12/12 MATCH), and the **first-five-commands** sequence quoted below.
- Console owner (the user): `W/worker-user-launch-guide/USER-LAUNCH-GUIDE.md` — the 2-minute plain-language guide (seven OK lines expected, OPTIONS = F7, what success/failure look like).
- Deep operational detail: `W/worker-return-session-master-plan/RETURN-SESSION-MASTER-PLAN.md` (Phase 0→3, ~75–125 min, 7 user actions) + `W/worker-controller-decision-brief/CONTROLLER-DECISION-BRIEF.md` (per-verdict branch tables, timing discipline) + the durable-facts `W/worker-knowledge-consolidation/KNOWLEDGE-BASE.md` ("READ THIS FIRST" — domains A–E).

## The two candidates

| | **Launch-9 candidate (Shape-A safe)** | **Launch-10 armed candidate** |
|---|---|---|
| Dir | `W/worker-comp6-driver/prx-candidate-composition6-1/` | `W/worker-launch10-prebuild/prx-candidate-composition6-l10-a/` (`-b` = byte-identical determinism repro) |
| Wrapped fself | `76ee6af3…` 30,773,943 B (VERIFIED; also at `C/console/comp6-staged-fself.prx`) | `d8cb4a76…` 30,818,247 B (VERIFIED) |
| Seed table | 3-slot: GUA+FName ctor+SCO seeded, `kGMalloc{false,…}`, `kRequiredSetComplete=false` (`SeedTable.h` `2cab0c52`, VERIFIED) | 4-slot armed: `kGMalloc{true,true,0x930add8,…}`, `kRequiredSetComplete=true` (`SeedTable.launch10.h` `59effe6e`, VERIFIED) |
| Delta between them | — | **exactly the 2-line SeedTable swap**: initializer source byte-identical `c85c42f7` (2044 lines, VERIFIED), 13/14 build objects sha-identical, bridge object byte-identical `16331ebe`, imports 196==196 (worker-launch10-prebuild REPORT §1) |
| Gate | FINAL gate receipt **GO (offline gate), failed_checks []**; D1 determinism PASS via 3 byte-identical packaging runs (`843a94d9`/`bce857c5`/`76ee6af3`) | gate v2 manifest mode **GO 15/15** (`gate-receipt-launch10-armed-a-vs-b.json`; the honest B2 first-FAIL preserved as `.b2-firstattempt.json`); 17/17 static checks both passes; second full pass byte-identical |
| Compiled behavior | Shape-A fold: `ScanGame` = 0x4a4 B refusal stub, zero post-arming code (post-build reviewer verified at objdump level) | Shape-A fold GONE: armed banner + `slot 3 gmalloc bound at` + `[Seeds] REFUTED` strings present; ScanGame lambdas 14 vs 0; +370 defined externs |
| Status | Built + gate-approved + staged **locally**; waiting on console reachability | Built + gate-approved; waiting per LAUNCH10-MATRIX row lookup (a staging decision, not a build wait) |

Build history honesty: the independent post-build review (`W/worker-comp6-post-build-reviewer/POST-BUILD-VERDICT.md`) recorded **NO-GO at its 23:35 review close** (packaging refused on a bridge-header pin drift; no candidate PRX existed then). That state was superseded the same night: the D1 failure was root-caused (stale `--reuse-composition-run` lineage default linking composition-4/run-3 — a comp-5-era leftover), fixed in the `d1fix` driver copy, and a full one-pass completed producing shipped candidate-1 with three-run byte-identity and the FINAL gate GO (worker-d1-determinism-diagnosis REPORT; gate-receipt-composition6-FINAL.json). The NO-GO document remains valuable as the independent verification of the Phase-A composition content (Shape-A fold, configured-roots strings, expected unit set, zero UND delta).

## The session-command sequence (6 lines)

From EVE-STATE "First five commands" + master plan Phase 2/3 (game fully CLOSED through staging; `HTTP=http://ps5-host-not-configured.invalid:8084`, `FTP=ftp://ps5-host-not-configured.invalid:2121`):

1. **Reachability**: `curl -sS --connect-timeout 10 "http://ps5-host-not-configured.invalid:8084/list_payloads"` → 200 + catalog OK; refused/empty = console still DOWN: STOP, no dispatches.
2. **Pre-image gate**: fetch `ftp://…/data/plugins/ploader.ini` → expect sha `8d331b3f…` 247 B; divergent = STOP + preserve + reconcile, never re-send.
3. **Staging first**: `python3 outputs/2026-10-04-zcode-controller/comp6_one_pass.py` → ODR pre-flight → gate v2 GO → stage comp6 `76ee6af3…` into the composition slot (upload + readback band check); `JOB FAILED` anywhere = STOP for the whole launch path.
4. **Game-closed batch** (final tool `5ca8d1c7 (post ninth-slot adoption)`, VERIFIED — selftest 405 ok/0 failed): `console_batch.py dll8` → `e2e` (13/13 + payload `a21771f8…`/681888, MODE=B gate) → `stage-ridealongs` (FINAL 9-entry ini `fd9c7393…`, INI-BOOTSTRAPPED branch, end state exactly **8 registered**) → `stage-settings` (v3 lockstep: 1448 B `d8faa4ec…` + 447 B `4d98ecd7…`, VERIFIED).
5. **Game session**: `console_batch.py game-evidence` waits → USER arms ploader (OK popup 1–60 s) → launches Townfall → CP0–CP7 + probe-verdict freshness gates (`igd1-6ebab07f2111` / `fc1pool1-78508a2e2940`) → load save → OPTIONS once → CROSS.
6. **Reconcile, then launch-10**: read evidence boxes 1–6 of LAUNCH10-MATRIX §4 → consistency rules → row lookup (R1–R9) → stage the armed candidate `d8cb4a76…` only if the row says so.

Laws throughout: dispatch EXACTLY ONCE; ≥30 s gaps; timeout = crash-signal; any gate mismatch = STOP + preserve + reconcile, never re-send. Full timeline with per-step receipts: RETURN-SESSION-MASTER-PLAN.md (as updated by worker-session-docs-final-sync's DOC-SYNC block).

## Honest expected outcomes

**Launch 9** (comp6 + 4 ridealongs; user: launch Townfall with a loaded save, wait ~1 min):
- First real console exercise of **conversion+configured roots+page-quantized lease+D2 text-base guards** — launch 8 proved conversion+context once; these three have never run in a real launch. Expected: they pass (offline gate + preflight GO + post-build review of the composed object).
- Then a **loud [Seeds] stop BEFORE any exec-side touch**: `failed(1,"[Seeds] Required-slot seed table incomplete: present [g_uobject_array fname_ctor_wchar static_construct_object_internal] missing [gmalloc]")` — module survives, game safe, nothing bound, no scan, no walk, no detour (compiled ScanGame is a 0x4a4-byte refusal stub; verified in the artifact). Do NOT misread `[Seeds] REFUTED:` (a different, consume-path, launch-10-class message). Engine-ready / mods-started / Item Spawner menu are **NOT expected**.
- **Guaranteed products independent of the stop**: the **IGW-1 verdict** {W_WORKS / W_FAILS_R_WORKS / BOTH_FAIL / FATAL} deciding the exec route AND the vtable route in one run; fc1's 11 data-side FName-pool checks (+ `byteprop_id_for_find_crosscheck`); observer + qdrv1 session records; first r11-lease/D2/roots console evidence.
- Full hop-by-hop prediction with failure signatures: `W/worker-launch9-prediction/LAUNCH9-PREDICTION.md` (fidelity verdict: CURRENT, line-drift caveat — FIDELITY-REPORT §1.1).

**Launch 10** (armed `d8cb4a76…`; strictly by matrix row): not a promise — a router. Per `W/worker-launch10-matrix-prep/LAUNCH10-MATRIX.md` §2: R1/R2 (W_WORKS): per-page transitions + PLH layer, GMalloc binds behind V1/V2, first engine-ready possible with I12 SCO-detour as the named hazard; R3/R4 (W_FAILS_R_WORKS): vtable-swap backend, D-1 is the gating unverified fact; R5/R9 (BOTH_FAIL/FATAL): no-exec-gate data-side attempt (diff set delivered, needs integration); R6/R7/R8: no arming / re-derivation / hold. `[Seeds] REFUTED` is the new early terminal if a seeded slot fails in-object validation. Consistency rules (§0) apply BEFORE the row lookup; T-OTHER lease/D2 refusals gate launch 10 entirely.

## What landed since INDEX-v2 (the ~30 deliveries, by theme)

| Theme | Deliveries (owner → artifact) |
|---|---|
| **Launch-9 candidate completed** | comp6 one-pass final build (`worker-comp6-driver/`: candidate-1 + FINAL gate GO; `prx-candidate-composition6-2` = second packaging run); independent post-build review, NO-GO-at-close superseded (`worker-comp6-post-build-reviewer/`); D1 root cause + d1fix driver + 3-run byte-identity (`worker-d1-determinism-diagnosis/`); ODR emitter-check receipts for run-3 + negative controls (`worker-odr-emitter-check/`; real one-pass receipt `receipt-one-pass.json` still absent — see open items); no-exec-gate is separate (below) |
| **GMalloc derived** | `worker-gmalloc-derivation/`: slot 3 = linked `0x930add8` → runtime `0x970add8`; FMallocBinned3 class-anchor + RELA-reconstruction route (1,661,599 relative relocs; vtable at linked `0x7ec26e0`); exactly ONE bootstrap store image-wide; 11 consumer thunks; two-qword data-side validator V1/V2 designed; null-GMalloc port path honestly rejected (would need a consumer-semantics change) |
| **Launch-10 made a staging decision** | `worker-seed-table-derivation/` (SEED-TABLE.json.launch10 + armed-table sequencing record); `worker-launch10-prebuild/` (armed candidate, 2-pass determinism, gate 15/15); `worker-launch10-matrix-prep/` (LAUNCH10-MATRIX.md R1–R9 — resolves INDEX-v2's PENDING); `worker-no-exec-mode-gate/` (U-SWAP-5 diff set, closes the matrix's GAP as proposal) |
| **Bridge r13 receipted** | `worker-bridge-r13-polish/`: r13 `10385e22` 734 lines (VERIFIED), 5 review fixes with unit receipts, refreshed host test, driver pins moved r12→r13; r12 `feed5254` byte-frozen (VERIFIED). INDEX-v2's "r13 NOT receipted" resolved |
| **Session machinery final** | FINAL 9-entry ini law + canonical 440 B template `fd9c7393` (VERIFIED; `worker-final-ini-layout/`); batch tool final adoption `5ca8d1c7 (post ninth-slot adoption)` selftest 405/0 (`worker-batch-final-ini-adoption/`, VERIFIED in place); full-session desk rehearsal 78/0 + branch walk (`worker-session-desk-rehearsal/`); DOC-SYNC of master plan + decision brief (`worker-session-docs-final-sync/`, `worker-controller-decision-brief/`); USER-LAUNCH-GUIDE (`worker-user-launch-guide/`); session archive scaffold implementing the archive law (`worker-session-archive-scaffold/`); EVE-STATE snapshot + corrected lines (`worker-session-eve-coherence/`) |
| **Closure wave** | Two preserved divergences resolved from local ground truth — dll8 appends 37–40 (dll8-first) and slot identity = `0911af0c` r11, `04869ab3` = superseded r2-era (`worker-divergence-reconciliation/RECONCILED.md`); acceptance-record spec defining a COMPLETE acceptance (`worker-post-spawn-closure-prep/` — goal still [TARGET]); app track closure recipe (`worker-app-track-closure/APP-SESSION-INTEGRATION.md`); DLL track milestone with 8 criteria + binding no-inflation clause (`worker-dll-track-closure/TRACK-COMPLETE.md`) |
| **App re-pin** | `worker-app-payload-repin/`: P0 APPLIED — `PAYLOAD_IDENTITY` → `a21771f8…`/681888 MODE=B in all 3 vendored copies (grep-verified), 42/42 API + 52/52 UI green; binary `a677e7b2…`/build_id `9b46b161…` proven UNCHANGED (deterministic rebuild byte-identical) — the task premise that the binary sha would change is corrected |
| **Knowledge + fidelity** | `worker-knowledge-consolidation/KNOWLEDGE-BASE.md` (domains A–E, 17 anti-patterns, read-first); `worker-ue4ss-docs-fidelity/FIDELITY-REPORT.md` (LAUNCH9-PREDICTION CURRENT; ENGINE-READY-PATH STALE; LAUNCH10-MATRIX CURRENT-as-router; MASTER-VERIFICATION-RUNBOOK STALE-with-fix; initializer is 2044 lines not 2006) |
| **R3 carrier fact-finding** | `worker-r3-zerosyscall-tier/` (R3 design verdict: SetFuncPtr real, A9 is the only hook dependency, 2 prerequisite diffs); `worker-r3-carrier-prep/` (PostRender REFUTED offline; survivors ReceiveTick/BlueprintUpdateAnimation/ambiguous "Tick"; oracle refutation recorded); `worker-carrier-probe-build/` (cc1 probe `modbridge_carrier.prx` BUILT: data-side FName-pool census, own jsonl channel, zero syscalls; 9th-ini-entry STAGE-PLAN proposal) |
| **Incomplete** | `worker-pre-session-inventory/` — **CLAIM.json only**; PRE-SESSION-INVENTORY.md and REPORT.md absent at read time (01:21 dir). Its cross-check task is NOT delivered; treat its citations as unverified until a successor runs them (this edition's §Cross-check covers the same ground partially) |

Track-1..8 carryovers unchanged from INDEX-v2: launches 1–8 history; evidence-tooling evolution; 6-family console PASS `133c5e9e` with dll8 pending; E2E promotion applied + composer splice + run-config B; ridealong artifacts (observer `1afa0997`, qdrv1 `52c32216`, fc1 `d2431bf8`, IGW-1 armed `48949316`); console inventory still r11-era per CONSOLE-DOWN.

---

## (a) PENDING console tests — the complete table

Nothing below has ever executed on a console beyond the pre-r11 history; all staging steps expect game fully CLOSED.

| # | Test | Plan (owner artifact) | Expected evidence | User effort |
|---|---|---|---|---|
| 1 | **Comp6 staging** (blocked only by console DOWN) | EVE-STATE cmd 3; master plan Phase 1; `C/comp6_one_pass.py` + ONE-PASS-CHECKLIST | comp6 `76ee6af3…` in the composition slot, readback band check EXACT; slot no longer r11 `0911af0c` | None (controller) |
| 2 | **dll8 Static-TLS payload run** | `worker-console-batch-runner/console_batch.py dll8` (final `5ca8d1c7 (post ninth-slot adoption)`); CONSOLE-STAGE-PLAN-8FAM Step 5 | 4 records appended at **37–40** (dll8-first; 41–44 only if dll7 fallback ran first — RECONCILED.md), build `c3aba79f`, static_tls enabled, imports 55 exact, memory.last 114688→163840, cleanup retired; readback EXACT vs `79281191…` 336,552 B (VERIFIED) | Game CLOSED |
| 3 | **E2E installer MODE B run** | `console_batch.py e2e`; STAGED-E2E-COMMANDS.md; promotion APPLIED (`63c28979`); payload re-pinned `a21771f8…`/681888 | stages 0–6 in e2e-status.jsonl; 13/13 cmd SHASUMS; per-dispatch cmd_gone + `.consumed` hash-equal + evid readback; §7 reset scoped to `/data/modbridge/**` | Game CLOSED; possibly sign-in prereqs |
| 4 | **Ridealong staging: FINAL ini + probes + settings v3** | `console_batch.py stage-ridealongs` + `stage-settings`; FINAL-INI-LAYOUT.md; USER-SESSION-GUIDE | ini end state BYTE-EXACT = canonical 440 B `fd9c7393…`, **exactly 8 registered** (mode-0 igf consumes no slot); possible INI-BOOTSTRAPPED verdict (ONE sanctioned whole-file STOR); 4 PRX uploads under SHT-strip readback law (`48949316`/`d2431bf8`/`52c32216`/`1afa0997`); settings v3 readback 1448 B + 447 B | Game CLOSED |
| 5 | **Launch 9** (comp6 + IGW-1 + fc1 + observer + qdrv1) | master plan Phase 3 + CP0–CP7; LAUNCH9-PREDICTION.md; CONTROLLER-DECISION-BRIEF branch tables | E1–E6 (conversion/roots/lease/D2) pass, then loud `[Seeds] Required-slot seed table incomplete … missing [gmalloc]` stop (T-A); IGW verdict {W_WORKS/W_FAILS_R_WORKS/BOTH_FAIL/FATAL} in `/data/modbridge-exec-writability.jsonl` seq 1..12; fc1 `validated` 11/11 + crosscheck (or named refusal / GRACE); observer log growth; qdrv1 C2 records; tick line `0x33ac8c0` | Arm ploader, launch Townfall, load save; NO OPTIONS this launch (menu path unproven past the gate) |
| 6 | **Launch 10** (armed `d8cb4a76…`, contingent) | LAUNCH10-MATRIX.md §4 boxes 1–6 → §0 rules → §2 row → §3 command delta | Per row: R1/R2 walks-behind-transitions + GMalloc bind (I12 hazard); R3/R4 vtable-swap/D-1; R5/R9 data-side engine-ready attempt; R6/R7/R8 evidence-only; `[Seeds] REFUTED` = new early terminal | After launch-9 reconciliation |
| 7 | **cc1 carrier census probe** (R3 fact F-1) | `worker-carrier-probe-build/` STAGE-PLAN (9th ini entry, final-ini law); CARRIER-CANDIDATES.md §5–6 | `/data/modbridge-carrier-facts.jsonl` census records (3 carrier names + 19 walk class-strings), 30×4 s window, GUA-gated, zero syscalls; decides whether the FuncPtr-marshal has a carrier | Rides a game session (own `= 2` line) |
| 8 | **Pad hardware probe** | `worker-pad-hardware-probe/` | `/data/pad-probe-events.jsonl`; 7 PASS criteria | Press OPTIONS in window; no game needed |
| 9 | **Rung-3 combined probe** (igf7 `695a17bd`) — optional | `worker-combined-igf7/` | 299+ records | One more launch |
| 10 | FName FC1 exec-side runtime probe — **HELD** (record-corrected: never wired; data-side fc1 rides launch 9) | `worker-fname-fc1-prep`/`worker-fc1-validator-probe/` | FC1-A data supersedes | Do NOT launch before exec-span verdict |
| 11 | **App-track console closure** (rides the same window) | `worker-app-track-closure/APP-SESSION-INTEGRATION.md` (P0 re-pin APPLIED) | MODE=B staged-manifest consumption on console; live-cookie round trip; jobs/oauth exercise | Real OAuth client_id = user-only |

## (b) Open design questions — owner artifacts

| Question | State | Owner artifact |
|---|---|---|
| Exec-page writability (X→RX/RWX) | Probe ARMED, rides launch 9; nothing proven; ring-0-fatal risk explicit | `worker-writability-probe-build/` + `worker-exec-writability/EXEC-WRITABILITY-EVIDENCE.md` (§5/§7 decision tree) |
| Vtable-swap hedge (needs D-1 R→RW on RELRO data page) | Design complete; premise corrected (vtables READ-ONLY L3); R1–R4 limits named | `worker-vtable-swap-design/VTABLE-SWAP-DESIGN.md` |
| R3 zero-syscall tier + carrier fact F-1 | Design verdict delivered; PostRender refuted offline; probe built, in-game fact pending | `worker-r3-zerosyscall-tier/R3-TIER-DESIGN.md`; `worker-r3-carrier-prep/CARRIER-CANDIDATES.md`; `worker-carrier-probe-build/` |
| Verdict-gating (arming-rule extension) | DESIGNED, NOT ARMED — `kExecWritabilityVerdictGating=false` | `worker-initializer-integration/SEED-TABLE-FORMAT.md` §6b |
| GMalloc in-game validation (V1/V2) | Derived candidate; seedable only behind the validator; armed build ready | `worker-gmalloc-derivation/GMALLOC-CANDIDATES.json`; `worker-launch10-prebuild/` |
| Seed slots 2/4 in-game confirmation | Derived (3-way consensus / population statistic); fc1 validator rides launch 9; REFUTED is terminal-never-replay | `worker-seed-table-derivation/SEED-TABLE.json`; `worker-fc1-validator-probe/` |
| PLH Prospero layer (U-SWAP-4) | DESIGN ONLY — no code; implementer required for R1/R2 rows | `worker-exec-writability/` §7.1; ue4ss-polyhook-platform-port report |
| No-exec-mode gate (U-SWAP-5) | Diff set delivered as PROPOSAL (11 hunks); needs integration if BOTH_FAIL/FATAL | `worker-no-exec-mode-gate/NO-EXEC-GATE.diff` + DESIGN |
| ProcessEvent family (R4) | Unbudgeted; global interception lost under swaps | `worker-vtable-swap-design/` §6.3 |
| Item Spawner acceptance (visible menu + spawn) | Never observed in-game; complete-acceptance manifest SPEC'd, goal [TARGET] | `worker-post-spawn-closure-prep/ACCEPTANCE-RECORD.md` |
| App user-gated items | Real client_id, live-cookie session, console consumption | `worker-app-download-runtime/`; `worker-app-track-closure/` |
| Static TLS console proof (7th/8th family) | dll8 staged, pending run #2 above | `worker-dll-8fam-combined/EXPECTED-DELTA-8FAM.md`; `worker-static-tls-7fam/` |
| Pre-session inventory audit | INCOMPLETE — CLAIM only | `worker-pre-session-inventory/CLAIM.json` (successor needed) |

---

## Cross-check — 15 claims (method: sha256/size recomputed on disk this pass unless noted; full detail in CLAIM.json)

1. **Launch-9 fself** `76ee6af3…` 30,773,943 B — VERIFIED at `worker-comp6-driver/prx-candidate-composition6-1/`; equals `C/console/comp6-staged-fself.prx` (VERIFIED) = EVE-STATE L1 = D1-report wrapped fself. Three sources agree.
2. **Launch-9 gate GO** — `gate-receipt-composition6-FINAL.json` parsed: verdict "GO (offline gate)", failed_checks `[]`, build_dir = candidate-1.
3. **Launch-10 armed fself** `d8cb4a76…` 30,818,247 B — VERIFIED at `prx-candidate-composition6-l10-a/`, matches worker-launch10-prebuild REPORT §4.
4. **Settings v3** — ini 1448 B `d8faa4ec…`, loadlist 447 B `4d98ecd7…` — VERIFIED; matches EVE-STATE L7/L8 and master-plan step 5.
5. **Canonical ini** — 440 B `fd9c7393…` — VERIFIED; line census 8×`= 2` + 1×`= 0` matches FINAL-INI-LAYOUT §1 ("exactly 8 registered").
6. **Final batch tool** — `console_batch.py` sha16 `5ca8d1c7 (post ninth-slot adoption)` — VERIFIED in `worker-console-batch-runner/` (adoption applied in place), matches the DOC-SYNC header of RETURN-SESSION-MASTER-PLAN.md (selftest 405 ok/0 failed, quoted).
7. **Seed tables** — `SeedTable.h` `2cab0c52` / `SeedTable.launch10.h` `59effe6e` — VERIFIED; matches launch10-prebuild REPORT §1 and FIDELITY-REPORT §0.
8. **Bridge r13/r12** — `10385e22` / `feed5254` — VERIFIED; matches worker-bridge-r13-polish shipped-bytes table and launch10-prebuild §3 (bridge object byte-identical `16331ebe`).
9. **Merged initializer** — `c85c42f7`, 2044 lines — VERIFIED (sha + wc -l); corrects the docs' "2006 lines" per FIDELITY-REPORT §0.
10. **Ledger** — 268 `zcode_` entries counted this pass (INDEX-v2 said 234 → +34); newest `zcode_worker_ue4ss_docs_fidelity`.
11. **GMalloc constants** — `0x930add8` and `0x970add8` confirmed present in `GMALLOC-CANDIDATES.json` (programmatic check); narrative quoted from worker-gmalloc-derivation REPORT.
12. **dll8 ELF** — `79281191…` — VERIFIED under `worker-dll-8fam-combined/out-sdk-composition/c3aba79f…/`; matches EVE-STATE L9 and master-plan step 1.
13. **App re-pin** — grep found `a21771f894dd43be…` in all 3 vendored `modmgr_api.py` copies (VERIFIED count=3); binary-unchanged conclusion quoted from worker-app-payload-repin REPORT §3.
14. **Desk rehearsal** — `SESSION-GO-NO-GO.json` exists and reads phase1_comp6 = "CONDITIONAL — MOCK ONLY…"; 78/0 quoted from REPORT; Phase-2 flows green on the fake drive.
15. **Console-state claims** (slot r11 `0911af0c`, console down, pre-image `8d331b3f`) — NOT verifiable from here (no console contact, by design and by law); triple-quoted from EVE-STATE S1/S3 + CONSOLE-DOWN NOTE, RECONCILED divergence-2 (cycle-receipt `staged_sha256=0911af0c`), and master-plan Phase 0. The 04869ab3 hash is confirmed SUPERSEDED (r2-era) by RECONCILED's size arithmetic.

Honesty notes: no >8 quality claim anywhere; Item Spawner activation never observed in-game; the launch-9 outcome statement is a prediction with named failure signatures, not a promise; every quoted PASS number belongs to its cited worker receipt — this edition re-verified only on-disk bytes/counts (16 artifacts) and never the console.
