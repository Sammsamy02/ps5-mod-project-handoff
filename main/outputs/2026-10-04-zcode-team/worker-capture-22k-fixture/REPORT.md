# REPORT.md — worker-capture-22k-fixture (2026-10-05)

## Task

Build the missing fixture for the >capture-capacity class: run1's live walk saw
`bridge_vmentry_entries=22761` against the 16,384 snapshot capacity
(truncated-honest; walk_stop=0) while run3 saw 10,028 (complete). The folded
v4 validator's envelope logic refuses "snapshot truncated" when blocks0 falls
at/above the captured envelope — designed for exactly this — but no fixture
exercised the >capacity shape with real-ish numbers.

## What was built

- `staging/SeedValidation.hpp` — byte-identical copy of the FOLDED v4 header
  (`outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp`,
  sha256 `f09ec9b3…a5de`, recorded in `receipts/sha256-staging.txt`). Originals
  untouched. Note: the folded header has NO retired alias — the fixture drives
  `publish_readable_spans` directly (the folded call-site shape).
- `host-test/test_capture22k_host.cpp` — 34-check suite extending the
  worker-fname-validator-v4 pattern (same harness, receipt-to-file argv[1],
  freestanding). One synthetic 22,761-record walk, address-ascending; bridge
  parity enforced by handing the filter ONLY the first 16,384 records (the
  stored snapshot prefix, like `bridge_spanpub_publish`'s bounded copy).
  Shape: pinned image low map + vintage giant + JIT (records 0-7), below-cut
  512 MiB heap span at record 8000, 16 medium readable spans below the cut
  (prefix qualifies 22 → top-16 saturates, mirroring the real G9
  `spanpub_cap=1`), three above-cut heap spans at records 17000/19000/22600
  (the last is the true heap top), sub-floor filler between.
- Receipts: `receipts/fixture-capture22k.txt` (+ byte-identical rerun),
  `receipts/fixture-compile.stderr` (0 bytes), `receipts/PASS-capture22k-fixtures.json`,
  `receipts/sha256-artifacts.txt`.

## Result: 34 passed, 0 failed (exit 0)

- Compile: `clang++ -std=c++20 -Wall -Wextra -I staging` — rc=0, zero warnings.
- Determinism: second run byte-identical.
- Suite map: R1-R5 real-run pinned constants (16,384 / 22,761 / 10,028 / 6,377
  omitted / folded-header constants); W0-W5 walk construction; P1-P8 prefix
  publication; E1-E5 the (c) envelope math; A1-A3 the (a) admit;
  T1-T5 the (b) truncation honesty; C1-C3 the (d) complete-capture contrast;
  D1 the caller-contract pin.

### The three directives, verified

(a) blocks0 `0x432320000` inside the below-cut published span **ADMITS**
(bounds_source `published-readable-spans`); boundaries lo-inclusive/hi−8;
END-TO-END shape + 'None' anchor + entry bounds pass.

(b) blocks0 inside EACH above-cut span (`0x6cf020000`, `0x76ba20000`,
`0x870c20000`) refuses **"snapshot truncated: cannot prove heap coverage at or
past the captured envelope (omitted mapping, not proven unmapped)"** with
bounds_source `capture-truncated` — never the "not covered/unmapped" class.
Boundary split ON the cut: blocks0 == envelope → truncation; envelope−8
(below the cut, unpublished sub-floor record) → honest unmapped class.

(c) `envelope_end = 0x6a39e8000` = max end over the CAPTURED prefix = last
captured record end — **the cut, not the true heap top** (`0x89c008000`).
First omitted record (16,384) starts at/above the envelope: omitted ⇔
at/above the cut. The published table holds no above-cut span and every
published span ends at/below the envelope.

(d) Contrast: publishing all 22,761 records moves the envelope to the true
walk end and the SAME three blocks0 values ADMIT — the truncation refusal is a
capture-coverage property of the 16,384 cut, and the capacity bump is the
matching lever.

Fixture defects found during development: 2 initial FAILs were fixture-side
arithmetic (the 128 KiB tail record {0x100118000,0x100138000,3} is exactly one
FName block capacity and QUALIFIES; totals are 22/25, not 21/24 — at-floor is
inclusive). The validator classified correctly in both runs. Fixed, rerun
34/34.

## Launch behavior to expect on a >16,384 boot

- Diagnostics: `bridge_vmentry_entries=<total parsed>` (may exceed 16,384 —
  it counts ALL parsed records), `bridge_vm_walk_stop=0` (the sysctl payload
  parsed cleanly; the cut is the SNAPSHOT capacity), then
  `bridge_spanpub_count=16`, `bridge_spanpub_total=<prefix qualifying>`,
  `bridge_spanpub_cap=1`, `bridge_spanpub_env_low/high` = the cut as a 64-bit
  split (G9 parity).
- Pool admission happens ONLY when blocks0 lands below the cut AND inside a
  published span (the two conditions are distinct — a below-cut span can lose
  the top-16 size contest; documented v4 C5 limit, counted via
  `bridge_spanpub_total/cap`).
- Above the cut: `check_pool_shape` refuses "snapshot truncated …" on every
  50 ms poll until the validation budget expires, then the terminal refusal:
  `[Seeds] REFUTED: FName pool at 0x<pool>: snapshot truncated: … (observed
  CurrentBlock <N>, Blocks[0] 0x<blocks0>; published readable spans <N>).
  Terminal for this launch; derive afresh with new evidence, never replay.`
  Module start fails; the game survives; no frozen-envelope fallback.
- run3 (10,028 ≤ 16,384) is the production norm: complete capture.

Full details + exact literals: `CAPTURE-CAPACITY.md` sections 4-5.

## For launch-12

If the truncated refusal fires live, the known lever is capacity — but first
verify the pool worker's REQUIRED core hunk (kMaxVmMappings 1024→16384 + the
`static` bss placement, DIFF.md §3) is actually folded in the launch-12 unit;
then consider 16,384 → 32,768. The stack-vs-bss law from that core hunk
applies to every array doubled: "stack would be unsafe on a game-hijacked
thread" — 32,768 × 24 B OsVmMapping = 768 KiB BSS (lmf side), 32,768 × 24 B
VmSpanRecord = 768 KiB BSS (bridge spanpub side), and the raw kinfo_vmentry
buffer at 32,768 × 1,160 B ≈ 36.3 MiB BSS (≈ 18.1 MiB today). Any bump is a
new bounded revision with distinct IDs; check the PRX BSS budget; never
loosen the validator to admit past the cut and never replay the refused
launch.

## Honest limits

Host pure-function proof only: no target run, no console contact, no
dereference of Blocks[0]. The walk is SYNTHETIC — 22,761/16,384/walk_stop=0/
10,028/spanpub_cap=1 are pinned real-run facts, but the address layout past
the pinned image spans is plausible-not-captured, and no real Blocks[0]
pointer value exists (worker-blocks0-provenance). The suite asserts relations
(envelope = captured max end; omitted ⇔ at/above envelope; above-cut ⇒
truncation class), not a predicted real map. Absolute fixture addresses
(e.g. the cut 0x6a39e8000) are synthetic; a real boot's cut differs.
