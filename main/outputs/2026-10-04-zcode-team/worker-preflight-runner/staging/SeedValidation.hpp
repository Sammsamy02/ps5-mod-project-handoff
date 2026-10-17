#pragma once
// SeedValidation.hpp — pure, engine-independent validation chain for the
// required-slot seed path of the merged staged UnrealInitializer unit.
//
// Provenance of every constant: FNAME-VERDICT.md section 4 (pool layout
// recovered from callee operand analysis, corroborated by the port's own
// NameTypes.hpp FNamePool constants: FNameBlockOffsetBits=16, entry stride 2,
// FNameEntryHeader bIsWide bit0 / LowercaseProbeHash bits5:1 / Len bits15:6).
//
// ---- v4 RE-correction (worker-fname-validator-v4, 2026-10-05) --------------
// Three launch-10 predicates FALSE-REFUSED a legitimate pool, per
// FNAME-POOL-RE.md (worker-re-fname-pool, Tier-1 capstone/lief on the
// sha-pinned decrypted eboot 3b6cfad4...):
//   (R1) CurrentBlock is the 0-BASED index of the OPEN block — 0 is valid and
//        common (FNamePool::Init memsets it to 0 and fills Blocks[0]
//        immediately; file 0x23a5d0). The engine's own getter (@0x23d570)
//        admits any entry with id>>16 <= CurrentBlock. v3's
//        `current_block >= 1` (and check_entry_bounds' strict
//        `block_index < current_block`) encoded a wrong "count" semantics.
//   (R2) STORAGE SEPARATION (labeling law): the pool's allocator is a
//        STATIC BSS FMallocBinned2-family OBJECT (runtime 0x97089b8, 76
//        pool tables, image BSS) — that is the ALLOCATOR, not the blocks
//        it returns. Blocks[0] is an ALLOCATED BLOCK whose storage class
//        follows the allocator's pools and is NOT pinned by static
//        evidence (RE section 7.1 lists its runtime placement as
//        unresolved). The launch-9 record for Blocks[0]'s mapping,
//        [0x970c000,0x9a34000) = LOAD7's BSS tail INSIDE the image extent
//        [0x400000,0xc0602c8), is a REGION-CLASS observation, not a
//        captured Blocks[0] pointer value. Either way v3's hard
//        "outside the image extent" clause could refuse a legitimate
//        pool on region-class evidence alone; image residency is
//        therefore no longer refusal evidence (RE section 4 item 3:
//        evidence contradictory — do not bake a region predicate in).
//   (R3) Entry 0 of Blocks[0] is ANSI "None" — STATIC evidence:
//        FNamePool::Init stores 'None' first (file 0x23a5d0: lea rsi,
//        [0x7b96c17] "None" -> store; FNAME-POOL-RE section 2.3), so the
//        entry decodes as u16 header with h>>6 == 4, h&1 == 0 and payload
//        'None'. This cheap content anchor replaces the dropped
//        outside-image clause as the wrong-base/wrong-pool guard.
// v4 changes ONLY what R1-R3 require: the engine invariant
// none_id>>16 <= current_block, image-INCLUSIVE span publication/membership
// (span-membership ownership coverage is unchanged), the entry-0 'None'
// anchor, and the matching check_entry_bounds correction (open block holds
// entries; CBO bounds them). Everything else — span-table-present loud
// refusal, no frozen fallback, truncation honesty, alignment, block
// capacity floor, top-16 publication window — is byte-for-byte v3.
//
// This header is deliberately freestanding (<cstdint>/<string_view> only) so
// the same bytes are host-tested with fixtures and compiled into the merged
// unit. All functions are total: they take plain values, never dereference.
// Refusal is the safe direction; a passed check is necessary, not sufficient
// (admission-grade, not identity-grade — FNAME-VERDICT section 5).

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace rc_seed
{
    // FNAME-VERDICT.md section 4: FNameEntryAllocator layout and interned ids.
    struct PoolLayout
    {
        uint64_t current_block_offset;        // +0x8    u32 CurrentBlock
        uint64_t current_byte_offset_offset;  // +0xC    u32 CurrentByteOffset
        uint64_t blocks_offset;               // +0x10   u64 Blocks[0]
        uint64_t none_id_offset;              // +0x14040 u32 id("None")
        uint64_t byte_property_id_offset;     // +0x14044 u32 id("ByteProperty")
        uint32_t block_offset_bits;           // 16
        uint32_t max_blocks;                  // 8192 (FNameMaxBlockBits = 13)
    };

    inline constexpr PoolLayout kPoolLayout{
        0x8, 0xC, 0x10, 0x14040, 0x14044, 16, 8192};

    inline constexpr std::string_view kNoneString{"None"};
    inline constexpr std::string_view kBytePropertyString{"ByteProperty"};

    struct Check
    {
        bool ok;
        const char* failure; // nullptr when ok
    };

    // Entry id decomposition (port NameTypes.hpp: block = id >> 16,
    // byte offset = (id & 0xFFFF) * 2, stride 2).
    struct EntryLocation
    {
        uint64_t block_index;
        uint64_t byte_offset;
    };

    [[nodiscard]] inline auto entry_location(uint32_t id) -> EntryLocation
    {
        return EntryLocation{
            static_cast<uint64_t>(id >> kPoolLayout.block_offset_bits),
            static_cast<uint64_t>(id & 0xFFFFu) * 2ull};
    }

    // ---- launch10-c runtime bounds (worker-fname-heap-runtime-bounds) ----
    // ---- v4 relaxation: worker-fname-validator-v4 (FNAME-POOL-RE.md R2) ----
    // The frozen heap envelope kHeapLo/kHeapHi = [0x8018c000,0x100118000) is
    // RETIRED from admission: it is vintage-transient non-image memory
    // (GEOMETRY.md section 6 — real at igf/igf2, absent from the igf3 7-span
    // walk), so a live heap placed anywhere else refused a legitimate pool.
    // The bound is replaced by CURRENT-INVOCATION span publication: the bridge
    // TU publishes the readable spans of ITS OWN kernel VMMAP walk (same walk
    // that feeds bridge_register_readable_ranges) — since v4 the published
    // spans are IMAGE OR NON-IMAGE (the allocator OBJECT is image BSS and
    // the region-class observation for Blocks[0]'s mapping was image BSS
    // tail; image residency is not refusal evidence — see
    // publish_readable_spans) — and admission
    // requires Blocks[0] to fall inside exactly one published span. Pure
    // values only — every function here stays total (never dereferences);
    // the caller owns all reads and has already classified the pool label as
    // data-readable (classify_seed_address) before this chain runs.
    struct AddressSpan
    {
        uint64_t lo; // inclusive
        uint64_t hi; // exclusive
    };

    // Image extent in FILE vaddrs (runtime extent = [base, base + this)).
    // Adjudicated: max(p_vaddr+p_memsz) = 0xbc602c8 over the phdr table of the
    // retained eboot (GEOMETRY.md section 2; runtime extent end 0xc0602c8 =
    // base 0x400000 + 0xbc602c8, GEOMETRY.md sections 3 and 8 item 4 — the
    // '0xc0602c8 as an OFFSET from base' spelling double-counts the slide and
    // is the exact false-digit class GEOMETRY section 8 retracts). Matches
    // kImageExtentVaddr in runtime_start_bridge_r13.cpp.
    inline constexpr uint64_t kImageFileExtentBytes = 0xbc602c8ull;

    // A span can only host Blocks[0] if it can hold ONE WHOLE FName block:
    // FNameBlockOffsetBits = 16 (kPoolLayout above; port NameTypes.hpp:155)
    // with entry stride 2 gives FNameBlockOffsets * Stride = 0x10000 * 2 =
    // 0x20000 contiguous bytes per engine allocation (FNAME-VERDICT.md
    // section 4 step 3/4). A single contiguous allocation lies inside one
    // kernel span, so the span containing Blocks[0] is necessarily >= this;
    // the publication floor therefore can never false-refuse a live pool.
    inline constexpr uint64_t kFNameBlockCapacityBytes =
        (1ull << kPoolLayout.block_offset_bits) * 2ull;

    // Bounded publication: top-N qualifying spans by size, published ascending
    // by address (coordinator directive: "top 16 by size, ascending").
    inline constexpr size_t kMaxPublishedSpans = 64;

    // PS5 PROT_READ (FNAME-VERDICT.md section 2 table; byte-parity with
    // kVmProtRead in runtime_start_bridge_r13.cpp:169).
    inline constexpr uint32_t kVmProtRead = 1u;

    // Minimal walk record (field parity with lmf::OsVmMapping; freestanding —
    // rc_seed never includes lmf/SDK types).
    struct VmSpanRecord
    {
        uint64_t start;
        uint64_t end; // exclusive
        uint32_t protection;
    };

    struct PublicationResult
    {
        uint32_t published;     // spans stored in out (<= kMaxPublishedSpans)
        uint32_t total;         // qualifying spans seen, including dropped
        uint32_t cap_saturated; // 1 when qualifying > published
        uint64_t envelope_end;  // max record end over the WHOLE captured
                                // snapshot: the capture is address-ordered
                                // (kernel VMMAP enumeration; c2-probe walker
                                // lineage), so a truncated capture is a
                                // PREFIX — every address below this point is
                                // completely enumerated, everything at/above
                                // it is UNKNOWN (omitted, not unmapped).
    };

    // Pure publication filter over one kernel VMMAP walk snapshot (the
    // bridge calls this over its FROZEN capture — the same-capture rule; host
    // fixtures drive the SAME function). v4 (FNAME-POOL-RE.md R2): a record
    // publishes ITSELF — its FULL [start,end) — when prot carries the READ
    // bit, it is at least one FName block capacity, and it fits the
    // top-N-by-size window. Image memory is NOT excluded anymore. Why:
    // the ALLOCATOR OBJECT is static image BSS, but Blocks[0] is an
    // ALLOCATED BLOCK whose storage class is not pinned (RE section 7.1);
    // the launch-9 REGION-CLASS observation placed Blocks[0]'s mapping in
    // LOAD7's BSS tail INSIDE the image extent, so v3's outside-extent
    // clipping (and the consumer-side outside-image refusal) could
    // false-refuse a legitimate pool on region-class evidence. Readable
    // image spans (LOAD1 R--, LOAD3/LOAD7 RW-) therefore publish alongside
    // non-image heap spans, and admission is ownership-covered by span
    // membership either way. The extent arguments are retained for
    // CALL-SITE STABILITY ONLY (the bridge TU passes the same literals; the
    // controller may drop them in a later signature fold). The caller
    // passes the WHOLE captured record set (readable or not): envelope_end
    // is computed over ALL of it, since non-readable records still prove
    // enumeration coverage.
    [[nodiscard]] inline auto publish_readable_spans(
        const VmSpanRecord* records, size_t record_count,
        [[maybe_unused]] uint64_t image_base,
        [[maybe_unused]] uint64_t image_extent_bytes,
        AddressSpan* out, size_t out_capacity) -> PublicationResult
    {
        // Top-N by size, insertion-maintained; address-sorted on the way out.
        AddressSpan kept[kMaxPublishedSpans];
        uint64_t kept_size[kMaxPublishedSpans];
        size_t kept_count = 0;
        PublicationResult result{0, 0, 0, 0};
        for (size_t i = 0; i < record_count; ++i)
        {
            const VmSpanRecord& r = records[i];
            if (r.end > result.envelope_end)
            {
                result.envelope_end = r.end; // enumeration coverage grows with every record
            }
            if ((r.protection & kVmProtRead) == 0 || r.end <= r.start)
            {
                continue;
            }
            const uint64_t size = r.end - r.start;
            if (size < kFNameBlockCapacityBytes)
            {
                continue; // cannot host a whole FName block
            }
            ++result.total;
            if (kept_count < kMaxPublishedSpans)
            {
                kept[kept_count] = AddressSpan{r.start, r.end};
                kept_size[kept_count] = size;
                ++kept_count;
            }
            else
            {
                // Replace the smallest kept span when the new one is
                // strictly larger (deterministic top-N by size).
                size_t smallest = 0;
                for (size_t k = 1; k < kept_count; ++k)
                {
                    if (kept_size[k] < kept_size[smallest]) { smallest = k; }
                }
                if (size > kept_size[smallest])
                {
                    kept[smallest] = AddressSpan{r.start, r.end};
                    kept_size[smallest] = size;
                }
            }
        }
        result.cap_saturated = result.total > kept_count ? 1u : 0u;
        // Publish ascending by address (evidence-stable ordering).
        for (size_t a = 0; a < kept_count; ++a)
        {
            for (size_t b = a + 1; b < kept_count; ++b)
            {
                if (kept[b].lo < kept[a].lo)
                {
                    const AddressSpan ts = kept[a]; kept[a] = kept[b]; kept[b] = ts;
                    const uint64_t zs = kept_size[a]; kept_size[a] = kept_size[b]; kept_size[b] = zs;
                }
            }
        }
        const size_t stored = kept_count < out_capacity ? kept_count : out_capacity;
        for (size_t k = 0; k < stored; ++k) { out[k] = kept[k]; }
        result.published = static_cast<uint32_t>(stored);
        return result;
    }


    // FNAME-VERDICT.md section 5 steps 1-2, v4 edition
    // (worker-fname-validator-v4; supersedes the launch10-c predicates):
    //   span table present (else loud "span table unavailable" refusal —
    //     never a frozen fallback, never silent admission);
    //   ENGINE INVARIANT (FNAME-POOL-RE.md R1, engine getter @0x23d570):
    //     none_id>>16 <= current_block — the None entry's block index can
    //     never exceed the open block. CurrentBlock is the 0-BASED index of
    //     the block currently being filled, so current_block == 0 is VALID
    //     and common (hundreds of names fit in block 0); v3's
    //     `current_block >= 1` false-refused such pools;
    //   Blocks[0] nonzero and 8-aligned (unchanged);
    //   Blocks[0] inside EXACTLY ONE PUBLISHED current-invocation readable
    //     span — v4 publishes IMAGE OR NON-IMAGE readable spans
    //     (FNAME-POOL-RE.md R2, storage separation: the allocator OBJECT is
    //     a static image-BSS FMallocBinned2-family object, but Blocks[0] is
    //     an ALLOCATED BLOCK whose storage class is not pinned; the launch-9
    //     REGION-CLASS observation placed Blocks[0]'s mapping in LOAD7's
    //     BSS tail INSIDE the image extent [0x970c000,0x9a34000) — a region
    //     observation, not a captured pointer — so the v3 outside-image
    //     clause + non-image-only membership refused a plausible live pool
    //     on region-class evidence). Span membership itself is UNCHANGED:
    //     the read must still be ownership-covered by the bridge's own
    //     kernel VMMAP walk. Wrong-base/wrong-pool protection moved to
    //     CONTENT: the entry-0 'None' anchor (check_none_anchor, called by
    //     the initializer right after this shape passes — an image-resident
    //     value like the pool label itself is not a block and fails the
    //     anchor loudly). No frozen envelope, no fallback.
    // Content identity stays downstream ('None'/'ByteProperty' entry
    // matching + probe id + the new entry-0 anchor).
    //
    // Truncation honesty (same-capture rule): capture_envelope_end is the
    // max record end over the WHOLE captured snapshot. The kernel VMMAP
    // enumeration is address-ordered, so a truncated capture is a PREFIX —
    // an admission miss BELOW the envelope is a genuine unmapped/unreadable
    // verdict, while a miss AT OR ABOVE the envelope must NEVER read as
    // 'unmapped' (the mapping may exist past the cut): it refuses with the
    // dedicated truncation message instead. A valid pool is therefore never
    // mis-refused as unmapped when its span was dropped by a short
    // capture — and with the bridge capture at 16,384 records (static bss;
    // launch-2 ENOMEM evidence; observed vm_total 10,019) the complete-
    // coverage case is the production norm (coverage-proven fixture H).
    struct PoolShapeCheck
    {
        bool ok;
        const char* failure;      // nullptr when ok
        const char* bounds_source; // provenance of the bound that was applied
    };

    // v4: the `base` parameter of v3 is REMOVED — it existed only for the
    // dropped image-extent clause. Published spans and the capture envelope
    // are absolute runtime addresses; the per-launch base check remains in
    // the initializer's Phase 1 (module MainExe view vs seed-table base).
    [[nodiscard]] inline auto check_pool_shape(uint32_t current_block,
                                               uint32_t none_id,
                                               uint64_t blocks0,
                                               const AddressSpan* published_spans,
                                               size_t published_span_count,
                                               uint64_t capture_envelope_end) -> PoolShapeCheck
    {
        if (published_spans == nullptr || published_span_count == 0)
        {
            return {false, "span table unavailable (bridge published no readable spans this invocation)",
                    "span-table-unavailable"};
        }
        if ((none_id >> kPoolLayout.block_offset_bits) > current_block)
        {
            return {false, "None entry block index exceeds CurrentBlock (engine invariant id>>16 <= CurrentBlock; pool not initialized or wrong pool/base)", "structural"};
        }
        if (blocks0 == 0)
        {
            return {false, "Blocks[0] is null", "structural"};
        }
        if ((blocks0 & 0x7ull) != 0)
        {
            return {false, "Blocks[0] is not 8-aligned", "structural"};
        }
        for (size_t i = 0; i < published_span_count; ++i)
        {
            if (blocks0 >= published_spans[i].lo && blocks0 < published_spans[i].hi)
            {
                return {true, nullptr, "published-readable-spans"};
            }
        }
        if (blocks0 >= capture_envelope_end)
        {
            return {false, "snapshot truncated: cannot prove heap coverage at or past the captured envelope (omitted mapping, not proven unmapped)",
                    "capture-truncated"};
        }
        return {false, "Blocks[0] not covered by any published readable span (unmapped or unreadable at this address; capture complete below the envelope)",
                "published-readable-spans"};
    }

    // Bounds that must hold before any entry byte is touched — v4 engine
    // semantics (FNAME-POOL-RE.md R1 + section 4 item 5; supersedes the v3
    // strict `block_index < current_block`, which false-refused entries in
    // the OPEN block — the block CurrentBlock indexes is precisely the one
    // being filled, and with CurrentBlock == 0 EVERY legitimate entry lives
    // in it):
    //   block index must exist (FNameMaxBlocks, unchanged);
    //   block_index > current_block  -> refuse (engine invariant
    //     id>>16 <= CurrentBlock, getter @0x23d570);
    //   block_index == current_block -> the entry is in the OPEN block: it
    //     must fit inside CurrentByteOffset together with header+payload;
    //   block_index <  current_block -> older block, pure-side OK (the
    //     sliver-terminator content check is a read; content identity is
    //     carried by entry_matches_ascii / check_none_anchor downstream).
    [[nodiscard]] inline auto check_entry_bounds(uint32_t id,
                                                 uint32_t current_block,
                                                 uint32_t current_byte_offset,
                                                 size_t needed_bytes) -> Check
    {
        const auto location = entry_location(id);
        if (location.block_index >= kPoolLayout.max_blocks)
        {
            return {false, "entry block index exceeds FNameMaxBlocks"};
        }
        if (location.block_index > current_block)
        {
            return {false, "entry block index exceeds CurrentBlock (engine invariant id>>16 <= CurrentBlock)"};
        }
        if (location.block_index == current_block)
        {
            if (location.byte_offset + needed_bytes > current_byte_offset)
            {
                return {false, "entry extends past CurrentByteOffset of the open block"};
            }
        }
        return {true, nullptr};
    }

    // FNAME-VERDICT.md section 5 step 3: entry = header u16 (LE) then Len
    // ANSI chars, no NUL terminator. bIsWide must be clear for an ASCII
    // expectation. `available` bounds the readable bytes behind `entry_bytes`.
    struct EntryMatch
    {
        bool ok;
        bool wide;
        uint32_t len;
        const char* failure; // nullptr when ok
    };

    [[nodiscard]] inline auto entry_matches_ascii(const uint8_t* entry_bytes,
                                                  size_t available,
                                                  std::string_view expected) -> EntryMatch
    {
        if (!entry_bytes || available < 2)
        {
            return {false, false, 0, "entry too small for the u16 header"};
        }
        const uint16_t header = static_cast<uint16_t>(entry_bytes[0]) |
                                static_cast<uint16_t>(static_cast<uint16_t>(entry_bytes[1]) << 8);
        const bool wide = (header & 0x1u) != 0;
        const uint32_t len = static_cast<uint32_t>(header >> 6);
        if (wide)
        {
            return {false, true, len, "entry is wide; ASCII expected"};
        }
        if (len == 0 || len != expected.size())
        {
            return {false, false, len, "entry length does not match expected string length"};
        }
        if (available - 2 < len)
        {
            return {false, false, len, "entry payload exceeds the readable bytes"};
        }
        for (uint32_t i = 0; i < len; ++i)
        {
            if (entry_bytes[2 + i] != static_cast<uint8_t>(expected[i]))
            {
                return {false, false, len, "entry characters differ from the expected string"};
            }
        }
        return {true, false, len, nullptr};
    }

    // ---- v4 entry-0 content anchor (FNAME-POOL-RE.md R3, section 4 item 6)
    // ---- Entry 0 of Blocks[0] is ANSI "None": Init stores it first
    // ---- (file 0x23a5d0 lea rsi, [0x7b96c17] "None" -> store -> id). With
    // ---- image residency no longer a refusal (R2), this cheap CONTENT
    // ---- identity check is the wrong-base/wrong-pool guard: a value that
    // ---- passes shape/alignment/membership but is not a real block base
    // ---- (e.g. the pool label itself) fails here loudly. Exact bit tests
    // ---- (delegated to entry_matches_ascii, same decode): u16 header
    // ---- h>>6 == 4 && (h&1) == 0 && payload == 'None'. Bounded read
    // ---- discipline is the caller's (same volatile byte-wise reads as the
    // ---- pool words; the address is covered by the published-span
    // ---- membership check that must already have passed).
    [[nodiscard]] inline auto check_none_anchor(const uint8_t* entry_bytes,
                                                size_t available) -> Check
    {
        const auto match = entry_matches_ascii(entry_bytes, available, kNoneString);
        if (!match.ok)
        {
            return {false, "Blocks[0] entry 0 is not the ANSI 'None' anchor (wrong pool or corrupt block)"};
        }
        return {true, nullptr};
    }

    // Admission-grade probe for data slots (SLOTS-FEASIBILITY section 4b):
    // non-null and 8-aligned. Explicitly NOT identity proof.
    // ---- Launch-10 read-path guard (controller audit 2026-10-05) ----
    // Verified phdr geometry (retained eboot sha 3b6cfad4..., file vaddrs;
    // runtime = file + base): LOAD0 --X [0,0x7479b1c), LOAD1 R--
    // [0x747c000,0x7eba918), LOAD3/7 RW- [0x7ebc000,0x9633418), LOAD8 ---
    // [0x9633420,0xbc602c8). Seed slots 2/4 (FName ctor, SCO) land in
    // exec-only LOAD0: prot=4 pages are hardware-READ-blocked on this
    // target and are never covered by the readable-span registration, so
    // code seeds are CALL-ONLY (assign + indirect call; detour-verification
    // reads stay skipped while seed-bound). Alignment gaps between LOADs
    // and all of LOAD8 are refused for both classes. The 6-span successful
    // registration establishes nothing about text coverage.
    enum class SeedAddressClass : uint8_t
    {
        DataReadable = 0, // readable image span: reads permitted after this check
        CodeCallOnly = 1, // exec-only text: assign + call only, data reads refused
        Refused = 2       // alignment gap or prot-none: neither reads nor calls
    };

    [[nodiscard]] inline auto classify_seed_address(uint64_t runtime_address, uint64_t base) -> SeedAddressClass
    {
        const uint64_t v = runtime_address - base;
        if (v < 0x7479b1cull) { return SeedAddressClass::CodeCallOnly; }                        // LOAD0 --X
        if (v >= 0x747c000ull && v < 0x7eba918ull) { return SeedAddressClass::DataReadable; }   // LOAD1 R--
        if (v >= 0x7ebc000ull && v < 0x9633418ull) { return SeedAddressClass::DataReadable; }   // LOAD3+LOAD7 RW-
        return SeedAddressClass::Refused;                                                       // gaps + LOAD8 ---
    }

    [[nodiscard]] inline auto seed_class_name(SeedAddressClass c) -> const char*
    {
        switch (c)
        {
        case SeedAddressClass::DataReadable: return "data-readable";
        case SeedAddressClass::CodeCallOnly: return "exec-only (call-only)";
        case SeedAddressClass::Refused: default: return "unmapped/prot-none gap (refused)";
        }
    }

    [[nodiscard]] inline auto check_pointer_probe(uint64_t value) -> Check
    {
        if (value == 0)
        {
            return {false, "seeded address is null"};
        }
        if ((value & 0x7ull) != 0)
        {
            return {false, "seeded address is not 8-aligned"};
        }
        return {true, nullptr};
    }

    // GMalloc data-side validators V1/V2 (worker-gmalloc-derivation
    // GMALLOC-CANDIDATES.json rank-1 validator; arming rule: slot 3 arms only
    // if V1 AND V2 pass, any failure refuses the table — terminal, nothing
    // bound). Pure: plain values, never dereference; the caller performs the
    // two volatile byte reads exactly like read_u64 (both reads stay in phdr7
    // BSS: the GMalloc variable, then the instance vptr — the RELRO vtable is
    // only ever COMPARED as a link-time constant, never dereferenced, so no
    // mprotect/FC-1 gate is involved at V1/V2 arming strength).
    //
    // V1: [GMalloc] must equal the derived FMallocBinned3 instance address
    // exactly (corroborating_instance: the factory constructs the singleton
    // IN PLACE as a global, so a correct bind is pointer-to-known-BSS — a
    // live-but-foreign heap pointer refuses here where check_pointer_probe
    // alone would pass).
    [[nodiscard]] inline auto check_gmalloc_instance(uint64_t loaded, uint64_t expected_instance) -> Check
    {
        if (loaded != expected_instance)
        {
            return {false, "[GMalloc] does not equal the derived FMallocBinned3 instance address"};
        }
        return {true, nullptr};
    }

    // V2: the instance vptr must equal the derived vtable address
    // (RELA-reconstructed 0x7ec26e0 linked / 0x82c26e0 runtime, phdr3 RELRO
    // readable in-game; equality to the derived constant, not a range guess).
    [[nodiscard]] inline auto check_gmalloc_vptr(uint64_t vptr, uint64_t expected_vtable) -> Check
    {
        if (vptr != expected_vtable)
        {
            return {false, "FMallocBinned3 instance vptr does not equal the derived vtable address"};
        }
        return {true, nullptr};
    }

    // Behavioral probe guard (FNAME-VERDICT.md section 5 step for the call):
    // the FName("ByteProperty", FNAME_Find) id must be nonzero and equal the
    // independently pool-read id at pool+0x14044.
    [[nodiscard]] inline auto check_probe_id(uint32_t probe_id, uint32_t pool_id) -> Check
    {
        if (probe_id == 0)
        {
            return {false, "behavioral probe returned NAME_None (ctor call did not take effect)"};
        }
        if (probe_id != pool_id)
        {
            return {false, "behavioral probe id diverges from the pool-read ByteProperty id"};
        }
        return {true, nullptr};
    }
}
