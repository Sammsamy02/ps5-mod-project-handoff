# REPORT — worker-verdict-rehearsal (launch-10 verdict-pipeline dress rehearsal)

Worker: worker-verdict-rehearsal · 2026-10-04 · local only · synthetic evidence, no console contact, no verdict advanced.
Companions: `RUN-RECEIPT.md` (per-set record + gap list), `CLAIM.json` (structured), `build-fixtures.sh` (deterministic builder), `fixtures/` + `runs/` (materials).

## 1. What was done

Built five synthetic launch-10 evidence sets from the pinned launch10-prep baselines and the oracle's verbatim strings, ran the upgraded classifier (`worker-launch10-evidence-upgrade/evidence.sh`) UNMODIFIED against each in `--from-dir --baseline-dir` mode, and recorded verdict/stage/timing/gaps per set. Nothing in evidence.sh was changed; all findings are reported for the evidence worker.

Fixture shape (all five): `UE4SS.log` = pinned 7591 B baseline (`edf404a5…`, recovered from the upgrade worker's own l10_success fixture head window and sha-verified) + appended oracle-verbatim new lines; `townfall_runtime_aligned_entry.log` and `modbridge-exec-write-probe.jsonl` byte-identical to baselines (shortest-config no-growth contract); `runtime-status.txt` = fresh O_TRUNC session (417/27641000; E: 419/31211900 vs baseline session 273); `modbridge-supplier-diag.jsonl` = oracle-schema events with tag spellings confirmed against the pinned staging sources (`runtime_start_bridge_r13.geometrydiag.cpp`, `runtime_module_entry_r4.c`), including the `anchor_cov`/`anchor_match` value encoding `(w<<16)|flag`.

Scenario contents:
- **A / BEST**: full diag chain S1→S9 with predicted values (vmentry 10019, walk_stop 0, lease spans 6 / reg 6, anchor w0..w5 per BG encoding, extent 0x400000→0xc0602c8, anchor_count=7, intervals_found=1, cfg attach 1/1, start_ok, return) + UE4SS.log armed marker, unbound line, all FIVE bind lines (slot 0, slot 3 V1+V2, pool 0x9765ec0, ctor 0x63d590, SCO 0x890aa0), engine-version/object/Kismet/required-object/vtable-walk/setup lines, `start_mods`, G2 load line `[Lua] [TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.` + POST-POPUP drain lines (`42 spawnable pickups found. Pages=3`, `Menu created.`) + PATH_TEST/ACTIVATION_TEST stamps + status ladder activated/engine-ready/mods-started.
- **B / GOOD**: same conversion+context pass; UE4SS.log carries armed + slot 0 + slot 3 (V1/V2 pass) then ends mid-module-lines; status `failed(1, <F15 value-carrying pool refusal>)` verbatim: `[Seeds] REFUTED: FName pool at 0x9765ec0: CurrentBlock < 1 (pool not initialized or wrong pool/base) (observed CurrentBlock 0, Blocks[0] 0x0 vs span [0x8018c000,0x100118000)). Terminal …never replay.`
- **C / MINIMAL-ACCEPT**: conversion passes (intervals_found=1 + full diag events); UE4SS.log armed + unbound only; status `failed(1, <F7 walkguard refusal>)` verbatim: `[Seeds] REFUTED: slot 0 g_uobject_array 0x97ba110: seed class exec-only (call-only) (expected data-readable); refusing reads. Terminal …never replay.`
- **D / FAIL**: the launch-9 geometry refusal recurs verbatim as an r4-prefixed entry line + matching status failed detail + sdiag refusal fan-out (`lmf_intervals_found=0`, `lmf_vmentry_cap/vm_scan_cap/linkmap_cap/provider_code/identity_source`, `runtime_start_failed`).
- **E / S-30S**: complete BEST sinks with an over-cap module_start: sdiag `module_start_entered 31200455 → runtime_start_ok 31237720` (+34783 ms > the 30 s ploader cap), PATH_TEST stamp 31277899001234 ns, full status ladder. Verdict must (and does) come from the durable sinks.

## 2. Results

| Set | Intended | Classifier | Match |
|---|---|---|---|
| A | BEST | `MODS_STARTED` | yes |
| B | GOOD | `FAILED_STATUS` (+ verbatim F15 detail, ladder without engine-ready) | yes via oracle ladder |
| C | MINIMAL-ACCEPT | `FAILED_STATUS` (+ verbatim F7 detail, sdiag conversion proof) | yes via oracle ladder |
| D | FAIL | `FAILED_S6_CONVERSION` (+ STAGEHIT, failline timing) | yes |
| E | S-30S | `MODS_STARTED` (+34783 ms visible in timing table) | yes |

All runs: exit 0, empty stderr, zero drift warnings. Timing extraction worked for every writer present (pathtest ns, r4 failline ms, sdiag t_ms deltas, status session point) with the per-writer guard printed every run. JSON tails agree with the summaries.

## 3. Gaps (reported, NOT fixed — evidence worker owns evidence.sh)

1. **GAP-1 (HIGH) — conversion success-path tags invisible.** `lmf_extent_base_low/high`, `lmf_extent_end_low/high`, `lmf_anchor_count`, `lmf_intervals_found` are real pinned tags (BG staging source) but absent from `stage_of_tag` and from `SDIAGSUM`. Effects observed: no `S6_CONVERSION` in success chains (A/B/C/E); the oracle D3/D4 gate values **anchor_count=7 and intervals_found=1 appear nowhere in the one-page summary** — the controller cannot verify "the geometry fix actually ran" or the predicted `intervals=1 next to lease_reg_spans=6` pair without reading the raw jsonl. Highest-value single fix candidate: add the six lmf tags to `stage_of_tag` (S6) and extend `SDIAGSUM` with `anchor_count=` / `intervals=`.
2. **GAP-2 (MEDIUM) — BEST-decisive mod lines not surfaced verbatim.** `progress markers: 4` counts them; `notable lines` does not print them; G2 (load prints) vs G3 (mod-error, also BEST) discrimination needs the raw log. Candidate: treat `TownfallItemSpawner` lines as CLASSLINE so they land in notable lines.
3. **GAP-3 (LOW) — chain-order display quirk.** `bridge_exepath_source` staged `S7_CFG_ATTACH` but emitted mid-walk → success chains read `S3→S7→S5→S4…`. Display-only.
4. **GAP-4 (LOW) — `crash_tail=3` on BEST summaries** (LoadMap/Tick walk lines). Precedence holds (MODS_STARTED wins); flagged so nobody misreads the JSON on launch day.
5. **GAP-5 (LOW) — STIME 24-row cap** hides conversion→start_ok deltas on event-rich boots (A: 45 events); E (19 events) shows the full spread.
6. **NOTE-1 — `FAILED_STATUS` is the correct classifier shape for GOOD/MINIMAL-ACCEPT.** Seed refusals reach only status detail (oracle F30) and the seeds grammar runs on text streams only, so B/C must not hit `SEEDS_REFUTED`. Every ladder input is on the page (verbatim status detail, ladder without engine-ready, sdiag conversion proof, notable `[Seeds]` bind lines). Controller rule rehearsed for launch day: slot-3 bind line present + later pool/ctor-class refusal = **GOOD** (F15 caps GOOD); bind lines absent + armed marker only = **MINIMAL-ACCEPT**; `FAILED_STATUS` + geometry-string detail with empty ladder = **FAIL**-family conversion refusal (cross-check D's direct line match).
7. **NOTE-2 — unexercised branches:** failline DELTA (two r4 stamps, same pid) and the real stop phase (`runtime_stop_begin`/`runtime_stop_result`); `module_start_return` maps to `S11_STOP`, so chains end `…→S11_STOP` even with no stop — label quirk only.

## 4. Fidelity caveats (synthetic values)

- Refusal/bind/timing strings are oracle-verbatim or oracle-pattern with the task-pinned values (anchor_count=7, spans=6, reg=6, intervals=1, pool 0x9765ec0, ctor 0x63d590, gmalloc 0x970add8 → 0x97089b8, slot0 0x97ba110, SCO 0x890aa0, derived vtable 0x82c26e0, extent 0x400000/0xc0602c8). mfa low = 35890 (0x8c32) per the oracle's plausible-bytes note.
- F29's Tick line rendered `vtable: 0x37ac8c0; scan: 0x0` (slide rule + armed-build null scan); the oracle row shows the unsclid literal `0x33ac8c0` — rendering unpinned, no classification impact.
- Timing numbers are internally consistent per writer and ordered coherently across writers (session-open between mfa and capture; PATH_TEST after module_start_return), but the tool itself never compares across writers (guard verified).
- Object names/addresses in F26-F29 lines are plausible placeholders; every such line classifies OTHER/CRASH_TAIL regardless of value.

## 5. Launch-day usage

The controller can classify real evidence with the exact commands rehearsed:
`evidence.sh --from-dir <fetched-dir> --baseline-dir <launch10-prep-dir> --json-tail`
and read the summary against `RUN-RECEIPT.md` §per-set + NOTE-1's mapping rules. Until GAP-1 is fixed, the controller must additionally check the raw `modbridge-supplier-diag.jsonl` for `lmf_anchor_count` / `lmf_intervals_found` (D3/D4 gate) — evidence.sh's page does not carry them yet.

This rehearsal advances no verdict; only the real console launch is evidence (oracle claim guard).
