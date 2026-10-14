# agent-GK — generalized IoStore multi-mod overlay builder (Claude round-6 item 3a)

2026-10-08/10. Mac-only, battery-safe: local python3.9, no network, no
console, no PRX builds, no model calls. All artifacts in
`outputs/2026-10-08-l25-prep/pak-overlay/`. The `.superpowers` trees were
used strictly read-only (as cited format pins P1-P8 + the Leon trial
records); nothing outside `pak-overlay/` was created or modified.

## Deliverables

| File | What |
|---|---|
| `pak-overlay/overlay_builder.py` | game-agnostic multi-mod overlay builder (1,932 lines, self-contained, python3.9, zero game names in logic — Rule-10 grep clean; names only in provenance docstrings) |
| `pak-overlay/test_overlay_builder.py` | 16 tests: synthetic version-span/conflict/priority/lifecycle/stale/encrypted fixtures + the REAL Leon records integration replay |
| `pak-overlay/UNSUPPORTED.md` | honest capability matrix (below) |
| `pak-overlay/mod-manager/INTEGRATION.md` | CLI contract sketch for GG's modmanager.py (no edits to GG's files) |
| `pak-overlay/leon-replay-drift.json` | drift receipt vs the one-game trial outputs (written by the integration test) |

## Capability matrix (summary; full version in UNSUPPORTED.md)

| Capability | Verdict |
|---|---|
| TOC v8 (ReplaceIoChunkHashWithIoHash) parse + emit + overlay | SUPPORTED — the only locally end-to-end pinned version (Townfall base + Leon PC trio through the historical trial) |
| TOC v3 (PartitionSize) | HEADER_PARSED verdict only (locally observed on Stellar Blade utocs); build refused (pre-v8 FIoChunkHash size not retained — HEADER-SPEC unknown 3) |
| TOC v1,v2,v4,v5,v6,v7,v9,v10 | explicit UNSUPPORTED refusals with reasons (no local observed file / unpinned array regions); version→engine mapping never guessed |
| Encrypted containers (flag, key GUID, v10 semantics) | REFUSED — keys out of scope Mac-side until Phase B; no local record contradicts |
| Signed containers | REFUSED (signature semantics unpinned) |
| Compressed IoStore blocks | REFUSED (all-stored profile only; matches every local capture) |
| Multi-partition base UCAS | REFUSED (single unbounded partition inputs; derived 2-partition shape is the proven Leon layout) |
| Multi-mod (N mods, one derived index + one sidecar) | SUPPORTED — deterministic order (priority, stable sha256(id)[:16], later-wins mirroring ~mods alphabetical semantics), conflict=reject default with full three-ledger report (chunk ids, package ids, virtual paths), later-wins policy optional, per-mod ledger of contributed package ids |
| Uninstall/rollback | SUPPORTED — per-base-index transaction journal mirroring the console-transaction.json shape (acknowledged_mutations, nonce paths, backup/original/sidecar); uninstall re-derives or restores pristine; every step sha-verified |
| Game-update hazard | STALE verdicts on install/uninstall/verify when the live index matches neither journal identity; never auto-repaired |
| Classic .pak mods | NOT this tool — refuses at magic; separate repak-staging path documented |
| Mod archive byte-identity | LAW held — mods opened mmap read-only; sidecar carries each mod's .ucas block stream VERBATIM (16-aligned segments); only the index and the merged ContainerHeader chunk (BLAKE3 IoHash recomputed) are derived |

## Test results — 16/16 PASS (14.6 s total)

- BLAKE3 (pure python, new): 35/35 official test vectors from the pinned
  upstream copy + extended XOF + 241-sample agreement with the pinned
  chunk_hash.dylib incl. all chunk-tree boundaries + chunked-update
  equivalence. (Found and fixed 3 real implementation bugs en route:
  left-vs-right rotations, double message permutation, parent-node CV.)
- Version span: v8 synthetic round-trip; v3/4/6/7/9/10 synthetic headers →
  exact UNSUPPORTED verdicts; encrypted/signed/compressed → exact refusal
  codes.
- Multi-mod: merge with per-mod ledgers; conflict default-reject then
  later-wins with winner-mapping proven by payload extraction; priority
  override changes the winner; input-order permutation → byte-identical
  derived shas.
- Lifecycle (CLI, temp paks dirs): install→verify OK→list→second-mod
  re-derive→uninstall-one == fresh single-mod build sha→uninstall-last ==
  original index sha restored, sidecar+journal gone; stale tamper → STALE
  on verify, refusal on install, stale verdict on uninstall; dry-run
  writes nothing (no journal).
- Leon integration (real records, read-only): replayed the retained
  one-game adapter inputs against the generalized builder. Semantic parity
  with the recorded trial: base 59478 → final 59573 chunks; 66 chunk-id
  replacements + the separately-handled base-header row (trial counted 67);
  95 added; container-header merge 38439+127 → 38500 packages (same
  66-replaced/61-added sets as the trial receipt); directory merge 56993
  files, 66 overrides; base block table prefix byte-identical; unaffected
  rows byte-identical; per-chunk extraction equality through the derived
  index (all self-verified in build_overlay).

## Documented drift vs the historical trial outputs (honest)

Derived shas intentionally differ from the trial's: the trial repacked the
mod payload through `retoc pack-raw` (its own block layout, 187,713,177 B
sidecar; 2,001,504 blocks); this builder carries the mod .ucas stream
verbatim + a derived merged-header tail (187,719,216 B; 2,001,505 blocks).
Details in `leon-replay-drift.json`. The verbatim carrying is strictly more
faithful to the byte-identity law; semantics proven equal as above.

## Notable engineering facts

- The base ContainerHeader chunk is read by bounded ranged reads from the
  base .ucas when present, or from a pinned pre-extracted file
  (--base-container-header) — the Leon fixture used the retained
  2.27 MB container-header.bin since the 128 GB base ucas is not local.
- The merged ContainerHeader chunk lives under the BASE container's header
  chunk id in a derived sidecar tail segment (the mod's own header chunk
  row is dropped, matching the proven trial behavior); its v8 meta hash is
  the only recomputed hash (BLAKE3-20, P6).
- The mod-header cache is keyed by (mod_id, utoc sha) — a stale-cache bug
  found by the tests and fixed.
- Engine acceptance of any derived index remains an unproved runtime
  question (same standing as the historical trial; PHASEA-PLANS #7 Phase 0
  owns that observation). Nothing here claims console/runtime proof.
