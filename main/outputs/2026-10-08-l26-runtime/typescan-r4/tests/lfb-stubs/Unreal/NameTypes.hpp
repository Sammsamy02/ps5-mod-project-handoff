// [LFB] fixture stub (agent-EY 2026-10-09): shadows <Unreal/NameTypes.hpp>
// for the -fshort-wchar LuaFNameBridge host harness ONLY. It provides the
// exact FName surface the REAL converter header consumes; the converter
// (include/LuaFNameBridge.hpp) is included VERBATIM — it is the code under
// test. The stub slot-2 ctor records the exact UTF-16 code units it
// receives (Claude item 5: "stub ctor") and synthesizes id = unit count;
// FNamePoolDecoder::try_tostring is a stub pool that decodes the recorded
// units back (a REAL round trip through the same buffers), so the bridge's
// OK/MISMATCH verdict is exercised for real.
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <String/StringType.hpp> // REAL (deps): CharType=wchar_t(2 under -fshort-wchar), StringType, STR

namespace RC::Unreal
{
    using uint32 = std::uint32_t;
    using uint64 = std::uint64_t;

    enum { NAME_SIZE = 1024 };

    enum EFindName
    {
        FNAME_Find,
        FNAME_Add,
    };

    struct FNameEntryId
    {
        uint32_t Value{};
        uint32_t ToUnstableInt() const { return Value; }
        static FNameEntryId FromUnstableInt(uint32_t v) { return {v}; }
    };

    // Seam notices (real signatures; recorded into the fixture log).
    void NotifyFNameCtorUnreadyOnce() noexcept;
    void NotifyFNameSeamRefusedOnce(const char* Reason, uint32 Length, uint32 At) noexcept;

    class FName
    {
      public:
        FNameEntryId ComparisonIndex{};
        uint32_t Number{};

        FName() = default;

        // The stub slot-2 native ctor holder: the exact usage surface of the
        // port's Function<FName(const CharType*, EFindName)> (the ctor bound
        // at image 0x23D590 on target).
        struct Fn
        {
            bool ready{true};
            bool is_ready() const { return ready; }
            FName operator()(const RC::CharType* units, EFindName mode) const;
        };
        static Fn ConstructorInternal;

        FNameEntryId GetComparisonIndex() const { return ComparisonIndex; }
        int32_t GetNumber() const { return static_cast<int32_t>(Number); }
        bool IsNone() const { return ComparisonIndex.ToUnstableInt() == 0; }
        StringType ToString() const; // stub pool decode (below)

      private:
        friend struct LfbStubPool;
    };

    // Stub pool: the ctor records (id -> exact units); try_tostring decodes
    // them back. The 'mismatch' fixture mode makes the decode lie by one
    // trailing 'X' so the bridge's MISMATCH verdict is exercised for real.
    namespace FNamePoolDecoder
    {
        // resize + indexed writes ONLY: even the (ptr,count) basic_string
        // ctor resolves to the prebuilt 4-byte-wchar __init under
        // -fshort-wchar (ASan-proven overread in this very harness), so the
        // stub never constructs StringType from a literal.
        inline StringType lfb_literal(const CharType* s, size_t n)
        {
            StringType out;
            out.resize(n);
            for (size_t i = 0; i < n; ++i) { out[i] = s[i]; }
            return out;
        }

        struct LfbStubPoolState
        {
            std::mutex mutex{};
            std::vector<StringType> by_id{}; // index = id (id 0 reserved: "None")
            bool lie{false};
            LfbStubPoolState() { by_id.emplace_back(lfb_literal(STR("None"), 4)); }
        };
        inline LfbStubPoolState& lfb_pool()
        {
            static LfbStubPoolState state{};
            return state;
        }
        inline bool try_tostring(const FName& name, StringType& out)
        {
            auto& pool = lfb_pool();
            std::lock_guard<std::mutex> lock(pool.mutex);
            const auto id = name.GetComparisonIndex().ToUnstableInt();
            if (id >= pool.by_id.size()) { return false; }
            const StringType& entry = pool.by_id[id];
            out.resize(entry.size());
            for (size_t i = 0; i < entry.size(); ++i) { out[i] = entry[i]; } // resize + []: stride-correct
            if (pool.lie && id != 0)
            {
                out.resize(out.size() + 1);
                out[out.size() - 1] = CharType('X');
            }
            return true;
        }
        inline void PublishVerifiedPoolAddress(uintptr_t) noexcept {}
        inline bool Ready() noexcept { return true; }
    } // namespace FNamePoolDecoder
} // namespace RC::Unreal
