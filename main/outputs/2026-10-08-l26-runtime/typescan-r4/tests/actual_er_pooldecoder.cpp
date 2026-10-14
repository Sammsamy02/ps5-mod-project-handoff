// agent-ER pool-decoder host fixture (2026-10-09, Claude directive 2).
// Compiles the VERBATIM FNamePoolDecoder region of the live l26 candidate
// src/NameTypes.cpp (regions/er_pooldecoder.inc, marker-extracted by the
// runner with sha pins) inside a stub universe (EE /tmp-variant-C fixture
// doctrine; the tree's production sources are read-only). The code under
// test is the production decoder: pure pool reads, no engine call, no
// admission gate, any thread.
//
// Mock pool: real page-aligned mmap memory with the frozen engine layout
// the decoder and rc_seed::kPoolLayout pin — CurrentBlock u32 @ +0x8,
// CurrentByteOffset u32 @ +0xC, Blocks[] u64 @ +0x10, 2-byte entry stride,
// entry = u16 header (bit0 wide, bits15:6 len) + chars. "None" at byte 0
// (id 0) then "ByteProperty" at byte 6 (id 0x3 — console parity: 4-char
// None occupies bytes 0..5).
//
// Suites (one process per suite; the dual-check state is process-global):
//   happy  — known ids (0 -> "None", 3 -> "ByteProperty"), ANSI + wide
//            entries, Number suffix ('_' + (Number-1)), second block,
//            refusal shapes (block > CurrentBlock, open-block overrun,
//            len 0, len > kMaxUnits, null block, misaligned block, corrupt
//            CurrentBlock), dual-check verify-once log, post-verify
//            concurrency, live-word re-read after publication.
//   refuse — corrupted ByteProperty probe entry: first use refuses LOUDLY,
//            Ready() false, every ToString falls to the legacy route.
//   race   — fresh process: 8 threads race the FIRST use; exactly one
//            verify log, racing losers take the legacy route for that
//            call, later calls decode (lock-free first-use CAS).
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <sys/mman.h>

// ---- REAL production constants (tree include/) — the decoder's dual-check
//      pins come from the retained profile header verbatim. ----
#include "EngineToStringProfile.hpp"

// ---- stub universe (documented shims; the er_pooldecoder.inc include
//      below is the verbatim production region) ----
using StringType = std::u16string;
using CharType = char16_t;
#define STR(x) u##x

// production LogLevel is an unscoped enum (OutputDevice.hpp), so the
// send<LogLevel::Error> explicit template argument converts to int32_t
enum LogLevel : int { Verbose = 0, Default, Normal, Warning, Error };

namespace RC::Unreal
{
    namespace detail
    {
        // tiny fmt subset: {} and {:X} with one integral trailing arg max
        // (the decoder only logs no-arg lines and the anomaly id line).
        inline void append_uint(StringType& out, uint64_t v, bool hex)
        {
            char16_t buf[24];
            const char16_t* digits = hex ? u"0123456789ABCDEF" : u"0123456789";
            int p = 0;
            if (v == 0) buf[p++] = u'0';
            while (v) { buf[p++] = digits[hex ? (v & 0xF) : (v % 10)]; v = hex ? (v >> 4) : (v / 10); }
            while (p) out.push_back(buf[--p]);
        }
        inline void fmt(StringType& out, const CharType* f)
        {
            for (; *f; ++f) out.push_back(*f);
        }
        template <class A, class... Rest>
        inline void fmt(StringType& out, const CharType* f, A&& arg, Rest&&... rest)
        {
            for (; *f; ++f)
            {
                if (*f == u'{' && f[1])
                {
                    bool hex = false;
                    const CharType* close = f + 1;
                    while (*close && *close != u'}') { if (*close == u'X' || *close == u'x') hex = true; ++close; }
                    if (!*close) { out.push_back(*f); continue; }
                    if constexpr (std::is_integral_v<std::decay_t<A>>)
                        append_uint(out, static_cast<uint64_t>(arg), hex);
                    fmt(out, close + 1, std::forward<Rest>(rest)...);
                    return;
                }
                out.push_back(*f);
            }
        }
    }
    namespace Output
    {
        inline std::mutex g_log_mutex;
        inline std::vector<StringType> g_log;
        inline std::vector<int> g_level;
        template <int Level = 0, typename... Args>
        inline void send(const CharType* f, Args&&... args)
        {
            StringType out;
            RC::Unreal::detail::fmt(out, f, std::forward<Args>(args)...);
            std::lock_guard<std::mutex> lock(g_log_mutex);
            g_log.push_back(std::move(out));
            g_level.push_back(Level);
        }
    }
}

namespace RC::Unreal
{
    // ---- fixture FName: the API surface the decoder consumes, real shape
    //      (GetComparisonIndex().ToUnstableInt(), GetNumber()). ----
    struct FNameEntryId
    {
        uint32_t Value{};
        uint32_t ToUnstableInt() const { return Value; }
    };
    class FName
    {
    public:
        uint32_t ComparisonIndex{};
        uint32_t Number{};
        FNameEntryId GetComparisonIndex() const { return FNameEntryId{ComparisonIndex}; }
        int32_t GetNumber() const { return static_cast<int32_t>(Number); }
    };

    // ---- VERBATIM production decoder (candidate/src/NameTypes.cpp) ----
#include "er_pooldecoder.inc"
}

// ---- mock byte-layout pool in real mmap memory ----
namespace
{
    struct MockPool
    {
        uint8_t* region{};
        size_t block0_cursor{};
        size_t block1_cursor{};
        static constexpr size_t kRegionSize = 0x10000;
        static constexpr size_t kBlock0Offset = 0x100;
        static constexpr size_t kBlock1Offset = 0x8000;

        void init()
        {
            region = static_cast<uint8_t*>(mmap(nullptr, kRegionSize, PROT_READ | PROT_WRITE,
                                                MAP_ANON | MAP_PRIVATE, -1, 0));
            if (region == MAP_FAILED) throw std::runtime_error("mmap failed");
            std::memset(region, 0, kRegionSize);
            block0_cursor = 0;
            block1_cursor = 0;
        }
        void finalize(uint32_t current_block)
        {
            const uintptr_t pool = reinterpret_cast<uintptr_t>(region);
            *reinterpret_cast<uint32_t*>(pool + 0x8) = current_block;
            *reinterpret_cast<uint32_t*>(pool + 0xC) = static_cast<uint32_t>(block1_cursor);
            *reinterpret_cast<uint64_t*>(pool + 0x10) = static_cast<uint64_t>(pool + kBlock0Offset);
            *reinterpret_cast<uint64_t*>(pool + 0x18) = static_cast<uint64_t>(pool + kBlock1Offset);
        }
        uint32_t intern(unsigned block, const char16_t* units_text, size_t units, bool wide)
        {
            size_t& cursor = block == 0 ? block0_cursor : block1_cursor;
            const size_t base = block == 0 ? kBlock0Offset : kBlock1Offset;
            if (cursor & 1) cursor += 1; // entries start at even bytes (stride-2 ids)
            const size_t byte_off = cursor;
            const uint16_t header = static_cast<uint16_t>((units << 6) | (wide ? 1u : 0u));
            region[base + byte_off] = static_cast<uint8_t>(header & 0xFF);
            region[base + byte_off + 1] = static_cast<uint8_t>(header >> 8);
            for (size_t i = 0; i < units; ++i)
            {
                if (wide)
                {
                    region[base + byte_off + 2 + i * 2] = static_cast<uint8_t>(units_text[i] & 0xFF);
                    region[base + byte_off + 2 + i * 2 + 1] = static_cast<uint8_t>(units_text[i] >> 8);
                }
                else
                {
                    region[base + byte_off + 2 + i] = static_cast<uint8_t>(units_text[i]);
                }
            }
            cursor = byte_off + 2 + units * (wide ? 2 : 1);
            return static_cast<uint32_t>((block << 16) | (byte_off / 2));
        }
        uint32_t intern_ansi(unsigned block, const char* text)
        {
            const size_t units = std::strlen(text);
            std::u16string wide;
            for (size_t i = 0; i < units; ++i) wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(text[i])));
            return intern(block, wide.data(), units, false);
        }
    };
    MockPool g_pool{};

    // agent-ES decoder-truncation fixture: boundary-controlled pool (file
    // scope: local structs cannot carry static data members). NOTE: this
    // host sandbox denies mprotect/MAP_FIXED, so there is NO PROT_NONE
    // guard page here — the boundary cases below are BEHAVIORAL proofs
    // (exact-fit must decode, over-bound must refuse with oob=true); the
    // read-before-refute ordering is proven by the happy suite's
    // PROT_NONE-block-pointer cases against the same decode().
    struct BoundaryPool
    {
        uint8_t* region{};
        size_t block1_cursor{};
        // header page | block0 full-capacity data | block1 data
        static constexpr size_t kHeaderPage = 0x1000;
        static constexpr size_t kBlock0Bytes = 0x20000; // == kBlockCapacityBytes
        static constexpr size_t kBlock1Bytes = 0x8000;
        static constexpr size_t kTotal = kHeaderPage + kBlock0Bytes + kBlock1Bytes;

        uint8_t* block0() const { return region + kHeaderPage; }
        uint8_t* block1() const { return region + kHeaderPage + kBlock0Bytes; }

        void init()
        {
            region = static_cast<uint8_t*>(mmap(nullptr, kTotal, PROT_READ | PROT_WRITE,
                                                MAP_ANON | MAP_PRIVATE, -1, 0));
            if (region == MAP_FAILED) throw std::runtime_error("truncation mmap failed");
            std::memset(region, 0, kTotal);
            block1_cursor = 0;
        }
        void finalize()
        {
            const uintptr_t pool = reinterpret_cast<uintptr_t>(region);
            *reinterpret_cast<uint32_t*>(pool + 0x8) = 1; // CurrentBlock: block0 CLOSED, block1 open
            *reinterpret_cast<uint32_t*>(pool + 0xC) = static_cast<uint32_t>(block1_cursor);
            *reinterpret_cast<uint64_t*>(pool + 0x10) = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block0()));
            *reinterpret_cast<uint64_t*>(pool + 0x18) = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(block1()));
        }
        void set_cursor(uint32_t cursor)
        {
            *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(region) + 0xC) = cursor;
        }
        // raw entry writer: exact control over placement and header len
        void write_entry(uint8_t* base, size_t byte_off, const char16_t* text, size_t units, bool wide, uint32_t len_override = 0)
        {
            const uint32_t len = len_override ? len_override : static_cast<uint32_t>(units);
            const uint16_t header = static_cast<uint16_t>((len << 6) | (wide ? 1u : 0u));
            base[byte_off] = static_cast<uint8_t>(header & 0xFF);
            base[byte_off + 1] = static_cast<uint8_t>(header >> 8);
            for (size_t i = 0; i < units; ++i)
            {
                if (wide)
                {
                    base[byte_off + 2 + i * 2] = static_cast<uint8_t>(text[i] & 0xFF);
                    base[byte_off + 2 + i * 2 + 1] = static_cast<uint8_t>(text[i] >> 8);
                }
                else
                {
                    base[byte_off + 2 + i] = static_cast<uint8_t>(text[i]);
                }
            }
        }
        uint32_t intern_block1(const char16_t* text, size_t units, bool wide)
        {
            if (block1_cursor & 1) block1_cursor += 1;
            const size_t byte_off = block1_cursor;
            write_entry(block1(), byte_off, text, units, wide);
            block1_cursor = byte_off + 2 + units * (wide ? 2 : 1);
            return static_cast<uint32_t>((1u << 16) | (byte_off / 2));
        }
        static uint32_t id_for(unsigned block, size_t byte_off)
        {
            return static_cast<uint32_t>((block << 16) | (byte_off / 2));
        }
    };

    int g_failures = 0;
    void check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "PASS " : "FAIL ") << what << "\n";
        if (!ok) ++g_failures;
    }
    size_t log_count(const char16_t* needle)
    {
        std::lock_guard<std::mutex> lock(RC::Unreal::Output::g_log_mutex);
        size_t n = 0;
        for (const auto& line : RC::Unreal::Output::g_log)
        {
            if (line.find(needle) != StringType::npos) ++n;
        }
        return n;
    }
    StringType to_u16(uint32_t v)
    {
        StringType out;
        RC::Unreal::detail::append_uint(out, v, false);
        return out;
    }
    // fixture ToString routing = the production wrapper semantics:
    // decoder first; ANY refusal falls to the legacy route for that call.
    StringType fixture_tostring(uint32_t index, uint32_t number)
    {
        const RC::Unreal::FName name{index, number};
        StringType out;
        if (RC::Unreal::FNamePoolDecoder::try_tostring(name, out)) return out;
        return u"<legacy:" + to_u16(index) + u">"; // legacy mock (documented shim)
    }
}

int main(int argc, char** argv)
{
    const std::string suite = argc > 1 ? argv[1] : "happy";
    g_pool.init();

    // Console-parity interning: "None" at byte 0 (id 0), "ByteProperty"
    // immediately after (byte 6 -> id 0x3).
    const uint32_t id_none = g_pool.intern_ansi(0, "None");
    const uint32_t id_byte_property = g_pool.intern_ansi(0, "ByteProperty");
    const uint32_t id_ansi = g_pool.intern_ansi(0, "AnsiEntry");
    const uint32_t id_ansi_high = g_pool.intern_ansi(0, "Hi\xC3"); // ANSI byte >= 0x80 widened 1:1
    const uint32_t id_wide = g_pool.intern(0, u"wide\u00E9name", 9, true); // wide entry, non-ASCII unit
    const uint32_t id_block1 = g_pool.intern_ansi(1, "SecondBlock");
    g_pool.finalize(1); // block 0 closed, block 1 open

    check(id_none == 0, "layout: None interns at id 0 (byte 0)");
    check(id_byte_property == 0x3, "layout: ByteProperty interns at id 0x3 (byte 6, console parity)");

    if (suite == "happy")
    {
        RC::Unreal::FNamePoolDecoder::PublishVerifiedPoolAddress(reinterpret_cast<uintptr_t>(g_pool.region));

        // --- first use: dual-check verify-once + log ---
        StringType s = fixture_tostring(id_byte_property, 0);
        check(s == u"ByteProperty", "known id 0x3 decodes to ByteProperty");
        check(log_count(u"[FN] pool-decoder verified (ByteProperty)") == 1, "dual-check logged verified once");
        check(RC::Unreal::FNamePoolDecoder::Ready(), "Ready() true after verified dual-check");

        // --- known ids ---
        check(fixture_tostring(id_none, 0) == u"None", "known id 0 decodes to None");

        // --- Number suffix ('_' + (Number-1) when Number > 0) ---
        check(fixture_tostring(id_byte_property, 1) == u"ByteProperty_0", "Number=1 -> suffix _0 (external)");
        check(fixture_tostring(id_byte_property, 3) == u"ByteProperty_2", "Number=3 -> suffix _2");
        check(fixture_tostring(id_byte_property, 1000000000u) == u"ByteProperty_999999999", "Number=1000000000 -> suffix _999999999");
        check(fixture_tostring(id_none, 0) == u"None", "Number=0 -> no suffix");

        // --- ANSI + wide entries ---
        check(fixture_tostring(id_ansi, 0) == u"AnsiEntry", "ANSI entry decodes");
        check(fixture_tostring(id_ansi_high, 0) == u"Hi\u00C3", "ANSI byte >= 0x80 widens 1:1 (engine in-place widen)");
        check(fixture_tostring(id_wide, 0) == u"wide\u00E9name", "wide (UTF-16) entry decodes, non-ASCII unit preserved");
        check(fixture_tostring(id_block1, 0) == u"SecondBlock", "second block (id>>16 == 1) decodes");
        check(fixture_tostring(id_block1, 2) == u"SecondBlock_1", "second block + Number suffix");

        // --- refusal shapes (Claude item 4): out-of-range ids hit the LOUD
        // '[FN] pool-decoder OOB id=0x.. block=.. offset=..' marker and are
        // refused BEFORE any entry read; shape refusals keep the anomaly
        // notice. Guard page: a PROT_NONE mapping standing in as the block
        // pointer — if any bound were missing, the entry read would fault
        // (and does fault under ASan), so survival IS the no-read proof.
        void* guard_page = mmap(nullptr, 0x1000, PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (guard_page == MAP_FAILED) throw std::runtime_error("guard mmap failed");
        {
            const uintptr_t pool = reinterpret_cast<uintptr_t>(g_pool.region);
            *reinterpret_cast<uint64_t*>(pool + 0x20) = reinterpret_cast<uint64_t>(guard_page); // Blocks[2] -> PROT_NONE
        }
        const uint32_t beyond = 2u << 16; // block 2 > CurrentBlock 1: refused before ANY read
        check(fixture_tostring(beyond, 0).rfind(u"<legacy:", 0) == 0, "block > CurrentBlock refused -> legacy route");
        check(log_count(u"[FN] pool-decoder OOB id=0x") == 1, "OOB marker fired for the poisoned block id");
        check(log_count(u"[FN] pool-decoder OOB id=0x20000 block=2 offset=0:") == 1, "OOB marker names id/block/offset exactly (no read: Blocks[2] is PROT_NONE)");
        const uint32_t beyond2 = (2u << 16) | 4u; // second distinct poisoned id in the same unbound block
        check(fixture_tostring(beyond2, 0).rfind(u"<legacy:", 0) == 0, "second poisoned id refused");
        check(log_count(u"[FN] pool-decoder OOB id=0x20004 block=2 offset=8:") == 1, "distinct poisoned ids each get their own marker (per-id dedupe)");
        const uint32_t open_overrun = (1u << 16) | static_cast<uint32_t>((g_pool.block1_cursor + 0x100) / 2);
        {
            // Blocks[1] swapped to the guard page: the overrun must be
            // refused on the live cursor BEFORE the entry read — a missing
            // bound would dereference PROT_NONE and crash this suite.
            const uintptr_t pool = reinterpret_cast<uintptr_t>(g_pool.region);
            const uint64_t saved_b1 = *reinterpret_cast<uint64_t*>(pool + 0x18);
            *reinterpret_cast<uint64_t*>(pool + 0x18) = reinterpret_cast<uint64_t>(guard_page);
            check(fixture_tostring(open_overrun, 0).rfind(u"<legacy:", 0) == 0, "open-block overrun refused BEFORE the entry read (guard page untouched)");
            *reinterpret_cast<uint64_t*>(pool + 0x18) = saved_b1;
        }
        check(log_count(u"[FN] pool-decoder OOB id=0x") == 3, "three distinct OOB ids, three markers");
        // shape refusal (in-bounds id, corrupt entry header): anomaly notice once
        {
            const size_t off = (g_pool.block1_cursor >= 2) ? ((g_pool.block1_cursor - 2) & ~static_cast<size_t>(1)) : 0; // largest EVEN in-bounds header slot
            const size_t base = g_pool.kBlock1Offset;
            g_pool.region[base + off] = 0x00; g_pool.region[base + off + 1] = 0x00; // len 0, ANSI
            const uint32_t id_len0 = static_cast<uint32_t>((1u << 16) | (off / 2));
            check(fixture_tostring(id_len0, 0).rfind(u"<legacy:", 0) == 0, "in-bounds len==0 header refused -> legacy route");
            check(log_count(u"[FN] pool-decoder anomaly: entry refused for id 0x") == 1, "shape refusal uses the anomaly notice (once per process)");
        }

        // header-shape refusals (direct decode, in-bounds offsets so the
        // header is actually read): len==0 is a shape refusal; the maximum
        // ENCODABLE len (1023 — the u16 header's Len field is 10 bits, so
        // units > kMaxUnits=1024 is unreachable defensive code) is refused
        // by the full-entry extent re-check once its payload would pass the
        // live cursor.
        {
            const uintptr_t pool = reinterpret_cast<uintptr_t>(g_pool.region);
            const size_t off = (g_pool.block1_cursor >= 2) ? ((g_pool.block1_cursor - 2) & ~static_cast<size_t>(1)) : 0; // largest EVEN in-bounds header slot
            const size_t base = g_pool.kBlock1Offset;
            g_pool.region[base + off] = 0x00; g_pool.region[base + off + 1] = 0x00; // len 0, ANSI
            const uint32_t id_len0 = static_cast<uint32_t>((1u << 16) | (off / 2));
            const auto r0 = RC::Unreal::FNamePoolDecoder::decode(pool, id_len0, 0);
            check(!r0.ok && !r0.oob, "len==0 header refused as a shape refusal (in bounds, header read)");
            const uint16_t maxenc = static_cast<uint16_t>(1023u << 6); // max encodable len
            g_pool.region[base + off] = static_cast<uint8_t>(maxenc & 0xFF);
            g_pool.region[base + off + 1] = static_cast<uint8_t>(maxenc >> 8);
            const uint32_t id_max = static_cast<uint32_t>((1u << 16) | (off / 2));
            const auto rm = RC::Unreal::FNamePoolDecoder::decode(pool, id_max, 0);
            check(!rm.ok && rm.oob, "max-encodable len refused by the full-entry extent re-check (payload would pass the cursor)");
        }
        // null block pointer / misaligned block / corrupt CurrentBlock (live words)
        {
            const uintptr_t pool = reinterpret_cast<uintptr_t>(g_pool.region);
            const uint64_t saved = *reinterpret_cast<uint64_t*>(pool + 0x18);
            *reinterpret_cast<uint64_t*>(pool + 0x18) = 0;
            check(!RC::Unreal::FNamePoolDecoder::decode(pool, id_block1, 0).ok, "null block pointer refused");
            *reinterpret_cast<uint64_t*>(pool + 0x18) = pool + 1;
            check(!RC::Unreal::FNamePoolDecoder::decode(pool, id_block1, 0).ok, "misaligned block pointer refused");
            *reinterpret_cast<uint64_t*>(pool + 0x18) = saved;
            const uint32_t saved_cb = *reinterpret_cast<uint32_t*>(pool + 0x8);
            *reinterpret_cast<uint32_t*>(pool + 0x8) = 9000; // >= kMaxBlocks
            check(!RC::Unreal::FNamePoolDecoder::decode(pool, id_none, 0).ok, "corrupt CurrentBlock (>= 8192) refused");
            check(fixture_tostring(id_none, 0).rfind(u"<legacy:", 0) == 0, "corrupt CurrentBlock -> legacy route");
            *reinterpret_cast<uint32_t*>(pool + 0x8) = saved_cb; // live re-read: restore
            check(fixture_tostring(id_none, 0) == u"None", "live words re-read every call (restore -> decode again)");
        }

        // --- concurrency after verify: any thread decodes correctly ---
        {
            std::atomic<int> mismatches{0};
            std::vector<std::thread> threads;
            for (int t = 0; t < 8; ++t)
            {
                threads.emplace_back([&]()
                {
                    for (int i = 0; i < 2000; ++i)
                    {
                        const uint32_t id = (i % 3 == 0) ? id_none : ((i % 3 == 1) ? id_byte_property : id_wide);
                        const uint32_t num = (i % 5 == 0) ? 7u : 0u;
                        StringType out;
                        const RC::Unreal::FName name{id, num};
                        if (!RC::Unreal::FNamePoolDecoder::try_tostring(name, out)) ++mismatches;
                        else if (id == id_none && out != (num ? u"None_6" : u"None")) ++mismatches;
                        else if (id == id_byte_property && out != (num ? u"ByteProperty_6" : u"ByteProperty")) ++mismatches;
                        else if (id == id_wide && out != (num ? u"wide\u00E9name_6" : u"wide\u00E9name")) ++mismatches;
                    }
                });
            }
            for (auto& th : threads) th.join();
            check(mismatches.load() == 0, "8 threads x 2000 decodes, all correct (any thread)");
        }
        check(log_count(u"[FN] pool-decoder verified (ByteProperty)") == 1, "verify log exactly once across threads");
    }
    else if (suite == "refuse")
    {
        // Corrupt the ByteProperty ENTRY (not the id): the first-use
        // dual-check must refuse loudly and the decoder stays off.
        const size_t base = g_pool.kBlock0Offset;
        const size_t bp_off = 6; // byte 6, per layout above
        g_pool.region[base + bp_off + 2] = 'X'; // "XyteProperty" != expected
        RC::Unreal::FNamePoolDecoder::PublishVerifiedPoolAddress(reinterpret_cast<uintptr_t>(g_pool.region));
        check(fixture_tostring(id_byte_property, 0).rfind(u"<legacy:", 0) == 0, "corrupted probe: ToString falls to legacy route");
        check(log_count(u"[FN] pool-decoder refused: ByteProperty probe did not decode") == 1, "refusal logged loudly");
        check(!RC::Unreal::FNamePoolDecoder::Ready(), "Ready() false after refused dual-check");
        check(fixture_tostring(id_none, 0).rfind(u"<legacy:", 0) == 0, "refusal is permanent for the process");
    }
    else if (suite == "truncation")
    {
        // agent-ES decoder-truncation fixture (2026-10-09). Console evidence
        // (/tmp/dec1.log 15:26) showed the type-scan refusal names cut at
        // varying widths; measurement proved every cut is EXACTLY HALF the
        // full name length (the halved-widen render through the broken
        // short-wchar codec, see UnrealInitializer send_narrow_diag) — NOT a
        // decoder defect. This suite pins the decoder side of that contract
        // forever, against the VERBATIM production region:
        //   1. names of 20/50/100/500 units (ANSI + wide, plus 1023 = the
        //      u16 header's max encodable len) decode COMPLETELY — exact
        //      size and content equality, the anti-truncation AND
        //      anti-halving canary (a units-vs-bytes confusion anywhere in
        //      the copy loop or its bound produces a prefix, never equality);
        //   2. block-capacity boundary: entries in a CLOSED block ending
        //      EXACTLY at kBlockCapacityBytes (0x20000) decode completely;
        //      entries whose payload would cross the capacity (by 2 bytes,
        //      or by a forged one-unit-longer header) are refused oob —
        //      the exact-fit cases discriminate the units-vs-bytes math:
        //      an ANSI entry at capacity-102 must decode (a doubled-for-
        //      ansi 'needed' would falsely refuse it) and a forged 101-unit
        //      wide header at capacity-202 must refuse (a units-as-bytes
        //      'needed' of 103 would falsely pass it and return 101 units);
        //   3. open-block cursor boundary: an entry ending EXACTLY at the
        //      live CurrentByteOffset decodes; the id AT the cursor and an
        //      entry whose payload would cross the cursor (into the guard)
        //      are refused oob before any read;
        //   4. OOB-marker exactness: successful decodes fire ZERO
        //      '[FN] pool-decoder OOB' markers; every genuinely-OOB id fires
        //      exactly one (per-id dedupe).
        struct LongCase { size_t units; bool wide; };
        const LongCase long_cases[] = {{20, false}, {20, true}, {50, false}, {50, true},
                                       {100, false}, {100, true}, {500, false}, {500, true},
                                       {1023, false}, {1023, true}};

        // deterministic long-name generator: letters+digits cycle; wide
        // variants inject non-ASCII units (U+00E9, U+4E2D) so a byte/unit
        // mixup cannot pass equality by accident.
        auto make_name = [](size_t units, bool wide) {
            StringType s;
            const char16_t* ansi_cycle = u"abcdefghijklmnopqrstuvwxyz0123456789";
            const char16_t wide_extra[] = {u'\u00E9', u'\u4E2D'};
            for (size_t i = 0; i < units; ++i)
            {
                if (wide && (i % 7) == 3) s.push_back(wide_extra[(i / 7) % 2]);
                else s.push_back(ansi_cycle[i % 36]);
            }
            return s;
        };

        BoundaryPool pool{};
        pool.init();
        // console-parity anchors in block 0 (id 0/0x3) so the first-use
        // dual-check passes and the decoder goes LIVE for this process.
        pool.write_entry(pool.block0(), 0, u"None", 4, false);
        pool.write_entry(pool.block0(), 6, u"ByteProperty", 12, false);

        struct Decoded { uint32_t id; size_t units; bool wide; uint32_t number; StringType expect; };
        std::vector<Decoded> decoded;
        for (const auto& c : long_cases)
        {
            const StringType expect = make_name(c.units, c.wide);
            const uint32_t id = pool.intern_block1(expect.data(), c.units, c.wide);
            decoded.push_back({id, c.units, c.wide, 0, expect});
        }
        // a long numbered name: 500 wide units + "_4242" suffix (Number=4243)
        {
            const StringType base = make_name(500, true);
            const uint32_t id = pool.intern_block1(base.data(), 500, true);
            decoded.push_back({id, base.size() + 5, true, 4243u, base + u"_4242"});
        }
        pool.finalize();

        RC::Unreal::FNamePoolDecoder::PublishVerifiedPoolAddress(reinterpret_cast<uintptr_t>(pool.region));
        const size_t oob_markers_before = log_count(u"[FN] pool-decoder OOB id=0x");
        const size_t anomaly_before = log_count(u"[FN] pool-decoder anomaly");

        for (const auto& d : decoded)
        {
            StringType out;
            const RC::Unreal::FName name{d.id, d.number};
            const bool ok = RC::Unreal::FNamePoolDecoder::try_tostring(name, out);
            check(ok, "long name (units=" + std::to_string(d.units) + (d.wide ? ",wide" : ",ansi") + ") decodes");
            check(ok && out.size() == d.expect.size(),
                  "long name length EXACT (units=" + std::to_string(d.units) + (d.wide ? ",wide" : ",ansi")
                      + "): got " + std::to_string(out.size()) + " want " + std::to_string(d.expect.size()));
            check(ok && out == d.expect, "long name content EXACT (no prefix, no halving, no garbage)");
        }
        check(log_count(u"[FN] pool-decoder OOB id=0x") == oob_markers_before,
              "successful long decodes fire ZERO OOB markers");
        check(log_count(u"[FN] pool-decoder anomaly") == anomaly_before,
              "successful long decodes fire ZERO anomaly notices");

        // --- 2. CLOSED-block capacity boundary (block 0, extent 0x20000) ---
        // Placements are non-overlapping at check time; each later write may
        // clobber already-checked entries (they are never re-decoded).
        // exact-fit WIDE: 500 units -> needed 1002; offset 0x20000-1002 (even)
        {
            const StringType expect = make_name(500, true);
            const size_t off = BoundaryPool::kBlock0Bytes - (500 * 2 + 2);
            pool.write_entry(pool.block0(), off, expect.data(), 500, true);
            const uint32_t id = BoundaryPool::id_for(0, off);
            const auto r = RC::Unreal::FNamePoolDecoder::decode(reinterpret_cast<uintptr_t>(pool.region), id, 0);
            check(r.ok && r.value == expect, "closed block: 500-unit WIDE entry ending EXACTLY at block capacity decodes completely");
        }
        // exact-fit ANSI: 100 units -> needed 102; offset 0x20000-102 (even)
        {
            const StringType expect = make_name(100, false);
            const size_t off = BoundaryPool::kBlock0Bytes - (100 + 2);
            pool.write_entry(pool.block0(), off, expect.data(), 100, false);
            const uint32_t id = BoundaryPool::id_for(0, off);
            const auto r = RC::Unreal::FNamePoolDecoder::decode(reinterpret_cast<uintptr_t>(pool.region), id, 0);
            check(r.ok && r.value == expect, "closed block: ANSI entry ending EXACTLY at block capacity decodes completely");
        }
        // 2 bytes past capacity (50-unit ANSI at capacity-52+2): refused oob
        {
            const StringType expect = make_name(50, false);
            const size_t off = BoundaryPool::kBlock0Bytes - (50 + 2) + 2;
            pool.write_entry(pool.block0(), off, expect.data(), 50, false);
            const uint32_t id = BoundaryPool::id_for(0, off);
            const auto r = RC::Unreal::FNamePoolDecoder::decode(reinterpret_cast<uintptr_t>(pool.region), id, 0);
            check(!r.ok && r.oob, "closed block: entry needing 2 bytes past capacity refused oob (no cut name returned)");
        }
        // forged +1 wide unit: 10 wide units fit EXACTLY at capacity-22; a
        // header claiming 11 needs 24 > 22 and must be refused. Small and
        // placed at its own offset (0x1FFEE) so no later write is disturbed.
        {
            const StringType expect = make_name(10, true);
            const size_t off = BoundaryPool::kBlock0Bytes - (10 * 2 + 2);
            pool.write_entry(pool.block0(), off, expect.data(), 10, true);
            const uint32_t id = BoundaryPool::id_for(0, off);
            const auto r_ok = RC::Unreal::FNamePoolDecoder::decode(reinterpret_cast<uintptr_t>(pool.region), id, 0);
            check(r_ok.ok && r_ok.value == expect, "closed block: 10-unit WIDE entry ending EXACTLY at capacity decodes completely");
            pool.write_entry(pool.block0(), off, expect.data(), 10, true, /*len_override=*/11);
            const auto r = RC::Unreal::FNamePoolDecoder::decode(reinterpret_cast<uintptr_t>(pool.region), id, 0);
            check(!r.ok && r.oob, "closed block: forged one-unit-longer wide header refused oob (units-vs-bytes canary: 10 wide units = 20 bytes, not 10)");
        }

        // --- 3. OPEN-block cursor boundary (block 1; data ends at the guard) ---
        const uint32_t cursor_at_end = static_cast<uint32_t>(BoundaryPool::kBlock1Bytes);
        {
            // exact fit: 2-unit ANSI entry ending exactly at the cursor
            const char16_t tail[] = {u'a', u'b'};
            const size_t off = BoundaryPool::kBlock1Bytes - 4; // 2 header + 2 chars
            pool.write_entry(pool.block1(), off, tail, 2, false);
            pool.set_cursor(cursor_at_end);
            const uint32_t id = BoundaryPool::id_for(1, off);
            const auto r = RC::Unreal::FNamePoolDecoder::decode(reinterpret_cast<uintptr_t>(pool.region), id, 0);
            check(r.ok && r.value == StringType{u"ab"}, "open block: entry ending EXACTLY at the live cursor decodes completely");
        }
        {
            // id AT the cursor: not even the header fits -> oob
            const uint32_t id = BoundaryPool::id_for(1, BoundaryPool::kBlock1Bytes);
            const auto r = RC::Unreal::FNamePoolDecoder::decode(reinterpret_cast<uintptr_t>(pool.region), id, 0);
            check(!r.ok && r.oob, "open block: id exactly at the cursor refused oob (no header room)");
        }
        {
            // header fits, 8-char payload would cross the cursor
            const StringType expect = make_name(8, false);
            const size_t off = BoundaryPool::kBlock1Bytes - 4; // header+2 fits, 8 chars do not
            pool.write_entry(pool.block1(), off, expect.data(), 8, false);
            const uint32_t id = BoundaryPool::id_for(1, off);
            const auto r = RC::Unreal::FNamePoolDecoder::decode(reinterpret_cast<uintptr_t>(pool.region), id, 0);
            check(!r.ok && r.oob, "open block: payload crossing the live cursor refused oob (no cut name returned)");
        }

        // --- 4. OOB-marker exactness (through production routing: only
        // try_tostring notifies; direct decode() is the pure core) ---
        {
            const uint32_t oob_ids[4] = {
                BoundaryPool::id_for(0, BoundaryPool::kBlock0Bytes - (50 + 2) + 2),  // 2 past capacity
                BoundaryPool::id_for(0, BoundaryPool::kBlock0Bytes - (10 * 2 + 2)),  // forged wide len
                BoundaryPool::id_for(1, BoundaryPool::kBlock1Bytes),                 // id at cursor
                BoundaryPool::id_for(1, BoundaryPool::kBlock1Bytes - 4),             // payload crosses cursor
            };
            for (const uint32_t id : oob_ids)
            {
                StringType out;
                const RC::Unreal::FName name{id, 0};
                check(!RC::Unreal::FNamePoolDecoder::try_tostring(name, out),
                      "genuinely-OOB id falls to the legacy route through production routing");
            }
            check(log_count(u"[FN] pool-decoder OOB id=0x") == oob_markers_before + 4,
                  "exactly 4 OOB markers for the 4 genuinely-OOB ids (none for successes)");
            StringType out;
            const RC::Unreal::FName repeat{BoundaryPool::id_for(1, BoundaryPool::kBlock1Bytes), 0};
            check(!RC::Unreal::FNamePoolDecoder::try_tostring(repeat, out),
                  "repeat of an already-notified OOB id still falls to the legacy route");
            check(log_count(u"[FN] pool-decoder OOB id=0x") == oob_markers_before + 4,
                  "repeat of an already-notified OOB id adds NO new marker (per-id dedupe)");
        }

        // The console widths (9,10,12,14,15,16,18,20,21,22,23,25...) were the
        // HALVED render of longer names; decode() must return those full
        // lengths unmolested. One spot check per observed halved width:
        for (const size_t units : {18u, 27u, 29u, 35u, 36u, 43u, 44u, 49u})
        {
            const StringType expect = make_name(units, false);
            const uint32_t id = pool.intern_block1(expect.data(), units, false);
            // intern_block1 advanced the cursor; re-finalize so the live word covers it
            pool.set_cursor(static_cast<uint32_t>(pool.block1_cursor));
            const auto r = RC::Unreal::FNamePoolDecoder::decode(reinterpret_cast<uintptr_t>(pool.region), id, 0);
            check(r.ok && r.value.size() == units && r.value == expect,
                  "console-observed width " + std::to_string(units) + " decodes at FULL length (halving was render-side)");
        }
    }
    else if (suite == "race")
    {
        // Fresh process: threads race the FIRST use; exactly one verify log,
        // losers take the legacy route for that call, later calls decode.
        RC::Unreal::FNamePoolDecoder::PublishVerifiedPoolAddress(reinterpret_cast<uintptr_t>(g_pool.region));
        std::atomic<int> legacy_hits{0};
        std::atomic<int> decoded_hits{0};
        std::atomic<int> wrong{0};
        std::vector<std::thread> threads;
        for (int t = 0; t < 8; ++t)
        {
            threads.emplace_back([&]()
            {
                for (int i = 0; i < 500; ++i)
                {
                    const RC::Unreal::FName name{id_byte_property, 0};
                    StringType out;
                    if (RC::Unreal::FNamePoolDecoder::try_tostring(name, out))
                    {
                        ++decoded_hits;
                        if (out != u"ByteProperty") ++wrong;
                    }
                    else
                    {
                        ++legacy_hits; // racing the first-use verify: safe
                    }
                }
            });
        }
        for (auto& th : threads) th.join();
        check(wrong.load() == 0, "race: no wrong decode ever returned");
        check(decoded_hits.load() > 0, "race: decoder live after first use");
        check(log_count(u"[FN] pool-decoder verified (ByteProperty)") == 1, "race: exactly one verify log");
        std::cout << "race stats: decoded=" << decoded_hits.load() << " legacy=" << legacy_hits.load() << "\n";
    }

    std::cout << (g_failures == 0 ? "SUITE-PASS " : "SUITE-FAIL ") << suite << "\n";
    return g_failures == 0 ? 0 : 1;
}
