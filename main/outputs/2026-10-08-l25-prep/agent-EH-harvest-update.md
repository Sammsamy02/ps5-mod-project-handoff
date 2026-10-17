# Agent EH — harvest tooling update for the l26 fix wave

Date: 2026-10-08 (late). Edited ONLY `harvest/{markers.json,harvest.py}`
(both backward compatible) + this report. No console contact; all testing
local (`--dry-run` + the new `--local-ue4ss` analysis-only mode; zero
network calls, zero file writes by the script itself).

## 1. markers.json — 5 new fix markers (68 -> 73 entries; no dupes)

All five carry correct meaning/phase/files/source; appended at the end of
the list, `_meta.purpose` extended with an agent-EH note including the
SHIP-STATE caveat (the 21:18 l26 build PRX e13b1d0b carries CP+CN+CL, so
add-fallback/[FN]/[UF] are live in that binary; DA's query-fallback is not
in any shipped binary yet — its absence proves nothing until the next
build launches).

| pattern | phase | meaning (condensed) | source |
|---|---|---|---|
| `[FFO] query-fallback` | findxof-fix | DA layer-2 verified-query fix ENGAGEMENT (`[n:X]` = constructed ComparisonIndex, deduped, 128-cap). PRESENCE next run = expected-and-healthy while the adapter root bug (N1) persists — it is the fix working; ABSENCE + successful lookups = root-fixed; absence + still-nil + no add-fallback = N1 falsified (re-read CR §4, A2 next) | agent-DA-verified-query-fix.md ("Marker string (exact)" + "Console acceptance signal") |
| `[FFO] add-fallback` | findxof-fix | CP layer-1 Find-first fix fallback (`[n:X]` = Add-result index). Zero lines for real classes = resolving via Find (observed l26-v2/v6 state); a line for a KNOWN class = GATE-1/pool-admission family, NOT N1 (DA discrimination) | agent-CP-findfirstof-fix.md §4 + §6 item 1; agent-DA note |
| `[UF] nofn` | ufunc-hardening | CN item 3 observable miss at the Lua call site; `[n:0]` discriminates NAME_None conversion failure from a real walk miss; deduped per (class,name), 128-cap | agent-CN-resolver-impl.md §1 item 3 + §3 |
| `[UF] miss` | ufunc-hardening | CN item 4 walk-miss telemetry `w= c= v=N 1=[a,b,c]`; v=0 => chain/offset/cast problem; many-visited-no-match => comparison problem | agent-CN §1 item 4 + §3 |
| `[FN] ctor unready` | ufunc-hardening | CN item 2 once-per-process notice (`NAME_None produced`); degradation can no longer be silent; semantics unchanged | agent-CN §1 item 2 + §3 |

## 2. harvest.py — zzdiag v8 auto-probe + v7 (and v4/v6) recognition

`diag_zz_verdict` is now version-aware: resident version read from the
`Z0 vN` load banner (`Z1 true` no longer leaks into the version list).

- **v8 auto-probe tokens** `T<n> db=v/n/i/e pc=v/n/i/e` (task directive;
  v8 not yet deployed): two NEW verdict fields —
  - `database-live` — YES only when any db probe value canonicalizes to
    valid (`db=v`/`db=valid`); feeds from v8 `T` tokens, v4 `D1 db=…`,
    and v3 `A3 db-nil`; distinct why-texts for probed-but-never-valid vs
    no-db-probe-in-this-log (both NO, never a false positive).
  - `auto-probe cycles` — count of T-token lines + distinct cycle ids +
    per-cycle db/pc values + `went-live` flag (db going live mid-session
    is visible cycle-by-cycle).
  - db/pc value canon: v/valid, n/nil/none, i/invalid, e/err/error.
- **v7 tokens recognized with meanings** (agent-CX-v7-probe.md table):
  Z1, S1r/S2r/S3r (+S*e), U1, U2r/U2e, U3r, F1r/F2r/F3r, A1r, K1/K2 —
  per-token meanings rendered in a token-inventory block; per-press
  CX-matrix states (RESOLUTION-HEALTHY / WRAPPER-BUG-SUSPECT /
  SCAN-PATH-ONLY-BROKEN / FILTER-NOT-EXCLUDING / S-PATH-ERRORS /
  COMMON-LAYER-BROKEN / V7-PARTIAL / IDLE, furthest-first ranked). v7
  24-char error tails (two-leading-space continuation lines, e.g.
  `GetPackageNameFromLongNa`) are captured and attributed to their token.
- **v4** (D1/D2/S1/S2/S3) and **v6** (N-count) session-proven tokens also
  classified (DB-LIVE / DB-NOT-LIVE / D2-FAIL; ALL-FINDALLOF-NIL /
  FINDALLOF-PARTIAL) per SESSION-L26V2-ANALYSIS.md.
- **Bug fixed by the version gate**: E/F chunk reassembly now applies to
  v2/v3 ONLY — previously v7's `F1r/F2r/F3r` FindFirstOf probes would
  have been misread as v3 error chunks ("C-FAIL"). v2/v3 behavior is
  byte-identical to before (regression-tested with a synthetic v3 log:
  B-FAIL state + E-run reassembly intact).

## 3. ACCEPTANCE summary line

`acceptance_verdict` wires the three existing ladder verdicts into one
line at the top of the VERDICT section and in the stdout summary:
`ACCEPTANCE: MOD MENU UP` fires ONLY when all three hit —
scan-succeeded (`spawnable pickups found`), menu created
(`Menu created.`), item spawned (`Spawned `) — otherwise
`ACCEPTANCE: MOD MENU NOT UP (scan-succeeded=…, menu created=…, item
spawned=…)`; UNKNOWN without a log. Report notes these are
ACCEPTANCE-LADDER.md log rungs 1-3 only (rung 0 = init/mod-started
verdicts; rung 4 klog-clean = the separate crash-signature verdict).
Synthetic positive test: all three markers present -> exact string
`ACCEPTANCE: MOD MENU UP`, stdout `acceptance=UP`.

## 4. Regression mode + results

New `--local-ue4ss PATH` flag: analysis-only over a saved log (no pulls,
no klog pickup, no file writes; report + verdicts to stdout; exit 0,
exit 2 on unreadable/missing file). The live FTP path is untouched.

| run | exit | verdict (stdout tail) | detail |
|---|---|---|---|
| `--dry-run` | 0 | 73-entry marker table + updated skeleton | no writes |
| `--local-ue4ss /tmp/l26v2.log` (real session, zzdiag v4) | 0 | `diag-zz=PRESENT:DB-NOT-LIVE database-live=NO auto-probe-cycles=0 acceptance=NOT-UP` | init=YES, mod-started=YES, menu-failure=SCAN-FAILED (6), 6 presses all DB-NOT-LIVE, 6x `D1 db=invalid` observations, no menu/spawn markers, 0/5 fix markers (correct: DA not shipped; CP/CN silent = names resolved via Find) — matches SESSION-L26V2-ANALYSIS.md known outcomes (no menu, db not live) |
| `--local-ue4ss /tmp/now3.log` (real session, zzdiag v7) | 0 | `diag-zz=PRESENT:S-PATH-ERRORS database-live=NO auto-probe-cycles=0 acceptance=NOT-UP` | 3 presses all S-PATH-ERRORS (S1e/S2e/S3e/U2e + tails `GetPackageNameFromLongNa`; F1r/F2r/F3r/A1r all n per CX row 9/3 family), no db probe in v7 (honest NO), scan-failed menu mode, 0/5 fix markers — matches the known outcomes |
| synthetic v8 (T1 db=n … T4 db=v + menu-up markers) | 0 | `diag-zz=PRESENT:DB-LIVE database-live=YES auto-probe-cycles=4 acceptance=UP` | per-cycle db/pc values, went-live=yes, `ACCEPTANCE: MOD MENU UP` exact |
| synthetic v3 (A3 ok / B1 no / E0,E1) | 0 | `diag-zz=PRESENT:B-FAIL`, E-run reassembled | v2/v3 backward compat intact |
| fix-marker roll-up (live-path snippet, tested via scan_markers) | — | `query-fallback=1 add-fallback=0 nofn=1 miss=0 ctor-unready=0` on the synthetic; all 0 on both real logs | live stdout gains `database-live=… auto-probe-cycles=… acceptance=…` + an `l26-fix-markers:` roll-up line |

## 5. Provenance / obligations

- Marker strings sourced verbatim from agent-DA-verified-query-fix.md
  (`[FFO] query-fallback [n:X]`), agent-CP-findfirstof-fix.md §4
  (`[FFO] add-fallback [n:{:X}]`), agent-CN-resolver-impl.md §3
  (`[FN] ctor unready: NAME_None produced`, `[UF] nofn [n:X] cls=[n:Y]`,
  `[UF] miss w=[n:X] c=[n:Y] v=N 1=[a,b,c]`); v7 token table from
  agent-CX-v7-probe.md; v4/v6 token semantics from
  SESSION-L26V2-ANALYSIS.md §2/§5/§6; ladder rungs from
  ACCEPTANCE-LADDER.md; regression logs /tmp/l26v2.log + /tmp/now3.log
  (raw, untouched).
- Backward compatibility: pre-l26 logs read NO/none/ABSENT exactly as
  before; the only behavioral change on old logs is the added acceptance
  summary (NOT UP) and the v7-F-token chunk-gate fix described above.
- The script still performs zero console contact in dry/local modes; the
  live path is unchanged except for the added verdict fields in its
  stdout/report.

— agent EH (ZCode), 2026-10-08.
