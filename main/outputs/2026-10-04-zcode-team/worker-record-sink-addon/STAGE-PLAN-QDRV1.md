# STAGE-PLAN-QDRV1 — staging the record-sink add-on alongside composition-5

Worker: worker-record-sink-addon, 2026-10-04. Proposal for the controller;
NO console contact was made. Read together with DESIGN.md section 8 (risks).

## 1. ploader.ini entry (=2 alongside the composition)

The loader stages modules from `/data/plugins/` via the ploader ini module
list. This add-on is an INDEPENDENT late-load module: it adds its own entry
as the next free index (referred to as `=2` for the two-module layout) and
does NOT touch the composition entry:

```ini
[modules]
1=/data/plugins/modbridge_supplier_composition.prx
2=/data/plugins/modbridge_qh_driver.prx
```

- File to stage: `modbridge_qh_driver.prx`
  (this dir; sha256
  `52c3221665a423ddd2b22faddd4ac59ea8fd0ab8c1880c61e225d32a43df70a4`,
  26157 bytes; verify the hash before replacing any owned file).
- Own identity: module/export library `modbridge_qh_driver` (the length-
  preserving post-packaging patch; see DESIGN.md 6) — distinct from the
  composition (`modbridge_supplier_composition`), the aligned-entry module,
  the observer, and the igf probe family. Safe to stage ALONGSIDE all of
  them.
- Channel: `/data/modbridge-ingame-facts.jsonl` — THE pinned C2 path
  (`record_sink.c:15`), opened O_APPEND|O_CREAT 0600. If the igf6 one-shot
  probe (`modbridge_ingame_facts.prx`) is currently staged on the same ini,
  consider unstaging it for the rung window (its job is done; this removes
  the only other C2 writer — every line remains self-contained JSON either
  way).
- Load-order independence: the add-on does not resolve any symbol of the
  composition; it only reads engine memory via pinned runtime addresses and
  writes its own channel. It works whether the composition reaches
  engine-ready or fails.

## 2. Expected behavior after launch (timeline)

Cadence: 30 polls x 4 s sleep, started at the add-on's module_start
(ploader load time, i.e. BEFORE engine-ready). Per poll (see DESIGN 4-5):

| Window (relative to module_start) | Expected C2 content |
|---|---|
| t=0 | seq1 `entry` + seq2 `startup` + seq3 `mapscan` (mapscan `trusted` may be 0..1 honestly) |
| each poll | one ig envelope `qh_counter` line: `admission_rcode` 1..7 while the engine window is unproven/ambiguous, 0 once the GUA header admits |
| first admitted poll +1 | `refused_bindings_absent` >= 1 recorded (wiring lags one poll BY DESIGN) |
| next 6 polls | `refused_layout_gate_closed` counted while the count rides above 10000 |
| stability met (~poll 7-10) | gate armed; first armed dispatch: `ticks_accepted` 0→1 (staged) |
| next poll | `ticks_accepted` 1→2 AND the standalone `{"record":"native_query_harness",...}` terminal line (status completed) |
| remaining polls | counter lines continue, `ticks_accepted` frozen at 2, `qh_record_emitted=1` |
| t+~2 min | `summary` line, thread exits, channel closed |

Expected terminal record (the pinned FormatRecordJsonLine shape):

```json
{"record":"native_query_harness","magic":"NQH1","status":"completed",
 "engine":"5.6","axes":{"non_shipping":0,"stats":0},"storage":"chunked_array",
 "count_source":"engine_uobject_array","game_thread_id_key":"0x<16 lowercase hex>",
 "counts":{"first":<GUA NumElements>,"second":<GUA NumElements>,"recorded":2},
 "delta":0,"monotonic":"equal","identity_checks":2,"threshold":10000,
 "ticks":{"dispatched":<N>,"accepted":2,"refusals_between":0},
 "gate_source":"igu_layout_a_live_header"}
```

counter envelope line shape (one per poll):

```json
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"qdrv1-c20c70ec4112",
 "seq":N,"exp":"qh_counter","rc":0,"errno":0,"fields":{
  "poll":P,"phase":"bindings_absent|gate_closed|armed|terminal",
  "map_trusted":0|1,"admitted":0|1,"admission_rcode":0..7,
  "gua_num_elements":...,"gua_max_elements":...,"gua_max_chunks":...,
  "gua_num_chunks":...,"gua_first_gc":...,"gua_last_non_gc":...,
  "ticks_dispatched":D,"ticks_accepted":A,
  "refused_bindings_absent":B,"refused_layout_gate_closed":G,
  "refused_identity_unknown":I,"refused_wrong_thread":W,
  "staged_resets":S,"qh_record_emitted":0|1}}
```

## 3. rung_check timing guidance

RUNG1's `ticks_accepted` strictly-increasing condition needs the two C2
snapshots to straddle the 0→1→2 ramp. The ramp is an 8 s window (two
accepted polls 4 s apart) opening once the admission gate stabilizes —
realistically 30-60 s after module_start (the engine's UObjectArray must
exist and pass the header-window admission). Records are FTP-fetchable from
launch (~1 min); the controller should:

1. wait for runtime-status `engine-ready` (the composition ladder),
2. run `rung_check.py` (default 3 s gap) and re-run every ~20-30 s;
3. a PASS requires a run whose snapshots straddle the ramp — expect this
   within the first few re-runs; AFTER the ramp the file freezes (terminal)
   and later runs honestly report ticks-flat FAIL. Never re-rate unchanged
   evidence; a new window needs a NEW launch.
4. `--discover` is the fallback if C2 itself is missing.

Expected rung-1 conditions from this channel: status engine-ready-or-later
(composition), c2_present, ticks ramp (window above), exactly ONE distinct
non-zero `game_thread_id_key` (the driver fold; two snapshots never see two
keys), >= 1 counted guard refusal (both `refused_bindings_absent` and
`refused_layout_gate_closed` expected), driver line seen.

Expected rung-2: the terminal record above (counts > 10000 from the live
GUA; monotonic "equal" or "non_decreasing" — a genuine decrease between the
two accepted polls honestly FAILs rung-2; do not re-rate).

## 4. Honesty boundaries for acceptance

- A rung-1/2 PASS from this channel evidences the RECORD PATH (seqlock
  record → FormatRecordJsonLine → IgSink JSONL) and REAL engine GUA count
  observations. It does NOT evidence execution on the engine's OS game
  thread (the add-on's driver thread dispatches; DESIGN.md 1.4). The
  in-game-thread claim stays with the composition's WIRING.md persistent-
  callback route in a later composition rev.
- `game_thread_id_key` values from this channel are driver-thread folds;
  do not correlate them with UE4SS-published game-thread identities.
- `engine/axes/gate_source` in the record are labeled add-on constants
  (`igu_layout_a_live_header`), not the composition's S2 scan facts.
