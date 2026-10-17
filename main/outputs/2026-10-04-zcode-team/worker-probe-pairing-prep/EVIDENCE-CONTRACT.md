# EVIDENCE-CONTRACT — what the paired probe window yields, line by line

Worker: worker-probe-pairing-prep · 2026-10-05 · PROPOSAL ONLY. The record
grammars below are the two probes' pinned machinery (host-tested with mocked
syscalls; the xwp shapes are corroborated by launch-9's REAL 15-record
capture, `outputs/2026-10-04-zcode-controller/launch9-evidence/xwp-jsonl-final.jsonl`).
Which verdict the console actually returns is a TARGET-only fact until the
window runs. No outcome is predicted; every shape is specified so the fetched
records can be classified without interpretation drift.

## 0. The combined window yields exactly two new evidence classes

1. **The IGW row-0 verdict** (`W_WORKS` / `W_FAILS_R_WORKS`, or the fail-closed
   refusal/absent classes) — the decision input for the walkguard fold
   (`RC_ASM_WALK_RX_GUARD` 0→1 admissible ONLY on success; the D-1-only route
   on `W_FAILS_R_WORKS`), i.e. LAUNCH11-MATRIX dimension **D2** for rows
   L11-1..L11-5.
2. **The FIRST REAL `Blocks[0]` capture** — resolving the
   candidate/synthetic label (worker-blocks0-provenance verdict: NO CAPTURE
   EXISTS; the image-span figure is mapping-class only) and calibrating
   validator v4's span-membership-either-class admission before launch-12
   arms it, i.e. LAUNCH11-MATRIX dimension **D3** (`VALIDATED`/`REFUSED`/
   `GRACE`/`ABSENT`).

Neither probe consumes the other's output; the pairing halves the launch cost
of these two orthogonal facts and preserves evidence-before ordering.

## 1. Probe 1 — `/data/modbridge-exec-write-probe.jsonl` (xwp rebuild)

Envelope (pinned `record_sink.c`):
`{"marker":"modbridge.execwriteprobe","schema":1,"build_id":"igd1-e030a09eb4d1","seq":N,"exp":"…","rc":<int>,"errno":<int>,"fields":{…}}`

Expected chain: **19 records** on the row-0-attempted path (launch-9's
confirmed=0 build produced 15; the rebuild's ONE semantic byte — row 0
`confirmed` 0→1 — replaces the row-0 `row`+`refusal` pair with the full
6-record attempt chain and shifts seq).

seq 1 `entry`:
```json
{"marker":"modbridge.execwriteprobe","schema":1,"build_id":"igd1-e030a09eb4d1","seq":1,"exp":"entry","rc":0,"errno":0,"fields":{"phase":"module_start","note":"exec-write probe skeleton; transition table drives everything; no write before the design's conf","jsonl_path":"/data/modbridge-exec-write-probe.jsonl","mirror_path":"/data/plugins/UE4SS.log","mirror_open_errno":0,"armed":1,"table_rows":6}}
```
seq 2 `startup` (budget + channels):
```json
…"seq":2,"exp":"startup","rc":0,"errno":0,"fields":{"phase":"post_init_array","ctors_run":2,"channel_open_rc":0,"channel_open_errno":0,"mirror_open_rc":0,"mirror_open_errno":0,"entry_marker_written":1,"write_failures_so_far":0,"armed":1,"table_rows":6,"attempt_budget":8}
```
seq 3 `row` (row 0, now CONFIRMED — the rebuild's delta):
```json
…"seq":3,"exp":"row","rc":0,"errno":0,"fields":{"row_index":0,"addr":"0x0000000007878000","from_prot":"0x0000000000000004","to_prot":"0x0000000000000005","page_count":1,"confirmed":1,"note":"IGW-1 T2: X->RX far page (past extent 0x7879b1c); read-test; restore follows","attempts_used":0}
```
seq 4 `vmmap_persist` (pre-state proof): `"exp":"vmmap_persist"` with
`span_start/span_end/span_prot` (span_prot `0x4` expected — pre-state), `attempt_addr:"0x…7878000"`, `attempt_len`, `walk_trusted:1`.
seq 5 `attempt` — **THE number** (`rc` carries the mprotect result):
```json
…"seq":5,"exp":"attempt","rc":0,"errno":0,"fields":{"row_index":0,"attempt_index":0,"addr":"0x…7878000","requested_prot":"0x…5","mprotect_route":"sceKernelMprotect_import"}}
```
seq 6 `rw_test`: `first_byte` (hex), `read_done:1`, `write_tested:0` (to_prot 0x5 has no W bit), `write_ok:0`, `recovered:0`.
seq 7 `restore`: `rc` = restore result, `restored_prot:"0x…4"`; success path rc==0 (outcome 10).
seq 8 `post_restore`: `post_walk_trusted`, `post_covered:1`, `post_observed_prot:"0x…4"`, `restored_matches_from_prot:1`.
seq 9–18: rows 1–5 as `row` (`"confirmed":0`) + `refusal` pairs —
`"exp":"refusal"` with `"outcome":2` (`XWP_SKIP_NOT_CONFIRMED`), `"no_memory_touched":1`, zero attempts (grammar proven by launch-9 seqs 5–14).
seq 19 `summary`:
```json
…"exp":"summary","rc":0,"errno":0,"fields":{"phase":"exec_write_probe_done","rows_seen":6,"rows_attempted":1,"rows_refused":0,"rows_skipped":5,"attempts_used":1,"attempt_budget":8,"budget_exhausted":0,"mirror_failures":<0 expected>,"armed":1,"mprotect_route":"sceKernelMprotect_import"}}
```

### Verdict shapes (classify the fetched seq 5 + seq 7/8)

| fetched shape | verdict | meaning |
|---|---|---|
| seq5 `rc==0` (outcome 8 `XWP_ATTEMPT_OK`) + seq6 `read_done:1` + seq7 rc==0 + seq8 `restored_matches_from_prot:1` | **`W_WORKS`** | the imported mprotect route flips the exec page on this firmware/composition; walkguard ACTIVE fold (`RC_ASM_WALK_RX_GUARD=1`) becomes admissible per WST-5 fold discipline |
| seq5 `rc<0` + recorded errno (outcome 7 `XWP_ATTEMPT_MPROTECT_FAIL`), seq7 restore still re-proves 0x4 | **`W_FAILS_R_WORKS`** | kernel-enforced exec protection; R-half confirmed, W-half not; D-1-only route (its own NEW design; this table's rows 4/5 stay `confirmed=0`) |
| seq4 `vmmap_persist` absent → `refusal` outcome 5 (`XWP_REFUSE_NOT_COVERED`) or 6 (`XWP_REFUSE_PROT_MISMATCH`), `attempts_used:0` | fail-closed refusal | the live map differs from the design pin; reconcile, never re-rate |
| no xwp jsonl at all | **`ABSENT`** | probe did not load (ini/file problem — the pre-image sha in the staging receipt proves which); no memory touched |
| a seq missing mid-chain | **`FATAL`** (LAUNCH11-MATRIX L11-8 override) | preserve first, never replay |

UE4SS.log: the mirror appends a write-through copy of every record — a
bounded block of `modbridge.execwriteprobe` JSON lines for this launch,
strictly between the loader banner and fc1pool's `FC1POOL:` line (file order
1→2).

## 2. Probe 2 — `/data/modbridge-fc1pool-facts.jsonl` (fc1pool)

Envelope (pinned `record_sink.c`):
`{"marker":"modbridge.ingamefacts","schema":1,"build_id":"fc1pool1-78508a2e2940","seq":N,"exp":"…","rc":0,"errno":0,"fields":{…}}`
Hex kv values are `"0x"` + 16 lowercase hex digits, quoted.

Expected chain: **6 records** on the PASS path (seq numbering starts at 3 in
the driver; entry/startup come from `module_entry.c`).

seq 1 `entry`:
```json
{"marker":"modbridge.ingamefacts","schema":1,"build_id":"fc1pool1-78508a2e2940","seq":1,"exp":"entry","rc":0,"errno":0,"fields":{"phase":"module_start","note":"fc1pool data-side FName-pool validator; entry precedes all checks"}}
```
seq 2 `startup`: `phase:"post_init_array"`, `ctors_run`, `channel_open_rc:0`, `channel_open_errno:0`, `entry_marker_written:1`, `write_failures_so_far:0`.
seq 3 `poll` (per attempted poll): `poll:0`, `phase:"anchor_admitted"`, `map_trusted:1`, `anchor_rcode:0`, `base_derived:"0x0000000000400000"`, `pool_runtime:"0x00000009765ec0"`, `base_matches_observed_bias:1` (the real-launch prediction; a divergent base is reported and validated, never guessed through).
seq 4 `fc1pool_result` — **THE CAPTURE RECORD** (PASS shape):
```json
…"seq":4,"exp":"fc1pool_result","rc":0,"errno":0,"fields":{"poll":0,"verdict":"validated","reason_code":"none","fc1pool_validated":1,"map_trusted":1,"anchor_admitted":1,"anchor_rcode":0,"base_derived":"0x0000000000400000","base_matches_observed_bias":1,"pool_runtime":"0x00000009765ec0","guard_byte":<u32>,"guard_byte_read":1,"current_block":<n≥0>,"current_byte_offset":<n>,"blocks0":"0x<16 hex — THE REAL Blocks[0]>","blocks0_canonical":1,"id_none":<u32, expected 0>,"id_none_is_zero_expected":1,"id_byteproperty":<u32 nonzero>,"byteprop_id_for_find_crosscheck":<same u32>,"checks_passed":11,"checks_total":11,"ue4ss_log_line_written":1}
```
seq 5 `fc1pool_entries` (raw evidence for offline re-derivation):
`pool_window_start/pool_window_end/pool_window_prot:3`, `blockptr_none`, `entry_addr_none`, `header_none`, `bytes16_none` (32-hex), `none_ascii_ok:1`, `blockptr_byteproperty`, `entry_addr_byteproperty`, `header_byteproperty`, `bytes16_byteproperty`, `byteproperty_ascii_ok:1`.
seq 6 `summary`: `polls_run`, `validated:1`, `write_failures:0`, `ue4ss_log_line_written:1`, `ue4ss_log_line_rc:0`.

UE4SS.log — exactly ONE new line this launch:
```
FC1POOL: build=fc1pool1-78508a2e2940 verdict=validated reason=none base=0x0000000000400000 pool=0x00000009765ec0 cb=<n> none=<n> byteprop=<n> checks=11/11
```

### Refusal shapes (each is a recorded TERMINAL verdict, `verdict:"refused"` hard / `"refused_after_grace"` transient at `polls_run:8`)

- hard: `pool_window_uncovered`, `pool_window_not_readable`, `current_block_insane` (CurrentBlock > 8192), `blocks0_invalid` / `blocks0_uncovered` (Blocks[0] zero/non-canonical/unmapped), `handles_window_uncovered`, `bootstrap_ids_degenerate`, `block_pointer_invalid` / `block_pointer_window_uncovered`, `entry_window_uncovered`, `entry_wide_flag_set`, `none_ascii_mismatch` / `byteproperty_ascii_mismatch` (bytes16 hex carries the observed bytes).
- grace: `map_untrusted`, `anchor_not_admitted`, `pool_uninitialized_after_grace` (CurrentBlock stayed 0 — remedy is a NEW launch window with the engine fully booted, never a re-rating).
- On ANY refusal the `blocks0`/`current_block` fields carry whatever was actually read (zeros where never reached) — still evidence, never replayed.
- `ABSENT`: no fc1pool jsonl and no `FC1POOL:` line → not loaded (ini/file problem).

## 3. What the capture resolves — the labeling + calibration consequences

1. **candidate/synthetic label resolved by reality, not argument.**
   - `blocks0` inside the image RW span (`[0x970c000,0x9a34000)` class, prot 3): the SYNTHETIC candidate-image-resident fixture class (V1/V1b/END-TO-END/H6) gets its REAL anchor — the label upgrades from candidate/synthetic to captured for that launch, with build id + session provenance.
   - `blocks0` outside the image, covered by a readable span: the heap-resident class (A8/H3, static-RE predicted `FMemory::Malloc` → GMalloc binned → OS heap) wins; the image-resident candidate stays synthetic and is now KNOWN synthetic.
   - Either way the ambiguity that forced v4's either-class admission is closed by the first real value.
2. **Validator v4 calibration for launch-12.** The record directly exercises v4's changed predicates against reality before the initializer arms it:
   - R1 invariant `(none_id >> 16) <= current_block` — checkable from `id_none` + `current_block` (v3's `current_block >= 1` false-refusal shape is directly falsifiable if `current_block == 0` appears WITH valid ids/entries);
   - R2 span-membership either-class admission — which class the real `blocks0` lands in;
   - R3 None-anchor content identity — `bytes16_none` (`header_none` bit tests `h>>6==4`, `h&1==0`, payload `'None'`) is the same evidence v4's `check_none_anchor` consumes;
   - the 8-alignment + nonzero `blocks0` predicates (kept exactly in v4) get their first real instance.
   - `byteprop_id_for_find_crosscheck` pins the data-side id the initializer's later behavioral `FNAME_Find("ByteProperty")` must equal.
3. **L11 matrix inputs.** D2 = the §1 verdict; D3 = `VALIDATED` (11/11 + crosscheck) / `REFUSED(<reason>)` (pool-side → L11-7 hold) / `GRACE` (→ GRACE note: NEW window, no composition change) / `ABSENT` (collapse rule). The consistency rules and row lookup then run exactly as LAUNCH11-MATRIX §1/§2 prescribes.

## 4. Cross-checks that make the window self-validating

- xwp seq1 `build_id` MUST be `igd1-e030a09eb4d1` — if it reads `igd1-6ebab07f2111` the WRONG BUILD was staged (hazard H-1): the window's IGW evidence is launch-9-class all-skip, and the staging receipts must be reconciled before any further exec-page work.
- fc1pool seq1 `build_id` MUST be `fc1pool1-78508a2e2940`.
- PATH_TEST terminator: fresh `PATH_TEST:` + `ACTIVATION_TEST:` lines prove full-list traversal (3/3 under Option P; 8/8 under Option A).
- ploader notifications: exactly 3 `OK (<ms>): <file>` lines (Option P) / 8 (Option A), ZERO FAIL; any FAIL → STOP + rollback path.
- Ordering in UE4SS.log: xwp mirror block, then the single `FC1POOL:` line, then `PATH_TEST:`/`ACTIVATION_TEST:` last — file order 1→2→3 made visible.
- Evidence channel pre-conditions: `/data/modbridge-fc1pool-facts.jsonl` opens `O_APPEND` — old lines persist harmlessly; the Step-0 stat baseline (size+sha) separates this launch's lines. Same discipline for the xwp jsonl.

## 5. Honest limits

- All shapes are pinned machinery + host tests + launch-9's real xwp grammar; NO console measurement exists yet for either probe in this configuration (the rebuild has never executed on target; fc1pool has never been injected at all).
- The real-launch prediction base=0x400000 / pool=0x9765ec0 / bias-match=1 is target-only; a divergent base is a recorded outcome, not a failure of the contract.
- No verdict is claimed in advance; `W_WORKS`/`W_FAILS_R_WORKS`/refusals are all acceptable evidence classes with pre-agreed consequences (§1 table, §3).
