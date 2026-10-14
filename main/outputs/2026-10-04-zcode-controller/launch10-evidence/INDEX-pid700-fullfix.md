# INDEX — pid700-fullfix evidence (sibling pointer; LAUNCH10-RESULTS.md left untouched)

`LAUNCH10-RESULTS.md` is a dated, single-worker classified receipt (worker-verdict-first-real,
2026-10-05, sections 0-6 + controller scope corrections). Appending a foreign-worker evidence
pointer would corrupt its provenance, so per the archive contract this sibling INDEX entry is
written instead. Nothing in `LAUNCH10-RESULTS.md` was modified.

## Archived: `pid700-fullfix/` (worker-evidence-archive, 2026-10-05)

The /tmp pulls of the full-fix candidate **07ac3e1e** boot (2026-10-05 18:40:10, session/pid
**700**) — previously NO pid-700 evidence existed anywhere under `outputs/`
(worker-stage-metrics, negative result). Contents:

- `primary/` — `fullfix-sdiag.jsonl` (1,557,187 B, 10,441 records, 19 epochs; pid 700 =
  553 records, t 86713681..86719427), `fullfix-status.txt` (400 B, session 700, states
  activated + [Seeds] REFUTED), `fullfix-ue4ss.log` (1,905 B, NUL-interleaved garble).
  Byte-identical copies of /tmp, sha256 in `SHA256-before.txt`/`SHA256-after.txt`.
- `related-sessions/` — same-chain earlier pulls with manifest: codecvt-run (587),
  directfix (601), inputfix (645), capbump (672) + the 30,848,856 B `codecvt-readback.prx`.
- `pid700-timeline.txt` — per-event pid-700 timeline (553 rows, t_ms).
- `epoch-view-fullfix.txt` — `epoch_view.py` correlation table (all epochs + pid-700 verdict).
- `MANIFEST.md`, `EVIDENCE.md` — hashes, chain, decoded numbers.

Headline decoded facts (all programmatic, see `pid700-fullfix/EVIDENCE.md`):

- First-ever captured **Blocks[0] = 0x22C0000000** (CurrentBlock 68, published spans 64) —
  worker-blocks0-provenance had found NO prior capture anywhere.
- Envelope `(env_high<<32)|env_low = 0x1100400000`; assert `0x22C0000000 >= envelope` PASSED
  (Δ ≈ 71.0 GiB) ⇒ pool REFUTED is the **capture-truncated class**, terminal for this launch.
- Spanpub cap bump first effective in pid 700: 64 published of 126 (pids 645/672: 16 of 122).
- `bridge_vmentry_entries=0x2756` (10070), `bridge_vm_walk_stop=0` (clean end).
- Runtime STARTED: conversion quartet D1-D5 met (extent [0x400000,0xc0602c8), anchors 7,
  intervals 1, cfg 1/1), `runtime_start_ok` at +831 ms, E4 `activated` literally on file.
- Mod declared absent via the double-garble: raw log bytes `u\0e\0 4\0s\0s\0/\0M\0o\0d\0s\0/\0T\0o\0w\0n\0`
  — message site `ensure_str(m_mod_path)` (l11-merged LuaMod.cpp:941, via `path::wstring()`),
  while the correct `modbridge::normalize_path_for_lua` override (:43-54) is dead for the
  message path.

Consumer rule: the status file is LATEST-STATE-ONLY for pid 700 alone; correlate durable
events by pid (LAUNCH10-RESULTS §Post-verification scope corrections).
