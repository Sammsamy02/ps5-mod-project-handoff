# REPORT — worker-ninth-slot-ini-adoption

Worker: worker-ninth-slot-ini-adoption, 2026-10-04/05. Status: **complete**.
NO console contact; host-side law/tool/doc revision only. Every console step
remains gated by the standing user-launch ritual. Full claim: `CLAIM.json`
(written first, finalized last). Raw provenance: as-found snapshots + unified
diffs in `law/` and `diffs/`; run receipts in `receipts/`.

## What was adopted

The ninth-slot bounded revision of the final-ini law, exactly per
`worker-carrier-probe-build/STAGE-PLAN.md` + its
`CONSOLE-BATCH-CONSTANTS-BRIEF.md` (that worker delivered proposal-only and
edited no owner file). The cc1 carrier probe
(`modbridge_carrier.prx`, `cc1-0864d65c1baf`, fself `d83410aa…`/26845 B,
bare-sdk `e72366c4…`/73088 B — re-verified on disk BEFORE pinning, run1==run2)
becomes ini entry 9, appended immediately AFTER the igf `= 0` line:

**New law: 9 entries / 8 registered / 8 of MAX_PLUGINS=10 (headroom 2) /
post-launch EXACTLY 8 `OK (<ms>): <file>` lines, ZERO FAIL.**
New canonical `ploader.ini.canonical`: **440 B, sha256 `fd9c7393…`** —
byte-verified this session as the old 400 B `bbc34c16…` 8-entry canonical +
EXACTLY ONE appended line (`/data/plugins/modbridge_carrier.prx = 2`), matching
the STAGE-PLAN §1 pin byte-for-byte. The pinned parser law is untouched
(MAX_PLUGINS=10, modes 0/1/2, mode-0 skip consumes no slot, file-order
registration, NO dedupe).

## 1. Law revision (worker-final-ini-layout chain, edited in place)

- `ploader.ini.canonical` — 9-entry bytes (as-found 400 B snapshot kept).
- `verify_final_ini_layout.py` — pins the 9-entry bytes, `EXPECTED_REGISTERED`
  7 -> 8, FINAL-INI-OK text (2/10 headroom), +4 selftest cases (carrier before
  igf / carrier mode-1 / duplicate carrier / the superseded 8-entry law itself
  now REJECTS — `carrier_only_8entry`). Selftest **13/13**; the canonical
  prints **FINAL-INI-OK** (receipts/verify_ninth_slot_run.txt). The STAGE-PLAN's
  "the verifier will print FINAL-INI-FAIL until its owner adopts" expectation is
  thereby resolved by this revision, as that brief deferred to the owner chain.
- `simulate_ploader_ini_final.py` — `CANONICAL_ENTRIES`/`STAGER_LINES` +
  carrier; `PREDECESSOR` auto-derives the igf anchor; counts 8 registered /
  8/10; "9th slot burned" parser law; permutation proof extended to the FULL
  **9! = 362880 staging orders, all converging byte-for-byte** to the canonical
  (real 13.1 s wall, no sampling) — receipts/simulate_ninth_slot_run.txt shows
  ALL variants PASS. Parser core + ensure machinery proven BYTE-IDENTICAL to
  the as-found file; only the entry table, stager set and count/text
  expectations moved.

## 2. Batch tool adoption (worker-console-batch-runner chain, edited in place)

`console_batch.py` (as-found `b00b3a69…`, the delivered prior revision):

- **CC1 dict** per the constants brief (keys `console_path`/`log_path` carried
  under the tool's historical names `target_prx`/`log` so the shared SHT-strip
  machinery is unchanged; `ini_line`/`ini_predecessor`/`ue4ss_line_prefix`
  pinned verbatim).
- **FINAL_INI**: +carrier at pinned mode 2 anchored to the igf line;
  `registered_expected_final` **8**; canonical **440 B `fd9c7393…`**; carrier
  in `stager_lines`; `post_launch_ok_lines_expected` **8**; superseded 8-entry
  identity recorded in a comment. FINAL_ORDER/LINE_OF/PREDECESSOR derive, so
  the content-anchored ensure op inserts the carrier line after igf `= 0` and
  the ONE sanctioned INI-BOOTSTRAPPED whole-file STOR now lands the 440 B law.
- **stage-ridealongs**: ride.0/ride.1 preflights cover FIVE fself + FIVE
  bare-sdk ELFs (cc1 readback 71528 / body `131ff1c2…`, see divergence below);
  ride.2/2b/3 gate text pins 9 entries / 8 registered + the cc1 ensure op;
  ride.5 baselines all THREE probe sinks; new **ride.9b** cc1 PRX upload +
  SHT-strip readback (all prior step ids ride.0–ride.10 unshifted); ride.10
  handoff = 8 registered, EXACTLY 8 OK lines, three probe sinks.
- **game-evidence**: ge.2 fetches **eight** evidence sinks; new **ge.3c** cc1
  census gate (`carrier_evidence_verdict`): strict JSON < 1 KiB envelope,
  build_id `cc1-0864d65c1baf` (foreign = FAIL), success = entry / startup
  (polls 30, poll_s 4) / mapscan / 30 `qh_counter` / `census_match` lines /
  `census_result` `CENSUS:VALIDATED` with controls `None_id_zero:1` +
  `ByteProperty_ascii_ok:1` and `negative_hits.PostRender:0` +
  `ZZZNotARealFunctionQQ:0` / `summary` `write_failures:0` / exactly ONE
  `CC1:` UE4SS.log line. `CENSUS:REFUSED` = honest terminal WARN (new staged
  window, never a re-rating); a `census_truncated:1` VALIDATED verdict stays a
  PASS with a BOUNDED-coverage note; a VALIDATED verdict with diverging
  control oracle = FAIL (corrupt evidence). Evidence-only: never certifies
  FUNC_Native, invocation rate, game-thread identity, or any UFunction walk.
- **Suites, all green, prior suites unshifted** (itemization in
  receipts/RECEIPTS-INDEX.txt): selftest **405 ok / 0 failed** (prior 372
  kept; 3 law-pinned checks updated; +33: structure, 4 ninth-slot end-state
  rejections, selftest M with 14 cc1 classifier checks, 10 anchor extensions);
  `tests/fake_console_drive.py` **92 green / 0 failed** (prior 90 kept; 9
  law-pinned names updated; +2: cc1 readback verdict + carrier-verdicts.json
  PASS) — the drive still converges its THREE different staging histories
  byte-exact to the NEW canonical; `plan` **OVERALL GREEN** across all flows.
- `USER-SESSION-GUIDE.md` updated to the 9-entry law (chain-owned doc).

**DIVERGENCE RESOLVED (the one place this revision did not copy the brief
blindly):** the delivered `readback_body_sha 1d17a27d…` is
sha256(bare[64:EOF]) — the WHOLE 73088 B tail including the SHT region
(reproduced exactly), NOT the §3 law the tool enforces for the other four
probes: sha256 of bytes 64..71528 of the segment-covered prefix (the only
window the ftpsrv channel serves). Pinned the prefix-law value
**`131ff1c2…`** (host-recomputed, run1==run2); the delivered constant is kept
as `readback_body_sha_as_delivered`; ride.1's gate text and
receipts/cc1-readback-constants-check.txt carry the full derivation. Length
71528 and every other constant verified byte-identical; law NOT widened. Had
the delivered constant been pinned, ride.1/ride.9b would refuse every real cc1
readback (constant false-FAIL, the exact "artifact/constants mismatch = STOP"
class).

## 3. Session docs — count cites 7 -> 8 (superseding docs-final-sync)

- `RETURN-SESSION-MASTER-PLAN.md` — new DOC-SYNC NINTH SLOT banner; step 3/4
  (9-entry law, 440 B `fd9c7393…`, FIVE PRX uploads incl. cc1 + its readback
  identity), Phase-2 gate "ini registers exactly 8 plugins", Phase-3 "eight
  evidence sinks" + the ge.3c cc1 gate, Prereq 4 chain note.
- `CONTROLLER-DECISION-BRIEF.md` — DOC-SYNC NINTH SLOT banner; §0b observer
  row (FIVE PRXs, 9-entry law, chain to this revision) and FINAL-ini row
  (SUPERSEDED-IN-PLACE: 8 registered / EXACTLY 8 OK / selftest 405/0); §3.3
  branch table (8 registered; bootstrap = 440 B `fd9c7393…`); §4.1 ge.2 eight
  sinks + new ge.3c bullet; §5 "ploader injection of 8 plugins" with the
  EXACTLY-8-OK expectation; §7 row.
- `USER-LAUNCH-GUIDE.md` — the five "seven OK lines" handoff mentions are now
  "eight OK lines" (plain-language register preserved; supersedes
  docs-final-sync U1–U5).
- The prior DOC-SYNC banners of docs-final-sync are preserved verbatim as
  history; the new banners supersede their counts.

## 4. Supersession banners (banner-only, bodies untouched)

`FINAL-INI-LAYOUT.md` and the final-ini-layout
`CONSOLE-BATCH-CONSTANTS-BRIEF.md` carry SUPERSEDED banners pointing at the
9-entry law, the cc1 brief, this dir's as-found snapshots, and this revision.
The 8-entry law remains the historical record (also still described by
worker-batch-final-ini-adoption's delivered CLAIM/REPORT, untouched).

## 5. Honesty boundaries

- Nothing has executed on target: ini bytes, cc1 PRX readback, census evidence
  are [TARGET] until the user-run ritual. The cc1 module itself has never
  executed on console (per its own STAGE-PLAN §9 — this adoption changes
  nothing about that).
- A `CENSUS:VALIDATED` gate PASS evidences only which of the 53 fixed names
  are interned in the live FName pool for that launch; it never certifies
  FUNC_Native / invocation rate / thread identity / UFunction walks.
- No replay of any failed/reserved job; no provider or quota actions; no
  unrelated file touched. The legacy stage-qdrv1/stage-observer flows and the
  worker-batch-final-ini-adoption revision record stay byte-identical
  (b00b3a69… preserved in diffs/).
- The permutation proof ran the full 9! = 362880 set (13.1 s) — no shortcut.

## 6. Deliverables (this directory)

CLAIM.json (first) · console_batch.py + tests/fake_console_drive.py +
USER-SESSION-GUIDE.md (adopted tool copies) · law/ (9-entry canonical +
verifier + simulator + as-found snapshots) · diffs/ (as-found snapshots +
unified patches for all 11 edited files) · receipts/ (suite runs, constants
divergence, hashes, RECEIPTS-INDEX.txt) · REPORT.md (last).

Result hashes: CLAIM.json `result_hashes` + receipts/final-result-hashes.txt.
Tool identity after adoption: console_batch.py
`456f9372f7c59a412f4db8247f82b424cc66b02efa2a19bbf7d679845f573734`.
