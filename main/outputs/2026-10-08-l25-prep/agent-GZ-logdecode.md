# agent-GZ-logdecode — NUL-interleave decoder + decode-first verdict re-run (2026-10-08)

Mac-only, battery-safe: local tool edits + runs; no builds, no console
contact, no runtime-tree edits. Task: Claude's finding that the session
logs are NUL-interleaved (dual-path log writer) and plain grep over them
caused the GO #3 overclaim; directive = decode properly everywhere, re-run
everything retained, report every verdict change honestly.

## Deliverables (exactly these)

| file | what |
|---|---|
| `harvest/decode_session_log.py` | NEW decoder, grounded in the writer's actual grammar (module docstring cites the code sites); CLI in/out + `--stats/--json/--raw/--self-test`; 15/15 property tests PASS (synthetic interleavings incl. race-split orphan fragments + the real menu-up log containing BOTH `[EU-VMT] arming refused` and `[LUA-FN] in=` — the exact plain-grep miss pair) |
| `harvest/harvest.py` | PATCHED: `read_lines` decodes in-memory before ANY marker/verdict scan (single choke point covers the FTP-pull path, klog pickup, `--local-ue4ss`, markers.json scans); per-file decode stats in the report Provenance; `--raw` restores legacy behavior for diffing; raw files on disk never rewritten |
| `harvest/bind_launch.sh` | PATCHED: `--watch` ladder watcher decodes after download (second file; raw fetch preserved); `RAW=1` legacy |
| `harvest/fast_cycle.sh` | PATCHED: watch-mode `tr -d '\000'` hack replaced by the decoder (`normalize_log`); verdict mode inherits harvest.py's decode; `RAW=1` legacy |
| `LOG-REDECODE-VERDICTS.md` | The verdict-change report (per-log stats table + changes + claim cross-checks) |
| `parked/KI-2-logwriter-clean-utf8/PROPOSAL.md` | The clean-UTF-8 logwriter labelled-change PROPOSAL (not implemented) + console-verification plan; registered in `parked/NOTES-REGISTER.md` (append-only) |
| this report | — |

NOT touched (verified in-scope-only): `grep_markers.py` (PRX binary
scanner, UTF-8+UTF-16LE by design — task says session-log tools only),
`prep_modded_launch.py` (PRX/ini readbacks, no session-log parsing),
`symbolize_crash.py` (klog crash blocks, narrow kernel text), GS's gate
suites (host-generated logs only).

## The grammar (from the writer code — typescan-r4 original/candidate + CE FIX-PROPOSAL + live bytes)

- Narrow path: format literals/native args -> `PosixFile::write_string_to_file`
  -> `encode_utf8` -> clean UTF-8, never 0x00.
- Wide path: args through the vendored codecvt `std::wstring_convert`
  (PS5 libc++ 2-byte-wchar wrappers over 4-byte-unit helpers) get their
  bytes interleaved with U+0000 units -> on disk `T\0o\0w\0n`. Lua
  prints in 2026-10-08 builds + fmt args (paths/hashes/mangled names)
  even in the newest build.
- Race: unlocked shared fd -> narrow messages inside wide messages,
  displaced tails, orphan leading-NUL fragments.
- Corpus invariants (all 22 NUL logs): no double NUL, no edge NUL, every
  NUL preceded by ASCII. Decode = remove NULs (pair walk for honest
  stats; NUL-after-newline counted orphan, never a pair). Clean logs are
  a strict no-op (property-tested).

## Decoder stats — real menu-up log

`claude2-menu-session-20261010.log`: 156513 -> 152455 bytes, 4058 NULs =
4058 wide pairs, 808 wide runs, 0 orphans, 1111 lines (553 narrow / 558
wide / 558 mixed). Decoded output makes `[EU-VMT] arming refused:
non-text-ent (R3 carriers remain the pump)` contiguous-ASCII greppable
(raw contiguous grep: 0 hits — the miss class, reproduced then fixed).

## Verdict changes (full detail in LOG-REDECODE-VERDICTS.md)

- 9 logs CHANGE, all the same direction: `menu failure mode NONE ->
  SCAN-FAILED` (harvest-l25-B-final x15 lines, B-live1 x4, l25A-live1/2
  x5, l25A-live3 x5, l25B-live2/3 x4, l25B-live4 x13, l26-crash x1,
  zzdiag1 x2, zzdiag-v2-session x1). Every 2026-10-08-era spawner
  session's `Scan failed:` lines were wide-written; raw scans found ZERO.
  WEAKENED claims: HARVEST-l25-B-final.md:2501 and HARVEST-l25-B-live1.md:1961
  ("no menu-failure marker in this log" — false; the failures existed,
  wide-encoded). Overall menu-not-up conclusions unchanged (menu/spawn
  still NO). Marker `Loaded v1.0` 0->1 in those logs.
- 13 NUL logs + all clean logs: NO verdict change (incl. claude2,
  now3/v6/l26v2 zzdiag runs; diag-zz already NUL-stripped per line;
  zzdiag-v2 truncations are byte loss, decode can't and doesn't invent).

## GO #3 chain re-verified with the decoder

- `[EU-VMT] arming refused: non-text-ent ...` line 625 00:53:06 — VERIFIED
  (reason now readable). `[ST] pump vmt REFUSED` line 660 — VERIFIED.
  "pump vmt ARMED in NO retained console log" — re-verified 0 hits on
  the decoded corpus. `[R3] carrier armed [n: F7BD] @...AA8BD0 (id 1)`
  line 780 — VERIFIED (8 total). Row-2 line 561, row-4 timestamps, row-6
  pad_l3r3 jsonl events (36/stage) — all VERIFIED.
- CORRECTED: GO's "145 `[l22] ExecuteInGameThread queued/drained/callback
  returned` lines" -> decoded recount = **87** `[l22]` dispatch lines
  (39+24+24) + 1 `[l23]` self-test = 88 total; 145 is the count of ALL
  `[l22]` lines. Dispatch-pump conclusion stands; count was ~1.67x
  overstated.

## Regression gate

`host-tests/run-regression.sh` re-run post-patch: 9/10 suites GREEN
(incl. every suite that could touch Python tooling; none of the suites
execute the patched harvest tools). ONE RED: `mod-manager` PASS=47
FAIL=2 — PRE-EXISTING tree divergence, NOT caused by my patches:
T14 pins agent-FJ's reported `zzdiag-v11/main.lua` = 4084 B, disk has
4094 B (edited 16:40, before my session's first write 16:42; zero NULs).
No code path connects my patches to that suite. Per scope rules I did
not fudge the test nor edit zzdiag-v11; flagging for the v11 owner/root.
Side observation (pre-existing, unfixed, out of scope): `fast_cycle.sh
watch`'s rung regex matches "Spawned " inside the spawner's *Failed at
configure pickup ... Spawned pickup has no valid inventory ID* failure
line, firing a false RUNG HIT; visible in my decoded watch smoke run.

## Logwriter proposal (task 4) — parked/KI-2-logwriter-clean-utf8/PROPOSAL.md

What the writer emits today (two grammars + race, with code cites and
corpus counts), the minimal labelled change (F4a: LuaLibrary explicit
`Unicode::DecodeUtf8` codec; F4b: sink-side NUL strip inside the F1/F3
device lock in `PosixFile::write_string_to_file` + one-shot
`[ST] logwriter nul-strip active` receipt line), and the console
verification plan (decoder reports nul_count==0 as the receipt; decoded
vs `--raw` verdict identity as acceptance; orphan==0 race check;
rollback = remove the pre-pass). NOT implemented.
