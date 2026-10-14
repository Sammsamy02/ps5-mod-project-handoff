# EDIT-LOG — worker-runbook-eighth-sink (2026-10-04)

Target: `outputs/2026-10-04-zcode-team/worker-menu-visibility-runbook/MASTER-VERIFICATION-RUNBOOK.md`
Edited IN PLACE. Structure preserved (sections 1-7, CP0-CP7 ordering, all original rows kept).

- as-found sha256: `e362dc9834486ee247d24f975d14790c6ba1675b0a325509fd30d71d1eb368ce` (519 lines)
- synced sha256:   `648a5c898dc4a74b6f3b9320e489ed5464b2fe8655fb97c154d5864b503ea51a` (612 lines)
- as-found copy: `as-found/MASTER-VERIFICATION-RUNBOOK.as-found.md` (this dir)

Adopted law synced: console_batch rev `456f9372` ge.2 eight-sink fetch + ge.3c census gate;
cc1 record shapes per `worker-carrier-probe-build/STAGE-PLAN.md` §6-§7.

## E1 — Header provenance note (after title)

Added a `REV 2026-10-04b — ninth-slot sync` banner: names the adopted law, the cc1 eighth
sink, the B9 tick-parenthetical corrections, the entry-r4 grammar fix, and points here.
Original honesty preamble unchanged.

## E2 — Merged-sources table (§ "Merged sources")

- `worker-tick-address-validation` row: dropped the stale "Tick derivation 0x37AC8C0"
  contribution wording (correction-register B9 listed :22); the row now carries the
  retraction + runtime `0x33ac8c0` (linked `0x2fac8c0`) fact and its provenance.
- Added 3 source rows: worker-carrier-probe-build STAGE-PLAN (cc1 shapes/9-entry ini law),
  console_batch rev 456f9372 (ge.2/ge.3b/ge.3c semantics), tick-correction-register +
  fidelity report (B9 + entry-r4 grammar).

## E3 — §1 retitled "the five sinks" → "the eight sinks"; table + writer note

- Writer-cite line now names the probe stage plans; documents the tool's actual ge.2 flow
  (S, L, D1, C2 + X, F, CC fetched in one pass; D2 covered by the staging baseline/absent
  check) and the adopted eight-sink counting (S, L, D1, D2, C2, X, F, CC).
- Three new sink rows: X `/data/modbridge-exec-write-probe.jsonl` (build_id
  `igd1-6ebab07f2111`, ge.3b), F `/data/modbridge-fc1pool-facts.jsonl` (build_id
  `fc1pool1-78508a2e2940`, fc1pool_result validated 11/11 + one `FC1POOL:` line, ge.3b),
  CC `/data/modbridge-carrier-facts.jsonl` (ninth slot, build_id `cc1-0864d65c1baf`,
  ge.3c).
- Added the honest writer-state note: unlike C2, X/F/CC have staged writers this session
  (9-entry ini, cc1 appended after the igf `= 0` line, mode `= 2`); absent probe PRX ⇒
  honest zero-records WARN, not a launch failure.

## E4 — §1 line-shape reference

- FIXED (fidelity item 3, source-verified `runtime_module_entry.c:198-202`): L entry-failure
  grammar now `[t=<monotonic ms>ms pid=<pid>] modbridge module_start failed: <stage>
  code=<n> detail=<...>`; kept stage/mfa code notes. Added a CAUTION that
  `post_launch_check.py`'s `FAILURE_LINE_RE` still anchors WITHOUT the prefix — flagged to
  the tool owner, not self-patched.
- Added: X/F/CC v3 envelope law (strict JSON < 1 KiB, `marker:"modbridge.ingamefacts"`,
  `schema:1`, `build_id`-disambiguated channels); CC success sequence (entry → startup
  polls:30/poll_s:4 → mapscan → 30 qh_counter → census_match → census_result → summary);
  CC `census_match` fields (name, table_index, kind, id, block, offset, block_ptr,
  entry_addr, bytes16; 53-name table; ≥3 expected ReceiveTick/Tick/BlueprintUpdateAnimation,
  live truth decides); CC `CENSUS:VALIDATED` PASS shape (controls, negative_hits,
  matched_count, wide_entries_skipped, census_entries_scanned, census_truncated:0,
  budget_entries:1048576 + base/pool/current_block/id fields; census_truncated:1 = BOUNDED
  census rule); CC `CENSUS:REFUSED` shape with all reason codes (4 census + 11 fc1-chain +
  2 census-walk + 3 grace-class); L mirror lines (ONE `CC1:` line ≤224 B, one `FC1POOL:`).

## E5 — §2 intro + new Step C + manual curl fallback

- Intro: added the census-window timing (cc1 = 30 polls × 4 s, ~2-3 min allowance per the
  tool handoff).
- NEW "Step C — probe verdict sinks X/F/CC (tool ge.2 fetch + ge.3b/ge.3c classify; ninth
  slot)": the tool's actual flow order (ge.1 → ge.2 eight-sink fetch → ge.3 CP0-CP4 →
  ge.3b → ge.3c → ge.4/4b → ge.5); ge.3b success signature; ge.3c success oracle; THE
  HONEST WARN-ON-REFUSAL RULE (CENSUS:REFUSED = honest TERMINAL WARN; grace-class remedy =
  NEW staged window with NEW ids; never re-rate unchanged evidence; never replay; zero
  records = WARN; WARN never blocks CP5); FAIL = structural sink breach (unparseable /
  ≥1 KiB / foreign build_id) → halt + reconcile; scope limit per STAGE-PLAN §9 (53 fixed
  names interned in the LIVE pool, that launch only; never FUNC_Native/invocation rate/
  game-thread identity/UFunction walk).
- Manual fetch fallback: added the 3 new bounded curls (X, F, CC paths) matching ge.2.

## E6 — §3 new CP row "CP4b — ninth-slot verdict sinks (evidence-only)" between CP4 and CP5

File/shape, Pass, WARN honesty (incl. no-census_result-yet and write_failures≠0 and ≠1 CC1
line), FAIL structural cases, and the never-gates-CP5 / never-touches-the-rung-ladder /
bounded-census notes. CP numbering preserved — no renumbering of CP5-CP7.

## E7 — CP2 pass-criterion parenthetical (B9)

"(linked `0x33ac8c0` + base `0x400000`; scan preferred by …)" → "(runtime value; linked
`0x2fac8c0` + base `0x400000` — the once-circulated `0x37ac8c0` was a base-convention
error, retracted by the SEED-TABLE byte decode; linked `0x33ac8c0` decodes mid-instruction;
fidelity note: under the armed seed path the scan arm is skipped — the seed constant IS the
pinned value)". Both-addresses-equal criterion kept (the live ge.3 still pins
`GAME["tick_line"]` with both slots equal).

## E8 — Gap G3 rewritten honestly

Was "the tick address is a prediction … no prologue byte-compare … exists". Now: the
prediction is RETRACTED and the byte-compare gap is CLOSED linked-side by SEED-TABLE; the
console-only residue is the never-observed live line (G7) + outcomes #6/#7
distinguishability.

## E9 — Pending-tests ledger: added T9 (ninth slot)

CP4b from a real launch: ge.3b 11/11 + cc1 census VALIDATED + exactly one `CC1:` line (or
an honest terminal REFUSED WARN recorded with reason_code; never re-rated; a revision needs
a NEW staged window). Explicitly not satisfiable from host evidence.

## Verified CURRENT (no edit) — task-2 sweep

- CP2/§6 tick constants `0x33ac8c0` — already fixed by the prior fidelity-fix pass (mtime
  evidence: runbook edited ~70 s after FIDELITY-REPORT.md).
- §4.3a restage tripwire — already re-pinned to settings v3 sha `d8faa4ec` (v3-authorized;
  pre-v3 `9f3fa127`).
- Error-code table (stage 1=CONVERSION 2=CONTEXT 3=START 4=INTERNAL) — CURRENT per fidelity
  §1.4 (r13 STOP_EXN renumber 2→5 does not touch it).
- D1 diagnostic tags, status ladder grammar, S/D1/L shapes, rung baselines, §4/§5 mod-line
  shapes, §6 triage rows, G1/G2/G4-G7 — re-checked, no drift found.
- Deliberately NOT edited: `worker-menu-visibility-runbook/CLAIM.json` :13/:40 and
  `REPORT.md` :23/:65 (retracted 0x37ac8c0 as historical provenance — correction-register
  category b, B7/B8; owner's records); `post_launch_check.py` (owner's tool — the regex/
  entry-r4 prefix tension is flagged in §1 and in REPORT.md).
