#include <Unreal/NameTypes.hpp>

#include <Unreal/Core/Containers/FString.hpp>
#include <Unreal/Core/Containers/FUtf8String.hpp>
#include <Unreal/Core/Containers/FAnsiString.hpp>
#include <Unreal/BPMacros.hpp>
#include <Unreal/UnrealInitializer.hpp>
#include "EngineToStringProfile.hpp"

// l26 hardening (agent-CN item 2): log-once notice for the unready-ctor
// NAME_None degradation declared in NameTypes.hpp.
#include <atomic>
#include <cstdint>
#include <cstring>
#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp> // [L27][EV string-seam] ensure_str for the seam refusal marker

#if defined(__PROSPERO__)
#include <cstdlib>
#include <memory>
#include <new>
#endif

namespace RC::Unreal
{
    Function<void(const FName*, class FStringOut&)> FName::ToStringInternal;
    UFunction* FName::Conv_NameToStringInternal{};
    UObject* FName::KismetStringLibraryCDO{};
    Function<FName(const CharType*, EFindName)> FName::ConstructorInternal;
    // [Seeds][L27-supersede] (agent-ER, Claude directive 2): the engine
    // ToStringInternal bind's published holder FName::ToStringNative is
    // DELETED — FName::ToString is owned by the pure in-port pool decoder
    // below (FNamePoolDecoder). The profile constants stay
    // (EngineToStringProfile.hpp): the label documents provenance and the
    // probe constants pin the decoder's first-use dual-check.

    void NotifyFNameCtorUnreadyOnce() noexcept
    {
        // Once per process, first caller wins; the notice is observational
        // only (L22Diagnostics::observe discipline): formatting/logger
        // failures are swallowed, construction semantics untouched.
        static std::atomic<bool> g_fname_ctor_unready_notified{false};
        bool expected = false;
        if (!g_fname_ctor_unready_notified.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return;
        }
        try
        {
            Output::send<LogLevel::Error>(STR("[FN] ctor unready: NAME_None produced\n"));
        }
        catch (...)
        {
        }
    }

    // [L27][EV string-seam] (agent-EV 2026-10-09): the FName construction
    // seam refused an input carrying the embedded-NUL corruption signature
    // (a stride-corrupted wide buffer — 'P',0,'l',0,... — that the engine's
    // NUL-scan ABI would silently truncate to 'P') or an overlong one.
    // [Q2][FK refusal-logging] (agent-FK 2026-10-10, Claude round-3 Q2):
    // the once-per-process gate is upgraded to the FIRST 10 occurrences per
    // refusal reason family, and every logged line carries a call-site tag
    // (low 12 bits of the immediate call site's return address — symbolize
    // it against the build's runtime.map to name the exact TU emission of
    // the header's inline sized-ctor audit that refused the input) plus the
    // hex units of the offending buffer's first min(len,8) units whenever
    // the audit hands this helper a buffer view. The exported signature
    // stays frozen at (const char*, uint32, uint32): the NameTypes.hpp call
    // sites and the deployed symbol
    // (_ZN2RC6Unreal26NotifyFNameSeamRefusedOnceEPKcjj) are unchanged, so
    // the 3-arg path renders units=n/a(3arg) — passing the real buffer
    // (Owned/StrName at the two header audit sites) is the staged follow-up
    // for the next header-touching round. Observational only: construction
    // semantics untouched (NAME_None, no throw), formatting/logger failures
    // swallowed. The wide literal + numeric args never interpolate narrow
    // text; ensure_str(...) args ride the seam-fixed DecodeUtf8
    // ((ptr,count) construction); the units payload is pure-ASCII hex.
    namespace
    {
        // Reason families are string-matched (the audit sites pass rodata
        // literals); unknown reasons share one bucket so a NEW reason still
        // logs its own first 10. 'embedded' and 'embedded-nul' are one
        // family: the deployed build spelled the reason 'embedded', the
        // candidate header 'embedded-nul' — one family keeps console lines
        // comparable across builds.
        constexpr int kSeamFamilyEmbedded = 0;
        constexpr int kSeamFamilyOverlong = 1;
        constexpr int kSeamFamilyOther = 2;
        constexpr int kSeamFamilyCount = 3;
        constexpr uint32_t kSeamRefusalFirstN = 10;

        std::atomic<uint32_t> g_seam_refusal_family_seen[kSeamFamilyCount]{};

        auto SeamRefusalFamilyOf(const char* Reason) -> int
        {
            if (!Reason)
            {
                return kSeamFamilyOther;
            }
            if (std::strcmp(Reason, "embedded-nul") == 0 || std::strcmp(Reason, "embedded") == 0)
            {
                return kSeamFamilyEmbedded;
            }
            return std::strcmp(Reason, "overlong") == 0 ? kSeamFamilyOverlong : kSeamFamilyOther;
        }

        // Renders the hex units of the first min(Length, 8) units of the
        // buffer the caller already validated length-wise — every read
        // stays inside that caller-validated extent, a null view (the
        // frozen 3-arg entry) is never dereferenced, and fixed buffers mean
        // no allocation inside this noexcept refusal path. Worst case
        // 8 units x (2*sizeof(CharType) hex digits) + separators fits the
        // 96-byte out buffer with room for the NUL.
        auto FormatSeamRefusalUnits(const CharType* Units, uint32 Length, char (&Out)[96]) -> const char*
        {
            if (!Units)
            {
                return "n/a(3arg)";
            }
            static constexpr char kHex[] = "0123456789abcdef";
            constexpr int kDigitsPerUnit = static_cast<int>(sizeof(CharType)) * 2;
            const uint32_t count = Length < 8 ? Length : 8;
            size_t used = 0;
            for (uint32_t i = 0; i < count && used + static_cast<size_t>(kDigitsPerUnit) + 1 < sizeof(Out); ++i)
            {
                if (i)
                {
                    Out[used++] = ':';
                }
                const uint32_t unit = static_cast<uint32_t>(Units[i]);
                for (int shift = kDigitsPerUnit * 4 - 4; shift >= 0; shift -= 4)
                {
                    Out[used++] = kHex[(unit >> shift) & 0xFu];
                }
            }
            Out[used] = '\0';
            return Out;
        }
    } // anonymous namespace

    void NotifyFNameSeamRefusedOnce(const char* Reason, uint32 Length, uint32 At) noexcept
    {
        // First-10-per-family gate (Claude Q2): a poisoned seam in a hot
        // loop still cannot flood the log, but the first 10 occurrences per
        // reason family now surface, each with its call-site identity.
        // Races only risk a duplicate line, never a missed family.
        const int family = SeamRefusalFamilyOf(Reason);
        const uint32_t seen = g_seam_refusal_family_seen[family].fetch_add(1, std::memory_order_acq_rel);
        if (seen >= kSeamRefusalFirstN)
        {
            return;
        }
        try
        {
            const uintptr_t call_site_tag = reinterpret_cast<uintptr_t>(__builtin_return_address(0)) & 0xFFFu;
            char units[96];
            Output::send<LogLevel::Error>(STR("[FN] seam refused: reason='{}' len={} at={} (NAME_None produced; embedded NUL in a wide buffer is the stride-corruption signature) call-site-tag=0x{:03x} units={}\n"),
                                          ensure_str(Reason ? Reason : "unknown"), Length, At, call_site_tag,
                                          ensure_str(FormatSeamRefusalUnits(nullptr, Length, units)));
        }
        catch (...)
        {
        }
    }

    /** An unpacked FNameEntryId */
    struct FNameEntryHandle
    {
        uint32 Block = 0;
        uint32 Offset = 0;

        FNameEntryHandle(uint32 InBlock, uint32 InOffset)
            : Block(InBlock)
            , Offset(InOffset)
        {
            checkName(Block < FNameMaxBlocks);
            checkName(Offset < FNameBlockOffsets);
        }

        FNameEntryHandle(FNameEntryId Id)
            : Block(Id.ToUnstableInt() >> FNameBlockOffsetBits)
            , Offset(Id.ToUnstableInt() & (FNameBlockOffsets - 1))
        {
        }

        operator FNameEntryId() const
        {
            return FNameEntryId::FromUnstableInt(Block << FNameBlockOffsetBits | Offset);
        }

        explicit operator bool() const { return Block | Offset; }
    };

    static uint32 GetTypeHash(FNameEntryHandle Handle)
    {
        return (Handle.Block << (32 - FNameMaxBlockBits)) + Handle.Block // Let block index impact most hash bits
            + (Handle.Offset << FNameBlockOffsetBits) + Handle.Offset // Let offset impact most hash bits
            + (Handle.Offset >> 4); // Reduce impact of non-uniformly distributed entry name lengths
    }

    uint32 GetTypeHash(FNameEntryId Id)
    {
        // 5.8 dropped the block/offset hash for HashCombineFast of the unstable int with itself
        if (Version::IsAtLeast(5, 8))
        {
            const uint32 UnstableInt = Id.ToUnstableInt();
            return NameHashCombineFast(UnstableInt, UnstableInt);
        }
        return GetTypeHash(FNameEntryHandle(Id));
    }

    // Special wrapper for 'UKismetStringLibrary::Conv_NameToString' that doesn't use StaticFindObject and is safe to use during init.
    FString UKismetStringLibrary_Conv_NameToString(FName InName)
    {
        // We make some assumptions in this function to increase performance.
        // We assume that the CDO, and function always exist and never change or get reallocated.
        // If you use BPMacros, there will be a lot of error checking, but since FName::ToString is a very hot function,
        // we need to avoid as much overhead as possible.

        static const auto ParamStructSize = FName::Conv_NameToStringInternal->GetParmsSize();
#if defined(__PROSPERO__)
        // GetParmsSize is uint16. This function carries InName and a return value;
        // a zero-byte frame cannot hold either and must not reach ProcessEvent.
        if (!ParamStructSize)
        {
            throw std::runtime_error{"Conv_NameToString has an empty parameter frame"};
        }
        auto ParamOwner = Private::MakePayloadParameterFrame(ParamStructSize);
        auto ParamData = ParamOwner.get();
#else
        auto ParamData = static_cast<uint8*>(_malloca(ParamStructSize));
#endif
        FMemory::Memzero(ParamData, ParamStructSize);

        static const auto Offset = []() {
            return FName::Conv_NameToStringInternal->FindProperty(FName(L"InName", FNAME_Find))->GetOffset_Internal();
        }();
        *std::bit_cast<FName*>(&ParamData[Offset]) = InName;

        FName::KismetStringLibraryCDO->ProcessEvent(FName::Conv_NameToStringInternal, ParamData);

        static const auto ReturnOffset = []() {
            return FName::Conv_NameToStringInternal->GetReturnProperty()->GetOffset_Internal();
        }();
        const auto RetValue = *std::bit_cast<FString*>(&ParamData[ReturnOffset]);
#if defined(__PROSPERO__)
        ParamOwner.reset();
#else
        _freea(ParamData);
#endif
        return RetValue;
    }

    const StringType ToStringInternalWrapper_UsingScan(const FName* name)
    {
        FStringOut string{};
        FName::ToStringInternal(name, string);

        StringType name_string{*string ? *string : STR("UE4SS_None")};
        return name_string;
    }

    const StringType ToStringInternalWrapper_UsingConv_NameToString(const FName* name)
    {
        const auto string = UKismetStringLibrary_Conv_NameToString(*name);

        StringType name_string{*string ? *string : STR("UE4SS_None")};
        return name_string;
    }

    // [Seeds][L27-supersede] (agent-ER, 2026-10-09; Claude directive 2):
    // PURE IN-PORT pool decoder for FName::ToString. This removes the entire
    // admission/identity/bind layer from the ToString path: no engine call,
    // no admission gate, any thread. The decode is the L14/L15 pool-side
    // verifier's algorithm (rc::fnameabi::roundtrip, NativeFNameChecks.hpp
    // lineage — it decoded 'ByteProperty' from pool words on EVERY console
    // run: '[L15] FName output verified: pool ToString=ByteProperty') plus
    // the engine's own ToStringInternal semantics re-implemented in-port
    // (EngineToStringProfile.hpp receipt: 'Blocks[id>>16] @ pool+0x10',
    // 'entry header (id&0xFFFF)*2', 'len = header>>6', wide bit, external
    // '_N' suffix when Number != 0). Spec (Claude verbatim):
    // "ComparisonIndex -> block/offset -> entry header (len + wide flag) ->
    // chars, plus '_' + (Number-1) when Number > 0."
    namespace FNamePoolDecoder
    {
        // Frozen engine facts, byte-identical to rc_seed::kPoolLayout
        // (SeedValidation.hpp) and the property guard's verified-pool
        // constants: CurrentBlock u32 @ pool+0x8, CurrentByteOffset u32 @
        // pool+0xC, Blocks[] u64 @ pool+0x10, 16 block-offset bits, 8192
        // max blocks, 2-byte entry stride.
        constexpr uint32_t kCurrentBlockOffset = 0x8;
        constexpr uint32_t kCurrentByteOffsetOffset = 0xC;
        constexpr uint32_t kBlocksOffset = 0x10;
        constexpr uint32_t kBlockOffsetBits = 16;
        constexpr uint32_t kMaxBlocks = 8192;
        constexpr uint32_t kEntryStride = 2;
        // Entry header (u16 LE): bit0 = wide, bits15:6 = Len in units —
        // what roundtrip() decodes ('header&1', 'header>>6') and what the
        // engine itself decodes ('movzx edx,word ptr [rax+rcx*2]';
        // 'shr r12d,6'; 'test dl,1').
        constexpr uint16_t kHeaderWideBit = 1u;
        constexpr uint32_t kHeaderLenShift = 6;
        // rc::fnameabi::kMaxUnits discipline (NAME_SIZE units).
        constexpr uint32_t kMaxUnits = 1024;
        // One whole FName block: FNameBlockOffsets * stride (rc_seed::
        // kFNameBlockCapacityBytes parity). A CLOSED block's own extent —
        // the bound Claude item 4 requires for blocks below the open one.
        constexpr uint64_t kBlockCapacityBytes = (1ull << kBlockOffsetBits) * kEntryStride;

        // First-use dual-check state (Claude directive): the decoder goes
        // live only after it decodes the pinned ByteProperty id to exactly
        // "ByteProperty" once. Unverified -> Verifying -> Verified|Refused,
        // monotonic; a refusal is permanent for the process (the legacy
        // routes keep FName::ToString — loud, never a crash).
        enum class VerifyState : int { Unverified = 0, Verifying = 1, Verified = 2, Refused = 3 };
        std::atomic<uintptr_t> g_verified_pool{0};
        std::atomic<int> g_verify_state{static_cast<int>(VerifyState::Unverified)};
        std::atomic<bool> g_anomaly_notified{false};

        // Admission-ops-independent live read path: byte-wise volatile
        // loads — the read_u64 discipline SeedArming uses (UnrealInitializer
        // GMalloc slots) and the same always-available pool-words path the
        // guard re-reads live at pool+0x8/+0xC (agent O, proven on every
        // console run). The pool address is admission-verified before
        // publication and every per-call access below is bounded by the
        // live words, so no read can leave the verified extents.
        auto load_u16(const volatile uint8_t* p) -> uint16_t
        {
            return static_cast<uint16_t>(static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8));
        }
        auto load_u32(const volatile uint8_t* p) -> uint32_t
        {
            uint32_t value{};
            for (size_t i = 0; i < 4; ++i)
            {
                value |= static_cast<uint32_t>(p[i]) << (8 * i);
            }
            return value;
        }
        auto load_u64(const volatile uint8_t* p) -> uint64_t
        {
            uint64_t value{};
            for (size_t i = 0; i < 8; ++i)
            {
                value |= static_cast<uint64_t>(p[i]) << (8 * i);
            }
            return value;
        }

        struct DecodeResult
        {
            bool ok{false};
            // Out-of-bounds index vs the LIVE pool words (Claude item 4):
            // the caller must mark this LOUDLY — the refusal happened
            // BEFORE any entry read, never past the pool extent.
            bool oob{false};
            StringType value{};
        };

        // The decoder core — PURE (only volatile reads of the verified
        // pool), total over ids (every refusal is a false return, never a
        // throw, never an out-of-pool dereference). Bounds mirror the L15
        // admission chain: rc_seed::check_entry_bounds (open block cursor,
        // engine invariant id>>16 <= CurrentBlock) + the roundtrip block
        // validations (live, non-null, 8-aligned, no pointer overflow).
        // Claude item 4 hardening: EVERY index is bounds-checked against
        // the live words BEFORE any entry read — block <= CurrentBlock
        // (live re-read) AND offset+needed inside the open block's
        // CurrentByteOffset cursor (last block) or the block's own extent
        // (kBlockCapacityBytes for closed blocks); any out-of-range index
        // is refused with oob=true and NO pool entry byte is touched.
        auto decode(uintptr_t pool, uint32_t comparison_index, uint32_t number) -> DecodeResult
        {
            if (!pool)
            {
                return {};
            }
            // Live bounds words (re-read every call, so entries interned
            // after admission are still bounded correctly).
            const uint32_t current_block = load_u32(reinterpret_cast<const volatile uint8_t*>(pool) + kCurrentBlockOffset);
            const uint32_t current_byte_offset = load_u32(reinterpret_cast<const volatile uint8_t*>(pool) + kCurrentByteOffsetOffset);
            if (current_block >= kMaxBlocks)
            {
                return {.oob = true}; // words corrupt: nothing is boundable; refuse before any read
            }

            const uint64_t block_index = static_cast<uint64_t>(comparison_index >> kBlockOffsetBits);
            const uint64_t byte_offset = static_cast<uint64_t>(comparison_index & ((1u << kBlockOffsetBits) - 1u)) * kEntryStride;
            if (block_index >= kMaxBlocks)
            {
                return {.oob = true}; // block id beyond FNameMaxBlocks
            }
            if (block_index > current_block)
            {
                return {.oob = true}; // engine invariant id>>16 <= CurrentBlock (live)
            }
            const uint64_t count_lower_bound = 2; // header alone: bound offset before any entry read
            if (block_index == current_block)
            {
                // The OPEN block: bounded by the live cursor
                // (CurrentByteOffset). Check offset alone first, then the
                // minimal need, so a poisoned offset is refused before the
                // header-length dependent checks below.
                if (byte_offset > current_byte_offset || count_lower_bound > current_byte_offset - byte_offset)
                {
                    return {.oob = true};
                }
            }
            else if (byte_offset > kBlockCapacityBytes || count_lower_bound > kBlockCapacityBytes - byte_offset)
            {
                // A CLOSED block: bounded by the block's own extent.
                return {.oob = true};
            }

            const uint64_t blocks_slot = pool + kBlocksOffset + block_index * 8;
            if (blocks_slot > UINT64_MAX - 8)
            {
                return {};
            }
            const uint64_t block = load_u64(reinterpret_cast<const volatile uint8_t*>(blocks_slot));
            if (!block || (block & 7u) != 0)
            {
                return {}; // roundtrip discipline: live, non-null, 8-aligned
            }
            if (byte_offset > UINT64_MAX - block)
            {
                return {};
            }
            const uint64_t entry = block + byte_offset;

            const uint16_t header = load_u16(reinterpret_cast<const volatile uint8_t*>(entry));
            const bool wide = (header & kHeaderWideBit) != 0;
            const uint32_t units = static_cast<uint32_t>(header) >> kHeaderLenShift;
            if (units == 0 || units > kMaxUnits)
            {
                return {};
            }
            const uint64_t count = static_cast<uint64_t>(units) * (wide ? 2u : 1u);
            const uint64_t needed = 2 + count;
            // Full-entry bound against the same live extent (open-block
            // cursor / closed-block capacity) now that the header length
            // is known — still before any character byte is read.
            const uint64_t extent = block_index == current_block ? static_cast<uint64_t>(current_byte_offset) : kBlockCapacityBytes;
            if (byte_offset > extent || needed > extent - byte_offset)
            {
                return {.oob = true};
            }
            if (entry > UINT64_MAX - needed)
            {
                return {};
            }

            // [L27][EV string-seam] (agent-EV 2026-10-09): the output string
            // is built with resize + indexed writes ONLY — no push_back. On
            // short-wchar targets, basic_string<wchar_t>::push_back can
            // resolve to the prebuilt library's 4-byte-wchar extern-template
            // instantiation and interleave a NUL after every unit
            // (host-reproduced under -fshort-wchar), corrupting EVERY
            // decoded name compared by FindXOf round-trips and walks.
            // resize + operator[] are inline and stride-correct everywhere
            // (host-audited at SSO and heap sizes).
            const size_t tail_units = (number != 0 ? 1 + 10 : 0); // '_' + at most 10 digits
            StringType out{};
            out.resize(static_cast<size_t>(units) + tail_units);
            size_t used = 0;
            const volatile uint8_t* chars = reinterpret_cast<const volatile uint8_t*>(entry + 2);
            for (uint32_t i = 0; i < units; ++i)
            {
                const uint32_t unit = wide
                    ? static_cast<uint32_t>(chars[i * 2]) | (static_cast<uint32_t>(chars[i * 2 + 1]) << 8)
                    : static_cast<uint32_t>(chars[i]);
                // Wide: UTF-16 code unit 1:1. ANSI: widen 1:1 — the
                // engine's own in-place widen (admission's <=0x7f narrow
                // discipline was proof-grade, not decode semantics).
                out[used++] = static_cast<CharType>(unit);
            }
            if (number != 0)
            {
                // Internal -> external number, exactly the engine tail:
                // 'dec ecx' then '_%i' append when Number != 0.
                out[used++] = static_cast<CharType>('_');
                char digits[12];
                int n = 0;
                uint32_t external = number - 1;
                do
                {
                    digits[n++] = static_cast<char>('0' + (external % 10));
                    external /= 10;
                } while (external);
                while (n)
                {
                    out[used++] = static_cast<CharType>(digits[--n]);
                }
            }
            out.resize(used);
            return {.ok = true, .value = std::move(out)};
        }

        // First-use dual-check (Claude directive 2 item 4): decode the
        // pinned ByteProperty id (rc::engtostring::kProbeId == 0x3, probe
        // constants retained in EngineToStringProfile.hpp) and require
        // exactly "ByteProperty". Logs once on success
        // ('[FN] pool-decoder verified (ByteProperty)'); on mismatch the
        // decoder is refused loudly and permanently for the process.
        auto ensure_verified(uintptr_t pool) -> VerifyState
        {
            const int observed = g_verify_state.load(std::memory_order_acquire);
            if (observed != static_cast<int>(VerifyState::Unverified))
            {
                return static_cast<VerifyState>(observed);
            }
            int expected = static_cast<int>(VerifyState::Unverified);
            if (!g_verify_state.compare_exchange_strong(expected, static_cast<int>(VerifyState::Verifying), std::memory_order_acq_rel))
            {
                // Another thread owns the first-use verify; anything other
                // than Verified makes THIS call take the legacy route.
                return static_cast<VerifyState>(expected);
            }
            const auto probe = decode(pool, rc::engtostring::kProbeId, 0);
            bool ok = probe.ok && probe.value.size() == rc::engtostring::kProbeExpectUnits;
            for (uint32_t i = 0; ok && i < rc::engtostring::kProbeExpectUnits; ++i)
            {
                ok = probe.value[i] == static_cast<CharType>(rc::engtostring::kProbeExpect[i]);
            }
            g_verify_state.store(static_cast<int>(ok ? VerifyState::Verified : VerifyState::Refused), std::memory_order_release);
            try
            {
                if (ok)
                {
                    Output::send(STR("[FN] pool-decoder verified (ByteProperty)\n"));
                }
                else
                {
                    Output::send<LogLevel::Error>(STR("[FN] pool-decoder refused: ByteProperty probe did not decode; legacy route retains FName::ToString\n"));
                }
            }
            catch (...)
            {
            }
            return ok ? VerifyState::Verified : VerifyState::Refused;
        }

        auto notify_anomaly_once(uint32_t comparison_index) -> void
        {
            bool expected = false;
            if (!g_anomaly_notified.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            {
                return;
            }
            try
            {
                Output::send<LogLevel::Error>(STR("[FN] pool-decoder anomaly: entry refused for id 0x{:X}; legacy route used for that call\n"), comparison_index);
            }
            catch (...)
            {
            }
        }

        // Claude item 4: out-of-range indices get a LOUD, per-id marker
        // naming exactly what was refused — the refusal happened BEFORE
        // any entry read (block <= live CurrentBlock and offset inside the
        // live cursor / block extent are checked first), so nothing ever
        // reads past the pool. Bounded per-id dedupe (16 ids, the
        // g_ffo_query_fallback_indices discipline): a poisoned id in a hot
        // loop must not flood the log, but a SECOND distinct poisoned id
        // still surfaces. Races only risk a duplicate line, never a miss.
        constexpr int kMaxOobNotifiedIds = 16;
        uint32_t g_oob_notified_ids[kMaxOobNotifiedIds]{};
        int g_oob_notified_count = 0;
        auto notify_oob(uint32_t comparison_index) -> void
        {
            for (int i = 0; i < g_oob_notified_count; ++i)
            {
                if (g_oob_notified_ids[i] == comparison_index) { return; }
            }
            if (g_oob_notified_count >= kMaxOobNotifiedIds) { return; }
            g_oob_notified_ids[g_oob_notified_count++] = comparison_index;
            try
            {
                const uint32_t block = comparison_index >> kBlockOffsetBits;
                const uint32_t offset = (comparison_index & ((1u << kBlockOffsetBits) - 1u)) * kEntryStride;
                Output::send<LogLevel::Error>(STR("[FN] pool-decoder OOB id=0x{:X} block={} offset={}: refused before any entry read (live CurrentBlock/CurrentByteOffset bound); legacy route used for that call\n"),
                                              comparison_index, block, offset);
            }
            catch (...)
            {
            }
        }

        // The ToString primitive: true + out on a verified decode; false
        // (fall to the legacy route for THAT call) when unpublished, refused, or
        // on any per-call refusal. Claude item 4: an out-of-range index
        // gets the loud '[FN] pool-decoder OOB ...' marker; a shape
        // refusal keeps the once-per-process anomaly notice.
        auto try_tostring(const FName& name, StringType& out) -> bool
        {
            const uintptr_t pool = g_verified_pool.load(std::memory_order_acquire);
            if (!pool)
            {
                return false;
            }
            if (ensure_verified(pool) != VerifyState::Verified)
            {
                return false;
            }
            auto decoded = decode(pool, name.GetComparisonIndex().ToUnstableInt(), static_cast<uint32_t>(name.GetNumber()));
            if (!decoded.ok)
            {
                if (decoded.oob)
                {
                    notify_oob(name.GetComparisonIndex().ToUnstableInt());
                }
                else
                {
                    notify_anomaly_once(name.GetComparisonIndex().ToUnstableInt());
                }
                return false;
            }
            out = std::move(decoded.value);
            return true;
        }

        void PublishVerifiedPoolAddress(uintptr_t pool_address) noexcept
        {
            // Single-shot: the first admission-verified address wins,
            // exactly like NativePropertyGuard::PublishVerifiedPoolAddress.
            if (!pool_address)
            {
                return;
            }
            uintptr_t expected = 0;
            g_verified_pool.compare_exchange_strong(expected, pool_address, std::memory_order_release, std::memory_order_relaxed);
        }

        bool Ready() noexcept
        {
            const uintptr_t pool = g_verified_pool.load(std::memory_order_acquire);
            return pool && ensure_verified(pool) == VerifyState::Verified;
        }
    } // namespace FNamePoolDecoder

    const StringType ToStringInternalWrapper(const FName* name)
    {
        using namespace UnrealInitializer;
        // [Seeds][L27-supersede] (agent-ER): the PURE IN-PORT pool decoder
        // (FNamePoolDecoder above) owns FName::ToString once the
        // SeedArming-verified pool is published and the first-use
        // ByteProperty dual-check passes — no engine call, no admission
        // gate, any thread. It displaces both the deleted L27 engine
        // ToStringInternal bind AND the per-call Kismet
        // Conv_NameToString ProcessEvent fallback that returned empty
        // in-world and broke both arms of the verified-query fix. Any
        // refusal (unpublished, dual-check failed, per-call shape
        // anomaly) falls to the legacy Scan/Kismet routes below, exactly
        // as before ('[FN] pool-decoder ...' markers name the live route).
        if (StringType decoded; FNamePoolDecoder::try_tostring(*name, decoded))
        {
            return decoded;
        }
        if (StaticStorage::GlobalConfig.FNameToStringMethod == FNameToStringMethod::Scan && FName::ToStringInternal.is_ready())
        {
            return ToStringInternalWrapper_UsingScan(name);
        }
        else if (FName::Conv_NameToStringInternal)
        {
            return ToStringInternalWrapper_UsingConv_NameToString(name);
        }
        else
        {
            throw std::runtime_error{"FName::ToString was not ready but was called anyway"};
        }
    }

    int32 FName::StaticSize_Private{-1};
    int32 FName::StaticAlignment_Private{-1};

    int32 FName::StaticSize()
    {
        if (StaticSize_Private < 0)
        {
            throw std::runtime_error{"[FName::StaticSize] StaticSize_Private is not valid"};
        }
        else
        {
            return StaticSize_Private;
        }
    }

    int32 FName::StaticAlignment()
    {
        if (StaticAlignment_Private < 0)
        {
            throw std::runtime_error{"[FName::StaticAlignment] StaticAlignment_Private is not valid"};
        }
        else
        {
            return StaticAlignment_Private;
        }
    }

    auto FName::ToString() -> StringType
    {
        return ToStringInternalWrapper(this);
    }

    auto FName::ToString() const -> const StringType
    {
        return ToStringInternalWrapper(this);
    }

    // Returns FString (TCHAR-based)
    FString FName::ToFString() const
    {
        StringType str = ToString();
        return FString(str);
    }

    FUtf8String FName::ToFUtf8String() const
    {
        StringType str = ToString();
        return FUtf8String(str);
    }

    FAnsiString FName::ToFAnsiString() const
    {
        StringType str = ToString();
        return FAnsiString(str);
    }

    uint32 FName::GetPlainNameString(TCHAR(&OutName)[NAME_SIZE])
    {
        const uint32 Entry = GetDisplayIndex().ToUnstableInt();
        auto String = FName(Entry).ToString();
        // l26 fix (agent-ES, decoder-truncation assignment 2026-10-09): the
        // upstream line multiplied the TCHAR COUNT by
        // sizeof(File::StringType::size_type) == sizeof(size_t) == 8 — a
        // units-vs-bytes confusion on the ToString surface. It over-read the
        // u16 buffer 4x (UB for every name) and overflowed the 1024-TCHAR
        // OutName for any name >= 257 units (800+ byte writes into a
        // 2048-byte buffer at 500 units, 8192 bytes at 1023). Correct unit
        // width is sizeof(TCHAR) == sizeof(CharType); the copy is bounded to
        // NAME_SIZE-1 units (engine ToStringTruncate doctrine) and always
        // NUL-terminated. No in-tree caller exists today (verified by
        // grep), so the tightened return contract (copied unit count) has
        // no consumer to disturb; the function can never again corrupt the
        // caller's stack while ToString is owned by the pool decoder.
        const uint32 CopyUnits = String.size() < NAME_SIZE - 1
            ? static_cast<uint32>(String.size())
            : static_cast<uint32>(NAME_SIZE - 1);
        std::memcpy(OutName, String.data(), static_cast<size_t>(CopyUnits) * sizeof(TCHAR));
        OutName[CopyUnits] = static_cast<TCHAR>('\0');
        return CopyUnits;
    }
}
