# RUN-RECEIPT — verdict-pipeline dress rehearsal (synthetic launch-10 evidence)

Worker: worker-verdict-rehearsal · 2026-10-04 · LOCAL ONLY, synthetic fixtures; no console contact; no real verdict advanced.
Classifier under test: `../worker-launch10-evidence-upgrade/evidence.sh` (run UNMODIFIED, `--from-dir` + `--baseline-dir fixtures/_baselines --json-tail --out runs/<S>.summary.txt`).
Oracle: `../worker-launch10-acceptance-criteria/ORACLE.md`. Post-popup layer: `../worker-post-popup-evidence/POST-POPUP.md`.

All five runs: exit 0, empty stderr, ZERO `WARN` lines (baseline drift guard clean — fixture head windows byte-match the pinned launch10-prep baselines). Raw outputs: `runs/A..E.summary.txt`, `runs/A..E.stdout.txt` (stdout = summary + one JSON tail line).

## Per-set record

| Set | Intended class (oracle) | Classifier VERDICT | Stage chains (supplier-diag / ue4ss-text) | Match? |
|---|---|---|---|---|
| A | BEST (seeds bound → engine-ready → mods-started → mod load attempted + post-OPTIONS drain) | `MODS_STARTED` | `S1→S2→S3→S7→S5→S4→S9→S11` / `S8B_INITIALIZER→S10_LADDER` | YES — BEST on the classifier vocabulary; ladder `[ activated engine-ready mods-started ]`, status `last=mods-started code=0` |
| B | GOOD (past V1/V2, F15 value-carrying pool refusal, no engine-ready) | `FAILED_STATUS` | `S1→S2→S3→S7→S5→S4→S9→S11` / `S8B_INITIALIZER` | YES with controller mapping — status detail carries the F15 refusal verbatim; ladder `[ activated ]` (no engine-ready); notable lines show armed+slot0+slot3 bind (past V1/V2) |
| C | MINIMAL-ACCEPT (conversion passes: intervals_found=1 + diag events; NEW walkguard refusal at slot 0) | `FAILED_STATUS` | `S1→S2→S3→S7→S5→S4→S9→S11` / `S8B_INITIALIZER` | YES with controller mapping — status detail carries the F7 walkguard refusal verbatim; sdiag proves conversion ran (lease 6/6, start_ok=1); notable lines show armed marker only (slot 0 never bound) |
| D | FAIL (launch-9 geometry refusal recurs verbatim) | `FAILED_S6_CONVERSION` | `S1→S2→S3→S7→S5→S4→S6→S11` / `S6_CONVERSION` | YES — failure line matched by `map_detail`, STAGEHIT attribution + conversion-refusal NEXT text, r4 failline timing extracted |
| E | S-30S special row (ploader 30 s cap; sinks complete anyway) | `MODS_STARTED` | `S1→S2→S3→S4→S7→S9→S11` / `S8B_INITIALIZER→S10_LADDER` | YES — timing table shows `runtime_start_ok t=31237720 +34783ms` (>30 s cap) while the verdict ladder stays BEST; DECLARATION LAW (judge sinks, not toast) is exercisable from the page |

## Timing table outputs (per-writer guard honored)

- A/B/C: `writer=pathtest stamp=27661022133455ns (first stamp)`; supplier-diag STIME deltas from `module_start_entered 27638412` (cap 24 rows — see GAP-5); `writer=runtime-status session stamp: pid=417 t=27641000ms`.
- D: adds `writer=failline pid=417 t=27646987ms (first stamp for this pid)` + pathtest stamp 27658446233901ns (after the refusal — ordering consistent).
- E: compact 19-event sdiag shows the whole spread: wiring +1345ms, vmentry +841ms, conversion events +18ms, then `runtime_start_ok +34783ms`, `module_start_return +41ms`; pathtest 31277899001234ns; status pid=419 t=31211900ms. Cross-writer subtraction never attempted by the tool (GUARD line present every run).

## JSON-tail spot checks (machine-readable verdicts agree with the summaries)

- A/E: `"verdict":"MODS_STARTED"`, `status.fresh=yes`, `last_state=mods-started`, `crash_tail:3` (benign, see GAP-4), `progress:4`.
- B/C: `"verdict":"FAILED_STATUS"`, `last_state=failed`, `last_code=1`, `ladder=" activated"`.
- D: `"verdict":"FAILED_S6_CONVERSION"`, `failure_lines:1`, `entry4_lines:1`.

## Strings the classifier FAILED to classify (gaps — evidence worker owns evidence.sh; nothing fixed here)

- **GAP-1 (HIGH): conversion success-path diag tags are invisible.** `lmf_extent_base_low/high`, `lmf_extent_end_low/high`, `lmf_anchor_count`, `lmf_intervals_found` are real pinned tags (confirmed in `worker-facts-geometry-fix/staging/runtime_start_bridge_r13.geometrydiag.cpp`) but are in NEITHER `stage_of_tag` NOR the SDIAGSUM extraction (which carries only lease_spans/lease_reg/vmentry/cfg_gamedir). Consequences seen: (a) no `S6_CONVERSION` in the success-path chains of A/B/C/E — S6 appears only via the D-style refusal fan-out; (b) the oracle D3/D4 gating values **anchor_count=7 and intervals_found=1 appear NOWHERE in the one-page summary** — the controller cannot apply the MINIMAL-ACCEPT gate or the predicted `intervals=1 next to lease_reg_spans=6` pair without opening the raw jsonl. In E they surface only incidentally inside timing rows; in A the 24-row STIME cap hides them entirely.
- **GAP-2 (MEDIUM): the BEST-decisive mod line is never printed verbatim.** `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` and the post-OPTIONS drain line `Menu created.` count only as `progress markers: 4`; notable-lines carries only CLASSLINE entries ([Seeds]/failure/IGW/observer). The NEXT text says "Check [TownfallItemSpawner] prints (count above)" but the page has no verbatim line, so distinguishing G2 (load prints) from G3 (mod-error lines — also BEST) needs the raw log.
- **GAP-3 (LOW): chain-order semantics.** `bridge_exepath_source` is staged `S7_CFG_ATTACH` but is emitted mid-walk (before anchors/lease), so success chains read `S3→S7→S5→S4→…` (first-seen order) — out of numeric order; display-only, no misattribution.
- **GAP-4 (LOW, note): `crash_tail=3` on a BEST summary.** LoadMap/Tick vtable-walk lines bump the crash-tail counter; precedence correctly keeps MODS_STARTED, but a skimmer of the JSON could misread it as the igf6 flag.
- **GAP-5 (LOW, note): STIME rows capped at 24.** Event-rich boots (A: 45 events) hide conversion→start_ok deltas; S-30S detection is readable only when the event count is small (E) or from raw jsonl.
- **NOTE-1 (mapping, not a defect): GOOD/MINIMAL-ACCEPT launches classify as `FAILED_STATUS`.** Per oracle F30 the seed refusals reach ONLY status detail, and the `[Seeds] REFUTED` grammar runs on text streams only — so B/C correctly do NOT hit the `SEEDS_REFUTED` verdict. Everything the ladder needs is on the page: verbatim `status detail:`, `ladder=[ activated ]` (engine-ready absent), sdiag conversion proof, and the notable `[Seeds]` bind lines. Controller rule rehearsed: slot-3 bind line present + pool/ctor-class refusal = GOOD (F15 caps GOOD); bind lines absent + armed marker only = MINIMAL-ACCEPT.
- **NOTE-2 (unexercised):** the failline DELTA branch (two r4 stamps, same pid) and the stop phase (`runtime_stop_begin`/`runtime_stop_result` → real S11) were not fixture-built. `module_start_return` maps to `S11_STOP`, so every chain ends `…→S11_STOP` even when nothing stopped — label quirk, tag parsed correctly.

## Fixture fidelity (what the synthetic sets contain)

- Bases: entry log and xwp byte-identical to the pinned baselines (shortest-config no-growth contract); UE4SS.log = pinned baseline (7591 B, `edf404a5…`) + appended oracle-verbatim new lines; runtime-status = fresh session (417 or 419) vs baseline session 273 → `fresh=yes`.
- Pinned strings used verbatim: F1/F2 armed+unbound, F6/F9/F14/F18/F21 bind set (A), F15 v2 value-carrying pool refusal with observed CurrentBlock 0 / Blocks[0] 0x0 (B), F7 walkguard refusal `seed class exec-only (call-only)` (C), D6 geometry refusal verbatim (D), G2 load line + POST-POPUP `Menu created.` drain line (A/E), H1/H2 PATH_TEST/ACTIVATION_TEST stamps.
- Predicted runtime values per task: anchor_count=7, lease spans=6, reg=6, intervals_found=1, pool 0x9765ec0, ctor 0x63d590, gmalloc 0x970add8 -> 0x97089b8, slot0 0x97ba110, SCO 0x890aa0, derived vtable 0x82c26e0 (F11-class), extent 0x400000/0xc0602c8.
- sdiag JSONL uses the oracle schema with tag spellings confirmed against the pinned BG/EN sources (`anchor_cov` value=(w<<16)|covered per BG source lines 391/397).
- Known synthetic-value caveat: F29's Tick vtable was rendered slid (`vtable: 0x37ac8c0; scan: 0x0`, armed-build null-scan shape); the oracle row renders the unsclid literal `0x33ac8c0` — unpinned rendering, no classification impact (line classifies OTHER/CRASH_TAIL either way).

Rebuild any time: `sh build-fixtures.sh` (deterministic; regenerates fixtures/{A..E}/from-dir from fixtures/_baselines).
