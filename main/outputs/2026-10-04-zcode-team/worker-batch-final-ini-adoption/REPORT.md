# REPORT.md — worker-batch-final-ini-adoption, 2026-10-04 (written last)

Adopter of `worker-final-ini-layout/CONSOLE-BATCH-CONSTANTS-BRIEF.md` into
`worker-console-batch-runner/console_batch.py` — the tool's NEXT bounded revision. Host-only;
**no console contact**; nothing has executed on the console, every console constant stays
[TARGET] until the Step-0 RETR / post-STOR RETR diffs.

## Ownership chain (recorded in CLAIM.json)

worker-console-batch-runner (original) → worker-batch-runner-8fam-patch →
worker-evidence-freshness-fixes (92b56889…) → worker-observer-staging-extension (fd21d171…,
as-found for this revision) → **worker-batch-final-ini-adoption (this revision, b00b3a69…)**.
All prior owners terminal/delivered. As-found snapshots + unified diffs preserved under
`diffs/`; result applied in place at `worker-console-batch-runner/` and copied here.

## Adopted (exactly per the brief)

1. **The definitive 8-entry law** (`FINAL_INI`): xwp-probe, fc1pool, composition, qdrv1,
   observer, aligned_entry, path_test, igf=0 — `expected_existing` pinned modes,
   `anchor_of` content-anchored ensure ops (FINAL-INI-LAYOUT §3.3, NOT tail-append),
   `registered_expected_final = 7` (mode-0 igf consumes no slot), partial flows use
   `<n active lines present>`, canonical 400 B / sha `bbc34c16…` template + final verifier
   paths. The end-state gate is now BYTE-EXACT equality with the canonical template via the
   final verifier (`verify_final_ini_layout.py`, executed unmodified), replacing the "5
   registered" count gate; every legacy STOP class is kept (duplicate, missing pre-existing,
   mode divergence, unknown line, unparseable, mode 3) and the form-divergence case
   (canonical order but non-canonical bytes) STOPs instead of rewriting.
2. **The ONE sanctioned bootstrap STOR**: when the Step-0 pre-image's present lines are not in
   canonical relative order (the historical aligned_entry/path_test-before-composition shape —
   which NO sequence of appends can fix), the flow performs exactly ONE whole-file STOR of the
   canonical template with `.bak` + byte-exact RETR diff, gated as its own **INI-BOOTSTRAPPED**
   verdict (`ini-bootstrap-verdict.txt` in the run dir). Never a silent rewrite, never a second
   attempt; anomalous pre-images still STOP.
3. **Both probes pinned and staged**: `XWP_ARMED` (fself `48949316…`/20013 B, bare
   `cfed5e3d…`/69464 B, readback law **67832 B / daea2ff4…**) and `FC1POOL` (fself `d2431bf8…`/
   22733 B, bare `54f8335e…`/69408 B, readback law **67848 B / 7e2dce27…**) — constants
   re-verified on disk and RECOMPUTED from the bare-sdk ELFs under the same max(p_offset+p_filesz)
   SHT-strip law this session, byte-identical to the brief. `stage-ridealongs` now uploads all
   FOUR PRXs (xwp, fc1, qdrv1, observer) under the §3 law with the one-re-STOR-then-abort rule.
4. **Evidence paths wired into game-evidence**: `/data/modbridge-exec-write-probe.jsonl`
   (build_id `igd1-6ebab07f2111`) and `/data/modbridge-fc1pool-facts.jsonl`
   (`fc1pool1-78508a2e2940`) fetched as sinks; a new `ge.3b` gate classifies freshness vs the
   staging baselines, strict-JSON envelope law, and the fc1 success signature
   (`fc1pool_result` verdict `validated` 11/11); honestly recorded fc1 refusals are WARN with
   their reason codes (never auto-retried); the single `FC1POOL:` UE4SS mirror line is recorded.
   These verdicts are EVIDENCE — the gate never certifies writability.
5. **stage-settings** (new flow): NEW-SETTINGS §3 lockstep restage of the v3 package bytes from
   `worker-settings-v3-package/package-v3` — settings ini 1448 B `d8faa4ec…`
   (`[Hooks] HookProcessInternal=0` + `HookProcessLocalScriptFunction=0`) + loadlist 447 B
   `4d98ecd7…` — freeze the staged pair first (recorded v2 inventory 834 B `9570ead3…` +
   446 B `3afd10ce…`; divergent/unowned = FAIL, never overwrite; already-v3 = honest no-op),
   upload as ONE pair, byte-exact readback BOTH. Host-side lockstep proof (loadlist row ==
   1448 == settings bytes) before any upload.
6. **Session order**: `all` = dll8 → e2e → stage-ridealongs(+probes) → stage-settings →
   game-evidence. Post-launch handoff now expects **EXACTLY 7** `OK (<ms>): <file>` ploader
   lines, zero FAIL (was 5).

## Verification (all green, receipts in `receipts/`)

- selftest **372 ok / 0 failed** (prior 271: 268 untouched, 3 updated off the superseded
  6-entry law; +101 new). New: FINAL-law plan/ensure/bootstrap/STOP/end-state fixtures, all
  24 ensure orders converging byte-exact, the historical pre-image proven non-convergent by
  appends, probe sink classifier both ways, settings pair lockstep law, SHT-strip round-trips
  for both new probes.
- fake console drive **90 green checks, 0 failed** (56 prior checks kept identical; 5 prior
  stage-ridealongs checks rewritten; +34 new). New scenarios: bootstrap on the historical
  pre-image, ensure-only, TWO different staging histories converging byte-exact (the drive-level
  order-permutation case), full no-op re-run (no ini STOR, no PRX upload, readback-only law
  verification), unknown-line STOP (no bootstrap, no upload, no store), settings
  freeze→upload→readback→idempotent no-op→divergent-freeze halt. All four PRX law verdicts OK;
  the final verifier subprocess genuinely executed inside the drive.
- `plan` OVERALL GREEN; each of the ten flows green individually.
- Adopted-law proofs re-run by this worker: verifier selftest **9/9**, simulator **13 variants
  PASS** (incl. the 8! = 40320-permutation byte-exact convergence), **FINAL-INI-OK** on the
  canonical template.

## Result hashes (sha256)

- `console_batch.py` — `b00b3a69ea5d7b56785cc2dd6eff00236058d2c3a42bd321ef827de7fa14e63a`
  (in place at `worker-console-batch-runner/console_batch.py`; copy here)
- `tests/fake_console_drive.py` — `d6160e91bca09f49971cf82efaed29053f4a202760f090aed85f3132c40e4f68`
- `USER-SESSION-GUIDE.md` — `49414a285dfbcb9dad33c67f0c67d789048f89dacc1703443dbea99893c7ba05`
- as-found: `console_batch.py` fd21d171… (snapshot), drive 47896a5e…, guide 33ec8a89… (snapshots + patches in `diffs/`)

## Honesty boundaries

- No console contact; staging/verification preparation only. The bootstrap STOR, ensure ops,
  probe readbacks, settings readbacks and probe verdict records are all [TARGET].
- The legacy `stage-observer` / `stage-qdrv1` flows and the superseded 6-entry
  `COMBINED_INI` law remain in the tool as documented fallback paths for pre-adoption console
  states; `all` and `stage-ridealongs` use FINAL_INI exclusively. `rollback-ridealongs` still
  flips the two ridealong lines (the definitive layout's probes/igf are untouched by it; igf=0
  is never flipped by any flow).
- The claimed host-recomputed readback constants reproduce from the pinned artifacts today; if
  the artifacts move, `ride.1`/`set.0` STOP by design (never widen the law).
- The 12-18 s launch-timing figures remain host-side estimates from the pinned parser constants,
  not measurements; no >8-quality or full-parity claims are made anywhere.
