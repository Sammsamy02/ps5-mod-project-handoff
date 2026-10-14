# LAUNCH10-RESULTS — first REAL launch-10 evidence, classified (worker-verdict-first-real)

Date: 2026-10-05 · Worker: worker-verdict-first-real · LOCAL ONLY, no console contact.
Inputs: `outputs/2026-10-04-zcode-controller/launch10-evidence/` (three real UE4SS.log
fetches + run3 runtime-status + run1/run3 supplier-diag jsonl + two data-fix artifacts),
classifier `outputs/2026-10-04-zcode-team/worker-launch10-evidence-upgrade/evidence.sh`
(unmodified), oracle `outputs/2026-10-04-zcode-team/worker-launch10-acceptance-criteria/ORACLE.md`
(incl. the merged final-build supplement). Baselines: `outputs/2026-10-04-zcode-controller/launch10-prep/`
— all four pinned sizes/shas VERIFIED before use (edf404a5/7591, 4b33b985/2343756,
57c40d4c/169, 20cff601/4362).

## 0. Method — per-run fixture construction (documented placement)

`evidence.sh --from-dir` expects exact fixture names, so each run got a constructed dir
(preserved under `fixtures/run{1,2,3}/`; classifier outputs under `classifier-outputs/`):

| run dir | UE4SS.log | runtime-status.txt | modbridge-supplier-diag.jsonl | notes |
|---|---|---|---|---|
| run1 | `run1-ue4ss.log` (7732 B, fetched AFTER run1) | — (not captured for run1) | `run1-sdiag.jsonl` (548 records, all pid 317) — RENAMED to the expected fixture name | log fetched after run1 ⇒ window ends at run1's refusal |
| run2 | `run2-ue4ss.log` (8211 B) | — (not captured) | — (no run2 sdiag fetch exists; run2's diag records survive inside the run3 cumulative set, pid 341) | |
| run3 | `run3-ue4ss.log` (8517 B) | `run3-runtime-status.txt` (117 B) | `run3-sdiag.jsonl` (1591 records = cumulative pids 317+341+355) — RENAMED | status is the post-terminal-write file |

Invocation (each, once): `evidence.sh --from-dir fixtures/<run> --baseline-dir <launch10-prep> --out classifier-outputs/<run>-summary.txt --json-tail > classifier-outputs/<run>-full.txt`. All exit 0.

Append-only contract VERIFIED: the first 7591 B of every run log sha-matches the
launch10-prep baseline head; windows are strictly incremental (run1 +141 B = the pid-317
refusal line; run2 +620 B; run3 +926 B). Zero classifier WARN/CONFIG-DRIFT lines on any run.

All three launches ran on ONE console boot (monotonic t_ms continuous: 60003456 →
60249173 → 60899781), fresh game pid per launch (317 → 341 → 355). Cross-sink timestamps
agree exactly: each sdiag `runtime_start_failed`/`mfa_activate_failed` t_ms equals the
t= prefix of the matching r4-prefixed UE4SS.log line.

## 1. Three-run table

| | run1 (pid 317) | run2 (pid 341) | run3 (pid 355) |
|---|---|---|---|
| Fix applied before run | (first launch-10 attempt) | `PS5Pad-sanitized.ini` (2958 B ASCII, sha 6ab119b3…) | `loadlist-sizefixed.txt` (declares `ue4ss/PS5Pad.ini 2958`, sha 54e3c2a0…) |
| Stage reached | Phase E2 — START stage (post-conversion): bridge walk, lease reg 6/6, spanpub, anchors, cfg, conversion ALL passed | Phase B — MFA activation (module prologue fully green, nothing bridge-side ran) | Phase E2 — START stage (post-conversion), identical ladder to run1 |
| Refusal (verbatim) | `[t=60004266ms pid=317] modbridge module_start failed: runtime_start_failed code=3 detail=PS5Pad.ini must be ASCII text without control bytes` | `[t=60249877ms pid=341] modbridge module_start failed: mfa_activate_failed code=-8 detail=size file is shorter than the load list says ue4ss/PS5Pad.ini` | `[t=60900581ms pid=355] modbridge module_start failed: runtime_start_failed code=3 detail=PS5Pad input unavailable: phase=6, error=-2137915388` (0x80920004) |
| Oracle class | E2 (EXPECTED-REFUSAL: prepare/start threw — pad profile) | B7 (EXPECTED-REFUSAL: loadlist/activation mismatch; named offending file) | E2 (EXPECTED-REFUSAL: pad provider init failed) |
| Classifier verdict | `FAILED_code3` | `FAILED_code-8` | `FAILED_code3` (status: session 355 fresh=yes, last=failed code=1, detail matches) |
| Outcome / liveness | entry returned 0, game continued UNMODDED (same boot carried runs 2-3); buffered-Lua harness green (sdiag seq 540-548); conversion clauses D1-D5 all met in sdiag, but no status capture ⇒ E4 unevidenced | **the runtime STARTED — record that**: `module_start_entered`, constructor_count=235 with all 235 ctor pairs return 0 (seq 1-478), `probe_lifecycle_result`=0 (NOT the B4 dead-rebuild), `runtime_wiring_begin` present, `mfa_activate_failed`=0xFFFFFFF8, `mfa_activate_os_error`=0, `module_start_return`=0, buffered-Lua green (seq 487-495). No bridge tags — refusal preceded capture, exactly per B7 | entry returned 0, game survived; PATH_TEST ran after the refusal (stamp 60971455045302); status written as a caught throw (`state 1 failed 1 …`), not a crash; buffered-Lua green (seq 540-548); **conversion passed with the EXACT predicted geometry** |
| Entry→refusal latency | 810 ms | 704 ms | 800 ms — S-30S ploader cap nowhere near |
| Verdict per oracle ladder | conversion ≥ MINIMAL-ACCEPT clauses met via sdiag; E4 `activated` NOT captured (no status fetch for run1) ⇒ honest bound: conversion pass is diag-proven, rung claim reserved | FAIL-class refusal is B7 — nothing downstream ran; no rung claim (its diagnostic value: named the offending file) | **MINIMAL-ACCEPT — CONFIRMED** (see §2) |

## 2. Run-3 verdict: the controller's MINIMAL-ACCEPT assertion — CONFIRMED

Oracle §2 MINIMAL-ACCEPT requires: (1) D1+D2 extent events present, D3 anchor_count>0,
D4 intervals_found>0, D5 cfg events present, E4 `activated` present; (2) the D6 geometry
refusal did NOT occur; (3) NO-CRASH holds. "Everything after conversion may be ANY named
failure … and the verdict still holds."

Clause-by-clause against the real lines (run3 sdiag, pid 355):

| clause | evidence (verbatim value lines) | verdict |
|---|---|---|
| D1+D2 extent | seq 532-535: `lmf_extent_base_low=0x00400000`, `lmf_extent_base_high=0x00000000`, `lmf_extent_end_low=0x0c0602c8`, `lmf_extent_end_high=0x00000000` — EXACTLY the predicted [0x400000, 0xc0602c8) | MET |
| D3 anchor_count>0 | seq 536: `lmf_anchor_count value=7` — EXACTLY the predicted igf3-shape 7 | MET |
| D4 intervals>0 | seq 537: `lmf_intervals_found value=1` — EXACTLY the predicted COALESCED value 1 (not 6); the launch-9 zero-interval death did not recur | MET |
| D5 cfg events | seq 530-531: `bridge_cfg_gamedir=1`, `bridge_cfg_workdir=1` | MET |
| E4 `activated` | the literal `state 1 activated 0` line is ABSENT from the fetched status (states=1: only `state 1 failed 1 PS5Pad input unavailable: phase=6, error=-2137915388`). Per the oracle's own two-openers note, the entry terminal `write_status_failed` O_TRUNC-replaces the earlier ack lines OF THE SAME LAUNCH. Activation is nonetheless proven by: diag `mfa_activated_low=36564` / `mfa_activated_high=0` (seq 484-485, ≈ the 0x8c32+overhead pattern of B6), and the refusal's stage code=3 (START) — reachable only AFTER MFA activation, bridge capture/conversion (code 1) and context (code 2) all completed | MET by evidence-equivalence, with the caveat recorded |
| clause 2: no D6 | no `no observed mapping interval intersects…` line anywhere in run3's window; `lmf_intervals_found=1` ≠ 0; the refusal fan-out (`lmf_vmentry_cap`/`lmf_vm_scan_cap`/`lmf_linkmap_cap`/`lmf_provider_code`/`lmf_identity_source`) is entirely ABSENT | MET |
| clause 3: NO-CRASH | `module_start_return=0` (seq 539); PATH_TEST stamped AFTER the refusal; buffered-Lua harness green; the game process survived (runs 1-3 share one boot, monotonic clock continuous); status carries a caught `failed` throw, not a crash tail (LoadMap-tail count 0) | MET |

**DECLARATION: run3 = MINIMAL-ACCEPT, CONFIRMED.** The controller's shorthand
("conversion pass + intervals>0 + diag events") is accurate for clauses 1-2; the one soft
point is E4, closed by the documented terminal-write truncation + the stage-3
reachability argument above. Belt-and-braces for launch-11: fetch runtime-status.txt
immediately on a failed run (or before the terminal write lands) so `activated` is
literally visible. Also confirmed incidentally: run1 met the same conversion clauses in
its sdiag (identical 7/1/6/extent values), so the geometry claim reproduced TWICE on real
boots; run3 additionally carries the fresh-session status.

Also verified on run3: **NO C12/C13** — no `lease coverage gap`, no `bridge_lease_gap_req`
/`bridge_lease_ref_*`/`near_*`, no `Owned read range includes unregistered bytes`
(the launch-9 killer stays dormant — r14 hardening held). **No REGRESSION markers**: no
B4 (`probe_lifecycle_result`=0), no Shape-A/Seeds/GMalloc lines, no DIAG-line or xwp
config drift, no W1-W7 walkguard strings (Phase F was never reached).

## 3. Predicted-pair verification (controller's anchor check)

Oracle D4/S2.2 predicted pair: `lmf_intervals_found=1` NEXT TO `bridge_lease_reg_spans=6`
(raw, uncoalesced), with `lmf_anchor_count=7`.

The classifier's SDIAGSUM (GAP-1 upgrade) surfaces them — run3 summary, verbatim:

```
supplier-diag jsonl: events=1591 pid_first=317 start_ok=0 start_failed=2 lease_spans=6 lease_reg=6 anchor_count=7 intervals_found=1 vmentry=10028 cfg_gamedir=1 (single-writer t_ms sequence above)
```

Raw lines underneath (run3-sdiag.jsonl, pid 355): `bridge_lease_reg_spans=6` (seq 502),
`bridge_lease_reg=6` (503), `bridge_cfg_gamedir=1` (530), `lmf_anchor_count=7` (536),
`lmf_intervals_found=1` (537). Run1's independent SDIAGSUM shows the same pair
(`events=548 … anchor_count=7 intervals_found=1 vmentry=22761 cfg_gamedir=1`).
**The predicted pair is CONFIRMED on real data, twice.**

Order caveat (honest record, no reorder applied): live emission order is lease_reg (502)
→ spanpub (504-508) → a SECOND capture walk (509-517) → anchors (518-529) → cfg → lmf —
the oracle S2.2 lists anchors (step 4) BEFORE lease_reg (step 5). First-seen order
preserved everywhere; values are what matter and they match.

## 4. What the NEXT run (pad-deferred candidate) should produce, per the oracle

The run-3 refusal names the pad provider (`phase=6, error=0x80920004`, 0x8092xxxx pad
error domain; exact symbol not pinned in the oracle). The controller's next candidate
defers the pad bridge. Oracle expectation for that run:

- Phases B-D replay exactly as run3: ctor set 235 green, wiring, `mfa_activated_low/high`
  (≈36564 pattern), walk stop=0, exepath fork source=0 resolved via `stat_ownmod=1`
  (observed live — see gap G8), spans=6/reg=6, spanpub set, anchors covered/matched +
  `anchor_c_either=1`, cfg=1/1, extent [0x400000,0xc0602c8), anchor_count=7,
  intervals_found=1, then **E3 `runtime_start_ok`** (no pad throw to stop it) and — with
  no terminal write — **E4 `state 1 activated 0` survives in status** for the first time.
- Phase F is the NEW territory: F1 armed marker `[Seeds] Required-slot seed table armed
  (slots 0/2/3/4)…`, F2 unbound-by-design, F6 slot 0 bound, F14 pool validated (v2
  "inside a published non-image readable span of <k>" per S2.3), F9 GMalloc `V1 instance
  + V2 vptr match`, F18 slot 2 behaviorally verified, F21 slot 4 bound, then F24-F28.
- Pinned expectation at `Finding GameEngine::LoadMap` (INI-F:1819, first guarded walk):
  **W1** `[WalkRxGuard] REFUSED: NO read attempted; walk text reads require the IGW RX
  verdict (LAUNCH10-MATRIX D2)…` in UE4SS.log + **W2** status failed
  `[WalkRxGuard] refused: guard_off_no_rx_verdict …` — the fold-0 refusal, NO read, zero
  syscalls, game survives. Per S1.2 that is a NAMED later-stage refusal ⇒ **verdict GOOD**
  (conversion+context clean, seeds past GMalloc V1/V2, W2 in clause 3, engine-ready
  absent). BEST is unreachable in this pass (F29→F34/F35 blocked by W2). If instead the
  walk READS (crash at the igf6 site) ⇒ FAIL/REGRESSION (unguarded unit shipped); any
  `anomaly_latched_earlier_restore_failed`/`rx_transition_denied` reason ⇒ REGRESSION
  (W3-W7: folded-ON object shipped).
- Capture note: fetch runtime-status.txt BEFORE any terminal write can truncate (or
  immediately on failure) so E4 `activated` is literally on file this time.

## 5. Classifier gaps found on REAL data (rehearsal was synthetic — report only, fixed nothing)

- **G1 — no E2/START-stage names in the verdict alphabet.** The pad refusals
  (`PS5Pad.ini must be ASCII…`, `PS5Pad input unavailable: phase=6, …`) and the loadlist
  size text fall through every `map_detail` pattern ⇒ STAGEHIT "failure line without a
  pinned detail string" and verdicts `FAILED_code3`/`FAILED_code-8`. The oracle classes
  these E2/B7 (EXPECTED-REFUSAL; E2 ⇒ verdict ≥ MINIMAL-ACCEPT). The classifier's raw
  verdict therefore UNDERSTATES the ladder position; a consumer must re-read against the
  oracle (as done here). Suggested (not applied): `FAILED_E2_START` / `FAILED_B7_MFA`
  patterns + a MINIMAL-ACCEPT state when the sdiag conversion quartet coexists with a
  code=3 refusal.
- **G2 — cumulative sdiag, no baseline cut / no per-pid split.** `classify_sdiag` reads
  the whole append-only file: run3's SDIAGSUM aggregates 1591 events across pids
  317/341/355; `pid_first=317` is STALE (current session is 355); `start_failed=2` mixes
  two sessions; the STIME sequence crosses session boundaries (+239,970 ms jumps), which
  contradicts the line's own "single-writer" framing. Last-write-wins for
  anchor_count/intervals/lease_spans/vmentry is only correct because run1 and run3
  happened to share values (7/1/6) — a divergent mid-session value would be silently
  overwritten by whichever pid emitted last. Per-session (pid-scoped) SDIAGSUM needed.
- **G3 — JSON tail under-represents sdiag evidence.** `stage_tags`, `bridge_cfg` and the
  observer counts count UE4SS.log-TEXT matches only (all 0 on these runs) while the sdiag
  carried 1591 events and the entire geometry chain; the JSON tail has NO
  anchor_count/intervals/lease_spans fields at all (text SDIAGSUM only). A JSON-only
  consumer would see "no staged markers" on the run that achieved MINIMAL-ACCEPT.
- **G4 — S11_STOP from `module_start_return`.** The chain renders
  `… -> S6_CONVERSION -> S11_STOP` on all three runs because `module_start_return=0`
  maps to S11_STOP; here it means "entry returned 0 after a loud refusal (game continues
  UNMODDED)", not a stop/unload. Display-granularity quirk, no misattribution of values.
- **G5 — fixture-name coupling.** `--from-dir` only recognizes the sdiag under the exact
  name `modbridge-supplier-diag.jsonl`; the evidence files are named `runN-sdiag.jsonl`
  (the task predicted this). Documented placement, not a code defect — but an operator
  trap worth a `--sdiag` alias eventually.
- **G6 — cumulative UE4SS.log windows.** Run2/run3 windows include PRIOR runs' refusal
  lines (failure_lines=3 on run3) and prior PATH_TEST pairs as "other new lines"; only
  failure lines carry r4 pid prefixes, so PATH_TEST stamps cannot be attributed to a
  specific run except by position. Verdict precedence (last failure line) landed
  correctly on all three runs — but correctness currently depends on the append-only
  ordering discipline holding.
- **G7 — capacity-bound question (oracle C1/S3 vs run1 live value).** run1's
  `bridge_vmentry_entries=22761` exceeds the pinned `kBridgeVmmapEntries=16384` while
  `bridge_vm_walk_stop=0` (clean end) — S3 predicts stop=2 for vm_total > 16384. Either
  the shipped capacity is larger than pinned, or the diag tag counts walked records
  rather than stored entries, or the truncation flag lives outside the diag set. Run3
  (the verdict run) is 10028 < 16384 with stop=0 — unaffected. Flagged for
  reconciliation; nothing on the verdict rides on it.
- **G8 — oracle C7 nuance (record against the row).** Live shows
  `bridge_exepath_source=0` WITH the r10 fork resolving successfully
  (`stat_ownmod=1`, `stat_ownmod_ec=0`) and conversion proceeding — C7's text reads
  source=0 as leading to refusal −1. The fork evidently refuses only when all three
  stats fail. Prediction-table wording to update at next oracle revision.
- **G9 — `bridge_spanpub_env_high` shape.** Live env pairs are
  low=4259840 (0x410000) / high=17 (run3) and low=1084030976 / high=16 (run1) — i.e. a
  64-bit low/high split, not "max captured record end" as a lone 32-bit reading of S2.2
  item 6 might suggest. `spanpub_cap=1` on both runs (16 published of total 119/127) is
  worth an oracle note too: saturation at kMaxPublishedSpans=16 occurred on BOTH boots.

## 6. Provenance

- Inputs (sha256-16): run1-ue4ss.log 4a4e0cf7280b66ff · run1-sdiag.jsonl d5ad9fa05e9bf4f7
  · run2-ue4ss.log e40bf151b3a921ad · run3-ue4ss.log 9aeeceeb709f9f04 · run3-sdiag.jsonl
  264ba150325dbe28 · run3-runtime-status.txt f5ba12265b77c918 · PS5Pad-sanitized.ini
  6ab119b3e8aef1d8 (2958 B) · loadlist-sizefixed.txt 54e3c2a0d3ba94f4.
- Baselines re-verified against pins: ue4ss edf404a5…/7591 B, entry 4b33b985…/2343756 B,
  status 57c40d4c…/169 B, xwp 20cff601…/4362 B.
- Classifier: `outputs/2026-10-04-zcode-team/worker-launch10-evidence-upgrade/evidence.sh`
  — executed unmodified from `--from-dir` fixtures; per-run full/summary/stderr preserved
  in `classifier-outputs/`. evidence.sh was run in host fixture mode only; no FTP, no
  console contact.
- Honest bounds: all evidence above is from the real console fetches supplied by the
  controller; the classifier was run locally; the rung declarations follow the oracle's
  own definitions. No verdict beyond MINIMAL-ACCEPT is claimed; no seed lines (Phase F)
  exist yet in any run's window; nothing here upgrades on re-reads of unchanged evidence.

## Post-verification scope corrections (controller, per user directive)
- TIMING SCOPE: the ~0.8s C_comp figure is measured for the CURRENT EARLY-REFUSAL path only. Full successful-startup timing and the ~71s inter-plugin gap remain UNMEASURED/UNEXPLAINED; both will be collected during the next NECESSARY run (no dedicated diagnostic launches).
- STATUS FILE SEMANTICS: runtime-status.txt is LATEST-STATE-ONLY (last-writer-wins by design). The authoritative activation/terminal evidence is the DURABLE per-build/run/PID events (the sdiag records + UE4SS mirror lines, which accumulate across epochs) — evidence reading correlates by pid and does NOT race the file. The earlier "fetch status before terminal write" caveat is superseded by this rule.
