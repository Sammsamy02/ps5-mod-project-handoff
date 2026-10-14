# VALIDATOR-V4.md — what changed and why, per RE finding

Worker: worker-fname-validator-v4 · 2026-10-05 · LOCAL ONLY, proposal-only.
Ownership: the RE-correction of the FName pool validator (launch-10
correctness — v3 FALSE-REFUSED a valid pool). Staged copies only; the
controller folds into `final-units` after review (fold instruction: DIFF.md §0).

Evidence base: `outputs/2026-10-04-zcode-team/worker-re-fname-pool/FNAME-POOL-RE.md`
(Tier-1 capstone/lief on the sha-pinned decrypted eboot `3b6cfad4…`), plus the
coordinator refinements (storage separation; labeling law).

## 0. STORAGE SEPARATION (read this first)

Two different things must never be conflated (user directive):

1. **The ALLOCATOR OBJECT** — a static BSS FMallocBinned2-family object at
   runtime `0x97089b8` (76 pool tables x 0x78 B, per-table mutex), constructed
   in place by ctor `0x4afd40`, reachable through the `GMalloc` slot at runtime
   `0x9750950` (FNAME-POOL-RE §2.5). It lives in image BSS. That is a fact
   about the ALLOCATOR.
2. **The ALLOCATED BLOCKS** — `Blocks[i]`, each a 0x20000-byte, align-2
   `FMemory::Malloc` result handed out by that allocator's freelists/pools
   (FNAME-POOL-RE §2.3/§2.4). A block's storage class follows the allocator's
   pools; the static evidence chain (GMalloc → binned allocator → OS-backed
   pool pages) does NOT pin any block to image BSS, and RE §7.1 lists the
   blocks' runtime placement as UNRESOLVED.

**Provenance verdict (worker-blocks0-provenance, definitive): NO CAPTURE
EXISTS.** No pinned observation ever recorded an actual Blocks[0] pointer
value. The `[0x970c000,0x9a34000)` figure is MAPPING-CLASS ONLY (igf3
v1_vmmap_page seq 4); FNAME-POOL-RE.md lines 30/168 mislabeled it as a
pointer claim — that conflation is on record. A real capture may come later
from the fc1pool probe (built, never injected; staging pairing being prepped
for a future user-coordinated window). Until then **neither storage class is
'observed'**, and the two fixture classes are symmetric:

- **Heap-resident class (A8/H3):** plausible per STATIC RE — the allocation
  is `FMemory::Malloc(0x20000, align 2)` → GMalloc binned → OS-heap class,
  likely OUTSIDE the image. Unproven (no capture).
- **Candidate-image-resident class (V1/V1b/END-TO-END/H6):** plausible per
  the mapping-class record (LOAD7 BSS tail region). Unproven (no capture);
  labeled candidate/synthetic per the law.

The v4 span-membership-either-class admission is exactly the right response
to that uncertainty: ownership coverage (the bridge's own kernel VMMAP walk)
is enforced for whichever class reality turns out to be, and content identity
(the 'None' anchor) guards wrong values in both.

The launch-9 record `[0x970c000,0x9a34000)` = LOAD7's BSS tail is a
**REGION-CLASS observation of the mapping that carried Blocks[0]** — it is
NOT a captured Blocks[0] pointer value, and no pinned observation of the
actual pointer exists (a separate worker is checking whether any pinned
observation captured it). Under the labeling law, everything downstream says
exactly that: the image-resident pool shape is the SYNTHETIC
**candidate-image-resident-shape**, a MUST-PASS because it is plausible per
the RE record and v3 refused that class on region-class evidence alone.

Consequence for the validator: a hard region predicate (inside/outside image)
is unwritable from current evidence in EITHER direction (RE §4 item 3:
"evidence is contradictory — do not bake either answer into the validator").
v4 therefore drops the region predicate and keeps OWNERSHIP coverage
(span membership) + CONTENT identity (the 'None' anchor) as the guards.

## 1. R1 — CurrentBlock semantics (the false-refuse fix)

- **What v3 said:** `current_block >= 1` in `check_pool_shape`, and
  `block_index >= current_block` refusal ("at or past CurrentBlock") in
  `check_entry_bounds`. Both encode "CurrentBlock is a count of blocks".
- **What the engine does:** `CurrentBlock` (u32 @ pool+0x8) is the 0-BASED
  index of the OPEN block. `Init` @ file 0x23a5d0 memsets it to 0 and fills
  `Blocks[0]` immediately; `Reserve` @ 0x246090 writes entries INTO
  `Blocks[CurrentBlock]` and increments only on a 0x20000 boundary. The
  getter @ 0x23d570 admits any entry with `id>>16 <= CurrentBlock`. A pool
  with hundreds of names legitimately has `CurrentBlock == 0` — v3 refused
  exactly that shape.
- **v4 change (check_pool_shape):** the strongest predicate available from
  ALREADY-READ words replaces `current_block >= 1`:
  `(none_id >> 16) <= current_block` — the None entry's block index cannot
  exceed the open block. New refusal string: `"None entry block index exceeds
  CurrentBlock (engine invariant id>>16 <= CurrentBlock; pool not initialized
  or wrong pool/base)"`. `blocks0 != 0` and 8-alignment are kept EXACTLY. The
  `base` parameter is removed (it only fed the dropped extent clause); the
  per-launch base check stays in the initializer's Phase 1.
- **v4 change (check_entry_bounds):** same bug one level deeper — the OPEN
  block legitimately holds entries, and with `CurrentBlock == 0` every legit
  entry lives in it, so the end-to-end chain would still have false-refused
  without this. Engine semantics per RE §4 item 5:
  `block_index > current_block` refuses (`"entry block index exceeds
  CurrentBlock (engine invariant id>>16 <= CurrentBlock)"`);
  `block_index == current_block` requires `offset + needed <= CurrentByteOffset`
  (`"entry extends past CurrentByteOffset of the open block"`);
  older blocks pass pure-side (the sliver-terminator check is a read; content
  identity is carried by the anchor + entry matches downstream).
  Scope note: this function was not named in the task list, but task 4's
  end-to-end mandate is unsatisfiable without it (fixture END-TO-END proves
  the composed chain on `current_block == 0`).

## 2. R2 — image-inclusive readable-span admission (storage-class humility)

- **What v3 said:** publication clipped every record to its OUTSIDE-EXTENT
  remainder(s) and membership required Blocks[0] outside the image extent AND
  inside a NON-image published span.
- **Why that is wrong:** §0 — the only image-residency "evidence" is a
  region-class observation of Blocks[0]'s mapping; the allocator OBJECT being
  image BSS says nothing about where blocks land; refusing image residency
  refused the plausible live pool.
- **v4 change:** `publish_readable_spans` (new primary; the retired name
  `publish_nonimage_readable_spans` delegates byte-for-byte so the bridge TU
  compiles unchanged until the controller's one-line rename) publishes each
  READABLE record IN FULL — no extent clipping, one span per qualifying
  record. `check_pool_shape` membership accepts image OR non-image published
  spans. UNCHANGED on purpose: prot&READ requirement (exec-only text and
  prot-none never publish — V5/V6/A6ii), the 0x20000 one-block capacity
  floor, top-16-by-size window with saturation counters, ascending order,
  same-capture envelope + truncation honesty (A16/A17/H5), span-table-present
  loud refusal with no frozen fallback (B1-B5).
- **Honest limit, pinned as a fixture (V4a/V4b):** an image-resident value
  that is NOT a block (e.g. the pool label itself) can now pass the SHAPE.
  That is the accepted trade: shape checks are region/word-only; wrong-value
  protection moved to CONTENT (R3). Ownership coverage is not weakened — the
  address must still be covered by the bridge's own kernel VMMAP walk.

## 3. R3 — the entry-0 'None' content anchor

- **Evidence (STATIC):** `Init` stores "None" first (file 0x23a5d0:
  `lea rsi, [0x7b96c17] "None"` → store → `NoneId` @ pool+0x14040), so entry
  0 of Blocks[0] decodes as u16 header with `h>>6 == 4`, `h&1 == 0`, payload
  'None'.
- **v4 change:** new pure `check_none_anchor(entry_bytes, available)`
  (delegates to `entry_matches_ascii`, which enforces exactly those bit tests
  + payload). The initializer Phase 3 performs ONE bounded 6-byte read at
  `blocks0` through the same volatile byte-wise discipline as every other
  pool read (`read_bytes_at`), only AFTER the shape (span-membership
  ownership coverage) passed, and refuses TERMINALLY on mismatch:
  `"Blocks[0] entry 0 is not the ANSI 'None' anchor (wrong pool or corrupt
  block)"`. This is the admission-grade content identity that replaces the
  dropped outside-image clause as the wrong-base/wrong-pool guard.

## 4. Refusal-string ledger (for the fold review)

| verdict | string (prefix) | state |
|---|---|---|
| table missing | `span table unavailable (bridge published no readable spans this invocation)` | kept, "non-image" wording dropped |
| invariant violated | `None entry block index exceeds CurrentBlock (engine invariant id>>16 <= CurrentBlock; ...)` | NEW (replaces `CurrentBlock < 1 ...`) |
| null block | `Blocks[0] is null` | kept |
| unaligned | `Blocks[0] is not 8-aligned` | kept |
| anchor mismatch | `Blocks[0] entry 0 is not the ANSI 'None' anchor (wrong pool or corrupt block)` | NEW |
| uncovered below envelope | `Blocks[0] not covered by any published readable span (unmapped or unreadable ...)` | kept (now also the image-coverage refusal class) |
| truncated capture | `snapshot truncated: cannot prove heap coverage ...` | kept |
| extent refusal | `Blocks[0] inside the image extent ...` | RETIRED (verified absent from the gate object) |
| entry beyond open block | `entry block index exceeds CurrentBlock (engine invariant id>>16 <= CurrentBlock)` | NEW (replaces `... at or past CurrentBlock`) |
| open-block CBO | `entry extends past CurrentByteOffset of the open block` | reworded from `... of its block` |

All verified PRESENT/ABSENT in the compile-gate object (REPORT.md §3).

## 5. What v4 does NOT claim

- NO CAPTURE EXISTS (definitive provenance verdict): no pinned observation
  ever recorded an actual Blocks[0] pointer value; the image-resident
  positive case is SYNTHETIC (candidate-image-resident-shape), MUST-PASS by
  design, not a replay of a captured value (labeling law). NEITHER storage
  class is 'observed' — the heap-resident positives (A8/H3) are equally
  plausible-but-unproven (static RE predicts the ALLOCATION goes OS-heap via
  GMalloc binned even though the ALLOCATOR OBJECT is static BSS).
- No claim that Blocks[0] IS image-resident or IS heap-resident — storage
  class is unknown; v4 simply refuses to guess (RE §7.1 unresolved); a real
  capture may come later from the fc1pool probe (built, never injected).
- No target run, no console contact, no dereference exercised here; the
  anchor read + entry reads remain target-side code proven only by compile
  gate + host fixtures of the pure functions.
- Not identity-grade: a passed chain is necessary, not sufficient; the
  behavioral ByteProperty probe and downstream consumers are unchanged.
