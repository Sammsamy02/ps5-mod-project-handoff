# ADMISSION-NOTE — worker-copyin-unit staged NEXT-candidate unit (v5.2)
worker-copyin-unit · 2026-10-05 · PS5 UE4SS runtime port
Status: PROPOSAL-ONLY STAGING. No shared lineage file was edited. No
composition run, no console dispatch, no game claims. Acceptance path: a
controller composition run with refreshed pins, integrating this trio as a
SET on the launch-blocking path.

## 0. What this unit is (v5.2, post-session-934 rebase)
Implementation of the worker-write-copyin production design note as the SOLE
listing-gap oracle, rebased across three controller evidence corrections:
(a) session 934: the raw `syscall` instruction is game-fatal in the sandboxed
game process (0xa002030a) — the mincore range verdict was REMOVED (controller
edits 13b/14) and every kernel touch must be an NID-resolved symbol;
(b) the definitive l11-1 supplier-contract facts gate the wrapper choice;
(c) worker-copyin-smoke R-4 + the user's standing rule: copy success must not
silently authorize later unsupported direct reads — once admitted via copyin,
EVERY downstream pool read in that boot consumes sink-captured bytes.

## 1. Staged file list (this worker dir) + new shas
- `staging/SeedValidation.hpp` — 234ee5dd… (base 8a9a1c83…)
- `staging/UnrealInitializer.cpp` — 62422c21… (base 1660b749…)
- `staging/runtime_start_bridge_r14_diag_spanpub.cpp` — ba305edd… (base abeaa06a…)
- `diffs/SeedValidation.hpp.8a9a1c83-to-234ee5dd.diff` (2 hunks)
- `diffs/UnrealInitializer.cpp.1660b749-to-62422c21.diff` (5 hunks)
- `diffs/runtime_start_bridge_r14_diag_spanpub.abeaa06a-to-ba305edd.diff` (2 hunks)
- `host-test/test_listing_gap_copyin.cpp` + `host-test/host-test-run.log`
- `provenance/SHA256SUMS.txt`
All three diffs verified: fresh-copy patch application reproduces the staged
bytes EXACTLY (hash round-trip, 2026-10-05).

## 2. Transport route taken + symbol evidence (contract-proven, no flags left open)
Route: raw-fd NID wrappers — `open(sink, O_CREAT|O_WRONLY|O_TRUNC, 0644)` +
`write(fd, TARGET_ADDR, chunk)` (THE copyin: kernel copies from the target
address into the sink through the already-imported `write`) + `read` (first-
page read-back of the consumed sink) + `close` + `unlink`. Per the
coordinator's definitive l11-1 supplier-contract facts (all status=unique):
IN-CONTRACT: open, close, write, read, rename, unlink, fopen, fwrite. NOT
in-contract (and NOT used): sceKernelOpen, sceKernelClose, lseek. Added
import delta of the bridge hunk: NONE. `remove`/`fstat`/`lseek` are not used.
The unlink-first/always-unlink cleanup law is kept (unlink IS in contract;
no rename workaround). stdio (fopen/fwrite, in contract) was rejected on
safety: buffered fwrite may CPU-copy the user buffer, turning a refused
source into an in-process SIGSEGV (game-fatal) instead of the kernel EFAULT
the oracle is built on — documented for the controller; whether Sony's
unbuffered stdio writes directly from the user buffer is unproven.
SINK SIZE: write-return accounting only (rc==chunk accumulation over
page-granular chunks) — no seek/fstat anywhere; the in-contract `read` gives
the first-page read-back that doubles as the vacuity gate (empty/short read
demotes to Unavailable, never admits) and fills the R-4 capture.
CONVENTION: wrappers are classic -1/errno (errno via __error, itself a
contract import); errno captured fresh pre/post write; raw rc/errno emitted.

## 3. Bridge diff summary (2 hunks: includes + probe block)
- `bridge_kernel_write_copyin_probe(addr, len)`: fixed sink
  `/data/modbridge-copyin-sink.bin`, unlink-first, page-granular stepping
  (chunks never cross a page boundary; len==8 = the unambiguous probe shape;
  16 MiB per-call budget; len==0/over-budget -> tag 3, zero calls).
- Classification: REFUSED(0) ONLY on -1/errno==EFAULT (never "unmapped" —
  address-class-unknown caveat documented in-file); UNAVAILABLE(-1) on
  sink-open failure, -1/EPERM or -1/other errno (policy/other-error class),
  partial (0<rc<chunk, never observed), reopen failure, or read-back
  empty/short (vacuous/truncated sink); READABLE(1) only on complete
  write-return accounting (landed==len) AND a full first-page read-back
  (got==min(len,0x1000)).
- Cleanup law: close + unlink ALWAYS once the sink exists (refusal paths
  included — an EFAULT leaves 0 bytes but the file exists).
- Capture store (R-4): bounded 3 slots keyed by probed address (Blocks[0]
  admission probe + up to two entry-referenced blocks); accessor
  `bridge_write_copyin_capture(addr, &bytes, &len)`.
- Tags: `pool_copyin_probe_fresh` (1/0/2/3), `pool_copyin_probe_rc`
  (0xFFFFFFFF encodes -1), `pool_copyin_probe_errno`,
  `pool_copyin_probe_sink` (accounted bytes). No range-verdict tags anywhere
  (the mincore path is gone and is NOT re-added).

## 4. SeedValidation.hpp diff summary (2 hunks)
- `CopyinProbeClass` enum (Unavailable/-1, Refused/0, Readable/1 — the
  probe's int codes); `ReadableCoverageSource::KernelWriteCopyin = 2` kept as
  the source taxonomy; `RangeMappedVerdict` marked RETIRED-with-provenance
  (nothing consumes it).
- `resolve_listing_gap(shape, copyin_class)` — sole-probe signature: span
  membership short-circuits; structural classes unrescuable; Refused refuses
  with the explicit "NOT an unmapped proof" note; Unavailable refuses
  ("the oracle itself failed, nothing is proven"); Readable admits with
  bounds_source "kernel-write-copyin-read" — authorization AND execution in
  one event. Refusal-note honesty preserved; no range-verdict coupling left.
- v5.2 evidence contract comment block: sole-probe law, address-class
  caveat, R-4 sink-fed validation law, transport law (in-contract symbols,
  write-return accounting, no seek, no raw syscall).

## 5. Initializer diff summary (5 hunks)
- Declarations: probe + capture accessor (range-verdict declaration already
  gone from the 1660b749 base; placeholder note replaced with the integrated
  contract).
- Gap block: on the first !shape.ok with a RESCUABLE class, call the probe
  ONCE per boot, `resolve_listing_gap(shape, copyin_class)`; admission
  address-pinned to the probed Blocks[0]; loop exit admits on
  `shape.ok || (gap_admitted_via_copyin && same blocks0)`.
- SINK-FED VALIDATION (R-4): on a copyin-admitted boot the entry-0 'None'
  anchor AND both validate_entry byte comparisons read SINK CAPTURE bytes
  (`copyin_entry_view`); a non-zero entry block is probed once (bounded:
  only blocks referenced by the two ids + block 0, capture slots cap 3); a
  capture miss, an over-cap block, or an entry beyond the captured first
  page REFUSES LOUDLY — never a direct block read. Span-membership boots
  keep the direct volatile reads unchanged. Bounds checks stay pure address
  arithmetic (`check_entry_bounds` untouched).
- Budget-expiry refusal message: "range-verdict resolution" wording ->
  "copyin-probe resolution" (no range-verdict tag/name left).
- Success message names the real provenance: "admitted via
  kernel-write-copyin-read (listing-gap owned-read probe; all entry bytes
  sink-fed)".

## 6. Host-test results (29/29 PASS — host-test/host-test-run.log)
- Part 1 (real staged header): 9 rows — span-membership untouched, structural
  gates even a Readable probe, Refused refuses on both rescuable classes with
  the address-class-unknown note, Unavailable refuses (oracle failure),
  Readable admits on both with exactly "kernel-write-copyin-read".
- Part 2 (host mirror of the staged bridge law): 16 rows — len-8 shape,
  page-chunk accumulation, exact-sum accounting, EFAULT->Refused, EPERM/EIO/
  EACCES->Unavailable, partial->Unavailable, empty/short read-back->never
  admits (vacuity law via in-contract `read`), reopen failure cannot upgrade
  a refusal, invalid-request classes.
- Part 3 (real staged decoders on capture bytes): 4 rows — 'None' anchor
  passes/refuses on captures; 'ByteProperty' matches from a capture slice;
  short/empty capture refuses (no direct-read fallback).
- The staged bridge hunk also passed a host SYNTAX check (native fd calls,
  diagnostic stub; zero diagnostics).

## 7. Comparator/pin cascade IF ADOPTED (per worker-luamod-lineage
##    COMPARATOR-REGEN-PROCEDURE; controller-owned; adopt the trio as a SET)
The initializer declares `bridge_kernel_write_copyin_probe` /
`bridge_write_copyin_capture` — defined ONLY by the bridge hunk; the runner's
unresolved-bridge-symbol check ("Unresolved bridge symbol = STOP") fails if
the set is split.
1. `launch11_one_pass.py::the_pass()` stage-header pin: SeedValidation.hpp
   8a9a1c83… -> 234ee5dd… (controller-owned executable pin).
2. `EDITIONS['units']` rows (verify_units): initializer
   1660b749… -> 62422c21…; SeedValidation rows 8a9a1c83… -> 234ee5dd…
   (launch10-build/final-units copy + armed-staging copy — mirror decision
   per procedure §4.3(c); the armed-staging initializer TU remains the older
   recorded generation, unchanged by this unit).
3. `EDITIONS['bridge_cli'].sha256`: abeaa06a… -> ba305edd… (the bridge pin
   the controller set for edit 14 becomes ba305edd with this hunk).
4. Declared-divergence entry for the bridge file in the EDITIONS change
   manifest (same shape as the ec53442b->f9747267 and edit-13/14 entries);
   sha fields must EQUAL measurement.
5. Comparators: no SeedValidation/initializer/bridge object comparator is
   pinned; initializer.o, bridge.o, and the composed object are re-measured
   fresh by both passes (determinism twin must agree). luamod untouched — NO
   luamod comparator regen.
6. Import surface: added==∅ expected (open/write/read/close/unlink already
   in the l11-1 contract; <fcntl.h> is header-only). The undefined-delta
   gate refuses anything else.
7. Post-refresh pre-flight: `launch11_one_pass.py verify` (HOLD-PASS) → pins
   → --full. The first in-game boot IS the design note's smoke test (the
   pool IS the planted readable range; a wrong Blocks[0] hypothesis exercises
   the refusal path game-safely — EFAULT, never SIGSEGV).

## 8. Honesty ledger
- Underlying evidence: standalone payload, separate process, never
  game-process evidence; in-game write-copyin semantics on game image/heap
  ranges remain a hypothesis until the first in-game boot.
- Known residuals (documented, bounded): sink size rests on write-return
  accounting + first-page read-back (no seek-class symbol in contract) —
  a full-size read-back was declined per the "write-return accounting only"
  direction; first-page captures cover entries within the first 0x1000 of a
  probed block — entries beyond it refuse loudly rather than read directly.
- This unit claims NO quality score, no AAA/parity/completion, and produces
  no fresh console or game evidence.
