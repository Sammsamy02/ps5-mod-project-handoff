// [L27][EV string-seam] host fixture (agent-EV 2026-10-09, Claude tasks
// 3-5). Claude's exact spec:
//
//   "build with -fshort-wchar so wchar_t is 2 bytes like Prospero (the
//    4-byte host wchar_t HIDES this bug). Push 'PlayerController' from a
//    Lua string through the ACTUAL binding glue into a stub ctor and
//    assert 16 code units arrive."
//
// THE BUG (host-reproduced before the fix, same class as the PS5 field
// evidence '[FFO] query-fallback ts:P' / '/Script/Engine' Find misses):
// on short-wchar targets, basic_string<wchar_t> members that resolve to
// the prebuilt library's 4-byte-wchar extern-template instantiation write
// 4-byte strides — DecodeUtf8's push_back produced the buffer bytes
// 'P',0,0,0,'l',0,0,0,... which every 2-byte-unit reader sees as
// 'P',NUL,'l',NUL,... and the engine ctor's NUL-scan ABI truncated to the
// real one-letter pool name 'P'. The init battery missed it because C++
// literals never touch the conversion seam.
//
// What is REAL here (vs stubbed):
//   REAL  Lua 5.5 interpreter (luaL_newstate/luaL_dostring/lua_tostring).
//   REAL  the verbatim Lua::get_string body shape (LuaMadeSimple.cpp:755).
//   REAL  candidate/include/Helpers/{String.hpp,Utf16Utf8.hpp} — the
//         production ensure_str/to_wstring/DecodeUtf8 seam conversions.
//   REAL  rc::fnameabi (l19 units NativeFNameAdapter.hpp) — the wchar_t ->
//         char16_t per-unit adapter that fronts the engine constructor.
//   REAL  the VERBATIM construct_with_string/construct_with_sized_string
//         bodies + the StringViewType/CharType* constructor bodies from
//         candidate/include/Unreal/NameTypes.hpp and the VERBATIM
//         NulTerminatedNameCopy + MakeFindXOfClassName from
//         candidate/src/UObjectGlobals.cpp (extracted marker+sha by the
//         runner — same discipline as agent-ER/ET).
//   STUB  the engine constructor (records the exact code units it
//         received), the object walk, Output::send (captured lines), the
//         round-trip mismatch shim (stub ctor's synthetic result cannot
//         lie here: the fixture asserts on the recorded ctor input).
//
// Run modes:
//   (default) green : the fixed seam — Lua -> 16 clean code units at the ctor.
//   red             : the corruption shape injected at the StringType layer
//                     ('P',0,'l',0,... — the unfixed-library interleave) —
//                     the FName seam must REFUSE loudly, never truncate.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <lua.hpp>

// ---- REAL production seam conversions (candidate first on the include
//      path, exactly like the target compile) ----
#include <String/StringType.hpp>
#include <Helpers/Utf16Utf8.hpp>
#include <Helpers/String.hpp>
#include <NativeFNameAdapter.hpp>

static_assert(sizeof(RC::CharType) == 2, "this fixture must run under -fshort-wchar (the 4-byte host wchar_t hides the seam bug)");

namespace RC
{
    namespace Unreal
    {
        enum EFindName { FNAME_Find = 0, FNAME_Add = 1 };
        using uint32 = std::uint32_t; // port Common.hpp alias
        using TCHAR = RC::CharType; // seam static_asserts compare widths
        enum { NAME_SIZE = 1024 }; // NameTypes.hpp namespace constant

        // Log capture for the seam markers (verbatim bodies call these).
        static std::vector<std::string> g_log;
        inline void NotifyFNameCtorUnreadyOnce() noexcept { g_log.push_back("[FN] ctor unready"); }
        inline void NotifyFNameSeamRefusedOnce(const char* Reason, uint32_t Length, uint32_t At) noexcept
        {
            char buf[160];
            std::snprintf(buf, sizeof(buf), "[FN] seam refused: reason='%s' len=%u at=%u", Reason ? Reason : "?", Length, At);
            g_log.push_back(buf);
        }

        // FMemory::Memcpy stub (production routes the engine wrapper; same
        // memcpy semantics).
        struct FMemory
        {
            static void* Memcpy(void* d, const void* s, size_t n) { return std::memcpy(d, s, n); }
        };

        struct FNameEntryId
        {
            uint32_t Value = 0;
            uint32_t ToUnstableInt() const { return Value; }
            static FNameEntryId FromUnstableInt(uint32_t v) { return {v}; }
        };

        // The stub ENGINE constructor: the real ABI shape
        // (out, const char16_t*, mode) reached through the REAL
        // rc::fnameabi adapter, exactly like native_fname_thunk.
        struct StubCtorRecorder
        {
            static inline int LastMode = -1;
            static inline std::vector<uint16_t> LastUnits{};
            static void engine_ctor(rc::fnameabi::Name* out, const char16_t* name, uint32_t mode)
            {
                LastMode = static_cast<int>(mode);
                LastUnits.clear();
                for (size_t i = 0; name[i] != 0 && i < rc::fnameabi::kMaxUnits; ++i)
                {
                    LastUnits.push_back(static_cast<uint16_t>(name[i]));
                }
                // Synthetic engine result: id = unit count (nonzero for any
                // non-empty name), number = 0.
                out->index = LastUnits.empty() ? 0u : static_cast<uint32_t>(LastUnits.size());
                out->number = 0;
            }
            static rc::fnameabi::Ctor ctor() { return &engine_ctor; }
        };

        // The verbatim NameTypes.hpp seam (construct_with_* + the string
        // ctors) lives inside a class with the exact member surface FName
        // gives it: ComparisonIndex/Number fields and the ConstructorInternal
        // callable (Function<> clone with the same usage surface).
        class FName
        {
          public:
            FName() = default;
            FNameEntryId ComparisonIndex{};
            uint32_t Number{};

            struct Fn
            {
                rc::fnameabi::Ctor fn = nullptr;
                bool is_ready() const { return fn != nullptr; }
                FName operator()(const RC::CharType* text, EFindName mode)
                {
                    const auto result = rc::fnameabi::invoke(fn, text, static_cast<uint32_t>(mode));
                    FName out;
                    if (result.ok)
                    {
                        out.ComparisonIndex = FNameEntryId::FromUnstableInt(result.value.index);
                        out.Number = result.value.number;
                    }
                    return out;
                }
                void assign_temp_address(void*) {}
                void reset_address() {}
            };
            static Fn ConstructorInternal; // defined out-of-line (Function<> discipline)

            bool IsNone() const { return ComparisonIndex.ToUnstableInt() == 0; }

            // ---- VERBATIM seam members (NameTypes.hpp, runner-extracted) ----
#include "ev_string_seam_members.inc"
        };

        FName::Fn FName::ConstructorInternal;

        // Stub for MakeFindXOfClassName's Add-fallback marker (declared
        // before the verbatim region that calls it).
        inline void LogFindXOfAddFallback(const FName&) {}

        // ---- VERBATIM UObjectGlobals.cpp helpers (runner-extracted):
        //      NulTerminatedNameCopy + MakeFindXOfClassName ----
#include "ev_string_seam_uobjectglobals.inc"

        // Round-trip mismatch shim: the stub ctor's synthetic index is the
        // RECEIVED unit count, so "constructed non-None" IS the round-trip
        // proof here; the real detector's semantics are covered by the
        // agent-DA fixture. The fixture's assertions read the recorded ctor
        // input directly, so this shim cannot mask a seam failure.
        inline bool FindXOfQueryMismatchSeam(const FName& Name, RC::StringViewType Query)
        {
            return Name.IsNone() || Name.ComparisonIndex.ToUnstableInt() != static_cast<uint32_t>(Query.size());
        }
    } // namespace Unreal
} // namespace RC

// =====================================================================================
// REAL Lua + the verbatim Lua::get_string body shape (LuaMadeSimple.cpp:755)
// =====================================================================================
static auto lua_get_string(lua_State* L, int force_index = 1) -> std::string_view
{
    std::string_view string = lua_tostring(L, force_index);
    lua_remove(L, force_index);
    return string;
}

// =====================================================================================
// The ACTUAL binding glue chain (LuaMod.cpp:1755 shape):
//   FindFirstOf(ensure_str(lua.get_string()))
// with the Prospero FindFirstOfVerified -> MakeFindXOfClassName ->
// FName(StringViewType) construction path (verbatim bodies via .inc).
// =====================================================================================
static auto FindFirstOfSeam(RC::StringViewType ClassName) -> void*
{
    const auto Name = RC::Unreal::MakeFindXOfClassName(ClassName);
    if (RC::Unreal::FindXOfQueryMismatchSeam(Name, ClassName))
    {
        return nullptr; // string-walk fallback stubbed out; nullptr is the observable
    }
    return reinterpret_cast<void*>(static_cast<uintptr_t>(Name.ComparisonIndex.ToUnstableInt()) + 1);
}

int main(int argc, char** argv)
{
    const bool red_mode = (argc > 1 && std::strcmp(argv[1], "red") == 0);

    RC::Unreal::FName::ConstructorInternal.fn = RC::Unreal::StubCtorRecorder::ctor();

    lua_State* L = luaL_newstate();
    if (!L)
    {
        std::printf("ev-string-seam: FAIL (no lua state)\n");
        return 1;
    }

    // ===== REAL Lua string origin: the chunk pushes exactly what a mod's
    // FindFirstOf("PlayerController") call would push. =====
    if (luaL_dostring(L, "return 'PlayerController'") != LUA_OK || !lua_isstring(L, -1))
    {
        std::printf("ev-string-seam: FAIL (lua chunk)\n");
        return 1;
    }

    if (!red_mode)
    {
        // ================= GREEN: the fixed seam =================
        // Hop A: Lua -> narrow bytes (get_string verbatim shape).
        const auto narrow = lua_get_string(L);
        if (narrow != std::string_view{"PlayerController"})
        {
            std::printf("ev-string-seam: FAIL hop A narrow len=%zu\n", narrow.size());
            return 1;
        }

        // Hop B: the production conversion seam (REAL ensure_str).
        const RC::StringType wide = RC::ensure_str(narrow);
        if (wide.size() != 16 || wide[0] != RC::CharType('P') || wide[1] != RC::CharType('l'))
        {
            std::printf("ev-string-seam: FAIL hop B wide len=%zu u0=%04X u1=%04X (stride corruption still present)\n",
                        wide.size(), (unsigned)wide[0], (unsigned)wide[1]);
            return 1;
        }
        for (size_t i = 0; i < wide.size(); ++i)
        {
            if (wide[i] == 0)
            {
                std::printf("ev-string-seam: FAIL hop B embedded NUL at %zu\n", i);
                return 1;
            }
        }

        // Hop C: the ACTUAL binding expression -> the verbatim FName sized
        // construction seam -> the REAL fnameabi adapter -> the stub engine
        // ctor. Claude's assertion: 16 code units arrive.
        RC::Unreal::StubCtorRecorder::LastUnits.clear();
        void* found = FindFirstOfSeam(RC::StringViewType{wide});
        const auto& units = RC::Unreal::StubCtorRecorder::LastUnits;
        if (units.size() != 16)
        {
            std::printf("ev-string-seam: FAIL hop C ctor received %zu units (want 16)\n", units.size());
            return 1;
        }
        static const char16_t kExpect[] = u"PlayerController";
        for (size_t i = 0; i < 16; ++i)
        {
            if (units[i] != static_cast<uint16_t>(kExpect[i]))
            {
                std::printf("ev-string-seam: FAIL hop C unit %zu = %04X (want %04X)\n", i, units[i], (unsigned)kExpect[i]);
                return 1;
            }
        }
        if (found == nullptr)
        {
            std::printf("ev-string-seam: FAIL hop C wrapper reported mismatch\n");
            return 1;
        }
        if (!RC::Unreal::g_log.empty())
        {
            std::printf("ev-string-seam: FAIL unexpected seam marker: %s\n", RC::Unreal::g_log[0].c_str());
            return 1;
        }

        // ===== StaticFindObject part shape (the ET contract, now enforced
        // at the FName layer): '/Script/Engine' sliced from
        // '/Script/Engine.Actor' constructs EXACTLY its own 15 units. =====
        {
            const RC::StringType long_name = RC::ensure_str(std::string_view{"/Script/Engine.Actor"});
            const RC::StringViewType part = RC::StringViewType{long_name}.substr(0, 15);
            RC::Unreal::StubCtorRecorder::LastUnits.clear();
            const auto part_copy = RC::Unreal::NulTerminatedNameCopy(part); // verbatim UObjectGlobals helper
            RC::Unreal::FName name{RC::StringViewType{part_copy}, RC::Unreal::FNAME_Find};
            if (RC::Unreal::StubCtorRecorder::LastUnits.size() != 15)
            {
                std::printf("ev-string-seam: FAIL sfo part ctor received %zu units (want 15)\n",
                            RC::Unreal::StubCtorRecorder::LastUnits.size());
                return 1;
            }
        }

        // ===== DecodeUtf8 regression pin: no push_back strides anywhere. =====
        {
            const auto round = RC::Unicode::DecodeUtf8(std::string_view{"ItemDatabaseSubsystem"});
            if (round.size() != 21 || round[1] != RC::CharType('t'))
            {
                std::printf("ev-string-seam: FAIL DecodeUtf8 regression len=%zu u1=%04X\n", round.size(), (unsigned)round[1]);
                return 1;
            }
        }

        // ===== Overlong refusal =====
        {
            std::string big(1200, 'x');
            const auto wide_big = RC::ensure_str(std::string_view{big});
            RC::Unreal::FName name{};
            name.construct_with_sized_string(wide_big.data(), wide_big.size(), RC::Unreal::FNAME_Find, nullptr);
            if (!name.IsNone() || RC::Unreal::g_log.empty())
            {
                std::printf("ev-string-seam: FAIL overlong not refused\n");
                return 1;
            }
        }

        std::printf("[ST] lua-ffo PlayerController PASS (16 code units at the ctor)\n");
        std::printf("ev-string-seam green: ALL PASS\n");
    }
    else
    {
        // ================= RED: the corruption shape must be REFUSED,
        // never silently truncated to one letter. =================
        const auto narrow = lua_get_string(L);
        (void)narrow;
        // The unfixed-library interleave: a StringType whose 2-byte units
        // read 'P',NUL,'l',NUL,... — built here explicitly because the
        // FIXED DecodeUtf8 can no longer produce it.
        RC::StringType corrupted;
        corrupted.resize(16);
        const char* kSrc = "PlayerController";
        for (size_t i = 0; i < 8; ++i)
        {
            corrupted[i * 2] = static_cast<RC::CharType>(static_cast<unsigned char>(kSrc[i]));
            corrupted[i * 2 + 1] = RC::CharType(0);
        }
        RC::Unreal::FName name{};
        name.construct_with_sized_string(corrupted.data(), corrupted.size(), RC::Unreal::FNAME_Find, nullptr);
        if (!name.IsNone())
        {
            std::printf("ev-string-seam: FAIL red — corruption constructed a name (index=%u)\n",
                        name.ComparisonIndex.ToUnstableInt());
            return 1;
        }
        if (RC::Unreal::StubCtorRecorder::LastUnits.size() > 1)
        {
            std::printf("ev-string-seam: FAIL red — ctor still received %zu units\n",
                        RC::Unreal::StubCtorRecorder::LastUnits.size());
            return 1;
        }
        bool refused = false;
        for (const auto& line : RC::Unreal::g_log)
        {
            if (line.find("seam refused") != std::string::npos && line.find("embedded-nul") != std::string::npos)
            {
                refused = true;
            }
        }
        if (!refused)
        {
            std::printf("ev-string-seam: FAIL red — no embedded-nul refusal marker\n");
            return 1;
        }
        std::printf("ev-string-seam red: REFUSED LOUDLY (PASS)\n");
    }

    lua_close(L);
    return 0;
}
