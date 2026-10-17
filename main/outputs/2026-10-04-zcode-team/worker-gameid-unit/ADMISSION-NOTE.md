# ADMISSION-NOTE — worker-gameid-unit staged NEXT-candidate unit
worker-gameid-unit · 2026-10-05/06 · PS5 UE4SS runtime port
Status: PROPOSAL-ONLY STAGING. No shared lineage file was edited (live bridge
`abeaa06a` / initializer `1660b749` are mid-integration). No composition run,
no console dispatch, no game claims. The only acceptance path is a controller
composition run with refreshed pins.

## 0. What this unit is
A BRIDGE-TU addition (single file, 4 hunks on
`runtime_start_bridge_r14_diag_spanpub.cpp` abeaa06a4987…): self-reporting game
identity at module_start — AFTER the existing spanpub block — so identity drift
can never masquerade as a runtime bug. Implements the worker-game-version
identity design:
stat(size+mtime+folded sig) → streaming sha256 of `/app0/eboot.bin` →
`game_eboot_sha256_w0..w3` → `game_content_version_packed` from
`/app0/sce_sys/pfs-version.dat` → the human-durable `gamedata` row in
runtime-status after state 1. Every failure path records its errno and emits
NOTHING else: absence is honest PENDING, never a fabricated identity
(analyzer_v2 hard-stops on ANY mismatch).

NOTE ON THE DESIGN-NOTE CITATION: the dispatch pointed at
`worker-game-version/results/` section 5; that dir holds raw evidence only
(both captured eboot binaries, pfs-version.dat, param.json, mount.lnk, the
supplier sdiag) and has NO results/ subdirectory on disk. The operative
consumer contract was taken from the actual landing zone:
`worker-runbook-v2/analyzer_v2.py` (GAME-IDENTITY section: word law, pinned
manifests, packed-version decode, gamedata-row handling) + the
`launch11-combined/EDITIONS.json` pins. All values below are from those, and
the pinned manifests were re-hashed locally this session (they match).

## 1. Staged file list (all under this worker dir)
- `staging/runtime_start_bridge_r14_diag_spanpub.cpp` — **012fb485…**
  (baseline abeaa06a…)
- `diffs/runtime_start_bridge_r14_diag_spanpub.abeaa06a-to-staged.diff`
  (4 hunks; verified to apply cleanly and reproduce the staged bytes EXACTLY:
  fresh-copy patch + cmp byte-identity, host-test-run.log TEST 3)
- `host-test/test_sha256_gameid.cpp` + `test_gameid_helpers.cpp` +
  `extract_hunk.py` + generated `.inc` extracts + `host-test-run.log`
- `provenance/SHA256SUMS.txt` (baseline, staged, diff, artifacts; baselines
  re-verified at open BEFORE any edit)

## 2. Diff summary (4 hunks, +456/−0 lines — strictly additive)
- Hunk 1 (includes, +11): `<cstdio>`, `<fcntl.h>`, `<time.h>` + the
  NID-routing law comment (fopen/fread are IN the frozen import contract —
  required_strong set, verified in `import-contract.json`; O_* are header
  constants; clock_gettime already in the frozen contract via the diag sink).
  `errno` stays the unqualified SDK macro (edit-11 lesson: `::errno` is a
  syntax error under the packaging compile).
- Hunk 2 (the gameid section, +456): the emission block staged between the
  spanpub accessors (`bridge_capture_envelope`) and the REMOVED-mincore comment
  block: a self-contained public-domain-shape SHA-256 (marker-bounded
  `[gameid-sha256-BEGIN..END]`, ~114 lines, no external symbols — host tests
  extract and test THESE exact bytes), strict pfs-version parser/renderer,
  `bridge_gameid_emit()` and `bridge_gameid_status_line()`. No raw syscalls
  anywhere (session-934 law).
- Hunk 3 (call site 1, +6): `bridge_gameid_emit();` immediately after
  `supplier->capture();` — after spanpub publish has run, BEFORE the lease-gap
  refusal so a refusing boot still leaves its identity evidence behind.
- Hunk 4 (call site 2, +6): `bridge_gameid_status_line(mod_root);` on the
  start-success path only (after `StartActivatedRuntimeWithAck` returned a
  non-null owner), before the MRS_OK return.

## 3. Emission law (what a log reader will see)
- stat: `gameid_eboot_size_low/high`, `gameid_eboot_mtime_low/high` (the
  established `_low/_high` halves convention), `gameid_stat_fold` = size_lo ^
  mtime_lo ^ mtime_hi (the "folded sig" — quick-diff tag, never primary).
- sha: `game_eboot_sha256_w0..w3` — digest bytes 0..15 as four BIG-ENDIAN u32
  words, exactly the analyzer_v2 reassembly law (`pack(">I")` chain == first
  32 manifest hex chars). CONSUMER NOTE: the dispatch text said "four u64
  words"; the consumer contract and the u32 diagnostic_event channel are both
  u32 — the u32 big-endian words are what analyzer_v2 actually compares
  (`words[i] & 0xFFFFFFFF`). Budget evidence: `gameid_sha_ms`.
- version: `game_content_version_packed` = `(maj<<24)|(min<<16)|(pat<<8)`
  parsed STRICTLY from 10 ASCII bytes `AA.BBB.CCC` (any trailing
  byte/newline/junk → `gameid_ver_fmt`, no tag, no fabrication); pinned
  02.003.000 → 0x02030000.
- honest refusals: `gameid_stat_ec` / `gameid_open_ec` / `gameid_ver_ec` /
  `gameid_ver_fmt` / `gameid_sha_err` (short read / grew mid-read) /
  `gameid_toobig` (>1 GiB cap) / `gameid_mtime_ec`, each followed by
  `gameid_unavailable=1`; `gameid_done=1` when the sequence completed. On the
  host (no /app0) the exact session-934 shape reproduces: stat_ec=2,
  ver_ec=2, no other tags (host-test-run.log TEST 2 rows 19-21).
- once-per-process: static guard (module_start is entry-guard serialized).

## 4. The runtime-status gamedata row (task item 4)
`gamedata\t<sha16>\t<ver-or-'-'>\t<size>\n` appended to
`<mod_root>/runtime-status.txt` AFTER RuntimeAckSink state 1 `activated`
already durably present. Mechanism: the module's PROVEN status-file IO shape
(mfa_status lineage POSIX open/write, proven in-game by session 934's own
runtime-status.txt) with `O_WRONLY|O_APPEND|O_NOFOLLOW`, whole-line bounded
partial-write retry. Deliberately NOT `mfa_status_open` (it O_TRUNCs — it
would destroy the live session file) and NOT a forged `state\t` row (that
would mint a seq number owned by the sink's counter). Written ONLY when the
sha is verified (no words → no row, ever); missing version renders `-`;
`gameid_status_line` 1=whole line, 2=partial (dropped line never claimed).
CONSUMER GAP (reported, not edited — shared lineage): analyzer_v2
`parse_status()` only builds `states` from `state\t` rows, so a bare
`gamedata\t` row is currently invisible to it and the gamedata rung stays
PENDING until a ~3-line analyzer follow-up recognizes the row type. The
sha/version tags themselves ARE parsed today (the GI gate works through sdiag
alone); the gamedata row is the human-durable layer the dispatch asked for.

## 5. /app0-path evidence audit (task: "CHECK WHICH PATH the in-game process can actually open")
- THE PROBE IS ALREADY IN THE BRIDGE: `stat_app0` / `stat_app0_ec` (r10 diag,
  lines 516-519 of abeaa06a) via `std::filesystem::is_regular_file("/app0/
  eboot.bin", ec)` — libc++ → NID-resolved libc stat. This unit reuses EXACTLY
  that mechanism (same include graph, same error_code style) for size/mtime.
- IN-GAME EVIDENCE (worker-game-version/modbridge-supplier-diag.jsonl, all 19
  captured sessions pid 317..700): `stat_app0=0`, `stat_app0_ec=2 (ENOENT)`,
  while `stat_ownmod=1 ec=0` (`/data/plugins/modbridge_supplier_composition.prx`
  exists) and `stat_rootdir=0`. So std::filesystem WORKS in-process and the
  /app0 title-mount guess has NEVER resolved in any game session so far.
- DUMP PATH IS FTPSRV-SIDE: mount.lnk → `/data/homebrew/Silent Hill Townfall
  PPSA33286` is the loader-side dump location visible to ftpsrv; it is NOT
  established as the in-game view, and hashing a dump file could not prove
  what is executing anyway. Per the dispatch, this unit designs for /app0
  FIRST and does NOT fall back to the dump path for the sha — an honest
  `gameid_stat_ec` is strictly better evidence than a plausible-but-unproven
  digest (the analyzer REFUSES on mismatch; a wrong-file hash would kill a
  good boot).
- CONSEQUENCE: on the current target the expected first-run outcome of this
  unit is `gameid_stat_ec=2` + `gameid_unavailable` + `gameid_ver_ec` and NO
  sha/version/gamedata (identity PENDING — truthfully). That is the intended
  evidence: it converts "identity unknown" into "the /app0 path question is
  now measured in the same session as the boot". If a future mount fix (or the
  shadowmount/r2 route) makes /app0 real, the unit starts emitting with zero
  changes. The pinned w0..w3 to compare against (runtime-only):
  decrypted 3b6cfad4 → `3b6cfad4 7dd17fea 17d429bc e80ae374`;
  toplevel  afe0a2a5 → `afe0a2a5 fc9e1c17 b215bdbb 3de3c80e`
  (both manifests re-hashed from the captured files this session; both ELF-
  magic, both exactly 194,241,424 bytes — the mounted-vs-decrypted question
  stays open BY DESIGN, the analyzer accepts either).

## 6. Host-test results (host-test/host-test-run.log, -Wall -Wextra -Werror -std=c++20)
- TEST 1 — sha256 impl (extracted VERBATIM from the staged TU): 9/9 PASS.
  NIST FIPS 180-4 vectors: empty, "abc", 448-bit (pad boundary), 1,000,000×'a';
  each also re-run with 13-byte chunks; 55..65 pad-boundary class
  chunked==one-shot; w0..w3 big-endian word law asserted on "abc".
- TEST 2 — the WHOLE gameid hunk verbatim (+host shims): 21/21 PASS — strict
  version parse/refusal matrix (8 cases incl. trailing-newline and 4-digit
  refusals), render law (incl. untruncated 3-digit fields), gamedata row
  lands VERBATIM after state 1 in a session-shaped scratch file with earlier
  rows untouched (append, not truncate), `-` placeholder for missing version,
  no-sha → no row, null/empty mod_root refused, once-per-process guard, and
  the host-honest refusal path (no /app0 → stat_ec=2 + unavailable, no sha
  words, no version tag, nothing fabricated).
- TEST 3 — diff round-trip: `patch(fresh baseline copy)` is byte-identical to
  the staged file (cmp).
- TEST 4 — budget timing ONLY (no digest assertion — the pinned prefix
  expectations are runtime-only by task law; the impl was tested, not the
  file): staged impl, 256 KiB chunks, over a REAL 194,241,424-byte file =
  564-567 ms on this host. On-target the measured `gameid_sha_ms` tag carries
  the truth; expected class 0.2-1 s once per module_start. The syntax-check
  pattern follows the staging-next preproof precedent (copyin-unit §5); the
  target compile remains the packaging pipeline's gate (out of this worker's
  bounded scope).
- Two defects were caught and fixed during host testing (before any staging
  freeze): a leftover garbage size-append line and a first-draft `final()`
  padding structure were replaced by the clean pad[] form; the harness itself
  had a tripped once-guard and row-length arithmetic bug (fixed in the TEST,
  not the unit).

## 7. Comparator/pin cascade IF ADOPTED (controller-owned; per COMPARATOR-REGEN-PROCEDURE)
1. `EDITIONS.json` (launch11-combined) `bridge_cli.sha256`:
   abeaa06a4987… → 012fb485dfe6… (runner --bridge-source/--bridge-sha256;
   verify_units gates it).
2. Undefined-import surface: EXPECTED added = ∅ (fopen/fread/fclose,
   open/write/close, clock_gettime are all already in the frozen contract /
   linked TU set; O_* flags are header constants). The fixed edition's
   undefined-delta gate must see exactly that.
3. .bss: +256 KiB (kGameidChunk) + ~1 KiB state — inside the bridge's existing
   BSS budget class (g_bridge_vmmap is ~38 MiB); worker-bss-budget reviewer
   may want to re-eye the total.
4. Comparators: no bridge object comparator is pinned → bridge.o and the
   composed object are re-measured fresh on both passes and must agree
   (determinism twin); luamod/asmhelper comparators untouched.
5. Freshen: no new files inside the composed TUs → no freshen-deps.txt change.
6. Consumer follow-ups this unit does NOT make (shared lineage): (a) the
   analyzer_v2 `gamedata\t` row recognition (§4); (b) nothing else — the sdiag
   tag vocabulary is already consumed by analyzer_v2's GAME-IDENTITY section.

## 8. Honesty ledger + bounds
- No game claims: whether /app0 resolves in-game is UNPROVEN (evidence says it
  did not in 19 sessions); the unit is built to measure it, not assume it.
- No quality scores, no AAA/parity/completion, no fresh console evidence.
- Staged bytes are the ONLY deliverable; the diff is verified round-trip;
  shared lineage untouched (baseline re-verified abeaa06a at open and again at
  close; nothing outside this worker dir was written except /tmp scratch).
- Bounded: stops here. Admission is a controller decision.

## 9. ADDENDUM — live-lineage drift observed at close (2026-10-05 21:29:57 local)
The shared lineage files MOVED while this unit was being staged (not by this
worker; nothing outside worker-gameid-unit/ and /tmp was written):
- `launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp`:
  abeaa06a4987… → **ba305edd…** (+199 changed lines: a `v5.2
  worker-copyin-unit` hunk — the NID-routed open/write/read/close/unlink
  write(2)-copyin listing-gap probe — folded into the same file).
- `launch11-combined/units/UnrealInitializer.cpp`:
  1660b749c86e… → **62422c21…** (not inspected; not this unit's file).
Consequences, verified: this unit's 4-hunk diff STILL APPLIES CLEANLY to the
moved live bridge (patch --dry-run + real apply, zero rejects; result
81b03068… would be the combined abeaa06a+copyin+gameid staging if the
controller wants both). The two hunks are region-disjoint (gameid sits before
the old mincore/REMOVED comment block; copyin after it). The controller owns
the fold order and the refreshed pins; this note claims nothing about the
copyin hunk's own admissibility. This worker's staging stays pinned to
abeaa06a as its recorded baseline, with the drift + the verified clean-apply
result recorded here for the reconciliation decision.
