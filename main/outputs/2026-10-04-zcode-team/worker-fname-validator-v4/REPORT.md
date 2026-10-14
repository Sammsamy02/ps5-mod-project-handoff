# REPORT.md — worker-fname-validator-v4

Verdict: the pool validator v3 FALSE-REFUSED a valid pool; the staged v4
correction is RE-grounded, host-proven 69/69 (all-PASS receipt recorded), and
compile-gated exit 0 with a warning multiset IDENTICAL to the v3 baseline and
all new refusal strings verified present in the gate object (retired strings
verified absent). Staged proposal only — nothing outside my output directory
was edited. The controller folds (DIFF.md §0 has the exact file map).

## 1. What changed (one paragraph)

`check_pool_shape` now enforces the ENGINE's own invariant — `none_id>>16 <=
current_block` (0-based open block; getter @0x23d570) — evaluated on
already-read words, replacing `current_block >= 1`; `blocks0 != 0` and
8-alignment are kept exactly. Publication (`publish_readable_spans`) and
membership now accept readable spans IMAGE OR NON-IMAGE — the outside-image
clause is dropped because the only image evidence is a REGION-CLASS mapping
observation (LOAD7 BSS tail [0x970c000,0x9a34000)), not a captured pointer,
and the ALLOCATOR OBJECT (static image-BSS FMallocBinned2-family, runtime
0x97089b8) is a different thing from the ALLOCATED BLOCKS it returns (storage
class of Blocks[0] unresolved, RE §7.1). Span-membership ownership coverage is
unchanged (prot&READ, 0x20000 floor, top-16, same-capture envelope,
truncation honesty, loud span-table refusal, no frozen fallback). NEW content
guard: `check_none_anchor` — entry 0 of Blocks[0] must decode as ANSI 'None'
(h>>6==4, h&1==0, payload 'None'; static evidence: Init stores it first),
checked by one bounded 6-byte volatile read at blocks0 AFTER the shape passes,
terminal on mismatch. `check_entry_bounds` got the same R1 fix (the OPEN
block legitimately holds entries; CBO bounds them) — without it the
end-to-end chain would still have false-refused at `current_block == 0`;
this scope extension is forced by the end-to-end mandate and is documented in
VALIDATOR-V4.md §1.

## 2. Proof state

- Host fixtures (real staged header, end-to-end): 69 passed, 0 failed, exit 0.
  `receipts/fixture-pool-bounds-v4.txt` +
  `receipts/PASS-pool-bounds-v4-fixtures.json`. Suite:
  `host-test/test_pool_bounds_host.cpp` (self-contained, freestanding;
  sha256 674897f05b5969cc8bf8be0381801ed05ef690127bd39e8419af3e8c6ca6d326).
  Rerun recipe:
  `clang++ -std=c++20 -Wall -Wextra -I<worker>/staging -o t host-test/test_pool_bounds_host.cpp`
  then `./t <receipt-path>` (binary writes its own receipt file; stdout pipe
  writes wedge in this environment).
- BOTH must-pass positives present and symmetric (provenance verdict:
  NO CAPTURE EXISTS — neither storage class is 'observed'):
  A8/H3 heap-resident class (static RE predicts the ALLOCATION via
  FMemory::Malloc(0x20000, align 2) -> GMalloc binned -> OS-heap, likely
  outside the image, even though the ALLOCATOR OBJECT is static BSS) and
  V1/V1b/END-TO-END/H6 candidate-image-resident class (mapping-class record
  only, labeled candidate/synthetic per the law). A real capture may come
  later from the fc1pool probe (built, never injected).
- Compile gate: the EXACT v3 190-entry argv (program + 189 args), exactly 3
  entries swapped (-c/-o/-MF), `LLVM_CONFIG=…/LLVM-21.1.8-macOS-ARM64/bin/
  llvm-config`: exit 0, 6 warnings = the baseline multiset (verified by
  normalized multiset diff against the v3 gate stderr) — zero new warning
  classes. `receipts/compile-gate.{argv.json,stderr,stdout}` +
  `receipts/unreal-unrealinitializer-v4.o`.
- Object strings: all new refusal strings PRESENT (plain ASCII `const
  char*`), success-log literal present as UTF-16LE (STR() wide literal), all
  four retired strings ABSENT (`inside the image extent (wrong-base`,
  `CurrentBlock < 1 (pool not initialized`, `non-image readable spans`,
  `at or past CurrentBlock`). Audit: REPORT §3 table + `receipts/object-strings.txt`.

## 3. Object string audit table

| string | state in gate object |
|---|---|
| `None entry block index exceeds CurrentBlock` | PRESENT |
| `not the ANSI 'None' anchor` | PRESENT |
| `span table unavailable (bridge published no readable spans this invocation)` | PRESENT |
| `snapshot truncated: cannot prove heap coverage` | PRESENT |
| `Blocks[0] not covered by any published readable span` | PRESENT |
| `entry block index exceeds CurrentBlock (engine invariant` | PRESENT |
| `entry extends past CurrentByteOffset of the open block` | PRESENT |
| `Blocks[0] is null` / `Blocks[0] is not 8-aligned` | PRESENT |
| `inside a published readable span (image or non-image) of` + `'None' anchor ok` | PRESENT (UTF-16LE) |
| `inside the image extent (wrong-base` | ABSENT (retired) |
| `CurrentBlock < 1 (pool not initialized` | ABSENT (retired) |
| `non-image readable spans` | ABSENT (retired) |
| `at or past CurrentBlock` | ABSENT (retired) |

## 4. Key fixture cases (full map in DIFF.md §3)

- V1/V1b + END-TO-END: candidate-image-resident-shape (SYNTHETIC, labeling
  law) — blocks0 in the LOAD7 readable span, `current_block == 0`, post-Init
  words, None/ByteProperty entry bounds in the open block, 'None' anchor:
  the whole chain MUST-PASS. This is the class v3 refused.
- V2: `none_id` block 1 with `current_block == 0` refuses (the directed
  engine-invariant refusal). V3: block == CurrentBlock passes.
- V4a/V4b: pinned honest limit — an image-resident non-block value (the pool
  label itself) passes the SHAPE and is refused by the ANCHOR; wrong-value
  protection is now content-level, by design (R2/R3 trade).
- A8/A9/H3/H4: the non-image heap positives survive (incl. the retired-frozen
  contradiction proof). A16/A17/H5: truncation honesty unchanged. B1-B5:
  loud no-fallback unchanged. C1-C5: top-16 cap unchanged. A6ii/V5/V6:
  exec-only/prot-none still never admit (coverage class). AL1: the retired
  alias the bridge still calls is byte-identical to the v4 primary.

## 5. Fold notes for the controller (details in DIFF.md §0)

1. Replace `final-units/SeedValidation.hpp` and `final-units/
   UnrealInitializer.cpp` with my staged copies; `SeedTable.h` unchanged.
2. ONE-LINE bridge companion (behavior-identical, required cleanup):
   `runtime_start_bridge_r14_diag_spanpub.cpp` ~line 730
   `publish_nonimage_readable_spans(` -> `publish_readable_spans(`, then
   delete the retired alias. Until renamed, the alias delegates
   byte-for-byte, so the bridge compiles and behaves correctly either way
   (fixture AL1). Two bridge comments ("non-image readable spans only",
   "image memory is refused by the extent check") become stale — comment-only.
3. Signature greps for review: `check_pool_shape` gains `none_id` (2nd param)
   and loses `base`; new `publish_readable_spans`; new `check_none_anchor`;
   `check_entry_bounds` signature unchanged (semantics corrected).
4. On-target expectation after fold: the success line now reads
   `[Seeds] FName pool validated at … inside a published readable span (image
   or non-image) of N; 'None' anchor ok; …`. A refusal naming `None entry
   block index exceeds CurrentBlock` or `not the ANSI 'None' anchor` is a
   genuine wrong-pool/corruption verdict — never replay it without new
   derivation evidence.

## 6. Honest limits / non-claims

- Pure-function + compile gates only: no target run, no console contact, no
  dereference of Blocks[0] exercised (the anchor read and entry reads are
  target-side code, proven here by compile + host fixtures of the pure
  parts).
- The image-resident positive is SYNTHETIC (candidate-image-resident-shape);
  NO CAPTURE EXISTS (definitive provenance verdict) — neither storage class
  (heap-resident nor candidate-image-resident) is 'observed'; both are
  plausible-but-unproven and both carry must-pass positives; storage class
  of Blocks[0] remains UNKNOWN and v4 does not guess in either direction.
  A real capture may come later from the fc1pool probe (built, never
  injected; future user-coordinated window).
- Top-16 cap can false-refuse if >16 qualifying readable spans exist in one
  capture (counted + diagnosed; image spans now compete for the window — the
  three LOAD image spans are large and survive realistically; documented,
  fixture C5).
- One fixture-side defect during development (V3 literal 0x20010 decodes to
  block 2, not 1); the validator classified it correctly; fixed and rerun
  69/69.
- This worker made no web/console requests, spawned no other workers, and
  edited nothing outside
  `outputs/2026-10-04-zcode-team/worker-fname-validator-v4/`.

## 7. Deliverables

- `staging/SeedValidation.hpp` (v4), `staging/UnrealInitializer.cpp` (v4),
  `staging/SeedTable.h` (byte-identical), `staging/*-v3-to-v4.diff`
- `host-test/test_pool_bounds_host.cpp` + compiled `host-test/test_pool_bounds_v4`
- `receipts/`: fixture-compile.stderr (0 B), fixture-pool-bounds-v4.txt,
  PASS-pool-bounds-v4-fixtures.json, compile-gate.{argv.json,stderr,stdout},
  unreal-unrealinitializer-v4.{o,d}, object-strings.txt
- `DIFF.md` (fold instruction + full unified diffs + fixture case map),
  `VALIDATOR-V4.md` (what/why per RE finding + storage separation),
  `CLAIM.json`, `REPORT.md` (this file)
