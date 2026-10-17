# agent-Z3 — provenance audit of 2026-10-08-l25-prep (READ-ONLY)

Date: 2026-10-08 ~14:30 local. Auditor verified every claimed artifact by direct
recompute (sha256, cmp, diff, stat), not by trusting report text. No audited
file was modified; this report is the only write.

Base: `/WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-08-l25-prep/`
(abbreviated `$PREP` below). `L24`/`L25` = the `2026-10-08-l24-runtime` /
`2026-10-08-l25-runtime` output trees.

## Verdict table

| artifact | exists | matches claim | notes |
|---|---|---|---|
| agent-F-crash-forensics.md | YES | PASS | 10,775 B, 152 lines; cites log lines `ue4ss-full.log:608`, `l24-ue4ss-4.log:609` etc. |
| agent-G-mod-start-gating.md | YES | PASS | 15,206 B; 20 strict `file:line` cites; spot-checks below — both resolved EXACT |
| agent-H-r3-marshal.md | YES | PASS | 16,080 B; 23 strict `file:line` cites; spot-checks below — both resolved EXACT |
| agent-I-mfa-audit.md | YES | PASS | 14,711 B; 25 strict cites (e.g. `modforge_activation.c:311-495`) |
| agent-J-log-digest.md | YES | PASS (nuance) | 11,604 B; line refs in prose/table form (`/tmp/l24-ue4ss-4.log` … 608/609 lines), not strict `file:NNN` tokens — intent satisfied |
| agent-M-l25-inventory.md | YES | PASS | 12,418 B; 4 strict cites + item status table |
| agent-K-l25-review.md | YES | PASS | 12,925 B; 29 strict cites (`hpp:86-88`, `.inl:159`, `TypeChecker.cpp:285-294`) |
| agent-N-review-fixes.md | YES | PASS (nuance) | 10,060 B; cites as `.inl:282-350`-style after first naming the file — intent satisfied |
| agent-S-item6-repoint.md | YES | PASS (nuance) | 3,387 B; cites `run_tests.py` "Line 10" prose-style; the edit itself independently verified below |
| agent-X-fix-verification.md | YES | PASS | 7,125 B; `TypeChecker.cpp:776-778`, `.inl:159-165` |
| agent-T-klog-capture.md | YES | PASS (nuance) | 12,138 B; file paths + "lines ~15097–15160" prose-style line refs |
| agent-B-bootstrap-order.md | YES | PASS (nuance) | 7,372 B; "lines 60–82 of the .inl" prose-style line refs |
| settings-arm-loadmap-only.ini (+ .sha256) | YES | PASS | sha256 recomputed = `f911e8eb47a7d3231d0e4e950daa9cad20ec5099f21678b0fbef845e7692bbb1` (matches claim AND sidecar); 1947 B |
| settings-arm-initgs-only.ini (+ .sha256) | YES | PASS | recomputed = `428f6e44e52a214c7ba2d596bc8769b68085b55cf8e106d4b67e2ceebff9b34` (matches claim + sidecar); 1947 B |
| settings-arm-both-on.ini (+ .sha256) | YES | PASS | recomputed = `b980cea75c46a390a2182b642e4991fcc830e7401afc740c086ca20c359d3ce2` (matches claim + sidecar); 1947 B; `cmp` byte-identical to `/tmp/l24-settings-backup.ini` |
| arms differ only in hook lines + comment line 60 | — | PASS | pairwise diffs: both-on vs loadmap-only = line 60 (trailing `.`) + line 63 `HookLoadMap`; both-on vs initgs-only = line 60 + line 64 `HookInitGameState`; loadmap vs initgs = lines 63–64 only. Nothing else differs |
| experiment-matrix.md | YES | PASS | 5,882 B; §"Decision tree" has all three v2 outcomes: A (crash → hooks exonerated, new bisect), B (no crash + mod starts → which hook safe to re-enable), C (no crash + no mod start → required-hook isolation) |
| claude-review-20261008-l25.md | YES | PASS | 7,858 B; Claude review directive receipt, honest that original text is not on disk |
| LAUNCH-PLAN.md | YES | PASS | 16,513 B; STEP 1.2 "Restore ORIGINAL UE4SS-settings.ini (byte-exact 1947 B)" w/ sha + `stat -f %z … MUST print 1947`; §2 Variant A procedure; §4 Variant B (twin PRX, diagnostic-only receipt); §5 VANILLA KLOG TEST w/ §5 decision line; §7 SAFETY BOX |
| CLAUDE-REPORT-20261008.md sections 1–9 + agent index | YES | PASS | §1–§8, §8b, §8c, §9 all present and ordered; agent index lists F, G, I, J, H, K, M + experiment-matrix.md — every referenced file exists in $PREP. (B/N/S/T/X reports postdate the report at 14:18–14:26 vs 14:13 and are correctly not indexed in it) |
| /tmp/l24-restore/ quarantine | YES | PASS | contains contaminated.inl (15,181 B), contaminated.hpp (2,774 B), contaminated-CHANGES.json (22,746 B) — all non-empty; both differ from the restored tree files (correct quarantine), plus clean NativePropertyPointerGuard.inl/.hpp copies |
| L25 tests/PRE_RUN_HASHES.txt — 5 hashes match current files | YES | PASS | all 5 recomputed from `L25/typescan-r4/…` match exactly; file also contains a post-run block (19:20:38Z) identical to the pre-run block (19:20:22Z) |
| L25 tests/run_tests.py → typescan-r4/include | YES | PASS | line 10: `include=Path('…/2026-10-08-l25-runtime/typescan-r4/include')`; no l24 path remains (docstring says l25/typescan-r4) |
| L25 tests/RESULTS.json 2/2 pass | YES | PASS | `results` = 2 entries (plain + asan-ubsan), both compile exit 0 + run exit 0 with all (a)–(d) PASS lines; its 4 recorded header sha256s match current files |
| L24 tree pristine — restore trio byte-exact | YES | PASS (see nuance 1) | guard .inl/.hpp match `/tmp/l24-restore/NativePropertyPointerGuard.{inl,hpp}` byte-exact (`cmp` clean). CHANGES.json: `/tmp/l24-restore/` holds NO clean CHANGES.json to compare against; instead verified byte-exact against the review-zip original `/tmp/l24zip/changes/typescan-r3/CHANGES.json` (sha `4f7ea6bd…`, identical), and it correctly DIFFERS from `contaminated-CHANGES.json` (sha `040e36fd…`) |
| L24 tree pristine — no mtime after 14:05 | YES | PASS (see nuance 2) | `find L24 -newermt '2026-10-08 14:05:00'` → 0 entries (files and dirs). Newest 3 files are the restored trio at 14:04:59; next-newest 13:35:50 |

## Citation spot-checks (agent G and H) — resolved against real sources

Agent G (citing `L24/typescan-r3/candidate/src/UnrealInitializer.cpp` and
`2026-10-08-l23-runtime/thread/src/LuaMod.cpp`):

1. `UnrealInitializer.cpp:527-528` — line 527 is exactly
   `Output::send(STR("Adding shutdown listener\n"));` and line 528 is exactly
   `UObjectArray::AddUObjectDeleteListener(&FShutdownDeleteListener::ShutdownDeleteListener);`. MATCH.
2. `UnrealInitializer.cpp:2120-2128` — `if (UnrealConfig.bHookLoadMap)` at :2120
   gating `UEngine::LoadMapInternal.assign_address(...)`. MATCH.
3. (bonus) l23 `LuaMod.cpp:7557` — `[l22] Lua mod started: {} (main script returned)`. MATCH.

Agent H (citing `2026-10-08-l22-runtime/profile/units/UnrealInitializer.cpp` and
`2026-10-08-l22-runtime/diagnostics/src/LuaMod.cpp`):

1. `UnrealInitializer.cpp:196-199` — `#ifndef UE4SS_R3_MARSHAL_MODE /
   #define UE4SS_R3_MARSHAL_MODE 1 / #endif` exactly at 196–199; bonus :2216-2218
   refuse_registration_site + "[R3] EngineTick detour unavailable; deferring
   game-thread pump to R3 marshal". MATCH.
2. `LuaMod.cpp:4994` — `[R3] FuncPtr-marshal armed on carrier {} (callback id {})`
   exactly at :4994; bonus :4817 "[R3] marshal is the game-thread pump …". MATCH.

Note: G and H cite different copies of LuaMod.cpp (l23-thread vs l22-diagnostics)
so their LuaMod line numbers differ by design; both copies exist and both
spot-checks resolved exactly.

## Extra inventory (item 9)

- `$PREP/harvest/` — appeared DURING this audit (dir mtime 14:28–14:29, latest
  write `harvest.py` 14:29:08): `harvest.py` (29,836 B), `markers.json`
  (12,141 B, 14:26). Matches the anticipated running-agent harvest tooling; not
  disturbed, not evaluated.
- `agent-Z3-provenance-audit.md` — this report (the audit's own output).
- Nothing else in `$PREP` is unclaimed: all 25 other directory entries map to
  the claimed artifact list above.

## Nuances (flagged, not failures)

1. **CHANGES.json restore reference.** The audit brief said the three restored
   files "match /tmp/l24-restore/ copies byte-exact" — true for .inl/.hpp, but
   `/tmp/l24-restore/` contains no clean CHANGES.json (only
   `contaminated-CHANGES.json`). The restore was instead verified byte-exact
   against the review-zip original `/tmp/l24zip/changes/typescan-r3/CHANGES.json`
   (sha `4f7ea6bd6bd34132b39d041259e430a1e5edcf5f5884acbe398368254ba01b82`,
   identical to the restored file). Substance of the claim holds; the stated
   reference path for the third file does not exist.
2. **14:05 boundary is razor-thin.** The three restored files are stamped
   14:04:59 — one second before the claimed 14:05 cutoff. No l24 file is at or
   after 14:05:00, so the claim holds, but the restore completed with <1 s of
   margin against its own stated bound.
3. **Citation format variance.** Reports B, J, N, S, T cite sources as file +
   prose line references ("lines 60–82 of the .inl", "Line 10") rather than
   strict `file:NNN` tokens. All twelve reports do carry file+line evidence;
   the two reports subjected to the mandated spot-check (G, H) resolved
   exactly, which supports the reliability of the prose-style ones.

## Totals

24 claimed artifact checks: **24 PASS, 0 FAIL** (3 nuances flagged above, none
overturning a claim). All recomputed hashes match; both G/H citation
spot-checks (2/2 each, plus 2 bonus each) resolved exactly against the cited
source files; the l24 tree is verifiably pristine and the quarantine + review-zip
provenance chain is intact.
