# MANIFEST — pid700-fullfix evidence archive

Archived by worker-evidence-archive, 2026-10-05. Source: `/tmp` pulls of the
launch-10/11 debugging chain, 2026-10-05 17:00–18:41 local. Every archived file was
copied byte-identically (`cmp` + sha256 before/after, both recorded).

## primary/ — the pid-700 full-fix session (07ac3e1e boot, 18:40:10)

| file | bytes | sha256 | /tmp original |
|---|---|---|---|
| fullfix-sdiag.jsonl | 1,557,187 | `5d40b782227eace0c6f7f78b2485630885e124c3d7d48eaec2a467d6e67d1403` | unchanged |
| fullfix-status.txt | 400 | `8236e3cf1ce086888c65fad76e6d11dcc5a70d1f10d29e9e8b7ee908961f5056` | unchanged |
| fullfix-ue4ss.log | 1,905 | `a976ca0f366d9f67d3b428198916248246bf69b7a2b5907e23b797cf8d00f4ae` | unchanged |

Hashes: `SHA256-before.txt` (/tmp) / `SHA256-after.txt` (archive) — identical sets.

## related-sessions/ — same pull chain, distinct earlier sessions (all FAILED/refused)

All cumulative sdiag files are append-only supersets; each row = newest epoch + session id
from its status file. Hashes in `SHA256-related-sessions.txt`, all verified against /tmp.

| file | bytes | session (status) | newest pid (recs) | terminal |
|---|---|---|---|---|
| codecvt-run-status.txt | 117 | 587 | — | construction REJECTED: garbled IniParser text ("TeIiasrfie opre" class) |
| codecvt-run-ue4ss.log | 10,959 | (587) | — | the codecvt failure run |
| codecvt-run-sdiag.jsonl | 980,218 | (587) | 587 (572) | cumulative pids 317..587 |
| codecvt-readback.prx | 30,848,856 | — | — | PRX readback pulled 17:00 for the codecvt forensics (binary, kept whole) |
| directfix-status.txt | 167 | 601 | — | failed: "Native configured InputSource does not match the supplied provider" |
| directfix-ue4ss.log | 11,315 | (601) | — | direct-conversion attempt run |
| directfix-sdiag.jsonl | 1,062,531 | (601) | 601 (552) | cumulative pids 317..601 |
| inputfix-status.txt | 400 | 645 | — | [Seeds] REFUTED … Blocks[0] 0x22C0000000, CurrentBlock 68, **spans 16** |
| inputfix-ue4ss.log | 1,905 | (645) | — | same garbled "Mod is absent" line class |
| inputfix-sdiag.jsonl | 1,309,782 | (645) | 645 (553) | cumulative pids 317..645 |
| capbump-status.txt | 400 | 672 | — | [Seeds] REFUTED … same Blocks[0], **spans 16** (cap bump did not land in this boot's spanpub) |
| capbump-ue4ss.log | 1,905 | (672) | — | raw (NUL-interleaved) |
| capbump-ue4ss.clean.log | 1,853 | — | — | DERIVED (NULs stripped), 18:24 — kept for readability, not console evidence |
| capbump-ue4ss.utf8.log | 2,795 | — | — | DERIVED (utf8 render), 18:24 — kept for readability, not console evidence |
| capbump-sdiag.jsonl | 1,392,251 | (672) | 672 (553) | cumulative pids 317..672 |

Chain: codecvt-run (587) → directfix (601) → inputfix (645) → capbump (672) → **fullfix (700)**.
Note: intermediate boots 615 and 631 appear only inside the cumulative sdiags (no separate
/tmp status); 631 is the first `runtime_start_ok` epoch.

## Observed in /tmp, NOT archived here (out of bounded scope; pointers only)

- `l10-*`, `l10b/c/d/e-*` (11:14–12:06) — launch-10 era; run1–run4 already archived at
  `../run1-*`, `../run3-*`, `../run4-*`, `../PS5Pad-sanitized.ini`, `../loadlist-sizefixed.txt`.
- `l11run-*`, `l11run2-*`, `l11run3-*` (15:46–16:21) — l11run2/l11run3 already archived at `../l11run2-*`, `../l11run3-*`.
- `l11diag-*`, `l11diag2-*` (16:43–16:47), `l11-codecvt-pass1..4.log` — l11 diagnostics, distinct probes, not in this pull chain.
- `l9-final-*`, `l9-*`, `ue4sslog-l2..l8`, `rtstatus-l2..l8`, `pathtest-*` — launch-1..9 era, archived elsewhere.
- Oct-4 probe jsonl (`allfam*`, `anchor-run*`, `c2-run*`, `igf5/6*`, `ingame-*`, `fileio-*`,
  `mutex-*`, `modbridge-results*`, `w9_*`, `crt-console`, `diag-run`, `livefacts-*`,
  `main_dbg`, `ue4ss-getc-ar-attempt.tmp`, `UE4SS-settings-complete.ini`, `codecvt_test.cpp`,
  `codecvt_x`) — separate investigations, not pid-700-chain pulls.

## Derived files produced by this archive (NOT console evidence)

- `pid700-timeline.txt` — pid-700 slice of `primary/fullfix-sdiag.jsonl`, 553 rows (seq, t_ms, dt, tag, value).
- `epoch-view-fullfix.txt` — full `epoch_view.py` output (all 19 epochs) with the pid-700 verdict section.
- `EVIDENCE.md` — decoded numbers, envelope assertion, double-garble byte proof, citations.
