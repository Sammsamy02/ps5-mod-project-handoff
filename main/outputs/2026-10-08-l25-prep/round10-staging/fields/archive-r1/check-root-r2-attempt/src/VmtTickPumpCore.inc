// =====================================================================================
// [EU FIX4 2026-10-09, agent-EU] GameEngine Tick VMT pump — host-testable core.
//
// Claude l26 fix 4 (verbatim technique source: l23-runtime profile
// VMT-FALLBACK.md, the copied-vtable fallback design): the GameEngine
// instance's vtable is copied from (vptr - 0x10) — the offset-to-top and
// RTTI words — through its LAST CODE ENTRY into a private writable copy;
// slot 0x310 (UEngine::Tick, the profiled offset; entry 98) is replaced
// with our hook; ONE aligned 8-byte vptr store publishes the swap. The
// hook calls the ORIGINAL Tick first (exactly once, same receiver/args),
// then publishes the game-thread identity on the first call and drains
// the game-thread queue in the R3 marshal's drain shape. The original
// vptr is restored before teardown. Data-only and XOM-safe: no code byte
// is ever written or read; the original vtable itself is never written.
//
// This .inc is deliberately engine-free (only <cstdint>/<cstddef>/<new>):
// the target unit (UnrealInitializer.cpp) and the host fixture
// (tests/actual_eu_vmt_pump.cpp) include the SAME core, so the vtable
// copy/swap/publish/restore mechanics are host-proven against mock
// objects. All platform policy — text-segment classification, logging,
// the real publication/drain bodies — enters through callables and is
// owned by the including TU. The R3 FuncPtr-marshal carriers REMAIN the
// fallback pump (unchanged); both drains share the queue-move semantics
// and the LuaMod dispatch-scope guard, so concurrent/sequential double
// drains are idempotent.
//
// Safety shape carried from VMT-FALLBACK.md where l26 fix 4 keeps it:
//   - the copy preserves the admitted runtime bit patterns of the
//     offset-to-top and typeinfo words verbatim (including a null
//     typeinfo) and the copy's address point is allocation + 16 bytes;
//   - the copied range covers every entry through the last plausible
//     code pointer in a BOUNDED scan, validated word-by-word against
//     the text segment (a non-text word inside the copied range is a
//     refusal, not a truncation);
//   - the replacement is fully prepared and verified BEFORE the single
//     aligned publication store; the allocation is retained for the
//     process lifetime (never freed — a pre-entry reader may hold the
//     copied vptr indefinitely);
//   - retirement restores the original vptr ONLY if the object still
//     owns our replacement address point (ownership loss is reported,
//     never clobbered);
//   - the hook never lets a diagnostic/drain failure omit or duplicate
//     the original call: the original is forwarded FIRST, everything
//     after it is exception-isolated.
// The l26 fix relaxes two doc prereqs by explicit controller decision
// (Claude l26 fix 4): the arming point is startup init (not a proven
// engine quiescence point — the single aligned store is the accepted
// publication primitive), and the table extent is the bounded validated
// scan (not a full construction-table proof).
// =====================================================================================
#ifndef RC_UE_VMT_TICK_PUMP_CORE_INC
#define RC_UE_VMT_TICK_PUMP_CORE_INC

#include <cstddef>
#include <cstdint>
#include <new>

namespace RC::Unreal::VmtTickPump
{
    // Itanium-style ABI: the object's vptr addresses the first virtual
    // slot; the two words before it are the signed offset-to-top [-2]
    // and the typeinfo pointer [-1]. The copy reproduces [-2..last].
    constexpr size_t kPrefixWords = 2;

    // The typed Tick entry (SysV: receiver RDI, float XMM0, bool ESI —
    // the profiled GameEngine::Tick entry preserves exactly these).
    using TickFn = void (*)(void*, float, bool);

    // Scan/validation outcome reasons (kept narrow — they are logged
    // through the including TU's diagnostics).
    enum class FailReason : uint8_t
    {
        None = 0,
        NullParams,
        NullObject,
        NullAddressPoint,
        TickSlotOutOfRange,
        NoCodeEntries,
        NonTextEntryInTable,
        NonTextTickEntry,
        OutOfMemory,
        CopyMismatch,
        NotArmed,
        AlreadyArmed,
        OwnershipLost,
    };
    [[nodiscard]] inline auto fail_reason_name(FailReason r) -> const char*
    {
        switch (r)
        {
        case FailReason::None: return "none";
        case FailReason::NullParams: return "null-params";
        case FailReason::NullObject: return "null-object";
        case FailReason::NullAddressPoint: return "null-address-point";
        case FailReason::TickSlotOutOfRange: return "tick-slot-out-of-range";
        case FailReason::NoCodeEntries: return "no-code-entries";
        case FailReason::NonTextEntryInTable: return "non-text-entry-in-table";
        case FailReason::NonTextTickEntry: return "non-text-tick-entry";
        case FailReason::OutOfMemory: return "out-of-memory";
        case FailReason::CopyMismatch: return "copy-mismatch";
        case FailReason::NotArmed: return "not-armed";
        case FailReason::AlreadyArmed: return "already-armed";
        case FailReason::OwnershipLost: return "ownership-lost";
        }
        return "unknown";
    }

    // All mutable pump state. A single instance owns one swap; the
    // target TU holds one file-static instance, the fixture holds its
    // own. No globals live in this header so each TU's copy is isolated.
    struct PumpCore
    {
        // --- swap identity ---
        void* object{nullptr};              // the instance whose vptr slot is swapped
        void** original_address_point{nullptr}; // original vptr value (== &original entries [0])
        void* original_tick_entry{nullptr}; // Tick slot value captured from the ORIGINAL table (called as-is)
        void* hook_entry{nullptr};          // our replacement Tick entry
        void** copy_allocation{nullptr};    // [-2 .. entry_count-1]; address point = copy_allocation + kPrefixWords
        size_t entry_count{0};              // number of copied virtual entries [0, entry_count)
        size_t tick_slot_index{0};

        // --- lifecycle ---
        bool armed{false};
        bool retired{false};
        bool ownership_lost{false};

        // --- runtime evidence (non-atomic counters; owner-visible only) ---
        uint64_t tick_count{0};
        uint64_t original_call_count{0};
        uint64_t drain_count{0};
        uint64_t publish_attempt_count{0};
        uint64_t first_tick_aborted_count{0};
        bool identity_published{false};

        [[nodiscard]] auto copy_address_point() const -> void**
        {
            return copy_allocation ? copy_allocation + kPrefixWords : nullptr;
        }

        // Read an 8-byte word without tearing assumptions on the target:
        // byte-wise volatile loads, the SeedArming read_u64 discipline.
        static auto load_word(const void* const* slot) -> uintptr_t
        {
            const auto* bytes = reinterpret_cast<const volatile uint8_t*>(slot);
            uintptr_t value{};
            for (size_t i = 0; i < 8; ++i)
            {
                value |= static_cast<uintptr_t>(bytes[i]) << (8 * i);
            }
            return value;
        }

        // Bounded scan for the LAST plausible code entry of the original
        // vtable. in_text(uint64_t) decides "plausible code pointer"
        // (target: the pinned text segment; fixture: a mock range). A
        // word is accepted iff in_text says so; the table extent is the
        // highest accepted index + 1. Every NON-code word INSIDE that
        // extent is a refusal (the copy must not carry a non-entry into
        // a live slot) — so the scan first finds the extent, then the
        // caller-side build re-validates each copied word. Returns the
        // entry count (> tick_slot) or 0 with *reason set.
        template <class InText>
        [[nodiscard]] static auto scan_table(const void* const* address_point, size_t max_entries, size_t tick_slot, InText in_text, FailReason* reason)
            -> size_t
        {
            *reason = FailReason::None;
            if (!address_point || !max_entries)
            {
                *reason = FailReason::NullAddressPoint;
                return 0;
            }
            size_t last_code = SIZE_MAX;
            for (size_t i = 0; i < max_entries; ++i)
            {
                const auto word = load_word(address_point + i);
                if (word != 0 && in_text(word))
                {
                    last_code = i;
                }
            }
            if (last_code == SIZE_MAX)
            {
                *reason = FailReason::NoCodeEntries;
                return 0;
            }
            if (tick_slot > last_code)
            {
                *reason = FailReason::TickSlotOutOfRange;
                return 0;
            }
            return last_code + 1;
        }

        // Build (allocate + fill + poke + verify) the private copy. Does
        // NOT publish — publish_swap() is a separate, explicit step so
        // the caller can log the prepared state first. The allocation is
        // retained forever: release() does not exist by design.
        template <class InText>
        [[nodiscard]] auto arm(void* object_, size_t tick_slot, void* hook, size_t max_entries, InText in_text, FailReason* reason) -> bool
        {
            *reason = FailReason::None;
            if (armed)
            {
                *reason = FailReason::AlreadyArmed;
                return false;
            }
            if (!object_ || !hook)
            {
                *reason = FailReason::NullParams;
                return false;
            }
            const auto* vptr_words = reinterpret_cast<void* const*>(*static_cast<void** volatile*>(object_));
            if (!vptr_words)
            {
                *reason = FailReason::NullAddressPoint;
                return false;
            }
            // The prefix words are read through the same byte-wise
            // discipline; their bit patterns (including null typeinfo)
            // are preserved verbatim into the copy.
            uintptr_t prefix[kPrefixWords]{};
            prefix[0] = load_word(vptr_words - 2);
            prefix[1] = load_word(vptr_words - 1);

            const size_t count = scan_table(vptr_words, max_entries, tick_slot, in_text, reason);
            if (count == 0)
            {
                return false;
            }
            // The Tick slot itself must be a code pointer in the text
            // segment (null/thunk-less slot is a refusal).
            const auto tick_word = load_word(vptr_words + tick_slot);
            if (!tick_word || !in_text(tick_word))
            {
                *reason = FailReason::NonTextTickEntry;
                return false;
            }

            void** allocation = static_cast<void**>(::operator new[]((kPrefixWords + count) * sizeof(void*), std::nothrow_t{}));
            if (!allocation)
            {
                *reason = FailReason::OutOfMemory;
                return false;
            }
            // Publish-ready state is staged into locals first; PumpCore
            // fields mutate only after the copy is fully verified.
            allocation[0] = reinterpret_cast<void*>(prefix[0]);
            allocation[1] = reinterpret_cast<void*>(prefix[1]);
            for (size_t i = 0; i < count; ++i)
            {
                if (i == tick_slot)
                {
                    allocation[kPrefixWords + i] = hook;
                    continue;
                }
                allocation[kPrefixWords + i] = reinterpret_cast<void*>(load_word(vptr_words + i));
            }
            // Verify: prefix verbatim, poked slot exact, every other
            // entry bit-identical, and no non-text word entered the copy.
            if (load_word(allocation + 0) != prefix[0] || load_word(allocation + 1) != prefix[1])
            {
                ::operator delete[](allocation);
                *reason = FailReason::CopyMismatch;
                return false;
            }
            for (size_t i = 0; i < count; ++i)
            {
                const auto copied = load_word(allocation + kPrefixWords + i);
                if (i == tick_slot)
                {
                    if (copied != reinterpret_cast<uintptr_t>(hook))
                    {
                        ::operator delete[](allocation);
                        *reason = FailReason::CopyMismatch;
                        return false;
                    }
                    continue;
                }
                if (copied != load_word(vptr_words + i))
                {
                    ::operator delete[](allocation);
                    *reason = FailReason::CopyMismatch;
                    return false;
                }
                // Validation law (Claude l26 fix 4): every entry inside the
                // copied extent is either null (a legal reserved slot) or a
                // text-segment code pointer — anything else refuses the arm.
                if (copied && !in_text(copied))
                {
                    ::operator delete[](allocation);
                    *reason = FailReason::NonTextEntryInTable;
                    return false;
                }
            }

            object = object_;
            original_address_point = const_cast<void**>(vptr_words);
            original_tick_entry = reinterpret_cast<void*>(tick_word);
            hook_entry = hook;
            copy_allocation = allocation;
            entry_count = count;
            tick_slot_index = tick_slot;
            armed = true;
            retired = false;
            ownership_lost = false;
            return true;
        }

        // ONE aligned 8-byte vptr store publishes the swap. Volatile
        // qualifies the store so the compiler cannot tear, fuse, or
        // elide it; alignment is the object's own (UEngine instances are
        // pointer-aligned). Readback is observational only.
        [[nodiscard]] auto publish_swap() -> bool
        {
            if (!armed || !object || !copy_address_point())
            {
                return false;
            }
            *reinterpret_cast<void** volatile*>(object) = copy_address_point();
            return load_word(static_cast<void**>(object)) == reinterpret_cast<uintptr_t>(copy_address_point());
        }

        // Restore the original vptr. ONLY writes when the object still
        // owns OUR replacement address point; a foreign writer is never
        // clobbered (reported via OwnershipLost).
        enum class RestoreOutcome : uint8_t { Restored, NotArmed, AlreadyRetired, OwnershipLost, RestoreMismatch };
        [[nodiscard]] auto retire() -> RestoreOutcome
        {
            if (!armed)
            {
                return RestoreOutcome::NotArmed;
            }
            if (retired)
            {
                return RestoreOutcome::AlreadyRetired;
            }
            const auto current = load_word(static_cast<void**>(object));
            const auto ours = reinterpret_cast<uintptr_t>(copy_address_point());
            if (current != ours)
            {
                retired = true;
                ownership_lost = true;
                return RestoreOutcome::OwnershipLost;
            }
            *reinterpret_cast<void** volatile*>(object) = original_address_point;
            if (load_word(static_cast<void**>(object)) != reinterpret_cast<uintptr_t>(original_address_point))
            {
                // The restore store did not land; state stays armed so a
                // later teardown pass can retry. Copy retained either way.
                return RestoreOutcome::RestoreMismatch;
            }
            retired = true;
            return RestoreOutcome::Restored;
        }
    };

    // Hook-body driver — the exact FIX 4 ordering, shared by the target
    // hook entry and the fixture's mock dispatch:
    //   1. forward to the captured ORIGINAL Tick entry exactly once,
    //      same receiver/delta/idle, direct call (never the object's
    //      virtual Tick — that would recurse);
    //   2. on the first call, publish the game-thread identity through
    //      the platform callback (refusal aborts the first-tick extras,
    //      never the original call, which already happened);
    //   3. run the first-tick self-test (FIX 5's UFunction line), then
    //      the time-boxed game-thread queue drain.
    // Everything after the original call is exception-isolated: a
    // diagnostic or drain failure must neither omit nor duplicate the
    // original Tick, and must not throw across the engine boundary.
    struct TickCallbacks
    {
        bool (*publish_identity)() noexcept;      // true => this caller IS the game thread
        void (*first_tick_selftest)() noexcept;   // FIX 5 UFunction self-test (game thread, once)
        void (*drain)() noexcept;                 // time-boxed game-thread queue drain
    };

    auto drive_tick(PumpCore& pump, const TickCallbacks& cb, void* self, float delta_seconds, bool b_idle) noexcept -> void
    {
        if (!pump.armed || pump.retired)
        {
            return;
        }
        ++pump.tick_count;
        // 1. Exactly-once original forwarding, before anything that can fail.
        if (pump.original_tick_entry)
        {
            ++pump.original_call_count;
            reinterpret_cast<TickFn>(pump.original_tick_entry)(self, delta_seconds, b_idle);
        }
        // 2+3. First-tick publication + self-test, then the drain. Each
        // stage is independently isolated so one failing stage cannot
        // skip a later one or unwind into the engine.
        if (!pump.identity_published)
        {
            ++pump.publish_attempt_count;
            bool published = false;
            try
            {
                published = cb.publish_identity && cb.publish_identity();
            }
            catch (...)
            {
                published = false;
            }
            if (!published)
            {
                ++pump.first_tick_aborted_count;
                return;
            }
            pump.identity_published = true;
            try
            {
                if (cb.first_tick_selftest)
                {
                    cb.first_tick_selftest();
                }
            }
            catch (...)
            {
            }
        }
        try
        {
            if (cb.drain)
            {
                ++pump.drain_count;
                cb.drain();
            }
        }
        catch (...)
        {
        }
    }

    // [FIX5 discipline] NUL-terminated u16->narrow-ascii copy for
    // self-test log lines. The PS5 SDK's broken short-wchar widen
    // (send_narrow_diag doctrine, /tmp/bind1.log evidence) is bypassed
    // entirely: characters are copied one at a time; any non-ascii unit
    // becomes '?'. Never reads past src's NUL or dst's cap-1; always
    // NUL-terminates.
    inline auto narrow_ascii_copy(const void* utf16_units, size_t max_units, char* dst, size_t dst_cap) -> size_t
    {
        if (!dst || dst_cap == 0)
        {
            return 0;
        }
        const auto* src = static_cast<const uint16_t*>(utf16_units);
        size_t written{};
        for (size_t i = 0; i < max_units && written + 1 < dst_cap; ++i)
        {
            const uint16_t unit = src[i];
            if (unit == 0)
            {
                break;
            }
            dst[written++] = unit < 0x80 ? static_cast<char>(unit) : '?';
        }
        dst[written] = '\0';
        return written;
    }
} // namespace RC::Unreal::VmtTickPump

#endif // RC_UE_VMT_TICK_PUMP_CORE_INC
