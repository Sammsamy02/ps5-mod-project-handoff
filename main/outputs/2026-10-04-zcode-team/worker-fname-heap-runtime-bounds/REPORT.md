# REPORT.md — worker-fname-heap-runtime-bounds (launch10-c)

Verdict: the frozen heap bound is REPLACED by current-invocation published-span
admission, host-proven 52/52 with the mandatory positive test, and compile-gated
on BOTH sides (initializer: exit 0 with exactly the 6 pre-existing baseline
warnings; bridge: exit 0 with zero stderr under -Wall -Wextra -Werror vs a clean
baseline). Staged proposal only — nothing canonical was edited; the controller
folds (initializer unit = my staging; bridge = my hunks onto consolidated
r14+diag fc06fe52).

## What changed (one paragraph)

`check_pool_shape` no longer consults `[0x8018c000,0x100118000)`. It now demands:
span table present (else loud "span table unavailable" refusal — never a frozen
fallback); CurrentBlock >= 1; Blocks[0] nonzero and 8-aligned; Blocks[0] OUTSIDE
the image extent [base, base+0xbc602c8) (GEOMETRY-adjudicated digit; the relayed
"+0xc0602c8" double-counted the slide); Blocks[0] inside exactly one PUBLISHED
non-image readable span from the bridge's OWN frozen VMMAP capture (prot&READ,
size >= one FName block 0x20000, top-16 by size, ascending). A coverage miss
BELOW the captured envelope (max captured record end) is a genuine
unmapped/unreadable verdict; a miss AT/ABOVE it refuses "snapshot truncated:
cannot prove heap coverage" — 'not in table' never means 'unmapped' when the
capture is a truncated prefix. Entry-chain bounds and content identity
('None'/'ByteProperty' + behavioral probe) are untouched.

## Deliverables

- `staging/UnrealInitializer.cpp` (v3, 4 hunks), `staging/SeedValidation.hpp`,
  `staging/SeedTable.h` (byte-identical), `staging/SeedValidation.hpp` is
  co-located for both TUs.
- `staging/runtime_start_bridge_r13.geometrydiag-spanpub.cpp` (full staged file,
  geometrydiag base) + `staging/spanpub-hunks-vs-geometrydiag.diff` (trimmed
  hunks for the r14+diag fold).
- `host-test/test_pool_bounds_host.cpp` + compiled `host-test/test_pool_bounds`;
  receipt `receipts/fixture-pool-bounds.txt` (52/52),
  `receipts/PASS-pool-bounds-fixtures.json`.
- Gates: `receipts/compile-gate.{argv.json,stderr}` +
  `receipts/unreal-unrealinitializer-spanpub.o` (190-arg, swapped, exit 0,
  warning multiset identical to baseline);
  `receipts/bridge-{baseline-compile,spanpub-compile}.stderr` (0 bytes each) +
  `receipts/bridge-{baseline-geometrydiag,spanpub}.o`.
- `DESIGN.md` (candidate ranking + adjudications), `DIFF.md` (hunks + anchors +
  REQUIRED core-capture hunk), `CLAIM.json`.

## Required positive test (user directive)

A8: blocks0 = 0x300020000 — inside a published non-image readable span
[0x300000000,0x380000000) of a realistic walk, outside the image extent, and
OUTSIDE the retired frozen range — passes the REAL new validator and is refused
by the OLD predicate (replicated verbatim in the fixture solely for that
contradiction proof). H3/H4 extend it to the launch-9-class 10,019-record
complete capture (heap at record ~3.4k, ~12.9 GiB — far past any 1,024-record
prefix).

## Truncation honesty + coverage (same-capture rule)

Publication reads the SAME frozen `mappings[]` the lease registration consumed —
one capture, no second sysctl, `g_image_regions` untouched (image-clamped
ScopedRegions are opaque and cannot cover a heap pointer anyway). The envelope
(max captured record end) is published and consumed by the validator
(A17/H5). Coverage option (a): the bridge capture is already 16,384 static bss
vs observed vm_total 10,019 — complete with margin; the REQUIRED core-side hunk
(kMaxVmMappings 1024→16384; cpp:203 stack buffer → static bss, 384 KiB) is
staged in DIFF.md §3 for the controller (not my TU; not compiled here).

## Registration-internals disclosure (flagged per instruction)

The spanpub hunks are new self-contained functions EXCEPT exactly two additive
touches inside `bridge_register_readable_ranges`: (1) one
`diagnostic_event("bridge_spanpub_count", 0)` line on the walk-failure early
return; (2) one `bridge_spanpub_publish(mappings, count, extent_begin,
kImageExtentVaddr);` call after the existing loop. r14+diag re-application
anchors are in DIFF.md §2 (only the call line references r14-local identifiers:
the frozen capture array, its count, and the extent pair).

## Post-merge positive-test re-run recipe (controller)

1. Fold: initializer unit = my `staging/UnrealInitializer.cpp` +
   `staging/SeedValidation.hpp` + byte-identical `SeedTable.h`; bridge =
   consolidated r14+diag + my spanpub hunks (DIFF.md §2), with
   `SeedValidation.hpp` co-located (or -I my staging dir) and the core-capture
   hunk applied (DIFF.md §3).
2. Host-side (already green here): `clang++ -std=c++20 -Wall -Wextra
   -I<staging> -o test_pool_bounds host-test/test_pool_bounds_host.cpp` then
   `./test_pool_bounds <out>.txt`; expect exit 0 and "52 passed, 0 failed".
   NOTE: run the binary so it writes its receipt to a FILE argument — writing
   to the harness stdout pipe wedges uninterruptibly in this environment (see
   Environment note).
3. Target-side (the merged build): boot to menu; the seed log must show
   `bridge_spanpub_count>=1` (and `bridge_spanpub_env_low/high`) BEFORE the
   `[Seeds]` block, then `[Seeds] FName pool validated at 0x9765ec0 (…Blocks[0]
   0x… inside published non-image readable span [0x…,0x…)…)`. THAT line is the
   production positive pass (it sits after the pool shape + 'None'/
   'ByteProperty' entry reads, i.e. after actual reads through Blocks[0] on
   kernel-covered memory). Any `seed_refused` naming `span table unavailable`
   or `snapshot truncated` means the capture/publication side needs the DIFF.md
   §3 hunk or the r14 fold re-checked — never a fallback to the frozen band.

## Honest limits

- Host proof is pure-function + compile gates only: no target run, no console
  contact, no dereference of Blocks[0] exercised here (the entry chain and the
  behavioral probe are unchanged launch-10 code, verified on target separately).
- Top-16 cap can false-refuse if more than 16 qualifying non-image spans exist
  in one capture; saturation is counted and diagnosed
  (bridge_spanpub_total/cap) and the fixture pins the behavior (C5). No such
  shape has ever been observed (giant-span evidence).
- `kHeapLo/kHeapHi` remain in the generated SeedTable (generator contract,
  provenance) with zero remaining references.
- Environment note: repeated host runs of this fixture wedged in
  uninterruptible wait when writing stdout; `sample` showed the second wedge in
  strlen(nullptr) — a real fixture bug (std::string from a null .failure),
  fixed; the residual environment sensitivity to stdout pipes remains, so the
  binary writes its receipt to a file argument. Several of my own stuck fixture
  processes (0 CPU, UE state) from those runs may linger until logout; they are
  self-owned test binaries, not game/console processes.
- 5 of the initial 47-case FAILs were fixture-authoring defects (literal scale
  typos, one wrong expectation, one ordering-violating fixture); the validator
  classified all five correctly as run. Suite is 52/52 after fixture fixes.

## Non-claims

No in-game pass, no completion/quality claim, no claim about provider
concurrency or capacity. This worker made no web/console requests and edited
nothing outside its output directory.
