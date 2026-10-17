# CURRENT-STATE-DIGEST-FINAL — the 3-minute read (session-eve final edition, 2026-10-05)

Compiled by `worker-index-final-edition` (read-only synthesis; no console contact; nothing launched). Covers the ~30 deliveries since INDEX-v2 (2026-10-04 23:05). Full tables with citations: `INDEX-FINAL.md` (same dir).

---

## THE status, one line

**Both launch candidates are built and gate-approved, sitting ready on the Mac — but the console's homebrew servers went DOWN at close before anything was staged, so the slot on the PS5 still holds the old r11 build: the first thing on return is the reachability check, then staging.**

## Read this first (cheat sheets)

- **Controller**: `W/worker-session-eve-coherence/EVE-STATE.md` — what is staged on console now (S1–S3), what is local-and-ready (L1–L12, 12/12 hashes re-verified), and the exact first-five commands.
- **You (console owner)**: `W/worker-user-launch-guide/USER-LAUNCH-GUIDE.md` — your part is ~2 minutes: launch the game, load a save, press OPTIONS once.
- **Durable facts**: `W/worker-knowledge-consolidation/KNOWLEDGE-BASE.md` ("READ THIS FIRST" — image facts, memory model, seed table, methods, 17 dead ends).

## The two candidates, ready and waiting

| | Launch-9 (safe, Shape-A) | Launch-10 (armed) |
|---|---|---|
| File | `worker-comp6-driver/prx-candidate-composition6-1/` (also copied to `C/console/comp6-staged-fself.prx`) | `worker-launch10-prebuild/prx-candidate-composition6-l10-a/` |
| Identity | fself `76ee6af3…` 30,773,943 B — re-verified on disk | fself `d8cb4a76…` 30,818,247 B — re-verified on disk |
| Gate | FINAL gate **GO, failed_checks []**; three packaging runs byte-identical | Offline gate **GO 15/15**; 17/17 static checks; second build byte-identical |
| Difference | 3-slot seed table: GMalloc deliberately **unseeded** → compiled loud refusal, zero exec-side code (ScanGame folds to a 0x4a4-byte stub) | **Only delta = the 2-line SeedTable swap**: GMalloc `0x930add8` seeded behind its validator, arming complete → full engine path compiled in |

Build-story honesty: an independent post-build review recorded NO-GO at 23:35 (packaging refused mid-flight on a bridge-header pin drift). That was fixed the same night — the D1 determinism failure was root-caused to a stale reuse-lineage driver default, and a full one-pass then produced the shipped candidate with the FINAL gate GO. The reviewer's content verification (Shape-A fold, expected unit set, zero import delta) stands.

## First commands on return (6 lines — game CLOSED throughout staging)

1. `curl http://ps5-host-not-configured.invalid:8084/list_payloads` → 200? No = console still down, STOP.
2. Fetch `/data/plugins/ploader.ini` → sha `8d331b3f…` (247 B)? Divergent = STOP + reconcile, never re-send.
3. `python3 outputs/2026-10-04-zcode-controller/comp6_one_pass.py` → stages comp6 `76ee6af3…` (upload + readback). `JOB FAILED` = STOP.
4. `console_batch.py dll8` → `e2e` → `stage-ridealongs` → `stage-settings` (final tool `5ca8d1c7 (post tools-final)`; ini ends byte-exact = 440 B `fd9c7393…`, **8 registered**; settings v3 1448 B `d8faa4ec…` + 447 B `4d98ecd7…`).
5. `console_batch.py game-evidence` waits → **you**: arm ploader, launch Townfall, load save (OPTIONS menu is NOT this launch).
6. Read the evidence per LAUNCH10-MATRIX §4 → pick the row → only then stage the armed `d8cb4a76…`.

## What launch 9 will honestly produce

1. **First real test** of conversion + configured roots + the r11 page-quantized lease + D2 text-base guards. Expected: pass (all gate-approved offline; never run in a real launch before).
2. **Then a loud stop before anything exec-side happens**: `[Seeds] Required-slot seed table incomplete: present [g_uobject_array fname_ctor_wchar static_construct_object_internal] missing [gmalloc]` — module survives, game safe, nothing bound. Engine-ready, mods-started, and the Item Spawner menu are **NOT expected**. (Not the same as `[Seeds] REFUTED:` — that is a launch-10-class message.)
3. **The guaranteed products**, independent of the stop: the **IGW-1 verdict** {W_WORKS / W_FAILS_R_WORKS / BOTH_FAIL / FATAL} — one probe run decides both the exec route and the vtable route; fc1's 11 FName-pool checks; observer + qdrv1 session records.
4. Full prediction with failure signatures: `worker-launch9-prediction/LAUNCH9-PREDICTION.md` (fidelity-checked CURRENT).

## What launch 10 is

A **staging decision, not a build wait** — the armed candidate is already built. Its outcome is routed by `worker-launch10-matrix-prep/LAUNCH10-MATRIX.md`: read launch-9's evidence boxes 1–6, apply the consistency rules, look up row R1–R9. R1/R2 (probe says writes work): engine-ready becomes possible, SCO detour is the named hazard; R3/R4 (read-only works): vtable-swap route, D-1 unverified; R5/R9 (both fail / fatal): data-side attempt using the delivered no-exec-gate diff; R6/R7/R8: evidence-only, no arming. `[Seeds] REFUTED` is the new early terminal if a seeded slot fails validation — terminal evidence, never replayed.

## The big change since the last digest

**GMalloc — the one REQUIRED unseeded slot — is now derived**: linked `0x930add8` → runtime `0x970add8`, found via the FMallocBinned3 class-anchor + RELA-reconstruction route (the image stores no absolute pointers — 1,661,599 relative relocs — which is why every earlier route died). Seedable only behind its two-qword data-side validator; it enters the binary only in the armed launch-10 build. Launch 9 still runs the 3-slot safe table **by explicit sequencing decision**.

## Also landed since INDEX-v2 (one line each)

- Bridge **r13 receipted** (5 review fixes, driver pins moved; r12 frozen) — resolves INDEX-v2's "r13 not receipted".
- **LAUNCH10-MATRIX.md written** (was CLAIM-only) + the U-SWAP-5 no-exec-gate gap **closed as a proposal** (11-hunk diff).
- **App re-pin applied**: payload pin → `a21771f8…`/681888 (MODE=B) in all 3 vendored copies; 42/42 + 52/52 suites green; the desktop binary proven UNCHANGED by it.
- **FINAL 9-entry ini law** + canonical 440 B template adopted into the batch tool (`5ca8d1c7 (post tools-final)`, selftest 405/0); full-session desk rehearsal **78 checks green** + all 6 branch shapes resolve; USER-LAUNCH-GUIDE + archive scaffold + EVE-STATE snapshot.
- **Closure wave**: both preserved divergences resolved from ground truth (dll8 records **37–40** dll8-first; slot identity = `0911af0c` r11, `04869ab3` superseded); app + DLL track-closure milestones recorded with binding no-inflation clauses; acceptance-record spec written (goal still [TARGET]).
- **Knowledge base + fidelity pass**: KNOWLEDGE-BASE.md (domains A–E); doc verdicts — LAUNCH9-PREDICTION CURRENT, ENGINE-READY-PATH STALE, LAUNCH10-MATRIX CURRENT-as-router, RUNBOOK STALE-with-fix; initializer is 2044 lines.
- **R3 carrier**: PostRender refuted offline; survivors ReceiveTick / BlueprintUpdateAnimation; the cc1 census probe `modbridge_carrier.prx` is BUILT to decide the last open fact in-game.
- **Incomplete**: `worker-pre-session-inventory/` delivered CLAIM.json only — its audit never landed.

## Pending console tests (all still pending; the console was never contacted this wave)

1. Comp6 staging (blocked only by console DOWN) → 2. dll8 run → 3. E2E MODE B → 4. ridealong ini + settings v3 staging → 5. **Launch 9** → 6. **Launch 10** by matrix row → 7. cc1 carrier probe (rides a session) → 8. pad probe (standalone) → 9. igf7 rung-3 (optional) → 10. FName exec-side probe (HELD) → 11. app console closure (rides the window). Plan + expected evidence for each: INDEX-FINAL.md §(a).

## Open design questions (owner artifacts)

Exec-writability verdict (`worker-exec-writability/`) · vtable-swap D-1 (`worker-vtable-swap-design/`) · R3 carrier F-1 (`worker-r3-carrier-prep/` + `worker-carrier-probe-build/`) · verdict-gating activation, constant false (`SEED-TABLE-FORMAT.md` §6b) · GMalloc V1/V2 validation (`worker-gmalloc-derivation/`) · slots 2/4 in-game confirmation (fc1 + seed-table owner) · PLH Prospero layer (design only) · no-exec-gate integration (proposal only) · Item Spawner acceptance (never observed; `ACCEPTANCE-RECORD.md`). Full table: INDEX-FINAL.md §(b).

## Standing honesty notes

Nothing in this wave touched the console. No >8 quality claim anywhere. Item Spawner activation has never been observed in-game. The launch-9 statement is a prediction with named failure signatures, not a promise. All PASS numbers are quoted from worker receipts; this edition re-verified 16 on-disk artifacts (hashes/sizes/counts) and never the console.
