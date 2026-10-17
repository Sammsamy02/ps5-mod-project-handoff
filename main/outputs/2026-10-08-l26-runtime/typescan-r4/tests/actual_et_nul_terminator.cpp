// [L27][ET nul-throw-armor] host fixture (2026-10-09, Claude fixes 1-3 for
// the mods-started=0 blocker). Claude's exact spec:
//
//   "build '/Script/Engine' from a SLICE of '/Script/Engine.Actor' —
//    construct the FName from the u16string copy of the slice — then decode
//    it with the pool decoder fixture — assert it decodes to EXACTLY
//    '/Script/Engine'. Round-trip compares are CASE-INSENSITIVE (like
//    FName)."
//
// The engine-side FName constructor takes `const CharType*` and scans to
// NUL (NameTypes.hpp forwards StringViewType::data() with NO length), so a
// string_view SLICE was over-read into the following path text: the console
// refusal "[StaticFindObject] long-name part '/Script/Engine' not in FName
// pool" was exactly this over-read (FNAME_Find interned the WHOLE
// "/Script/Engine.Actor" string for the "/Script/Engine" part). The throw
// escaped the first long-path lookup in on_program_start (R3 carrier
// arming), UE4SSProgram's single TRY swallowed it, start_lua_mods never
// ran — mods-started=0 while the event loop lived.
//
// The decode side is the VERBATIM production pool decoder
// (candidate/src/NameTypes.cpp FNamePoolDecoder region, marker+sha
// extracted by the runner — same discipline as the agent-ER fixture). The
// NUL-scan ctor mock does a REAL scan-to-NUL over the fixture buffer (no
// simulated result): a slice that is not NUL-terminated at its end really
// reads through ".Actor". The NulTerminatedNameCopy body below is the
// VERBATIM production helper from candidate/src/UObjectGlobals.cpp (the
// runner asserts textual equality against the production source).
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <type_traits>
#include <vector>

// ---- REAL production constants (tree include/) — the decoder's dual-check
//      pins come from the retained profile header verbatim. ----
#include "EngineToStringProfile.hpp"

// ---- stub universe (documented shims; er_pooldecoder.inc below is the
//      verbatim production region) ----
using StringType = std::u16string;
using CharType = char16_t;
using StringViewType = std::u16string_view;
#define STR(x) u##x

enum LogLevel : int { Verbose = 0, Default, Normal, Warning, Error };

namespace RC::Unreal
{
    namespace detail
    {
        // tiny fmt subset: {} and {:X} with one integral trailing arg max
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
        inline std::vector<StringType> g_log;
        inline std::vector<int> g_level;
        template <int Level = 0, typename... Args>
        inline void send(const CharType* f, Args&&... args)
        {
            StringType out;
            RC::Unreal::detail::fmt(out, f, std::forward<Args>(args)...);
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

// ---- VERBATIM production helper (candidate/src/UObjectGlobals.cpp;
//      the runner asserts this text equals the production body) ----
namespace production
{
    using ::StringViewType;
    using ::StringType;
    static auto NulTerminatedNameCopy(StringViewType View) -> StringType
    {
        if (View.empty()) { return StringType{}; }
        return StringType{View.data(), View.size()};
    }
}

// ---- mock byte-layout pool (real page-aligned mmap memory, frozen engine
//      layout: CurrentBlock u32 @ +0x8, CurrentByteOffset u32 @ +0xC,
//      Blocks[] u64 @ +0x10, 2-byte entry stride, u16 header (bit0 wide,
//      bits15:6 len) + chars) with the console-parity anchors "None" at id 0
//      and "ByteProperty" at id 0x3 so the decoder's first-use dual-check
//      passes and the decoder goes LIVE for this process. ----
namespace
{
    struct MockPool
    {
        uint8_t* region{};
        size_t block0_cursor{};
        static constexpr size_t kRegionSize = 0x10000;
        static constexpr size_t kBlock0Offset = 0x100;

        void init()
        {
            region = static_cast<uint8_t*>(mmap(nullptr, kRegionSize, PROT_READ | PROT_WRITE,
                                                MAP_ANON | MAP_PRIVATE, -1, 0));
            if (region == MAP_FAILED) throw std::runtime_error("mmap failed");
            std::memset(region, 0, kRegionSize);
            block0_cursor = 0;
        }
        void finalize()
        {
            const uintptr_t pool = reinterpret_cast<uintptr_t>(region);
            *reinterpret_cast<uint32_t*>(pool + 0x8) = 0; // single closed block
            *reinterpret_cast<uint32_t*>(pool + 0xC) = static_cast<uint32_t>(block0_cursor);
            *reinterpret_cast<uint64_t*>(pool + 0x10) = static_cast<uint64_t>(pool + kBlock0Offset);
        }
        uint32_t intern(const char16_t* units_text, size_t units, bool wide)
        {
            if (block0_cursor & 1) block0_cursor += 1; // entries at even bytes (stride-2 ids)
            const size_t byte_off = block0_cursor;
            const uint16_t header = static_cast<uint16_t>((units << 6) | (wide ? 1u : 0u));
            region[kBlock0Offset + byte_off] = static_cast<uint8_t>(header & 0xFF);
            region[kBlock0Offset + byte_off + 1] = static_cast<uint8_t>(header >> 8);
            for (size_t i = 0; i < units; ++i)
            {
                if (wide)
                {
                    region[kBlock0Offset + byte_off + 2 + i * 2] = static_cast<uint8_t>(units_text[i] & 0xFF);
                    region[kBlock0Offset + byte_off + 2 + i * 2 + 1] = static_cast<uint8_t>(units_text[i] >> 8);
                }
                else
                {
                    region[kBlock0Offset + byte_off + 2 + i] = static_cast<uint8_t>(units_text[i]);
                }
            }
            block0_cursor = byte_off + 2 + units * (wide ? 2 : 1);
            return static_cast<uint32_t>(byte_off / 2);
        }
        uint32_t intern_ansi(const char* text)
        {
            const size_t units = std::strlen(text);
            std::u16string wide;
            for (size_t i = 0; i < units; ++i) wide.push_back(static_cast<char16_t>(static_cast<unsigned char>(text[i])));
            return intern(wide.data(), units, false);
        }
    };
    MockPool g_pool{};

    // Engine-side pool hash: keys are compared CASE-INSENSITIVELY (FName's
    // lowercase probe); decode returns the STORED spelling.
    std::map<std::u16string, uint32_t> g_stored_lower; // lowercased key -> id
    std::map<uint32_t, std::u16string> g_stored_spelling; // id -> stored spelling

    std::u16string to_lower(std::u16string_view s)
    {
        std::u16string out;
        for (char16_t c : s)
        {
            if (c >= u'A' && c <= u'Z') out.push_back(static_cast<char16_t>(c - u'A' + u'a'));
            else out.push_back(c);
        }
        return out;
    }

    uint32_t intern_name(const char16_t* text, bool wide)
    {
        const size_t units = std::char_traits<char16_t>::length(text);
        const uint32_t id = g_pool.intern(text, units, wide);
        g_stored_lower[to_lower(std::u16string_view{text, units})] = id;
        g_stored_spelling[id] = std::u16string{text, units};
        return id;
    }

    // The ENGINE constructor contract under test: `const CharType*` scanned
    // to NUL, then FNAME_Find (case-insensitive pool lookup; silent {0,0}
    // miss). The NUL-scan here is REAL — it reads the fixture buffer until
    // the terminating u'\0', exactly what ConstructorInternal does on the
    // target for a slice that is not NUL-terminated at its logical end.
    uint32_t engine_fname_find(const char16_t* scanned_to_nul)
    {
        std::u16string key;
        for (const char16_t* p = scanned_to_nul; *p; ++p) key.push_back(*p);
        const auto it = g_stored_lower.find(to_lower(key));
        if (it == g_stored_lower.end()) return 0; // silent FNAME_Find miss
        return it->second;
    }

    // Decode through the VERBATIM production decoder.
    std::u16string decode_id(uint32_t id)
    {
        const RC::Unreal::FName name{id, 0};
        StringType out;
        if (!RC::Unreal::FNamePoolDecoder::try_tostring(name, out)) throw std::runtime_error("decoder refused a known id");
        return out;
    }

    bool ieq(std::u16string_view a, std::u16string_view b)
    {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            const char16_t ca = (a[i] >= u'A' && a[i] <= u'Z') ? static_cast<char16_t>(a[i] - u'A' + u'a') : a[i];
            const char16_t cb = (b[i] >= u'A' && b[i] <= u'Z') ? static_cast<char16_t>(b[i] - u'A' + u'a') : b[i];
            if (ca != cb) return false;
        }
        return true;
    }

    int g_failures = 0;
    void check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "PASS " : "FAIL ") << what << "\n";
        if (!ok) ++g_failures;
    }
}

int main()
{
    g_pool.init();
    // Console-parity anchors: "None" at byte 0 (id 0), "ByteProperty" at
    // byte 6 (id 0x3) — the decoder's first-use dual-check probe.
    (void)g_pool.intern_ansi("None");
    const uint32_t id_byte_property = g_pool.intern_ansi("ByteProperty");
    // The two names of the bug: the PACKAGE part as its own pool entry, and
    // the WHOLE path (what the over-read used to intern/look up).
    const uint32_t id_engine_package = intern_name(u"/Script/Engine", true);
    const uint32_t id_whole_path = intern_name(u"/Script/Engine.Actor", true);
    const uint32_t id_actor = intern_name(u"Actor", true);
    g_pool.finalize();

    check(id_byte_property == 0x3, "layout: ByteProperty interns at id 0x3 (dual-check anchor, console parity)");

    // Publish + go live (first use runs the ByteProperty dual-check).
    RC::Unreal::FNamePoolDecoder::PublishVerifiedPoolAddress(reinterpret_cast<uintptr_t>(g_pool.region));
    check(decode_id(id_byte_property) == u"ByteProperty", "decoder live: probe id 0x3 decodes to ByteProperty");
    check(RC::Unreal::FNamePoolDecoder::Ready(), "Ready() true after verified dual-check");

    // =====================================================================
    // Claude's exact spec: build "/Script/Engine" from a SLICE of
    // "/Script/Engine.Actor".
    // =====================================================================
    const std::u16string full_path = u"/Script/Engine.Actor";
    const StringViewType slice{full_path.data(), 14}; // "/Script/Engine", NOT NUL-terminated at its end
    check(slice == u"/Script/Engine", "slice content is '/Script/Engine' (logical)");
    check(slice.data()[slice.size()] == u'.', "slice is NOT NUL-terminated at its end (the bug precondition)");

    // ---- RED (old call shape): the raw view is handed to the no-length
    //      ctor — the NUL-scan reads THROUGH ".Actor" — reproducing the
    //      console refusal's cause (wrong intern key -> FNAME_Find miss for
    //      the package, or the WRONG entry when the whole path is interned).
    {
        const uint32_t over_read_id = engine_fname_find(slice.data());
        check(over_read_id != id_engine_package,
              "RED: raw-slice construction does NOT yield the package entry (NUL-scan over-read through '.Actor')");
        check(over_read_id == id_whole_path,
              "RED: raw-slice construction resolves the WHOLE '/Script/Engine.Actor' string (the exact console failure shape)");
        // And the pure-miss shape (nothing else interned in the pool): the
        // over-read key misses — the old code path FNAME_Find -> IsNone ->
        // the EC throw -> on_program_start abort -> mods-started=0.
        const std::u16string only_package = u"/Script/Engine.NotInterned";
        const StringViewType slice2{only_package.data(), 14};
        const uint32_t miss_id = engine_fname_find(slice2.data());
        check(miss_id == 0, "RED: over-read key FNAME_Find miss -> IsNone (the throw condition the EC arm used to hit)");
    }

    // ---- GREEN (Claude's spec, the FIX 1 production shape): construct the
    //      FName from the u16string COPY of the slice (the verbatim
    //      production NulTerminatedNameCopy), then decode it with the pool
    //      decoder fixture and assert it decodes to EXACTLY "/Script/Engine".
    {
        const StringType copy = production::NulTerminatedNameCopy(slice); // std::u16string(view.data(), view.size())
        check(copy.size() == 14, "copy is explicitly sized (14 units)");
        check(copy.data()[copy.size()] == u'\0', "copy IS NUL-terminated at its end (std::basic_string guarantee)");
        const uint32_t fixed_id = engine_fname_find(copy.data()); // the engine ctor scans to NUL — which is now at the slice end
        check(fixed_id == id_engine_package, "GREEN: copy construction yields the package entry '/Script/Engine'");

        const std::u16string decoded = decode_id(fixed_id);
        check(decoded == u"/Script/Engine", "GREEN: decoded to EXACTLY '/Script/Engine' (no '.Actor' tail, no truncation)");
        check(ieq(decoded, u"/Script/Engine"), "GREEN: round-trip compare CASE-INSENSITIVE passes (exact-case equality)");
        check(!ieq(decoded, u"/Script/Engine.Actor"), "GREEN: the over-read string does NOT compare equal to the slice");
    }

    // ---- CASE-INSENSITIVE round trip (like FName): a different-case query
    //      constructs the SAME entry; the decoded stored spelling still
    //      compares case-insensitively equal to the query.
    {
        const std::u16string lower_query = u"/script/engine";
        const uint32_t lower_id = engine_fname_find(lower_query.data());
        check(lower_id == id_engine_package, "case-insensitive pool: '/script/engine' constructs the SAME entry as '/Script/Engine'");
        const std::u16string decoded = decode_id(lower_id);
        check(decoded == u"/Script/Engine", "decode returns the STORED spelling");
        check(ieq(decoded, lower_query), "round-trip compare of stored spelling vs different-case query is CASE-INSENSITIVE equal (FName semantics)");
    }

    // ---- The empty-view guard of the production helper.
    {
        check(production::NulTerminatedNameCopy(StringViewType{}).empty(), "empty view -> empty copy (null-data guard, no UB)");
    }

    // ---- Second slice family from the console evidence: the SHORT-name
    //      carrier candidates ("Actor" sliced from "Actor:ReceiveTick").
    {
        const std::u16string carrier = u"Actor:ReceiveTick";
        const StringViewType actor_slice{carrier.data(), 5}; // "Actor", NOT NUL-terminated at its end
        const uint32_t over_read = engine_fname_find(actor_slice.data());
        check(over_read != id_actor && over_read == 0,
              "RED: 'Actor' slice of 'Actor:ReceiveTick' over-reads to the whole string (FNAME_Find miss — the R3 candidate shape)");
        const uint32_t fixed = engine_fname_find(production::NulTerminatedNameCopy(actor_slice).data());
        check(fixed == id_actor, "GREEN: 'Actor' copy construction resolves the real Actor entry");
        check(decode_id(fixed) == u"Actor", "GREEN: 'Actor' decodes EXACTLY (tail slice 'ReceiveTick' is NUL-safe already; head slice needed the copy)");
    }

    std::cout << (g_failures == 0 ? "SUITE-PASS et-nul-terminator\n" : "SUITE-FAIL et-nul-terminator\n");
    return g_failures == 0 ? 0 : 1;
}
