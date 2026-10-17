# DESIGN.md — worker-fname-heap-runtime-bounds (launch-10 blocking fix, launch10-c)

Status: staged proposal, host-proven, compile-gated both sides. Not folded into any
canonical file; the controller folds (initializer unit = my staged copy; bridge =
my spanpub hunks re-applied onto consolidated r14+diag).

## 0. Problem

launch-10's armed admission gates `Blocks[0]` through the frozen span
`[kHeapLo,kHeapHi) = [0x8018c000,0x100118000)`. GEOMETRY.md section 6 adjudicated
that span as VINTAGE-TRANSIENT non-image memory: real at igf/igf2, absent from the
igf3 7-span walk. A live heap anywhere else makes the armed path refuse loudly —
safe, but it masks every deeper validation step and can never admit a present-day
pool whose heap does not live in that vintage band.

## 1. Candidate ranking (dispatch task 2; the original three sources)

| rank | source | safety (new reads) | false-accept | false-refuse | cost | verdict |
|---|---|---|---|---|---|---|
| 1 | (a) bridge VMMAP walk results, published as a bounded non-image readable-span table | ZERO new memory reads: the publication is a filter over the ALREADY-taken frozen capture (same-capture rule); admission is span membership of an already-read value | a wrong pointer into ANOTHER readable non-image span passes shape — but the downstream 'None'/'ByteProperty' entry chain + behavioral probe still refutes (admission stays necessary-not-sufficient, same grade as launch-10) | only if the heap span is (i) sub-floor (impossible: a span must hold one whole 0x20000 FName block) or (ii) dropped by the top-16 cap (counted, diagnosed) — and truncation is distinguished from unmapped via the captured envelope | bridge: ~80 lines additive; initializer: ~40; rc_seed pure + host-testable | **ADOPTED, narrowed by the controller's predicate** |
| 2 | (c) guarded read through Blocks[0] (readability classification instead of a numeric range) | one guarded read through the unvalidated pointer — worse than (a): it PERFORMS the read the shape check exists to gate | readability alone admits any readable span, incl. image BSS (wrong-pool passes shape) | low | low | folded INTO (a): the read-classification role is played by kernel-reported span membership; no probe read exists in the final design |
| 3 | (b) FMallocBinned3 internal allocation metadata | reads INSIDE an unpinned private layout — every read is a new unverifiable class | wrong-layout guesses pass garbage | high (layout guess) | a full new derivation task (only the vtable/instance addresses are pinned by GMALLOC-CANDIDATES.json) | REJECTED |

The controller's ownership relays further narrowed (a): publication source = the
walk-results snapshot (NEVER `g_image_regions`, which is image-extent-clamped and
opaque `ScopedRegion` tokens — image-only by design); admission class (ii)
(non-image) only; empty table = loud refusal, never a frozen-envelope fallback.

## 2. The extent digit (adjudicated before implementing)

The relay wrote "[base, base+0xc0602c8)". With base 0x400000 that double-counts
the slide (end 0x100602c8 would classify the vintage heap band as "image").
GEOMETRY.md sections 3 and 8-item-4 are the authority: file-vaddr extent =
max(p_vaddr+p_memsz) = **0xbc602c8**; runtime extent = [0x400000, 0xc0602c8) =
[base, base+0xbc602c8). The bridge's own `kImageExtentVaddr = 0xbc602c8` agrees.
Implemented as `rc_seed::kImageFileExtentBytes = 0xbc602c8`; fixture G2 pins
base+extent == 0xc0602c8.

## 3. Final admission predicate (check_pool_shape v2)

`check_pool_shape(current_block, blocks0, published_spans, published_span_count,
capture_envelope_end, base) -> {ok, failure, bounds_source}`

In order:
1. table unavailable (null/zero count) → refuse, naming "span table unavailable",
   bounds_source `span-table-unavailable` — FIRST, so an environment failure is
   named loudly for any pool state; never a frozen-envelope fallback.
2. `current_block < 1` → legacy string preserved.
3. `blocks0 == 0` → legacy string preserved.
4. `blocks0 & 7` → "Blocks[0] is not 8-aligned" (allocator-produced blocks are
   at least 8-aligned; strengthens "plausible allocated memory").
5. inside [base, base+0xbc602c8) → refuse "inside the image extent
   (wrong-base/wrong-pool…)": FNAME-VERDICT section 4 — the pool struct lives in
   phdr7 BSS (image), the blocks come from Malloc (heap, outside).
6. membership in one published span → PASS, bounds_source
   `published-readable-spans`. Spans are kernel-reported prot&READ, outside the
   extent, remainder >= one FName block capacity (0x20000 = FNameBlockOffsets ×
   Stride 2, from pinned kPoolLayout/NameTypes.hpp:154-158), top-16 by size,
   ascending by address.
7. miss below the envelope → refuse "not covered … capture complete below the
   envelope" (genuine unmapped/unreadable).
8. miss at/above the envelope → refuse "snapshot truncated: cannot prove heap
   coverage …" (bounds_source `capture-truncated`) — 'not in table' NEVER means
   'unmapped' when the capture is a truncated prefix (kernel enumeration is
   address-ordered).

Port-header validation of the predicate (relay item 3): block decomposition
`id>>16 / (id&0xFFFF)*2` + `FNameMaxBlocks=8192` + `FNameBlockOffsetBits=16`
(NameTypes.hpp:154-158), pool layout +0x8/+0xC/+0x10 and interned ids at
+0x14040/+0x14044 (FNAME-VERDICT section 4, recovered from the init/find/insert
callees). Entry-chain reads through Blocks[0] remain bounded by the UNCHANGED
check_entry_bounds; content identity stays in the unchanged 'None'/'ByteProperty'
matching + behavioral probe.

## 4. Publication (bridge side, same-capture rule)

`bridge_spanpub_publish(mappings, count, extent_lo, extent_bytes)` runs inside
`bridge_register_readable_ranges` (r13 geometrydiag base; r14 anchor = after its
frozen-capture walk completes) over the SAME `mappings[]` snapshot the lease
registration consumed — one capture, no second sysctl, no `g_image_regions`
repurposing. It copies the WHOLE snapshot (non-readable records still prove
enumeration coverage) and calls the pure
`rc_seed::publish_nonimage_readable_spans` (host-tested here, single
implementation both sides consume). Accessors (extern "C", intra-module, no new
PRX imports/exports): `bridge_readable_span_count`, `bridge_readable_span_at`,
`bridge_capture_envelope`. Diagnostics: `bridge_spanpub_count/total/cap/env_low/
env_high`.

Coverage (coordinator option (a)): the bridge capture is ALREADY 16,384 records
in static bss (r4; launch-2 ENOMEM evidence) vs launch-9 observed vm_total
10,019 → complete with margin; fixture H proves a complete 10,019-record capture
publishes a heap span at record ~3.4k (~12.9 GiB) and admits the real pool. The
CORE-side capture (live_module_facts.hpp:85 `kMaxVmMappings=1024`, cpp:203 STACK
buffer) is the conversion-side VmScan, NOT the admission input; its
16384+static-bss bump is delivered as a REQUIRED staged hunk in DIFF.md (§3) for
the controller's fold — not compiled by me (different worker's TU).

## 5. Rejected variants (kept for the record)

- Frozen-span fallback when derivation is unavailable: SUPERSEDED by controller
  directive — empty table is a loud refusal; B5 fixture pins that an in-envelope
  blocks0 is refused without a table.
- Publishing image spans / accepting image-resident blocks0: refused by the
  extent check regardless of publication content (fixture D3).
- Widening `g_image_regions` to cover the heap: would change PLH lease semantics
  (scan could walk the heap) — publication is an admission table only; the lease
  registration set stays byte-identical.
