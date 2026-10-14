# REPORT — worker-ledger-archive-snapshot

Session-eve ARCHIVE SNAPSHOT of the transport ledger, produced read-only from
`.superpowers/native-observations/transport-experiments-progress.json`. No console
contact, no nested subagents, no writes outside this worker directory. The ledger
itself was never modified.

## Deliverables (this directory)

| File | Purpose |
|---|---|
| `CLAIM.json` | Claim written first; scope/bounds and at-claim source hash. |
| `ledger-session-eve.json` | Frozen full copy of the ledger (717,340 bytes, parses clean, 280 `zcode_*` entries). |
| `ledger-session-eve-index.md` | Navigable TOC: every `zcode_*` key with utc + one-line status (truncated 120 chars) + serialized size, grouped into 8 tracks with a documented first-match grouping rule. |
| `REPORT.md` | This file: integrity stats, largest entries, bloat flags, DELTA contract. |
| `.freeze_stamp.json` | Internal freeze timestamp used by the index header. |

## 1. Integrity stats (from the frozen copy)

- Ledger file: 717,340 bytes; 611 top-level keys; 280 `zcode_*` entries; 331 non-zcode keys.
- Non-zcode key prefix census (top): native 98, portable 18, goal 15, current 10, latest 9, blocked 8, windows 8, completed 6, original 6, next 5, last 5, leon 5, user 5, app 5.
- `zcode_*` utc range: `2026-10-04T15:29:51.005185+00:00` .. `2026-10-05T07:57:06.857311+00:00`
  (~16.5 h of session activity). 279/280 entries carry `utc`; the only missing one is
  **`zcode_handoff`** (flagged for the return session; harmless but non-conformant).
- Ledger top-level save stamp `utc` = `2026-10-05T07:57:06.857311+00:00` (last write before freeze).

Per-track counts (sum = 280):

| Track | Entries |
|---|---|
| milestones-controller (non-`worker_` keys) | 73 |
| runtime-build | 57 |
| sessions-governance | 36 |
| mod-pipeline | 33 |
| layout-memory | 33 |
| worker-general | 26 |
| diagnostics | 12 |
| gameplay-verification | 10 |

Largest 5 entries by serialized size (sum of all entries: 257,617 B; median 749.5 B):

| Key | Bytes | x median |
|---|---|---|
| `zcode_composition_startup_diagnosis` | 4,382 | 5.8x |
| `zcode_worker_ingame_probe` | 4,304 | 5.7x |
| `zcode_worker_4_integration` | 4,234 | 5.6x |
| `zcode_v3_final` | 3,935 | 5.2x |
| `zcode_takeover_2026_10_04` | 3,879 | 5.2x |

Bloat assessment: the three entries above the flag threshold (>= max(3x median, 4,000 B))
were inspected. Each is a multi-finding diagnosis/probe record with several embedded
sub-dicts (e.g. `import_analysis` 1.2 KB + `ROOT_CAUSE_FOUND` 1.4 KB;
`first_launch_attempt` + `second_launch_diagnosis`; `code_integrated` +
`independent_review`). All content is structured evidence, not duplicated or runaway
text. Verdict: legitimately dense, no pathological bloat; no ledger surgery recommended.

## 2. DELTA contract (hash anchor)

The ledger is live and grows every cycle. The return session must resolve any
"which state did the plan assume" question by hash against THIS frozen reference:

- **Snapshot sha256:** `5a31de8fc2b39e887b1969bc3231223319daa3c22e717fdcb89429381d804cad`
- **Freeze utc:** `2026-10-05T08:00:38.934285+00:00`
- Source sha256 at freeze time: identical (`5a31de8f...804cad`) — copy verified byte-identical at freeze.
- Ledger `utc` stamp captured inside the snapshot: `2026-10-05T07:57:06.857311+00:00`
- Last `zcode_*` entry in the snapshot: `zcode_worker_engine_ready_walkthrough` range ends `2026-10-05T07:57:06Z` (max entry utc).

Resolution procedure for the return session:

1. `shasum -a 256 outputs/2026-10-04-zcode-team/worker-ledger-archive-snapshot/ledger-session-eve.json`
   must reproduce the snapshot sha256 above; if not, the snapshot itself was tampered
   with and this archive is void.
2. Hash the live ledger. If it still equals the snapshot sha256, the ledger has not
   ticked since freeze and all return-session planning may cite entries directly.
3. If the live hash differs, the plan's assumptions are defined against the frozen
   copy; diff live vs `ledger-session-eve.json` (e.g. `python3 -m json.tool` + diff, or
   key-set compare of `zcode_*` keys) to enumerate exactly which entries are new since
   the snapshot. Do not retro-read new entries as if they were known at planning time.

Observed divergence (proves the freeze is load-bearing): at report close
(`2026-10-05T08:04Z`) the live ledger already hashed
`e22288273555883144aadafb1a2b9538dbca7cc0a6619a9737b49662b1f67a46` — it ticked after
the freeze. Any return-session planning made against `5a31de8f...804cad` refers to
THIS snapshot, not the live file.

## Notes / caveats

- The index's track grouping is a documented heuristic (first-keyword-match, stated in
  the index header); it is a navigation aid, not ledger semantics. Every one of the 280
  `zcode_*` keys appears exactly once (verified by count).
- `zcode_handoff` has no `utc` field; it is listed in the index with `NO-UTC` and sorts
  alphabetically within its track.
- Entry sizes in the index are `len(json.dumps(entry))` bytes — an honest relative
  measure of ledger weight, not on-disk bytes.
- No acceptance/quality claims are made here; this is a bookkeeping archive only.
