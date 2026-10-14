#pragma once
// =========================================================================================
// [L11PB VT-R1] DispatchDataDetour — the dispatch-data (vtable/FuncPtr slot swap) hook
// backend, prebuilt PREPARED-UNARMED by worker-launch11-vtable-prebuild (2026-10-04).
//
// Design: TEAM:worker-vtable-swap-design/VTABLE-SWAP-DESIGN.md §4 (thunk + saved-entry
// discipline), §5 R1 (exact consumed-PLH surface), §3 (write-window ladder, D-1).
// Consumed surface it must expose — PLH::x64Detour as consumed at
// port/unreal/include/Unreal/Hooks/Internal/DetourInstance.hpp:164-166 and
// ue4ss-native-owned-default-detour-1/port/x64Detour.cpp:30:
//     ctor(const uint64_t fnAddress, const uint64_t fnCallback, uint64_t* userTrampVar)
//     hook() -> bool, unHook() -> bool, isHooked() -> bool
// with the saved original entry written into *userTrampVar (§4.3: the "trampoline" IS the
// original function — no instruction byte is ever copied or patched).
//
// ARMING STATE (honest, pre-verdict):
//  * This file contains ZERO syscall instructions and NO mprotect mechanics. The D-1
//    data-page transition (mprotect R->RW on a prot=1 RELRO page, raw syscall 74) is the
//    IGW-1/D-1 probe's open question (VTABLE-SWAP-DESIGN §3 sequencing: D-1 is meaningful
//    only after IGW-1's outcome is reconciled; the igf5 raw-syscall crash class stays
//    inside its falsifiable-probe containment). Until then DispatchDataTransition
//    REFUSES (kAvailable=false): every hook() fails closed at the transition gate and the
//    existing EnsureHooked -> ERROR_ID -> loud-throw path fires unchanged. The refusal is
//    LOUD and names the consequence, mirroring the initializer's refusal discipline.
//  * The slot registry (R2) is empty until the verdict-gated capture at the walks is
//    staged (TEAM:worker-launch11-vtable-prebuild/build-ready/); hook() refuses on the
//    empty registry for the same reason.
//  * The backend is SELECTED only when UE4SS_DISPATCH_DATA_BACKEND=1 at the
//    DetourInstance.hpp seam. The staged default is 0 = PLH::x64Detour, i.e. the composed
//    runtime behavior is identical to the current lineage.
// =========================================================================================

#include <atomic>
#include <bit>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

#include <DynamicOutput/Output.hpp>
#include <Helpers/String.hpp>

namespace RC::Unreal::Hook
{
    // -------------------------------------------------------------------------------------
    // R2 capture record: what the verdict-gated slot capture at the existing vtable walks
    // pushes in (VTABLE-SWAP-DESIGN §5 R2). Keyed by entry_value — which is exactly the
    // `fnAddress` R1 receives (the walk's resolved entry == the initializer's resolved
    // function address, cross-checked at SINI:1315-1378 for Tick).
    // -------------------------------------------------------------------------------------
    struct DispatchSlotRecord
    {
        void** SlotAddress;   // the exact writable word (vtable+i or &UFunction::FuncPtr)
        void* EntryValue;     // the slot's value at capture time (the original entry)
        const void* ClassInstance; // the live instance the walk read from (log/coverage only)
    };

    // Single-writer discipline: all recording happens on the serialized init thread, but
    // the mutex keeps the registry honest against any future second walker.
    class DispatchDataRegistry
    {
    public:
        static auto record_slot(void** slot_address, void* entry_value, const void* class_instance) -> void
        {
            std::lock_guard<std::mutex> guard(s_mutex());
            s_slots().push_back(DispatchSlotRecord{slot_address, entry_value, class_instance});
        }

        // Hot-path-safe variant for the walk-macro capture (build-ready VT-R2): records
        // only when this EXACT (slot, entry) pair is not already recorded. The address
        // walks run at hook-registration time (init thread), so the linear scan is
        // bounded by the number of dispatch targets, not by call frequency.
        static auto record_slot_once(void** slot_address, void* entry_value, const void* class_instance) -> void
        {
            std::lock_guard<std::mutex> guard(s_mutex());
            for (const auto& record : s_slots())
            {
                if (record.SlotAddress == slot_address && record.EntryValue == entry_value) return;
            }
            s_slots().push_back(DispatchSlotRecord{slot_address, entry_value, class_instance});
        }

        // Returns nullptr when no capture recorded this entry — hook() refuses loudly.
        static auto find_by_entry(uint64_t entry_value) -> const DispatchSlotRecord*
        {
            std::lock_guard<std::mutex> guard(s_mutex());
            for (const auto& record : s_slots())
            {
                if (std::bit_cast<uint64_t>(record.EntryValue) == entry_value) return &record;
            }
            return nullptr;
        }

        static auto size() -> uint64_t
        {
            std::lock_guard<std::mutex> guard(s_mutex());
            return s_slots().size();
        }

    private:
        static auto s_mutex() -> std::mutex&
        {
            static std::mutex mutex;
            return mutex;
        }
        static auto s_slots() -> std::vector<DispatchSlotRecord>&
        {
            static std::vector<DispatchSlotRecord> slots;
            return slots;
        }
    };

    // -------------------------------------------------------------------------------------
    // D-1 write-window transition seam (VTABLE-SWAP-DESIGN §3 steps 1/2/5). The verdict-
    // gated implementation (raw syscall 74 constants per C-6, sysctl-202 mapscan prot gate,
    // re-protect + mapscan verify, one page in flight) is authored ONLY after the IGW/D-1
    // evidence exists; this prebuild ships the refusing state so no unverified syscall
    // mechanic enters the composed runtime ahead of its falsifiable probe.
    // -------------------------------------------------------------------------------------
    class DispatchDataTransition
    {
    public:
        // Fail-closed by construction: the pre-verdict backend never opens a write window.
        static constexpr bool kAvailable = false;

        // Would mapscan-gate prot ∈ {1 (vtable page), 3 (heap FuncPtr page)}, refuse prot=4
        // (never exec), then mprotect R->RW for a 0x4000 write window and record rc/errno
        // write-through. Returns nullptr while kAvailable is false.
        static auto arm_write_window(void** /*slot_address*/) -> void**
        {
            return nullptr;
        }

        // Would re-protect PROT_READ, fresh-mapscan verify prot back to 1, and fail closed
        // (stop the ladder) on anomaly — the igf restore discipline (§3 step 5).
        static auto restore_write_window(void** /*window*/) -> bool
        {
            return false;
        }
    };

    // -------------------------------------------------------------------------------------
    // The backend. Lifetime/arbitration mirror the consumed PLH surface:
    //  * constructed by TDetourInstance::InstallHook at the single construction site,
    //  * ActivateHook calls hook(), DeactivateHook (PROSPERO) only asks isHooked(),
    //  * Invoke reaches the original through Trampoline == saved original entry (§4.3).
    // -------------------------------------------------------------------------------------
    class DispatchDataDetour
    {
    public:
        DispatchDataDetour(const uint64_t fnAddress, const uint64_t fnCallback, uint64_t* userTrampVar) :
            m_fnAddress(fnAddress), m_fnCallback(fnCallback), m_userTrampVar(userTrampVar) {}

        ~DispatchDataDetour() = default;
        DispatchDataDetour(const DispatchDataDetour&) = delete;
        DispatchDataDetour& operator=(const DispatchDataDetour&) = delete;

        // VTABLE-SWAP-DESIGN §3 ladder, fail-closed at every gate. Returns false on any
        // refusal — the caller's existing ERROR_ID -> loud-throw path fires unchanged
        // (DetourInstance.hpp EnsureHooked; SINI:1384-1387 shape).
        auto hook() -> bool
        {
            // §3 step 3 precondition data: the R2 capture for THIS resolved entry.
            const DispatchSlotRecord* record = DispatchDataRegistry::find_by_entry(m_fnAddress);
            if (!record) [[unlikely]]
            {
                Output::send<LogLevel::Error>(STR("[DispatchData] refused: no slot capture for entry 0x{:X} — the R2 walk capture is not armed; hook cannot identify its slot\n"), m_fnAddress);
                return false;
            }

            // §3 steps 1/2: D-1 write window. Pre-verdict this is the refusing seam.
            if (!DispatchDataTransition::kAvailable) [[unlikely]]
            {
                Output::send<LogLevel::Error>(STR("[DispatchData] refused: D-1 data-page transition not available (pre-verdict refusing seam); no write window opened\n"));
                return false;
            }
            void** window = DispatchDataTransition::arm_write_window(record->SlotAddress);
            if (!window) [[unlikely]]
            {
                Output::send<LogLevel::Error>(STR("[DispatchData] refused: write window arm failed (mapscan prot gate or mprotect rc recorded); this hook does not arm\n"));
                return false;
            }

            // §2.1 identity pre-check: the ONLY safe write is into a word whose current
            // value equals the independently resolved original entry. Drift ⇒ refuse.
            if (*record->SlotAddress != record->EntryValue) [[unlikely]]
            {
                DispatchDataTransition::restore_write_window(window);
                Output::send<LogLevel::Error>(STR("[DispatchData] refused: slot value drifted since capture (override, wrong layout, or mid-engine vtable change); write refused loudly\n"));
                return false;
            }

            // §4.4 commit order: (2) original saved (3) armed release (4) single aligned
            // 8-byte store (5) readback-verify — no instruction byte is ever touched, so
            // the PLH prologue-patch race class does not exist in this backend.
            m_slotAddress = record->SlotAddress;
            m_originalEntry = record->EntryValue;
            *m_userTrampVar = std::bit_cast<uint64_t>(m_originalEntry);
            m_armed.store(true, std::memory_order_release);
            std::atomic<void*>* slot = std::bit_cast<std::atomic<void*>*>(record->SlotAddress);
            slot->store(std::bit_cast<void*>(m_fnCallback), std::memory_order_relaxed); // aligned 8-byte store
            if (slot->load(std::memory_order_relaxed) != std::bit_cast<void*>(m_fnCallback)) [[unlikely]]
            {
                // §3 step 4 readback failure ⇒ fail closed: restore the original entry
                // (the window is still open) and refuse.
                slot->store(m_originalEntry, std::memory_order_relaxed);
                m_armed.store(false, std::memory_order_release);
                DispatchDataTransition::restore_write_window(window);
                Output::send<LogLevel::Error>(STR("[DispatchData] refused: slot readback mismatch after store; restored original, hook not armed\n"));
                return false;
            }

            // §3 step 5: re-protect R + mapscan verify. Anomaly ⇒ fail closed.
            if (!DispatchDataTransition::restore_write_window(window)) [[unlikely]]
            {
                Output::send<LogLevel::Error>(STR("[DispatchData] FAILED CLOSED: page restore anomaly after swap; ladder stopped — do not continue installing from an unrestorable page\n"));
                return false;
            }
            Output::send<LogLevel::Warning>(STR("[DispatchData] slot swap armed: entry 0x{:X} -> thunk, original saved in Trampoline\n"), m_fnAddress);
            return true;
        }

        // §4.5: teardown restore does not exist and must not be promised — process-lifetime
        // hooks, matching the PROSPERO DeactivateHook precedent (DetourInstance.hpp:191-205).
        // Restore would need the same external-quiescence authority PLH's unHook needs.
        auto unHook() -> bool
        {
            return false;
        }

        auto isHooked() const -> bool
        {
            return m_armed.load(std::memory_order_acquire);
        }

    private:
        uint64_t m_fnAddress;    // registry key: the resolved original entry (R1 design)
        uint64_t m_fnCallback;   // the replacement: the port's own StaticHookFn
        uint64_t* m_userTrampVar; // out-word: receives the saved ORIGINAL entry (§4.3)
        void** m_slotAddress{nullptr};
        void* m_originalEntry{nullptr};
        std::atomic<bool> m_armed{false};
    };

    // -------------------------------------------------------------------------------------
    // Free-function capture seam for the build-ready VT-R2 walk-macro diff
    // (UnrealVirtualBaseVC.hpp). The macro's arming guard declares ONLY this free
    // function (a class forward-declaration cannot declare members), keeping the VC
    // header's armed surface free of any class-definition/ODR coupling; this inline
    // definition is the single definition across all armed TUs.
    // -------------------------------------------------------------------------------------
    inline auto vtable_slot_record_once(void** slot_address, void* entry_value, const void* class_instance) -> void
    {
        DispatchDataRegistry::record_slot_once(slot_address, entry_value, class_instance);
    }
}
