# DIFF.md — worker-fname-validator-v4 (RE-correction of the pool validator)

Base: `outputs/2026-10-04-zcode-controller/launch10-build/final-units/` (v3).
Evidence: `outputs/2026-10-04-zcode-team/worker-re-fname-pool/FNAME-POOL-RE.md`
(R1 CurrentBlock 0-based open block, engine predicate `id>>16 <= CurrentBlock`
@0x23d570; R2 storage separation + region-class-only image evidence; R3 entry-0
'None' anchor). Coordinator refinements folded: storage separation, labeling
law, no reinstated outside-image restriction.

## 0. FOLD INSTRUCTION (exact)

| staged file (this dir) | replaces | note |
|---|---|---|
| `staging/SeedValidation.hpp` | `outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp` | whole file; consumed by BOTH the initializer TU and the bridge TU (`--stage-header` co-location unchanged) |
| `staging/UnrealInitializer.cpp` | `outputs/2026-10-04-zcode-controller/launch10-build/final-units/UnrealInitializer.cpp` | whole file; 7 hunks confined to the `SeedArming` seed path (§2) |
| `staging/SeedTable.h` | — | NO change; byte-identical copy staged only for include resolution. Do not replace. |
| `host-test/test_pool_bounds_host.cpp` | — | host fixture suite (not part of final-units); rerun recipe in REPORT.md §2 |

REQUIRED companion fold (bridge TU, ONE line, behavior-identical):
`outputs/2026-10-04-zcode-controller/launch10-build/final-units/runtime_start_bridge_r14_diag_spanpub.cpp`
line ~730: switch `rc_seed::publish_nonimage_readable_spans(` to
`rc_seed::publish_readable_spans(` and delete the retired alias from
`SeedValidation.hpp` (marked RETIRED NAME in the staged header). Until that
rename lands, the alias delegates byte-for-byte to the v4 filter, so the
bridge compiles and behaves correctly either way (proven by fixture AL1).
Completion marker: `publish_nonimage_readable_spans` must have ZERO references
after the fold. The two bridge comments that say "non-image readable spans
only / image memory is refused by the extent check" (bridge TU ~lines 700 and
743) become stale at fold — comment-only, update or strike them in the same
change.

Signature changes to grep for at fold review:
- `check_pool_shape(current_block, none_id, blocks0, spans, count, envelope)` —
  `none_id` INSERTED as 2nd param; `base` REMOVED (only fed the dropped
  image-extent clause). Sole caller: the staged initializer Phase 3.
- `publish_readable_spans(...)` — new primary; same params as v3
  `publish_nonimage_readable_spans` (extent args now `[[maybe_unused]]`,
  call-site stability).
- `check_none_anchor(const uint8_t*, size_t)` — new.
- `check_entry_bounds` — signature UNCHANGED; semantics corrected (open block
  holds entries; CBO bounds them).

## 1. SeedValidation.hpp — final-units v3 -> staged v4 (full unified diff)

```diff
--- /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-build/final-units/SeedValidation.hpp	2026-10-05 08:37:07
+++ /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-fname-validator-v4/staging/SeedValidation.hpp	2026-10-05 09:55:35
@@ -7,6 +7,44 @@
 // NameTypes.hpp FNamePool constants: FNameBlockOffsetBits=16, entry stride 2,
 // FNameEntryHeader bIsWide bit0 / LowercaseProbeHash bits5:1 / Len bits15:6).
 //
+// ---- v4 RE-correction (worker-fname-validator-v4, 2026-10-05) --------------
+// Three launch-10 predicates FALSE-REFUSED a legitimate pool, per
+// FNAME-POOL-RE.md (worker-re-fname-pool, Tier-1 capstone/lief on the
+// sha-pinned decrypted eboot 3b6cfad4...):
+//   (R1) CurrentBlock is the 0-BASED index of the OPEN block — 0 is valid and
+//        common (FNamePool::Init memsets it to 0 and fills Blocks[0]
+//        immediately; file 0x23a5d0). The engine's own getter (@0x23d570)
+//        admits any entry with id>>16 <= CurrentBlock. v3's
+//        `current_block >= 1` (and check_entry_bounds' strict
+//        `block_index < current_block`) encoded a wrong "count" semantics.
+//   (R2) STORAGE SEPARATION (labeling law): the pool's allocator is a
+//        STATIC BSS FMallocBinned2-family OBJECT (runtime 0x97089b8, 76
+//        pool tables, image BSS) — that is the ALLOCATOR, not the blocks
+//        it returns. Blocks[0] is an ALLOCATED BLOCK whose storage class
+//        follows the allocator's pools and is NOT pinned by static
+//        evidence (RE section 7.1 lists its runtime placement as
+//        unresolved). The launch-9 record for Blocks[0]'s mapping,
+//        [0x970c000,0x9a34000) = LOAD7's BSS tail INSIDE the image extent
+//        [0x400000,0xc0602c8), is a REGION-CLASS observation, not a
+//        captured Blocks[0] pointer value. Either way v3's hard
+//        "outside the image extent" clause could refuse a legitimate
+//        pool on region-class evidence alone; image residency is
+//        therefore no longer refusal evidence (RE section 4 item 3:
+//        evidence contradictory — do not bake a region predicate in).
+//   (R3) Entry 0 of Blocks[0] is ANSI "None" — STATIC evidence:
+//        FNamePool::Init stores 'None' first (file 0x23a5d0: lea rsi,
+//        [0x7b96c17] "None" -> store; FNAME-POOL-RE section 2.3), so the
+//        entry decodes as u16 header with h>>6 == 4, h&1 == 0 and payload
+//        'None'. This cheap content anchor replaces the dropped
+//        outside-image clause as the wrong-base/wrong-pool guard.
+// v4 changes ONLY what R1-R3 require: the engine invariant
+// none_id>>16 <= current_block, image-INCLUSIVE span publication/membership
+// (span-membership ownership coverage is unchanged), the entry-0 'None'
+// anchor, and the matching check_entry_bounds correction (open block holds
+// entries; CBO bounds them). Everything else — span-table-present loud
+// refusal, no frozen fallback, truncation honesty, alignment, block
+// capacity floor, top-16 publication window — is byte-for-byte v3.
+//
 // This header is deliberately freestanding (<cstdint>/<string_view> only) so
 // the same bytes are host-tested with fixtures and compiled into the merged
 // unit. All functions are total: they take plain values, never dereference.
@@ -59,13 +97,18 @@
     }
 
     // ---- launch10-c runtime bounds (worker-fname-heap-runtime-bounds) ----
+    // ---- v4 relaxation: worker-fname-validator-v4 (FNAME-POOL-RE.md R2) ----
     // The frozen heap envelope kHeapLo/kHeapHi = [0x8018c000,0x100118000) is
     // RETIRED from admission: it is vintage-transient non-image memory
     // (GEOMETRY.md section 6 — real at igf/igf2, absent from the igf3 7-span
     // walk), so a live heap placed anywhere else refused a legitimate pool.
     // The bound is replaced by CURRENT-INVOCATION span publication: the bridge
-    // TU publishes the non-image readable spans of ITS OWN kernel VMMAP walk
-    // (same walk that feeds bridge_register_readable_ranges), and admission
+    // TU publishes the readable spans of ITS OWN kernel VMMAP walk (same walk
+    // that feeds bridge_register_readable_ranges) — since v4 the published
+    // spans are IMAGE OR NON-IMAGE (the allocator OBJECT is image BSS and
+    // the region-class observation for Blocks[0]'s mapping was image BSS
+    // tail; image residency is not refusal evidence — see
+    // publish_readable_spans) — and admission
     // requires Blocks[0] to fall inside exactly one published span. Pure
     // values only — every function here stays total (never dereferences);
     // the caller owns all reads and has already classified the pool label as
@@ -128,16 +171,28 @@
 
     // Pure publication filter over one kernel VMMAP walk snapshot (the
     // bridge calls this over its FROZEN capture — the same-capture rule; host
-    // fixtures drive the SAME function). A record publishes its
-    // OUTSIDE-EXTENT remainder(s) when: prot carries the READ bit, the
-    // remainder is at least one FName block capacity, and it fits the
-    // top-N-by-size window. Image memory is never published (admission
-    // accepts class (ii) only). The caller passes the WHOLE captured record
-    // set (readable or not): envelope_end is computed over ALL of it, since
-    // non-readable records still prove enumeration coverage.
-    [[nodiscard]] inline auto publish_nonimage_readable_spans(
+    // fixtures drive the SAME function). v4 (FNAME-POOL-RE.md R2): a record
+    // publishes ITSELF — its FULL [start,end) — when prot carries the READ
+    // bit, it is at least one FName block capacity, and it fits the
+    // top-N-by-size window. Image memory is NOT excluded anymore. Why:
+    // the ALLOCATOR OBJECT is static image BSS, but Blocks[0] is an
+    // ALLOCATED BLOCK whose storage class is not pinned (RE section 7.1);
+    // the launch-9 REGION-CLASS observation placed Blocks[0]'s mapping in
+    // LOAD7's BSS tail INSIDE the image extent, so v3's outside-extent
+    // clipping (and the consumer-side outside-image refusal) could
+    // false-refuse a legitimate pool on region-class evidence. Readable
+    // image spans (LOAD1 R--, LOAD3/LOAD7 RW-) therefore publish alongside
+    // non-image heap spans, and admission is ownership-covered by span
+    // membership either way. The extent arguments are retained for
+    // CALL-SITE STABILITY ONLY (the bridge TU passes the same literals; the
+    // controller may drop them in a later signature fold). The caller
+    // passes the WHOLE captured record set (readable or not): envelope_end
+    // is computed over ALL of it, since non-readable records still prove
+    // enumeration coverage.
+    [[nodiscard]] inline auto publish_readable_spans(
         const VmSpanRecord* records, size_t record_count,
-        uint64_t image_base, uint64_t image_extent_bytes,
+        [[maybe_unused]] uint64_t image_base,
+        [[maybe_unused]] uint64_t image_extent_bytes,
         AddressSpan* out, size_t out_capacity) -> PublicationResult
     {
         // Top-N by size, insertion-maintained; address-sorted on the way out.
@@ -145,8 +200,6 @@
         uint64_t kept_size[kMaxPublishedSpans];
         size_t kept_count = 0;
         PublicationResult result{0, 0, 0, 0};
-        const uint64_t extent_lo = image_base;
-        const uint64_t extent_hi = image_base + image_extent_bytes;
         for (size_t i = 0; i < record_count; ++i)
         {
             const VmSpanRecord& r = records[i];
@@ -158,43 +211,31 @@
             {
                 continue;
             }
-            // Two disjoint outside-extent remainders per record (a record
-            // straddling the whole extent is pathological but handled).
-            const AddressSpan parts[2] = {
-                AddressSpan{r.start, r.end < extent_lo ? r.end : extent_lo},
-                AddressSpan{r.start > extent_hi ? r.start : extent_hi, r.end}};
-            for (const auto& span : parts)
+            const uint64_t size = r.end - r.start;
+            if (size < kFNameBlockCapacityBytes)
             {
-                if (span.lo >= span.hi)
+                continue; // cannot host a whole FName block
+            }
+            ++result.total;
+            if (kept_count < kMaxPublishedSpans)
+            {
+                kept[kept_count] = AddressSpan{r.start, r.end};
+                kept_size[kept_count] = size;
+                ++kept_count;
+            }
+            else
+            {
+                // Replace the smallest kept span when the new one is
+                // strictly larger (deterministic top-N by size).
+                size_t smallest = 0;
+                for (size_t k = 1; k < kept_count; ++k)
                 {
-                    continue; // empty or fully inside the extent
+                    if (kept_size[k] < kept_size[smallest]) { smallest = k; }
                 }
-                const uint64_t size = span.hi - span.lo;
-                if (size < kFNameBlockCapacityBytes)
+                if (size > kept_size[smallest])
                 {
-                    continue; // cannot host a whole FName block
-                }
-                ++result.total;
-                if (kept_count < kMaxPublishedSpans)
-                {
-                    kept[kept_count] = span;
-                    kept_size[kept_count] = size;
-                    ++kept_count;
-                }
-                else
-                {
-                    // Replace the smallest kept span when the new one is
-                    // strictly larger (deterministic top-N by size).
-                    size_t smallest = 0;
-                    for (size_t k = 1; k < kept_count; ++k)
-                    {
-                        if (kept_size[k] < kept_size[smallest]) { smallest = k; }
-                    }
-                    if (size > kept_size[smallest])
-                    {
-                        kept[smallest] = span;
-                        kept_size[smallest] = size;
-                    }
+                    kept[smallest] = AddressSpan{r.start, r.end};
+                    kept_size[smallest] = size;
                 }
             }
         }
@@ -217,12 +258,53 @@
         return result;
     }
 
-    // FNAME-VERDICT.md section 5 steps 1-2, launch10-c edition: CurrentBlock
-    // >= 1; Blocks[0] nonzero, 8-aligned, OUTSIDE the image extent, and
-    // inside exactly one PUBLISHED current-invocation readable span (class
-    // (ii), non-image). No frozen envelope, no fallback: an empty span table
-    // is a loud refusal naming it, never silent admission. Content identity
-    // stays downstream ('None'/'ByteProperty' entry matching + probe id).
+    // RETIRED NAME — kept ONLY so the bridge TU
+    // (runtime_start_bridge_r14_diag_spanpub.cpp:730) compiles UNCHANGED at
+    // the v4 fold. Since v4 this is the SAME image-inclusive filter as
+    // publish_readable_spans (delegates byte-for-byte); the "nonimage" in
+    // the name no longer describes it. The controller's fold MUST switch
+    // that one bridge call site to publish_readable_spans and delete this
+    // alias in the same change (behavior-identical, grep-able marker:
+    // publish_nonimage_readable_spans must have ZERO references when the
+    // fold is complete).
+    [[nodiscard]] inline auto publish_nonimage_readable_spans(
+        const VmSpanRecord* records, size_t record_count,
+        uint64_t image_base, uint64_t image_extent_bytes,
+        AddressSpan* out, size_t out_capacity) -> PublicationResult
+    {
+        return publish_readable_spans(records, record_count, image_base,
+                                      image_extent_bytes, out, out_capacity);
+    }
+
+    // FNAME-VERDICT.md section 5 steps 1-2, v4 edition
+    // (worker-fname-validator-v4; supersedes the launch10-c predicates):
+    //   span table present (else loud "span table unavailable" refusal —
+    //     never a frozen fallback, never silent admission);
+    //   ENGINE INVARIANT (FNAME-POOL-RE.md R1, engine getter @0x23d570):
+    //     none_id>>16 <= current_block — the None entry's block index can
+    //     never exceed the open block. CurrentBlock is the 0-BASED index of
+    //     the block currently being filled, so current_block == 0 is VALID
+    //     and common (hundreds of names fit in block 0); v3's
+    //     `current_block >= 1` false-refused such pools;
+    //   Blocks[0] nonzero and 8-aligned (unchanged);
+    //   Blocks[0] inside EXACTLY ONE PUBLISHED current-invocation readable
+    //     span — v4 publishes IMAGE OR NON-IMAGE readable spans
+    //     (FNAME-POOL-RE.md R2, storage separation: the allocator OBJECT is
+    //     a static image-BSS FMallocBinned2-family object, but Blocks[0] is
+    //     an ALLOCATED BLOCK whose storage class is not pinned; the launch-9
+    //     REGION-CLASS observation placed Blocks[0]'s mapping in LOAD7's
+    //     BSS tail INSIDE the image extent [0x970c000,0x9a34000) — a region
+    //     observation, not a captured pointer — so the v3 outside-image
+    //     clause + non-image-only membership refused a plausible live pool
+    //     on region-class evidence). Span membership itself is UNCHANGED:
+    //     the read must still be ownership-covered by the bridge's own
+    //     kernel VMMAP walk. Wrong-base/wrong-pool protection moved to
+    //     CONTENT: the entry-0 'None' anchor (check_none_anchor, called by
+    //     the initializer right after this shape passes — an image-resident
+    //     value like the pool label itself is not a block and fails the
+    //     anchor loudly). No frozen envelope, no fallback.
+    // Content identity stays downstream ('None'/'ByteProperty' entry
+    // matching + probe id + the new entry-0 anchor).
     //
     // Truncation honesty (same-capture rule): capture_envelope_end is the
     // max record end over the WHOLE captured snapshot. The kernel VMMAP
@@ -231,7 +313,7 @@
     // verdict, while a miss AT OR ABOVE the envelope must NEVER read as
     // 'unmapped' (the mapping may exist past the cut): it refuses with the
     // dedicated truncation message instead. A valid pool is therefore never
-    // mis-refused as unmapped when its heap span was dropped by a short
+    // mis-refused as unmapped when its span was dropped by a short
     // capture — and with the bridge capture at 16,384 records (static bss;
     // launch-2 ENOMEM evidence; observed vm_total 10,019) the complete-
     // coverage case is the production norm (coverage-proven fixture H).
@@ -242,21 +324,25 @@
         const char* bounds_source; // provenance of the bound that was applied
     };
 
+    // v4: the `base` parameter of v3 is REMOVED — it existed only for the
+    // dropped image-extent clause. Published spans and the capture envelope
+    // are absolute runtime addresses; the per-launch base check remains in
+    // the initializer's Phase 1 (module MainExe view vs seed-table base).
     [[nodiscard]] inline auto check_pool_shape(uint32_t current_block,
+                                               uint32_t none_id,
                                                uint64_t blocks0,
                                                const AddressSpan* published_spans,
                                                size_t published_span_count,
-                                               uint64_t capture_envelope_end,
-                                               uint64_t base) -> PoolShapeCheck
+                                               uint64_t capture_envelope_end) -> PoolShapeCheck
     {
         if (published_spans == nullptr || published_span_count == 0)
         {
-            return {false, "span table unavailable (bridge published no non-image readable spans this invocation)",
+            return {false, "span table unavailable (bridge published no readable spans this invocation)",
                     "span-table-unavailable"};
         }
-        if (current_block < 1)
+        if ((none_id >> kPoolLayout.block_offset_bits) > current_block)
         {
-            return {false, "CurrentBlock < 1 (pool not initialized or wrong pool/base)", "structural"};
+            return {false, "None entry block index exceeds CurrentBlock (engine invariant id>>16 <= CurrentBlock; pool not initialized or wrong pool/base)", "structural"};
         }
         if (blocks0 == 0)
         {
@@ -266,11 +352,6 @@
         {
             return {false, "Blocks[0] is not 8-aligned", "structural"};
         }
-        if (blocks0 >= base && blocks0 < base + kImageFileExtentBytes)
-        {
-            return {false, "Blocks[0] inside the image extent (wrong-base/wrong-pool arithmetic; pool blocks are heap allocations)",
-                    "published-readable-spans"};
-        }
         for (size_t i = 0; i < published_span_count; ++i)
         {
             if (blocks0 >= published_spans[i].lo && blocks0 < published_spans[i].hi)
@@ -287,9 +368,20 @@
                 "published-readable-spans"};
     }
 
-    // Bounds that must hold before any entry byte is touched: the block index
-    // must exist, and an entry in the current block must fit inside
-    // CurrentByteOffset together with its header and payload.
+    // Bounds that must hold before any entry byte is touched — v4 engine
+    // semantics (FNAME-POOL-RE.md R1 + section 4 item 5; supersedes the v3
+    // strict `block_index < current_block`, which false-refused entries in
+    // the OPEN block — the block CurrentBlock indexes is precisely the one
+    // being filled, and with CurrentBlock == 0 EVERY legitimate entry lives
+    // in it):
+    //   block index must exist (FNameMaxBlocks, unchanged);
+    //   block_index > current_block  -> refuse (engine invariant
+    //     id>>16 <= CurrentBlock, getter @0x23d570);
+    //   block_index == current_block -> the entry is in the OPEN block: it
+    //     must fit inside CurrentByteOffset together with header+payload;
+    //   block_index <  current_block -> older block, pure-side OK (the
+    //     sliver-terminator content check is a read; content identity is
+    //     carried by entry_matches_ascii / check_none_anchor downstream).
     [[nodiscard]] inline auto check_entry_bounds(uint32_t id,
                                                  uint32_t current_block,
                                                  uint32_t current_byte_offset,
@@ -300,15 +392,15 @@
         {
             return {false, "entry block index exceeds FNameMaxBlocks"};
         }
-        if (location.block_index >= current_block)
+        if (location.block_index > current_block)
         {
-            return {false, "entry block index at or past CurrentBlock"};
+            return {false, "entry block index exceeds CurrentBlock (engine invariant id>>16 <= CurrentBlock)"};
         }
-        if (location.block_index + 1 == current_block)
+        if (location.block_index == current_block)
         {
             if (location.byte_offset + needed_bytes > current_byte_offset)
             {
-                return {false, "entry extends past CurrentByteOffset of its block"};
+                return {false, "entry extends past CurrentByteOffset of the open block"};
             }
         }
         return {true, nullptr};
@@ -359,6 +451,29 @@
         return {true, false, len, nullptr};
     }
 
+    // ---- v4 entry-0 content anchor (FNAME-POOL-RE.md R3, section 4 item 6)
+    // ---- Entry 0 of Blocks[0] is ANSI "None": Init stores it first
+    // ---- (file 0x23a5d0 lea rsi, [0x7b96c17] "None" -> store -> id). With
+    // ---- image residency no longer a refusal (R2), this cheap CONTENT
+    // ---- identity check is the wrong-base/wrong-pool guard: a value that
+    // ---- passes shape/alignment/membership but is not a real block base
+    // ---- (e.g. the pool label itself) fails here loudly. Exact bit tests
+    // ---- (delegated to entry_matches_ascii, same decode): u16 header
+    // ---- h>>6 == 4 && (h&1) == 0 && payload == 'None'. Bounded read
+    // ---- discipline is the caller's (same volatile byte-wise reads as the
+    // ---- pool words; the address is covered by the published-span
+    // ---- membership check that must already have passed).
+    [[nodiscard]] inline auto check_none_anchor(const uint8_t* entry_bytes,
+                                                size_t available) -> Check
+    {
+        const auto match = entry_matches_ascii(entry_bytes, available, kNoneString);
+        if (!match.ok)
+        {
+            return {false, "Blocks[0] entry 0 is not the ANSI 'None' anchor (wrong pool or corrupt block)"};
+        }
+        return {true, nullptr};
+    }
+
     // Admission-grade probe for data slots (SLOTS-FEASIBILITY section 4b):
     // non-null and 8-aligned. Explicitly NOT identity proof.
     // ---- Launch-10 read-path guard (controller audit 2026-10-05) ----
```

## 2. UnrealInitializer.cpp — final-units v3 -> staged v4 (full unified diff)

```diff
--- /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-controller/launch10-build/final-units/UnrealInitializer.cpp	2026-10-05 08:37:07
+++ /WORKSPACE/.codex/worktrees/translation-core/what/outputs/2026-10-04-zcode-team/worker-fname-validator-v4/staging/UnrealInitializer.cpp	2026-10-05 09:55:06
@@ -613,15 +613,25 @@
     namespace SeedArming
     {
         // ---- launch10-c published-span accessors (worker-fname-heap-runtime-bounds)
+        // ---- v4 admission (worker-fname-validator-v4): the published spans
+        // ---- are the bridge's READABLE spans, IMAGE OR NON-IMAGE. Why the
+        // ---- v3 outside-image rule is gone (FNAME-POOL-RE.md R2, storage
+        // ---- separation): the ALLOCATOR OBJECT is a static image-BSS
+        // ---- FMallocBinned2-family object, but Blocks[0] is an ALLOCATED
+        // ---- BLOCK whose storage class is not pinned; the launch-9
+        // ---- REGION-CLASS observation placed Blocks[0]'s mapping in LOAD7's
+        // ---- BSS tail inside the image extent (no captured Blocks[0]
+        // ---- pointer value exists), so refusing image residency refused a
+        // ---- plausible live pool. Wrong-base protection is now the
+        // ---- entry-0 'None' content anchor below.
         // ---- Defined by the bridge TU (runtime_start_bridge_r13.geometrydiag-spanpub.cpp)
         // ---- in the SAME composed PRX; intra-module link, no new PRX imports or
         // ---- exports. The bridge publishes, at module_start (before conversion —
         // ---- the same walk that feeds bridge_register_readable_ranges), the
-        // ---- non-image readable spans of its own kernel VMMAP snapshot:
-        // ---- prot&READ, clipped to the outside of [base, base+0xbc602c8), at
-        // ---- least one FName block capacity (0x20000), top-16 by size,
-        // ---- ascending by address. These bounds replace the retired frozen
-        // ---- kHeapLo/kHeapHi envelope for Blocks[0] admission.
+        // ---- readable spans of its own kernel VMMAP snapshot:
+        // ---- prot&READ, at least one FName block capacity (0x20000), top-16
+        // ---- by size, ascending by address. These bounds replace the retired
+        // ---- frozen kHeapLo/kHeapHi envelope for Blocks[0] admission.
         extern "C" uint32_t bridge_readable_span_count();
         extern "C" int bridge_readable_span_at(uint32_t index, uint64_t* lo, uint64_t* hi);
         // Captured-envelope end (max record end over the WHOLE snapshot):
@@ -715,6 +725,22 @@
             return value;
         }
 
+        // v4 entry-0 anchor read: bounded volatile byte-wise read at an
+        // absolute runtime address (same discipline as read_u32/read_u64;
+        // data-side only). Used for the Blocks[0] entry-0 'None' anchor —
+        // the address is ownership-covered by the published-span membership
+        // check that must already have passed.
+        static auto read_bytes_at(uint64_t runtime_addr, size_t needed) -> std::vector<uint8_t>
+        {
+            std::vector<uint8_t> bytes(needed, 0);
+            const auto* p = reinterpret_cast<const volatile uint8_t*>(runtime_addr);
+            for (size_t i = 0; i < needed; ++i)
+            {
+                bytes[i] = p[i];
+            }
+            return bytes;
+        }
+
         static auto read_pool_words(uint64_t pool_runtime) -> PoolWords
         {
             const auto* pool = reinterpret_cast<const volatile uint8_t*>(pool_runtime);
@@ -849,8 +875,14 @@
             {
                 seed_refused(fmt::format("slot 1 fname pool 0x{:X}: seed class {} (expected data-readable); refusing reads", pool_runtime, rc_seed::seed_class_name(cls)));
             }
-            // ---- launch10-c: snapshot the bridge-published non-image readable
-            // spans (worker-fname-heap-runtime-bounds). The frozen
+            // ---- launch10-c: snapshot the bridge-published readable spans
+            // ---- v4 (worker-fname-validator-v4): IMAGE OR NON-IMAGE (the
+            // ---- allocator OBJECT is static image BSS; Blocks[0]'s storage
+            // ---- class is not pinned — the launch-9 region-class
+            // ---- observation put its mapping in LOAD7's BSS tail, no
+            // ---- captured pointer value exists; wrong-base protection is
+            // ---- the entry-0 'None' anchor below).
+            // (worker-fname-heap-runtime-bounds). The frozen
             // kHeapLo/kHeapHi envelope is RETIRED from admission: it is
             // vintage-transient non-image memory (GEOMETRY.md section 6), real
             // at igf/igf2 and absent from the igf3 7-span walk, so a live heap
@@ -864,7 +896,7 @@
             const uint32_t spanpub_published = bridge_readable_span_count();
             if (spanpub_published == 0)
             {
-                seed_refused(fmt::format("FName pool admission at 0x{:X}: span table unavailable (bridge published 0 non-image readable spans; Blocks[0] cannot be bounded; frozen envelope retired)", pool_runtime));
+                seed_refused(fmt::format("FName pool admission at 0x{:X}: span table unavailable (bridge published 0 readable spans; Blocks[0] cannot be bounded; frozen envelope retired)", pool_runtime));
             }
             rc_seed::AddressSpan published[rc_seed::kMaxPublishedSpans];
             uint32_t published_count = 0;
@@ -904,7 +936,13 @@
             {
                 CheckStartupWait();
                 words = read_pool_words(pool_runtime);
-                const auto shape = rc_seed::check_pool_shape(words.current_block, words.blocks0, published, published_count, capture_envelope, base);
+                // v4 shape admission: engine invariant none_id>>16 <=
+                // CurrentBlock (CurrentBlock is the 0-BASED index of the OPEN
+                // block — 0 is valid), Blocks[0] nonzero + 8-aligned, and
+                // membership in one PUBLISHED readable span (image or
+                // non-image). The already-read words feed the invariant; no
+                // extra reads.
+                const auto shape = rc_seed::check_pool_shape(words.current_block, words.none_id, words.blocks0, published, published_count, capture_envelope);
                 if (shape.ok && words.none_id != 0 && words.byte_property_id != 0)
                 {
                     break;
@@ -912,12 +950,28 @@
                 if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - pool_wait_start).count() > validation_budget.count())
                 {
                     seed_refused(shape.ok ? fmt::format("FName pool at 0x{:X}: interned ids still zero after the wait budget (pool never initialized)", pool_runtime)
-                                          : fmt::format("FName pool at 0x{:X}: {} (observed CurrentBlock {}, Blocks[0] 0x{:X}; published non-image readable spans {})",
+                                          : fmt::format("FName pool at 0x{:X}: {} (observed CurrentBlock {}, Blocks[0] 0x{:X}; published readable spans {})",
                                                        pool_runtime, shape.failure, words.current_block, words.blocks0, published_count));
                 }
                 WaitForStartupRetry(std::chrono::milliseconds(50));
             }
 
+            // ---- v4 entry-0 content anchor (FNAME-POOL-RE.md R3) ----
+            // With image residency no longer a refusal, wrong-base/wrong-pool
+            // protection is CONTENT: entry 0 of Blocks[0] is ANSI 'None'
+            // (header u16 with h>>6 == 4, h&1 == 0, payload "None"). Bounded
+            // 6-byte read through the same volatile byte discipline; the
+            // address is ownership-covered by the published-span membership
+            // check that just passed. Terminal on mismatch (content identity,
+            // same class as the entry matches below — never retried).
+            {
+                const auto anchor_bytes = read_bytes_at(words.blocks0, 2 + rc_seed::kNoneString.size());
+                if (const auto anchor = rc_seed::check_none_anchor(anchor_bytes.data(), anchor_bytes.size()); !anchor.ok)
+                {
+                    seed_refused(fmt::format("FName pool at 0x{:X}: Blocks[0] 0x{:X}: {}", pool_runtime, words.blocks0, anchor.failure));
+                }
+            }
+
             const auto validate_entry = [&](uint32_t id, const rc_seed::Check& bounds, std::string_view expected) -> void {
                 if (!bounds.ok)
                 {
@@ -935,7 +989,7 @@
             validate_entry(words.byte_property_id,
                            rc_seed::check_entry_bounds(words.byte_property_id, words.current_block, words.current_byte_offset, 2 + rc_seed::kBytePropertyString.size()),
                            rc_seed::kBytePropertyString);
-            Output::send(STR("[Seeds] FName pool validated at {} (CurrentBlock {}, Blocks[0] 0x{:X} inside a published non-image readable span of {}; 'None' id 0x{:X}, 'ByteProperty' id 0x{:X})\n"),
+            Output::send(STR("[Seeds] FName pool validated at {} (CurrentBlock {}, Blocks[0] 0x{:X} inside a published readable span (image or non-image) of {}; 'None' anchor ok; 'None' id 0x{:X}, 'ByteProperty' id 0x{:X})\n"),
                          reinterpret_cast<void*>(pool_runtime), words.current_block, words.blocks0, published_count, words.none_id, words.byte_property_id);
 
             // ---- Phase 4: bind the ctor + behavioral ByteProperty probe (FNAME-VERDICT section 5) ----
```

## 3. Fixture suite changes (host-test/test_pool_bounds_host.cpp, self-contained v4 suite)

The suite is rewritten for v4 semantics; result 69/69 (v3 was 52/52). Case map:

- RETAINED verbatim in intent: G1-G5 constants; A8 REQUIRED POSITIVE (heap
  blocks0 outside the retired frozen range + old-predicate contradiction),
  A9 vintage; A10-A12 span lo/hi boundaries; A15 (first byte past extent =
  coverage class); A16/A17 truncation-honesty split; A19/A20/A21 legacy
  strings + provenance; B1-B5 span-table-unavailable + no-frozen-fallback;
  C1-C5 top-16 cap; D2; E1 partial snapshot; F1/F3 entry regression; H0-H5
  coverage proof; old_frozen_predicate replica (contradiction proof only).
- NEW (v4): A1/A2 re-expect 7 published spans (3 image readable + 4
  non-image); A6i readable image spans publish IN FULL (v4 inversion of v3
  "A6 no image memory published"); A6ii exec/prot-none never publish;
  V1/V1b MUST-PASS candidate-image-resident-shape (SYNTHETIC per labeling
  law — region-class mapping observation [0x970c000,0x9a34000), NOT a
  captured Blocks[0] pointer value; plausible per FNAME-POOL-RE; the class
  v3 false-refused); V2 none_id block 1 > CurrentBlock 0 refusal (the
  directed engine-invariant case); V3 block == CurrentBlock boundary;
  V4a pinned honest limit (pool-label value passes SHAPE) + V4b the
  'None'-anchor refusal of pool-label bytes; V5/V6 unreadable-image and
  exec-text refuse BY COVERAGE (extent string retired); AN1-AN6 anchor
  decode matrix; END-TO-END candidate-image-resident-shape through
  publication -> shape -> anchor -> entry bounds (None id 0 offset 0,
  ByteProperty id 3 offset 6, CBO 20, current_block 0); F2i/F2r/F2x
  open-block entry semantics; H6 same candidate shape under the 10,019-record
  complete capture; AL1 retired alias == primary.
- BOTH MUST-PASS POSITIVES, symmetric and plausible-but-unproven (provenance
  verdict worker-blocks0-provenance: NO CAPTURE EXISTS — neither storage
  class is 'observed'): HEAP-RESIDENT class A8/H3 (static RE predicts the
  ALLOCATION via FMemory::Malloc(0x20000, align 2) -> GMalloc binned ->
  OS-heap, likely OUTSIDE the image, even though the ALLOCATOR OBJECT is
  static image BSS) and CANDIDATE-IMAGE-RESIDENT class V1/V1b/END-TO-END/H6
  (mapping-class record only). Span-membership-either-class admission is the
  designed response to the uncertainty; a real capture may come later from
  the fc1pool probe (built, never injected).
- RETIRED WITH CAUSE (v3 cases): A13/A14 (outside-image refusal — R2),
  A18 ('CurrentBlock < 1' refusal — R1), D3 extent-precedence (R2), F2
  strict 'at or past CurrentBlock' refusal (R1) — each inverted or replaced
  by the V/D3i/F2i cases above with comments.

## 4. Receipts index (this worker, receipts/)

- `fixture-compile.stderr` (0 bytes; clang++ -std=c++20 -Wall -Wextra)
- `fixture-pool-bounds-v4.txt` — "69 passed, 0 failed", exit 0
- `PASS-pool-bounds-v4-fixtures.json` — full class coverage + labeling-law statement
- `compile-gate.argv.json` — the EXACT v3 190-entry argv (program + 189 args),
  exactly 3 entries swapped: -c input, -o object, -MF dep
- `compile-gate.stderr` / `.stdout` — exit 0; exactly the 6 baseline warnings
  in the 4 baseline classes (-Wswitch x2, -Wdeprecated-enum-enum-conversion
  x2, -W#pragma-messages, -Wundefined-inline); zero new classes
- `unreal-unrealinitializer-v4.o` (+ `.d`) — gate object, 1,939,584 bytes
- `object-strings.txt` + strings audit (in REPORT.md section 3): all new
  refusal strings PRESENT; all retired strings ABSENT; success-log literal
  present as UTF-16LE (STR() wide literal)
