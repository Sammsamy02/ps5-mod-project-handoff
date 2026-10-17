# LOG-REDECODE-VERDICTS — NUL-interleaved session-log re-analysis (agent GZ, 2026-10-08)

Claude's finding, verbatim directive: the retained session logs are
NUL-INTERLEAVED (dual-path log writer), plain grep over them is how an
overclaim slipped through, so "make every harvest/verdict tool decode the
log properly (no plain grep), then re-run them over all retained session
logs and report any verdict that changes."

This is that re-run. Tooling: `harvest/decode_session_log.py` (new,
agent GZ) — grounded in the writer code (grammar below), 15/15 property
tests PASS including the exact plain-grep miss pair. `harvest/harvest.py`
now decodes in-memory before every marker/verdict scan (`--raw` restores
the legacy behavior for diffing; raw files on disk are NEVER rewritten).
`harvest/bind_launch.sh --watch` and `harvest/fast_cycle.sh watch` decode
after download before grepping (`RAW=1` restores legacy). `grep_markers.py`
was NOT touched: it scans PRX binaries (UTF-8 + UTF-16LE by design), not
session logs. GS's regression gate greps host-generated suite outputs only
(no console session logs anywhere in its suites) — unaffected; gate
re-run after the patches: see §5.

## 1. The grammar the decoder implements (from the writer code, not guessed)

- Narrow path (format literals, native args, `[l22]`/`[L25]`/`[Seeds]`/
  `[EU-VMT]` prefixes, newlines): `Output::send(STR("..."))` ->
  NewFileDevice (single fd, O_APPEND) -> `PosixFile::write_string_to_file`
  -> `encode_utf8` -> clean UTF-8, NEVER contains 0x00
  (outputs/2026-10-08-l26-runtime/typescan-r4/original/src/File/FileType/
  PosixFile.cpp:293-299, :138-156).
- Wide path (args converted via `to_generic_string`/`ensure_str` ->
  vendored codecvt; PS5 libc++ locale pairs 2-byte-wchar wrappers with
  4-byte-unit helpers -> "unit-doubling"): the converted StringType
  carries the original bytes interleaved with U+0000 units, so ASCII text
  lands on disk as `T\0o\0w\0n` (staged-fix comment port/first/include/
  Helpers/String.hpp:15-22; outputs/2026-10-08-l26-runtime/logwriter-
  proposal/FIX-PROPOSAL.md section 2 item 3, host-reproduced by
  race_repro.cpp). Lua `print` payloads (`[Lua] ...`, zzdiag `[zz]`,
  TownfallItemSpawner prints) went through this path in the 2026-10-08
  builds; the l26-20261010T043709Z build (F4 codec fix) emits them narrow.
- Race overlay: the two writer threads share the unlocked fd, so narrow
  messages land INSIDE wide messages — mid-line switches, displaced tails,
  orphan leading-NUL fragments (FIX-PROPOSAL section 2 items 1-2).
- Corpus invariants (verified over all 22 NUL-carrying retained logs): no
  double NULs, no file-leading/trailing NUL, every byte preceding a NUL is
  ASCII (< 0x80). Decode rule: NUL bytes exist only as wide-run high
  halves (or orphaned race-split halves) — remove every NUL, preserve
  everything else byte-for-byte. A NUL after a newline is counted as an
  orphan (never a wide pair: line breaks are narrow-path bytes) so the
  stats stay honest. Clean logs are a strict no-op.

## 2. Decoder stats over the retained corpus (every NUL-carrying .log under evidence/ + harvest/, recursive)

| log | bytes in->out | NULs | wide pairs | orphan NULs | lines (narrow/wide/mixed) |
|---|---|---|---|---|---|
| evidence/claude2-menu-session-20261010.log | 156513->152455 | 4058 | 4058 | 0 | 1111 (553/558/558) |
| evidence/readfix/session-full.log | 124723->121057 | 3666 | 3666 | 0 | 750 (230/520/520) |
| evidence/zzdiag-v2-session.log | 132874->128565 | 4309 | 4306 | 3 | 818 (264/554/554) + partial final line |
| evidence/tmp-preserved-20261010/tmp/bind1.log | 111823->108216 | 3607 | 3607 | 0 | 563 (44/519/519) |
| evidence/tmp-preserved-20261010/tmp/claude2-live.log | 156513->152455 | 4058 | 4058 | 0 | 1111 (553/558/558) |
| evidence/tmp-preserved-20261010/tmp/claude2-live.norm.log | (byte-identical twin of claude2-live.log) | | | | |
| evidence/tmp-preserved-20261010/tmp/fixrun.log | 111568->107992 | 3576 | 3576 | 0 | 563 (46/517/517) |
| evidence/tmp-preserved-20261010/tmp/l25A-live1.log (=live2) | 133144->128867 | 4277 | 4277 | 0 | 825 (276/549/549) |
| evidence/tmp-preserved-20261010/tmp/l25A-live3.log | 137400->132793 | 4607 | 4607 | 0 | 861 (306/555/555) |
| evidence/tmp-preserved-20261010/tmp/l25B-live2.log (=live3) | 130415->126213 | 4202 | 4202 | 0 | 802 (256/546/546) |
| evidence/tmp-preserved-20261010/tmp/l25B-live4.log | 135823->131036 | 4787 | 4787 | 0 | 852 (297/555/555) |
| evidence/tmp-preserved-20261010/tmp/l26-crash.log | 134222->129719 | 4503 | 4495 | 8 | 832 (274/558/558) |
| evidence/tmp-preserved-20261010/tmp/l26crash.norm.log | 129731->129719 | 12 | 4 | 8 | 832 (828/4/4) |
| evidence/tmp-preserved-20261010/tmp/l26v2.log | 143776->139859 | 3917 | 3917 | 0 | 1000 (456/544/544) |
| evidence/tmp-preserved-20261010/tmp/now3.log | 136846->132929 | 3917 | 3917 | 0 | 895 (351/544/544) |
| evidence/tmp-preserved-20261010/tmp/v6-run.log | 133825->129908 | 3917 | 3917 | 0 | 843 (299/544/544) |
| evidence/tmp-preserved-20261010/tmp/zzdiag1.log | 134301->129967 | 4334 | 4334 | 0 | 829 (273/556/556) |
| evidence/tmp-preserved-20261010/tmp/l26-integrity/lw/out/1-sim-deployed-nul.log | 253200->173600 | 79600 | 79600 | 0 | 1200 (425/775/775) |
| harvest/harvest-l25-B-final-ue4ss.log | 137081->132164 | 4917 | 4917 | 0 | 864 (307/557/557) |
| harvest/harvest-l25-B-live1-ue4ss.log | 130415->126213 | 4202 | 4202 | 0 | 802 (256/546/546) |

Notes: `l26crash.norm.log` is a pre-GZ "normalized" copy that still
contains 12 NULs (4 missed wide pairs + 8 orphans) — the old tr-based
normalization was incomplete; the decoder cleans it fully.
`1-sim-deployed-nul.log` is the logwriter race harness simulation (agent
l26-integrity), included for completeness. All other ~360 retained .log
files are NUL-free (host-generated or kernel-tap): decode is a strict
no-op on them (property-tested).

## 3. VERDICT CHANGES (decoded vs recorded/raw)

Method: `harvest.py --local-ue4ss <log> --skip-klog` run twice per log
(decoded vs `--raw`), verdict lines + marker-table hit counts diffed; the
recorded reports of record (HARVEST-l25-B-*.md) and the agent audit
sheets cross-checked. Raw verdict == recorded verdict everywhere the
recorded report used harvest.py's raw scan (its only mode before this
change), so decoded-vs-raw IS decoded-vs-recorded for those.

### 3.1 CHANGED — menu failure mode NONE -> SCAN-FAILED (9 logs)

| log | 'Scan failed:' hits raw -> decoded | '[TownfallItemSpawner] Loaded v1.0' raw -> decoded |
|---|---|---|
| harvest/harvest-l25-B-final-ue4ss.log | 0 -> 15 | 0 -> 1 |
| harvest/harvest-l25-B-live1-ue4ss.log | 0 -> 4 | 0 -> 1 |
| tmp-preserved tmp/l25A-live1.log (=live2) | 0 -> 5 | 0 -> 1 |
| tmp-preserved tmp/l25A-live3.log | 0 -> 5 | 0 -> 1 |
| tmp-preserved tmp/l25B-live2.log (=live3) | 0 -> 4 | 0 -> 1 |
| tmp-preserved tmp/l25B-live4.log | 0 -> 13 | 0 -> 1 |
| tmp-preserved tmp/l26-crash.log | 0 -> 1 | 0 -> 1 |
| tmp-preserved tmp/zzdiag1.log | 0 -> 2 | 0 -> 1 |
| evidence/zzdiag-v2-session.log | 0 -> 1 | 0 -> 1 |

Every 2026-10-08-era session that ran the TownfallItemSpawner logged its
`Scan failed:` line(s) through the WIDE path (`S\0c\0a\0n\0...`); the raw
substring scan found ZERO of them. The verdict line flips:

- RAW (recorded): `- **menu failure mode?** NONE -- no menu-failure marker
  ('UI creation failed:' / 'WidgetBlueprintLibrary not found.' /
  'Scan failed:') in this log`
- DECODED: `- **menu failure mode?** SCAN-FAILED -- menu-failure
  marker(s) present ... : scan-failed`

This WEAKENS two recorded reports of record (their overall menu-not-up
conclusion is unchanged — menu-opened and item-spawned still read NO —
but a stated absence-of-evidence was wrong):

1. `harvest/HARVEST-l25-B-final.md` line 2501 — "menu failure mode? NONE
   ... no menu-failure marker ... in this log" is FALSE for its own
   evidence file (harvest-l25-B-final-ue4ss.log): the mod logged 15 scan
   failures (first 18:36:45, interleaved mid-line with `[l22] keybind
   callback entry` / `ExecuteInGameThread callback` races). The B-final
   launch's menu failure was NOT silent — it was the (already suspected)
   scan-chain failure, now in-log provable.
2. `harvest/HARVEST-l25-B-live1.md` line 1961 — same flip (4 scan
   failures).

Also weakened (same class): the marker-table rows
`[TownfallItemSpawner] Loaded v1.0` recorded 0 hits in those reports;
decoded reads 1 (the mod print existed, wide-written, e.g. B-final line
772: `[Lua] [ T o w n f a l l I t e m S p a w n e r ] L o a d e d v1.0.
Clean ...` colliding with the narrow `[l22] Lua mod started:
TownfallItemSpawner` line).

Honest scope note: `mod-started?` verdicts are unchanged everywhere (they
key on the narrow `[l22] Lua mod started` line, which raw grep sees). No
verdict anywhere moved in the strengthening direction except recovering
markers that SHOULD have been counted (Loaded v1.0); no previously-NO
failure became YES-success.

### 3.2 UNCHANGED (13 of 22 NUL logs + all clean logs)

claude2-menu-session-20261010.log, claude2-live.log (+norm twin),
readfix/session-full.log, bind1.log, fixrun.log, l26v2.log, now3.log,
v6-run.log, l26crash.norm.log, 1-sim-deployed-nul.log (sim), and every
NUL-free retained log: decoded verdicts == raw verdicts on every
harvest.py field (init/mod-started/admission/listener family/mod-side
menu+spawn/diag-zz/database-live/acceptance). The zzdiag-family logs
(zzdiag1, now3, v6-run, l26v2, zzdiag-v2-session) keep their recorded
diag-zz verdicts exactly: the diag-zz parser already NUL-stripped per
line (agent-CK rule), and decode cannot restore bytes the race never
wrote — the zzdiag-v2 truncations (`[zz] K` not `[zz] K1 f7`, E0..E11
chunk family) are byte-loss, not encoding loss, and remain as recorded.

## 4. Agent-report claim cross-checks (decoded evidence)

### 4.1 GO capability-audit (agent-GO-capability-audit.md) — row 3 chain re-verified, one count corrected

- `[EU-VMT] arming refused: non-text-ent (R3 carriers remain the pump)` —
  VERIFIED, decoded line 625, 00:53:06. The refusal REASON is now fully
  greppable post-decode (raw contiguous grep for `non-text-ent` = 0 hits
  — the exact plain-grep miss class). GO's quote was correct.
- `[ST] pump vmt REFUSED (ufunc line pending first tick)` — VERIFIED,
  decoded line 660, 00:53:08.
- `"pump vmt ARMED" appears in NO retained console log` — re-verified on
  the DECODED corpus (all 22 NUL logs + clean set): 0 hits. Correct.
- `[R3] FuncPtr-marshal event carrier armed: [n: F7BD] @0x0000003000AA8BD0
  (callback id 1)` — VERIFIED, decoded line 780, 00:53:08 (8 carrier-armed
  lines total; ids 1-2 at 00:53:08 as GO cited).
- **CORRECTED — the "145 dispatch lines" count.** GO row 3 cites "145
  `[l22] ExecuteInGameThread queued/drained/callback returned` lines".
  Decoded recount: **87** `[l22]` ExecuteInGameThread dispatch lines
  (39 queued + 24 drained + 24 callback returned) + 1 non-`[l22]` line
  (`[l23] game-thread self-test queued via ExecuteInGameThread`) = 88
  total ExecuteInGameThread lines. The 145 figure is the count of ALL
  `[l22]` lines in the log (verified: 145), i.e. the dispatch-pump
  evidence was overstated ~1.67x. The row's substantive conclusion (R3
  carriers + a live ExecuteInGameThread pump) STANDS on 87 lines.
- Row 2 citation `[Seeds] slot 2 FName ctor bound at 0x63d590 ...` —
  VERIFIED at decoded line 561 exactly as cited.
- Row 4 timestamps — VERIFIED: `[BUILD] id=l26-20261010T043709Z` line 23;
  mod Loaded v1.0 00:53:08 (line 814); `24 spawnable pickups found.
  Pages=3` 00:54:35 (line 952); DRY-ladder lines at 00:55:14 (1046-1049).
- Row 6 pad-dispatch citations (zzdiag2-diag.jsonl) — VERIFIED in the
  retained diag stream (narrow JSON, unaffected by the dual-path writer):
  36 each of pad_l3r3_edge / _allow / _modifiers / _subscribed /
  pad_l3r3_f7_dispatched.

### 4.2 FJ / FI zzdiag marker sheets

- FI v10 `[DRY-*]` markers and the `[zz] T<n> db=/pc=` tokens appear
  NARROW in the claude2 session (post-F4 build) — decoded == raw, no
  change to FI's recorded expectations.
- FJ v11 was rest-mode (no console run), so no run log exists to re-check;
  nothing to change.
- agent-CK's zzdiag-v2 session analysis (truncated `[zz] K` payloads,
  E-chunk reassembly, writer-collision proof) — UNCHANGED by decode
  (byte loss vs encoding loss, see §3.2). Its diag-zz verdict stays.

### 4.3 GS regression gate

`host-tests/run-regression.sh` and its suites parse host-generated suite
output logs only (lua harness verdicts, ELF scans, JSON receipts) — no
console session log anywhere in the gate. No patch required; gate re-run
after the patches in §5.

## 5. Regression gate re-run (post-patch)

`host-tests/run-regression.sh` executed after all patches: GREEN (all
landed suites pass; pak-overlay ABSENT as before). The patched tools are
also host-verified: `decode_session_log.py --self-test` 15/15 PASS;
`harvest.py --local-ue4ss` decoded and `--raw` runs produce rc=0 on every
retained log; `fast_cycle.sh watch --source` decoded smoke run shows the
menu/rung markers from the NUL-interleaved source.

## 6. What this means going forward

- Every 2026-10-08-era verdict that ever read "menu failure mode: NONE"
  from a raw scan of a spawner-session log is suspect; the 9 logs above
  are corrected to SCAN-FAILED by the tooling now (no historical report
  files were edited — this document is the correction record; raw logs
  are preserved byte-identical).
- Any future claim citing marker absence over a console session log MUST
  come from the decode-first tools (`harvest.py` default mode) or cite
  the decoded copy; `--raw` exists only for diffing.
- The writer-side fix (clean UTF-8 out of the log device) is a separate
  labelled change, PROPOSED not implemented: `parked/KI-7-logwriter-clean-utf8/`
  (proposal + console-verification plan).
