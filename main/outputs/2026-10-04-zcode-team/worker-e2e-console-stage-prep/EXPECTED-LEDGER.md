# EXPECTED-LEDGER — MODBRIDGE_INST_E2E MODE B run, e2e-status.jsonl shapes 0..6

Target file: `/data/modbridge/state/e2e-status.jsonl` (read back per dispatch
by `evid` into `$EV/e2e-status.jsonl`; closing copy
`$EV/MODBRIDGE_INST_E2E-e2e-status.jsonl`). Every shape below is the
STAGED-E2E-COMMANDS.md §4 expected blocks VERBATIM (host-proven: the merged
chain replayed this exact dispatch set 35/35, transcript sha `31a4764a…`) with
the MODE B additions from CONFIG-ENTRY-DESIGN secs 2-4. Key order in every
line is fixed by `worker-f-r2-fix/owned/e2e_status.c` — byte order matters,
these are not semantic JSON.

## Line grammar (only these shapes may appear)

```
begin   {"event":"begin","cmd":"<verb>","title":"<title or "">","stage":"<stage-tag>"}
status  {"event":"status","stage":"<stage-tag>","outcome":"ok","detail":"<detail>"}
end     {"event":"end","exit":<int>}
refusal {"event":"status","stage":"e2e-cmd","outcome":"refused","detail":"<reason>"}   # MODE B refusals ONLY; zero in a green run
```

Stage mapping (schema §7, unchanged): `records`→`stage-0-baseline` (title ""),
`register-mod`→`stage-1-download`, `prepare`/`prepare-archive`→
`stage-2-prepare`, `install` (incl. removals)→`stage-3-install`,
`simulate-runtime`→`stage-4-5-runtime-ack`, `recover`→`recovery`.

Green-run totals: **35 new lines** — 0 unmatched begins, 0 failed ends, 0
`stage:"e2e-cmd"` lines, 0 refusals. Per dispatch (MODE B), additionally on
FTP: `intake/e2e-cmd.json` GONE, `intake/e2e-cmd.json.consumed` present and
sha-equal to the staged cmd bytes (`cmd_gone` proof; overwrite of a stale
`.consumed` is legal and expected).

## S0 — stage 0 baseline (records), dispatch 1 — 2 lines

```json
{"event":"begin","cmd":"records","title":"","stage":"stage-0-baseline"}
{"event":"end","exit":0}
```
records.txt: only the header `modforge-records	1`. This pair is also the
MODE B G1 verdict — it proves the config entry consumed a cmd file and drove
the verb on the re-pinned payload.

## S1 — stage 1-owner (register-mod), dispatch 2 — 2 lines

```json
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
```
records delta: new row `movspeed  PPSA33286  downloaded  /data/modbridge/intake/movspeed.zip`.

## S2a — stage 2a (prepare-archive, auto-bridge), dispatch 3 — 4 lines

```json
{"event":"begin","cmd":"prepare-archive","title":"PPSA33286","stage":"stage-2-prepare"}
{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"manifest-v1.bin"}
{"event":"status","stage":"stage-2-prepare","outcome":"ok","detail":"auto-bundle composed mfb"}
{"event":"end","exit":0}
```
Halt points: missing `auto-bundle composed mfb` with exit 0 = no recognized
roles — STOP, do not install; last line `{"event":"end","exit":2}` = refusal
path — STOP, rollback (DELE `movspeed.q`, `state/bundles/movspeed.mfb`, cmd
files), never replay the identical request.

## S2b — stage 2b bundle flow, dispatches 4-6 — 6 lines total

```json
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
{"event":"begin","cmd":"register-mod","title":"PPSA33286","stage":"stage-1-download"}
{"event":"end","exit":0}
{"event":"begin","cmd":"prepare","title":"PPSA33286","stage":"stage-2-prepare"}
{"event":"end","exit":0}
```
(2b-1 mm-skin register, 2b-2 mm-lua register, 2b-3 prepare of both.) records:
mm-skin/mm-lua rows `prepared`, bundle columns at their intake paths.

## S3 — stage 3 transactional install, dispatch 7 — 3 lines

```json
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin,mm-lua"}
{"event":"end","exit":0}
```
Precondition: game NOT running (C4). records: three rows `installed`; game row
generation 1 with loadlist sha (the only commit truth); root
`/data/modbridge/roots/PPSA33286/` gains `.modforge-root`, `loadlist.txt`,
staged files; journal gone after commit.

Interrupted-transaction recovery (ONLY if the last end line is absent or a
journal remains): one `recover.json` dispatch —
```json
{"event":"begin","cmd":"recover","title":"","stage":"recovery"}
{"event":"end","exit":0}
```

## S4-S5 — simulated runtime acks, dispatches 8-11 — 8 lines total

Per simulate dispatch (×2):
```json
{"event":"begin","cmd":"simulate-runtime","title":"PPSA33286","stage":"stage-4-5-runtime-ack"}
{"event":"end","exit":0}
```
Per interleaved records dispatch (×2): the S0 pair again.
records deltas: `activated`/running=0, then `mods-started`/running=1.
**SIMULATED ack-file proof — NOT a real runtime acceptance. Label every
derived report SIMULATED.**

## S6 — stage 6 uninstall (order fixed mm-lua → mm-skin → movspeed), dispatches 12-14 + closing records — 8 + 2 lines

CORRECTION vs the dry doc (STAGED §4.6): removals DO emit the F-R2 status
line WHILE MODS REMAIN, detail = remaining mods in registration order; only
the final removal is silent.

6-1 (remove mm-lua):
```json
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed,mm-skin"}
{"event":"end","exit":0}
```
6-2 (remove mm-skin):
```json
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"status","stage":"stage-3-install","outcome":"ok","detail":"movspeed"}
{"event":"end","exit":0}
```
6-3 (remove movspeed) — silent status, no line between begin/end:
```json
{"event":"begin","cmd":"install","title":"PPSA33286","stage":"stage-3-install"}
{"event":"end","exit":0}
```
Closing records dispatch: the S0 pair once more.
records: generations advance 2, 3, 4; each removed mod drops to `prepared`;
after the last removal the root is the empty managed set (`.modforge-root` +
header-only `loadlist.txt`), game row runtime resets.

## §F Failure table — malformed/missing shapes and recovery

Exit-code taxonomy first (CONFIG-ENTRY-DESIGN sec 2; the EX_CONFIG 6 delta is
controller-acked, schema §6/§8 still say EX_BAD):

| situation | exit | status file | cmd file |
|---|---|---|---|
| dispatch ok | 0 | begin…end pair(s) | consumed (`.consumed` hash-equal) |
| file absent at slot (bare dispatch, no fresh STOR) | 2 EX_BAD | NOTHING (stdout `error bad_request e2e-cmd: file absent` + usage only) | n/a |
| file present but refused (BOM, oversize >4096, non-UTF-8, parse, unknown key, bad type, deny-listed verb/option, allow-list violation) | **6 EX_CONFIG (delta)** | if `state_root` validated: exactly ONE `{"event":"status","stage":"e2e-cmd","outcome":"refused","detail":"<reason>"}`; parse-level failures (BOM/oversize/UTF-8/parse/schema/verb/state_root): stdout only | LEFT IN PLACE for inspection |
| consume-rename failure | 2 EX_BAD | NOTHING (`error e2e-cmd consume failed` + usage on stdout) | stays as `e2e-cmd.json` (unconsumed) |

No begin line is EVER emitted on a refusal; refusals never mkdir the state dir
(so an early refusal may fail to write its one-liner silently — bounded emit
semantics). Recovery for any refusal: STOP; FTP-DELE the refused cmd file from
`intake/` ONLY after recording its bytes + the reason; fix host-side with a
NEW distinct file/revision; never replay the identical request.

| # | observation (shape) | meaning | recovery |
|---|---|---|---|
| F1 | stage-0 dispatch: no new lines, no status file at all | bare dispatch did not reach the entry OR state dir unwritable | two bare dispatches with no status file ⇒ staged payload is NOT the config-entry revision — corrupted staging. STOP; re-do §3/§4 identity gate (`RB_SHA` must be `a21771f8…`); never widen the gate |
| F2 | `cmd_gone` prints "NOT CONSUMED" (e2e-cmd.json still present), no refusal line | dispatch never ran the entry (manager-level failure, wrong URL, or process died before entry) | check `list_payloads` + manager 200; reconcile before ANY re-dispatch (no duplicate Send; ambiguous attempts get reconciled, not replayed) |
| F3 | `cmd_gone` consumed-hash != last cmdfiles.log line | a DIFFERENT file drove the dispatch (stale/overwritten cmd) | STOP; record both hashes; DELE `e2e-cmd.json` + `.consumed` after capturing bytes; re-stage the correct file with the `cmd_put` stale guard; then re-dispatch ONCE |
| F4 | one `stage:"e2e-cmd"` refusal line + exit 6 | file present but refused (see taxonomy) — verb never executed, no store mutation | record detail reason + file bytes; DELE the refused file; correct host-side (new file bytes); never auto-retry |
| F5 | exit 2 with `file absent`/`consume failed` stdout but a cmd file IS present | consume-rename failed (entry saw it, could not rename) | filesystem-level: re-check intake dir via FTP (permissions, stale `.consumed`); do NOT re-dispatch until reconciled |
| F6 | S2a: exit 0 but `auto-bundle composed mfb` line missing | archive had no recognized roles — nothing to install | STOP before install; keep records; report distinctly (corpus problem, not payload problem) |
| F7 | any stage: `end` with exit != 0 (2 on refusal-adjacent paths, 2 = refusal path in S2a) | refusal/failure path — stage did not complete | S2a refusal: rollback per S2A halt points. Others: STOP, collect, report distinctly; rollback owned tree; never replay the identical request |
| F8 | begin present with NO matching `end` (process died mid-verb) | crash/interrupt mid-dispatch | if during S3/S6 (transactional): run the ONE `recover.json` dispatch, expect begin recover / end 0; then reassess from records.txt. Else STOP + collect |
| F9 | S3 interrupted: journal still present or last end absent | transaction did not commit | `recover.json` once (expected pair above). Rollback for a COMPLETED install is stage 6 itself, NOT a reset |
| F10 | game running detected at S3 | precondition C4 violated | STOP immediately; do NOT retry live; defer remainder to a payload-only window; rollback = full-tree reset (abort-before-commit only) |
| F11 | S6-1/S6-2: status line MISSING (dry-doc behavior) or detail != exactly the remaining mods in registration order | stage-6 CORRECTION violated — chain behavior diverges from host proof | STOP; collect full file; report distinctly; do not proceed to the next removal |
| F12 | line count != the per-stage counts (2/2/4/6/3/8/8+2) or total != 35 | extra/missing events — schema drift or unmodeled behavior | STOP; diff against EXPECTED-LEDGER shapes; collect; report distinctly |
| F13 | `cmd_put` "STALE e2e-cmd.json — DELE it, do not overwrite" | leftover unconsumed/refused cmd file from an earlier attempt | capture bytes + reason first, then DELE it and its `.consumed`; only then STOR the next file |
| F14 | upload readback `RB_SHA`/`RB_SZ` mismatch | corrupted staging (or wrong file uploaded) | NO-GO: STOP; re-upload ONCE to the SAME alias; if it mismatches again, stop and record — never dispatch divergent bytes |
| F15 | manager unresponsive / `list_payloads` != 200 | console not in payload-only runnable state | STOP all console steps; no probing of alternate ports (8084 is pinned; 9223 is a doc erratum) |

## Disposition rules

- All shapes match: stage PASSED; write the receipt JSON (per CONSOLE-STAGE-PLAN
  §6), preserve copies, run the full-tree reset, hand the console back
  payload-only to the UE4SS critical path.
- Any mismatch: freeze `$EV` copies + receipt with the exact divergent lines;
  no re-dispatch, no reset decision mid-session; reconciliation happens
  host-side first. Console evidence is never re-rated unchanged.
