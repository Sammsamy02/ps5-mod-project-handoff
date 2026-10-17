#include <Unreal/UObjectArray.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UKismetSystemLibrary.hpp>
#include <Unreal/FMemory.hpp>
#include <DynamicOutput/Output.hpp>
#include <File/Macros.hpp>
#include <Helpers/String.hpp>
#include <cstdint>
#include <mutex>
#include <stdexcept>

#if defined(__PROSPERO__)
// L25: shared composition diagnostic sink (diagnostic.c). Same in-link C symbol
// PS5PadSource.cpp already declares; defined in-link by the composition, so
// this adds no new import and no new undefined symbol.
extern "C" void diagnostic_event(const char* tag, unsigned int value);
#endif

namespace RC::Unreal
{
#include <MemberVariableLayout_SrcWrapper_FUObjectItem.hpp>
#include <MemberVariableLayout_SrcWrapper_TUObjectArray.hpp>
#include <MemberVariableLayout_SrcWrapper_FUObjectArray.hpp>

    // Object flags that carried this state before it moved to EInternalObjectFlags in 4.11.
    // Values are raw ObjectBase.h bits, identical across 4.7 through 4.10. They are deliberately
    // not EObjectFlags: the UObject flag API translates canonical EObjectFlags into the running
    // version's layout, so handing it these raw bits would silently remap them.
    enum class EPre411ObjectFlags : int32
    {
        RF_Native = 0x00000004,
        RF_RootSet = 0x00000080,
        RF_Unreachable = 0x00000100,
        RF_AsyncLoading = 0x00000800,
        RF_PendingKill = 0x00004000,
        RF_Async = 0x00800000, // 4.8 and later only
    };

    struct Pre411FlagPair
    {
        EInternalObjectFlags Internal;
        EPre411ObjectFlags Legacy;
    };

    static constexpr Pre411FlagPair Pre411FlagPairs[]{
            {EInternalObjectFlags::Native, EPre411ObjectFlags::RF_Native},
            {EInternalObjectFlags::RootSet, EPre411ObjectFlags::RF_RootSet},
            {EInternalObjectFlags::Unreachable, EPre411ObjectFlags::RF_Unreachable},
            {EInternalObjectFlags::AsyncLoading, EPre411ObjectFlags::RF_AsyncLoading},
            {EInternalObjectFlags::PendingKill, EPre411ObjectFlags::RF_PendingKill},
            {EInternalObjectFlags::Async, EPre411ObjectFlags::RF_Async},
    };

    // RF_Async only exists from 4.8; every other pair covers all of 4.7 through 4.10
    static auto Pre411FlagExists(const Pre411FlagPair& Pair) -> bool
    {
        return Pair.Legacy != EPre411ObjectFlags::RF_Async || Version::IsAtLeast(4, 8);
    }

    // Internal flags to raw pre-4.11 object flags. Flags with no equivalent are dropped, so a
    // multi-flag write applies the subset that exists rather than failing.
    static auto ToPre411ObjectFlags(EInternalObjectFlags InFlags) -> int32_t
    {
        const auto Flags = static_cast<int32_t>(InFlags);
        int32_t Result{};
        for (const auto& Pair : Pre411FlagPairs)
        {
            if (!Pre411FlagExists(Pair)) { continue; }
            if (Flags & static_cast<int32_t>(Pair.Internal)) { Result |= static_cast<int32_t>(Pair.Legacy); }
        }
        return Result;
    }

    // Raw pre-4.11 object flags back to internal flags
    static auto FromPre411ObjectFlags(EObjectFlags InFlags) -> int32_t
    {
        const auto Flags = static_cast<int32_t>(InFlags);
        int32_t Result{};
        for (const auto& Pair : Pre411FlagPairs)
        {
            if (!Pre411FlagExists(Pair)) { continue; }
            if (Flags & static_cast<int32_t>(Pair.Legacy)) { Result |= static_cast<int32_t>(Pair.Internal); }
        }
        return Result;
    }

    static int32_t GetFlagsFromFlagsAndRefCount(int64_t FlagsAndRefCount)
    {
        return static_cast<int32_t>(FlagsAndRefCount >> 32);
    }

    static int32_t GetRefCountFromFlagsAndRefCount(int64_t FlagsAndRefCount)
    {
        return static_cast<int32_t>(FlagsAndRefCount & 0xFFFFFFFF);
    }

    bool FUObjectItem::IsUnreachable() const
    {
        return !!(GetFlagsInternal() & static_cast<int32_t>(EInternalObjectFlags::Unreachable)) || !GetUObject();
    }

    bool FUObjectItem::IsPendingKill() const
    {
        return !!(GetFlagsInternal() & static_cast<int32_t>(EInternalObjectFlags::PendingKill));
    }

    void FUObjectItem::SetRootSet()
    {
        SetFlags(EInternalObjectFlags::RootSet);
    }

    void FUObjectItem::UnsetRootSet()
    {
        UnsetFlags(EInternalObjectFlags::RootSet);
    }

    bool FUObjectItem::IsRootSet()
    {
        return !!(GetFlagsInternal() & static_cast<int32_t>(EInternalObjectFlags::RootSet));
    }

    void FUObjectItem::SetGCKeep()
    {
        // Pre-4.11: No FUObjectItem exists
        if (Version::IsAtMost(4, 10)) { return; }
        SetFlags(EInternalObjectFlags::GarbageCollectionKeepFlags);
    }

    void FUObjectItem::UnsetGCKeep()
    {
        // Pre-4.11: No FUObjectItem exists
        if (Version::IsAtMost(4, 10)) { return; }
        UnsetFlags(EInternalObjectFlags::GarbageCollectionKeepFlags);
    }

    bool FUObjectItem::IsGCKeepSet()
    {
        return !!(GetFlagsInternal() & static_cast<int32_t>(EInternalObjectFlags::GarbageCollectionKeepFlags));
    }

    UObject* FUObjectItem::GetUObject() const
    {
        // Pre-4.11: 'this' actually points to a slot containing a UObjectBase*
        // The array stores UObjectBase* directly, not FUObjectItem structs
        if (Version::IsAtMost(4, 10))
        {
            UObjectBase* const* Slot = reinterpret_cast<UObjectBase* const*>(this);
            return static_cast<UObject*>(*Slot);
        }

        // Missing: Flag stuff for 5.7+
#if UE_ENABLE_FUOBJECT_ITEM_PACKING
        static_assert("GetUObject for 5.7+ with UE_ENABLE_FUOBJECT_ITEM_PACKING == 1 is unimplemented");
#else
        return std::bit_cast<UObject*>(GetObject());
#endif
    }

    bool FUObjectItem::HasAnyFlags(EInternalObjectFlags InFlags) const
    {
        // Any flag with no equivalent on this version simply does not match
        return !!(GetFlagsInternal() & int32(InFlags));
    }

    int32_t FUObjectItem::GetFlagsInternal()
    {
        return const_cast<const FUObjectItem*>(this)->GetFlagsInternal();
    }

    int32_t FUObjectItem::GetFlagsInternal() const
    {
        if (Version::IsAtLeast(5, 7))
        {
            return GetFlagsFromFlagsAndRefCount(GetFlagsAndRefCount());
        }
        else if (Version::IsAtLeast(4, 13))
        {
            return GetFlags();
        }
        else if (Version::IsAtMost(4, 10))
        {
            // No FUObjectItem; the state lives on the UObject
            UObject* Obj = GetUObject();
            return Obj ? FromPre411ObjectFlags(Obj->GetObjectFlags()) : 0;
        }
        else
        {
            return GetClusterAndFlags() & static_cast<int32>(EInternalObjectFlags::AllFlags);
        }
    }

    void FUObjectItem::SetFlags(EInternalObjectFlags InFlags)
    {
        // Pre-4.11 the state lives on the UObject, written raw: the canonical flag API would
        // remap these bits into the running version's layout
        if (Version::IsAtMost(4, 10))
        {
            const auto Translated = ToPre411ObjectFlags(InFlags);
            if (Translated == 0) { return; }
            if (UObject* Obj = GetUObject())
            {
                auto& Flags = Obj->GetObjectFlags();
                Flags = static_cast<EObjectFlags>(static_cast<int32_t>(Flags) | Translated);
            }
            return;
        }
        if (Version::IsAtLeast(5, 7))
        {
            auto Flags = GetFlagsFromFlagsAndRefCount(GetFlagsAndRefCount());
            Flags |= static_cast<int32_t>(InFlags);
            GetFlagsAndRefCount() = static_cast<int64_t>(Flags) << 32 | GetRefCountFromFlagsAndRefCount(GetFlagsAndRefCount());
        }
        else if (Version::IsAtLeast(4, 13))
        {
            GetFlags() |= static_cast<int32_t>(InFlags);
        }
        else
        {
            GetClusterAndFlags() |= static_cast<int32_t>(InFlags);
        }
    }

    void FUObjectItem::UnsetFlags(EInternalObjectFlags InFlags)
    {
        if (Version::IsAtMost(4, 10))
        {
            const auto Translated = ToPre411ObjectFlags(InFlags);
            if (Translated == 0) { return; }
            if (UObject* Obj = GetUObject())
            {
                auto& Flags = Obj->GetObjectFlags();
                Flags = static_cast<EObjectFlags>(static_cast<int32_t>(Flags) & ~Translated);
            }
            return;
        }
        if (Version::IsAtLeast(5, 7))
        {
            auto Flags = GetFlagsFromFlagsAndRefCount(GetFlagsAndRefCount());
            Flags &= ~static_cast<int32_t>(InFlags);
            GetFlagsAndRefCount() = static_cast<int64_t>(Flags) << 32 | GetRefCountFromFlagsAndRefCount(GetFlagsAndRefCount());
        }
        else if (Version::IsAtLeast(4, 13))
        {
            GetFlags() &= ~static_cast<int32_t>(InFlags);
        }
        else
        {
            GetClusterAndFlags() &= ~static_cast<int32_t>(InFlags);
        }
    }

    int32_t FUObjectItem::GetRefCountInternal()
    {
        return const_cast<const FUObjectItem*>(this)->GetRefCountInternal();
    }

    int32_t FUObjectItem::GetRefCountInternal() const
    {
        // Pre-4.11: No FUObjectItem exists, no ref count
        if (Version::IsAtMost(4, 10)) { return 0; }

        if (Version::IsAtLeast(5, 7))
        {
            return GetRefCountFromFlagsAndRefCount(GetFlagsAndRefCount());
        }
        else
        {
            return GetRefCount();
        }
    }

    bool FUObjectItem::IsValid(bool bEvenIfPendingKill) const
    {
        return bEvenIfPendingKill ? !IsUnreachable() : !(IsUnreachable() || IsPendingKill());
    }

    FUObjectItem**& TUObjectArray::GetObjects()
    {
        return GetObjects420();
    }
    const FUObjectItem**& TUObjectArray::GetObjects() const
    {
        return GetObjects420();
    }

    FUObjectItem* TUObjectArray::GetObjectPtr(int32_t Index) const
    {
        static const auto ItemSize = FUObjectItem::UEP_TotalSize();
        if (Version::IsAtMost(4, 7))
        {
            // Plain TArray<UObjectBase*>: contiguous slots; FUObjectItem methods treat
            // 'this' as a slot holding a UObjectBase* in pre-4.11 engines
            return std::bit_cast<FUObjectItem*>(std::bit_cast<UObjectBase**>(GetObjects()) + Index);
        }
        if (Version::IsAtMost(4, 10))
        {
            const int32 ChunkIndex = Index / NumElementsPerChunk_Indirect;
            if (ChunkIndex >= GetNumChunks()) { return nullptr; }
            const int32 WithinChunkIndex = Index % NumElementsPerChunk_Indirect;
            const auto ChunkPtr = GetChunks()[ChunkIndex];
            // Technically, this is a UObjectBase**, not sure how this is going to work.
            return std::bit_cast<FUObjectItem*>(ChunkPtr + WithinChunkIndex);
        }
        else if (Version::IsAtMost(4, 19))
        {
            return std::bit_cast<FUObjectItem*>(&std::bit_cast<uint8_t*>(GetObjects())[Index * ItemSize]);
        }
        else
        {
            const int32_t ChunkIndex = Index / NumElementsPerChunk;
            const int32_t WithinChunkIndex = Index % NumElementsPerChunk;
            const auto Chunk = GetObjects()[ChunkIndex];
            return std::bit_cast<FUObjectItem*>(&std::bit_cast<uint8_t*>(Chunk)[WithinChunkIndex * ItemSize]);
        }
    }

    FUObjectItem* TUObjectArray::GetObjectPtr(int32_t Index)
    {
        static const auto ItemSize = FUObjectItem::UEP_TotalSize();
        if (Version::IsAtMost(4, 7))
        {
            // Plain TArray<UObjectBase*>: contiguous slots; FUObjectItem methods treat
            // 'this' as a slot holding a UObjectBase* in pre-4.11 engines
            return std::bit_cast<FUObjectItem*>(std::bit_cast<UObjectBase**>(GetObjects()) + Index);
        }
        if (Version::IsAtMost(4, 10))
        {
            const int32 ChunkIndex = Index / NumElementsPerChunk_Indirect;
            if (ChunkIndex >= GetNumChunks()) { return nullptr; }
            const int32 WithinChunkIndex = Index % NumElementsPerChunk_Indirect;
            const auto ChunkPtr = GetChunks()[ChunkIndex];
            // Technically, this is a UObjectBase**, not sure how this is going to work.
            return std::bit_cast<FUObjectItem*>(ChunkPtr + WithinChunkIndex);
        }
        else if (Version::IsAtMost(4, 19))
        {
            return std::bit_cast<FUObjectItem*>(&std::bit_cast<uint8_t*>(GetObjects())[Index * ItemSize]);
        }
        else
        {
            const int32_t ChunkIndex = Index / NumElementsPerChunk;
            const int32_t WithinChunkIndex = Index % NumElementsPerChunk;
            const auto Chunk = GetObjects()[ChunkIndex];
            return std::bit_cast<FUObjectItem*>(&std::bit_cast<uint8_t*>(Chunk)[WithinChunkIndex * ItemSize]);
        }
    }

    const FUObjectItem& TUObjectArray::operator[](int32_t Index) const
    {
        return *GetObjectPtr(Index);
    }

    FUObjectItem& TUObjectArray::operator[](int32_t Index)
    {
        return *GetObjectPtr(Index);
    }

    void UObjectArray::SetupGUObjectArrayAddress(void* address)
    {
        GUObjectArray = static_cast<FUObjectArray*>(address);
    }

    void* UObjectArray::GetGUObjectArrayAddress()
    {
        return GUObjectArray;
    }

    bool UObjectArray::IsValid(FUObjectItem* ObjectItem, bool bEvenIfPendingKill)
    {
        return ObjectItem->IsValid(bEvenIfPendingKill);
    }

    bool UObjectArray::IsStale(FUObjectItem* ObjectItem, bool bEvenIfPendingKill)
    {
        return bEvenIfPendingKill ? (ObjectItem->IsPendingKill() || ObjectItem->IsUnreachable()) : (ObjectItem->IsUnreachable());
    }

    // =====================================================================================
    // L25 (Claude l25 items 2-4): UObjectDeleteListeners registration hardening.
    //
    // ENGINE-SIDE GROUND TRUTH (vendored official UE 5.6.1 source,
    // outputs/2026-10-08-l23-layout-generic/official-source/.../UObject/UObjectArray.h):
    //   :1432  TArray<FUObjectDeleteListener*> UObjectDeleteListeners;   (engine-owned member)
    //   :1434  mutable FTransactionallySafeCriticalSection UObjectDeleteListenersCritical;
    //   :1133  COREUOBJECT_API void AddUObjectDeleteListener(FUObjectDeleteListener*);  (impl NOT vendored)
    // THREADSAFE_UOBJECTS is hard-defined 1 (CoreUObject/Public/UObject/ObjectMacros.h:33),
    // so the critical section EXISTS in shipping PS5 builds; every header-inline engine use
    // of the array takes it via UE::TScopeLock (UObjectArray.h:1460, :1469), and the engine's
    // own Add/Remove (non-vendored Private/UObjectArray.cpp) does the same per that pattern.
    //
    // DOES OUR REGISTRATION TAKE UObjectDeleteListenersCritical? NO. It cannot:
    //   1. The engine's own locked AddUObjectDeleteListener is COREUOBJECT_API inside the
    //      statically linked game image; the PS5 game executable exports no runtime symbol
    //      table we can resolve by name, this port's signature database has no pattern for
    //      it, and the seed table (SEED-TABLE.json slots 0-4) has no seed for it. Calling
    //      it is therefore not reachable from this build.
    //   2. Taking the critical section ourselves is not possible either:
    //      UObjectDeleteListenersCritical has no published offset in this port's generated
    //      member layout (MemberVariableLayout_SrcWrapper_FUObjectArray.hpp only exposes
    //      ObjFirstGCIndex..ObjAvailableListEstimateCount, NOT the critical sections), and
    //      FTransactionallySafeCriticalSection's internal layout is build-feature
    //      dependent — blind-locking a guessed offset would corrupt adjacent GUObjectArray
    //      state. LockGUObjectArray()/UnlockGUObjectArray() below are still TODO stubs for
    //      the same reason (and guard ObjObjectsCritical, the wrong lock, anyway).
    //   3. Registering on the game thread once the R3 marshal pump is armed would narrow
    //      the race but the pump's game-thread action queue lives in LuaMod.cpp
    //      (m_game_thread_actions drained by funcptr_marshal_hook), which this unit cannot
    //      reach without cross-TU surgery outside this change set.
    // So the write stays unlocked; we bound it instead with everything below (Claude l25
    // item 4 "make the unlocked write as safe as possible"):
    //   - our own mutex serializing THIS file's add/remove writers against each other
    //     (init-thread Add vs OnUObjectArrayShutdown self-Remove vs LuaMod's
    //     FLuaObjectDeleteListener Add from the event-loop thread);
    //   - a sane-header gate that refuses cleanly (skip + loud log + diagnostic event,
    //     init continues) instead of walking a corrupt array;
    //   - GMalloc-bound check before any growth (RC FMemory.cpp:32-44 routes Realloc
    //     through the seed-bound ENGINE GMalloc instance — allocator mismatch is NOT a
    //     hazard in this build, but a null GMalloc deref on growth would be);
    //   - explicit no-realloc/realloc/fresh-alloc path logging, because realloc swaps
    //     the engine-owned Data buffer while the game thread may be iterating it under
    //     the engine lock we do not hold — the residual, documented race.
    // =====================================================================================
    namespace
    {
        std::mutex GUObjectDeleteListenerWriteMutex{};

        // L25 sanity bound for the listener TArray header. The engine core registers a
        // handful of delete listeners (single digit; e.g. annotation/GC bookkeeping
        // subsystems) and TArray growth slack stays small (4..64), so 256 separates a
        // plausible Max from garbage with >16x headroom; a Max read through a mis-bound
        // GUObjectArray offset lands in the millions or is negative. Anything above 256
        // is treated as an insane header and registration is refused.
        constexpr int32_t MaxPlausibleDeleteListeners = 256;

        // L25 diagnostic reason codes for the 'l25_dl_refused' event.
        enum class L25RefusalCode : uint32_t
        {
            NullGUObjectArray = 1,
            LayoutOrAppendException = 2,
            InsaneHeader = 3,
            DuplicateListener = 4,
            GMallocUnboundForGrowth = 5,
            // Claude review item 4 (2026-10-08 relay): growth is required but the
            // engine-owned Data buffer failed the heap-plausibility screen, so
            // Append/Realloc is not called. Codes 1-5 keep their meanings.
            ImplausibleDataForGrowth = 6,
        };

        auto l25_listener_header_sane(const void* data, int32_t num, int32_t max) -> bool
        {
            if (num < 0 || max < 0) { return false; }
            if (num > max) { return false; }
            if (max > MaxPlausibleDeleteListeners) { return false; }
            if (data == nullptr) { return num == 0 && max == 0; }
            const auto data_address = reinterpret_cast<uintptr_t>(data);
            if (data_address % alignof(void*) != 0) { return false; }
            return data_address <= static_cast<uintptr_t>(-1) - static_cast<uintptr_t>(max) * sizeof(void*);
        }

        // =================================================================================
        // Claude review item 4 (2026-10-08 relay) — pre-Append allocator logging +
        // growth refusal scaffolding (agent AD).
        //
        // ALIGNED with agent-AB §7 (outputs/2026-10-08-l25-prep/
        // agent-AB-gmalloc-audit.md, now on disk; binary-verified against the
        // game eboot sha 3b6cfad4… and corroborated by agent-AH's independent
        // vtable reconstruction): the GMalloc snapshot additionally reads the
        // RAW vtable slot contents at +0x28/+0x30/+0x38/+0x40/+0xd0 — exactly
        // the words the FMemory::Realloc dispatch chain consumes — using the
        // tree's existing safe-reader idiom (byte-wise reads through const
        // volatile unsigned char*, exactly the seed-slot read_u64 in
        // UnrealInitializer.cpp:793-801) behind the same arithmetic
        // plausibility screen. Read-only diagnostics; these slots are NEVER
        // called through (AB §3/§7: calling GetDescriptiveName via the wrapper
        // would throw on a map without the key). Interpretation table (module
        // base 0x400000): vptr 0x82c2e28 = bootstrap allocator
        // ("Unspecified allocator"), 0x82c26e0 = FMallocBinned3 ("Binned3");
        // +0x38 reading 0x4b8a20 (TryMalloc default wrapper) is the signature
        // of dispatch through the pre-fix one-slot-low 5_06 default map.
        //
        // x86-64 4-level paging canonical user-address bound (PS5 target included):
        // anything at/above 2^47 is a kernel address, not engine heap.
        constexpr uintptr_t L25CanonicalUserMax = (uintptr_t{1} << 47) - 1;

        // Word-read plausibility screen: non-null, pointer-aligned, inside the
        // canonical user range (which also implies the 8-byte read cannot wrap).
        auto l25_pointer_word_plausible(uintptr_t address) -> bool
        {
            return address != 0
                && address % alignof(void*) == 0
                && address <= L25CanonicalUserMax;
        }

        // Tree safe-reader idiom: byte-wise load of one machine word through a
        // const volatile pointer so the compiler cannot widen, cache, or elide the
        // access. NOT a fault guard — callers screen the address first (the same
        // doctrine l25_listener_header_sane states: this TU cannot prove
        // page readability; the screen is arithmetic and the markers discriminate
        // the rest). GMalloc's slot was V1/V2-verified at seed bind time
        // (UnrealInitializer.cpp:1097-1109), so these reads revisit proven pages.
        auto l25_read_word(const void* address) -> uintptr_t
        {
            const auto* base = static_cast<const volatile unsigned char*>(address);
            uintptr_t value{};
            for (size_t i = 0; i < sizeof(uintptr_t); ++i)
            {
                value |= static_cast<uintptr_t>(base[i]) << (8 * i);
            }
            return value;
        }

        // One guarded allocator snapshot: the GMalloc VARIABLE address (this
        // TU's definition — reloc/ODR sanity), its call-time value (the engine
        // FMalloc* slot the seed bound), the instance read through *variable,
        // and the instance vptr read through *(void**)instance (the exact word
        // the Append/Realloc virtual dispatch would consume — Claude's fault
        // hypothesis (a)). Unbound/null stages read as 0 and are skipped, so the
        // snapshot itself never dereferences an unscreened pointer. Per agent-AB
        // §7 the snapshot also carries the raw vtable SLOT contents at
        // +0x28/+0x30/+0x38/+0x40/+0xd0: map's old "Malloc"/true-Malloc/
        // TryMalloc-default(true Realloc under the pre-fix map)/true-Realloc/
        // GetDescriptiveName. Slot reads are guarded by the same plausibility
        // screen applied to the vptr and are never called through.
        struct L25GMallocSnapshot
        {
            const void* variable_address{};
            uintptr_t variable_value{};
            uintptr_t instance{};
            uintptr_t vptr{};
            uintptr_t slot_28{};
            uintptr_t slot_30{};
            uintptr_t slot_38{};
            uintptr_t slot_40{};
            uintptr_t slot_d0{};
        };

        auto l25_snapshot_gmalloc() -> L25GMallocSnapshot
        {
            L25GMallocSnapshot snap;
            snap.variable_address = static_cast<const void*>(&GMalloc);
            snap.variable_value = reinterpret_cast<uintptr_t>(GMalloc);
            if (l25_pointer_word_plausible(snap.variable_value))
            {
                snap.instance = l25_read_word(reinterpret_cast<const void*>(snap.variable_value)); // *variable
                if (l25_pointer_word_plausible(snap.instance))
                {
                    snap.vptr = l25_read_word(reinterpret_cast<const void*>(snap.instance)); // *(void**)instance
                }
            }
            if (l25_pointer_word_plausible(snap.vptr))
            {
                // agent-AB §7 slot reads: raw contents of the vtable words the
                // FMalloc virtual dispatch would index, no calls through them.
                const auto read_slot = [&](uintptr_t offset) -> uintptr_t
                {
                    const uintptr_t slot_address = snap.vptr + offset;
                    return l25_pointer_word_plausible(slot_address)
                        ? l25_read_word(reinterpret_cast<const void*>(slot_address))
                        : 0;
                };
                snap.slot_28 = read_slot(0x28);
                snap.slot_30 = read_slot(0x30);
                snap.slot_38 = read_slot(0x38);
                snap.slot_40 = read_slot(0x40);
                snap.slot_d0 = read_slot(0xd0);
            }
            return snap;
        }

        // Claude review item 4 refusal screen: when Append must GROW the array,
        // the engine-owned Data buffer must additionally be heap-plausible —
        // non-null (growth with a live allocation), pointer-aligned, inside the
        // canonical user range, and not wrapping over the live Num elements.
        // Applied only on the live-buffer (realloc) growth path: after the
        // unconditional sane-header gate, Data==nullptr is reachable solely as
        // the canonical empty header Num==Max==0, whose Append is a fresh
        // allocation with no engine-owned storage handed to Realloc — that path
        // keeps O's fresh-alloc marker and proceeds. Readability itself stays
        // unprovable from this TU (same admission as l25_listener_header_sane);
        // refusing here means the arithmetic screen failed, i.e. the header was
        // NOT verified sane enough to hand to Realloc.
        auto l25_data_heap_plausible(const void* data, int32_t num) -> bool
        {
            if (data == nullptr) { return false; }
            const auto address = reinterpret_cast<uintptr_t>(data);
            return l25_pointer_word_plausible(address)
                && address <= static_cast<uintptr_t>(-1) - static_cast<uintptr_t>(num) * sizeof(void*);
        }

        auto l25_diag(const char* tag, uint32_t value) -> void
        {
#if defined(__PROSPERO__)
            diagnostic_event(tag, value);
#else
            (void)tag; (void)value;
#endif
        }
    }

    void UObjectArray::AddUObjectCreateListener(FUObjectCreateListener* Listener)
    {
        auto& CreateListeners = GUObjectArray->GetUObjectCreateListeners();
        if (CreateListeners.Contains(Listener))
        {
            throw std::runtime_error{"Cannot add a listener because it already exists in TArray"};
        }
        CreateListeners.Append(TArray{Listener});
    }

    void UObjectArray::RemoveUObjectCreateListener(FUObjectCreateListener* Listener)
    {
        GUObjectArray->GetUObjectCreateListeners().RemoveSingleSwap(Listener);
    }

    void UObjectArray::AddUObjectDeleteListener(FUObjectDeleteListener* Listener)
    {
        // L25 (items 2-4): hardened + instrumented. Original body was
        // GetUObjectDeleteListeners() / Contains->throw / Append with no lock, no
        // validation, no logging; init stalled between the "Adding shutdown listener"
        // log line and the call's return in every observed PS5 session.
        std::lock_guard Lock(GUObjectDeleteListenerWriteMutex);
        l25_diag("l25_dl_add_begin", 0);
        Output::send(STR("[L25][DeleteListener] Add enter (GUObjectArray={} listener={})\n"),
                     GUObjectArray ? reinterpret_cast<void*>(GUObjectArray) : nullptr, reinterpret_cast<void*>(Listener));
        if (GUObjectArray == nullptr)
        {
            Output::send(STR("[L25][DeleteListener] REFUSED ({}) : GUObjectArray is null; skipping registration and continuing init\n"),
                         static_cast<uint32_t>(L25RefusalCode::NullGUObjectArray));
            l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::NullGUObjectArray));
            return;
        }
        try
        {
            // NOTE: GetUObjectDeleteListeners() itself throws std::runtime_error when the
            // runtime member-offset table lacks 'UObjectDeleteListeners' for this engine
            // version (MemberVariableLayout_SrcWrapper_FUObjectArray.hpp:137-145) — that
            // throw is caught here and logged instead of aborting init.
            auto& DeleteListeners = GUObjectArray->GetUObjectDeleteListeners();
            const void* pre_data = DeleteListeners.GetData();
            const int32_t pre_num = DeleteListeners.Num();
            const int32_t pre_max = DeleteListeners.Max();
            // Claude review item 4 (2026-10-08 relay): growth-required flag and the
            // guarded GMalloc variable/instance/vptr snapshot join the pre-Append dump,
            // so the launch harvest sees the exact allocator state the Append/Realloc
            // virtual dispatch would consume BEFORE the call is made.
            const bool growth_required = (pre_num >= pre_max) || pre_data == nullptr;
            const L25GMallocSnapshot pre_gmalloc = l25_snapshot_gmalloc();
            Output::send(STR("[L25][DeleteListener] header pre: Data={} Num={} Max={} growth_required={} GMallocVar={} GMalloc={} GMallocInst={} vptr={} slots +0x28={} +0x30={} +0x38={} +0x40={} +0xd0={}\n"),
                         const_cast<void*>(pre_data), pre_num, pre_max, growth_required ? 1 : 0,
                         const_cast<void*>(pre_gmalloc.variable_address),
                         reinterpret_cast<void*>(pre_gmalloc.variable_value),
                         reinterpret_cast<void*>(pre_gmalloc.instance),
                         reinterpret_cast<void*>(pre_gmalloc.vptr),
                         reinterpret_cast<void*>(pre_gmalloc.slot_28),
                         reinterpret_cast<void*>(pre_gmalloc.slot_30),
                         reinterpret_cast<void*>(pre_gmalloc.slot_38),
                         reinterpret_cast<void*>(pre_gmalloc.slot_40),
                         reinterpret_cast<void*>(pre_gmalloc.slot_d0));
            l25_diag("l25_dl_pre", static_cast<uint32_t>(pre_num));
            l25_diag("l25_dl_gmalloc_pre", static_cast<uint32_t>(pre_gmalloc.instance & 0xFFFFFFFFu));
            if (!l25_listener_header_sane(pre_data, pre_num, pre_max))
            {
                Output::send(STR("[L25][DeleteListener] REFUSED ({}) : insane UObjectDeleteListeners header (Data={} Num={} Max={}); skipping registration and continuing init\n"),
                             static_cast<uint32_t>(L25RefusalCode::InsaneHeader), const_cast<void*>(pre_data), pre_num, pre_max);
                l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::InsaneHeader));
                return;
            }
            // L25 pre-read guard (agent AT, 2026-10-08; AK item-4 finding 2): Contains
            // dereferences Data, so the growth refusals 5 and 6 run BEFORE Contains —
            // refusal 6 is a true pre-read screen (5 stays ahead of 6, order preserved).
            if (growth_required && pre_max > 0)
            {
                if (GMalloc == nullptr || *GMalloc == nullptr)
                {
                    Output::send(STR("[L25][DeleteListener] REFUSED (5) : growth required (Num==Max={}) but GMalloc is unbound; skipping registration and continuing init\n"),
                                 static_cast<uint32_t>(L25RefusalCode::GMallocUnboundForGrowth), pre_max);
                    l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::GMallocUnboundForGrowth));
                    return;
                }
                // Claude review item 4 refusal rule: "If growth is required and the
                // header isn't verified sane, refuse cleanly instead of calling
                // Realloc." On this path the sane gate has passed AND a live
                // engine-owned buffer exists (Data!=nullptr is guaranteed here: null
                // Data only survives the gate as Num==Max==0, excluded by pre_max>0),
                // so the strengthened check verifies that buffer is heap-plausible
                // before Append can hand it to Realloc. Failures refuse with the new
                // code 6; Append/Realloc is NOT called. The screen sits above Contains
                // so the refusal fires without reading the buffer it is screening.
                if (!l25_data_heap_plausible(pre_data, pre_num))
                {
                    Output::send(STR("[L25][DeleteListener] REFUSED (6) : growth required (Num==Max={}) but Data={} is not heap-plausible (non-null/aligned/canonical arithmetic screen); refusing instead of calling Realloc; skipping registration and continuing init\n"),
                                 static_cast<uint32_t>(L25RefusalCode::ImplausibleDataForGrowth), pre_max, const_cast<void*>(pre_data));
                    l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::ImplausibleDataForGrowth));
                    return;
                }
            }
            if (DeleteListeners.Contains(Listener))
            {
                // L25 behavior change: previously std::runtime_error (aborting init);
                // item 2 requires a clean refusal so init continues.
                Output::send(STR("[L25][DeleteListener] REFUSED (4) : listener already present (Num={} Max={}); skipping duplicate registration and continuing init\n"),
                             static_cast<uint32_t>(L25RefusalCode::DuplicateListener), pre_num, pre_max);
                l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::DuplicateListener));
                return;
            }
            const bool growth_needed = growth_required;
            if (growth_needed && pre_max > 0)
            {
                // Residual documented race: Append reallocates the engine-owned buffer
                // (through the engine GMalloc, FMemory.cpp Realloc wrapper) and frees the
                // old Data while the game thread may be mid-iteration under the engine's
                // UObjectDeleteListenersCritical — which we cannot take (see block comment).
                Output::send(STR("[L25][DeleteListener] RISK: capacity full (Num==Max={}); Append will reallocate the engine-owned buffer while the game thread may iterate UObjectDeleteListeners (engine critical section NOT held); proceeding\n"),
                             pre_max);
                l25_diag("l25_dl_risk", static_cast<uint32_t>(pre_max));
            }
            else if (growth_needed)
            {
                Output::send(STR("[L25][DeleteListener] fresh allocation (Num==Max==0); no live old buffer to free\n"));
            }
            DeleteListeners.Append(TArray{Listener});
            const void* post_data = DeleteListeners.GetData();
            const int32_t post_num = DeleteListeners.Num();
            const int32_t post_max = DeleteListeners.Max();
            const char* path = (post_data == pre_data && post_max == pre_max) ? "no-realloc" : (pre_max == 0 ? "fresh-alloc" : "realloc");
            Output::send(STR("[L25][DeleteListener] header post: Data={} Num={} Max={} (path={})\n"),
                         const_cast<void*>(post_data), post_num, post_max, ensure_str(path));
            l25_diag("l25_dl_post", static_cast<uint32_t>(post_num));
            // Claude review item 4: the same GMalloc variable/instance/vptr trio
            // re-read in the success path — a changed variable value, changed
            // instance, or changed/cleared vptr across the Append is direct
            // evidence for/against the GMalloc-virtual-call fault hypothesis.
            // Slot fields per agent-AB §7: the raw vtable words bracket the
            // Append so a mid-init allocator swap is visible per-field.
            const L25GMallocSnapshot post_gmalloc = l25_snapshot_gmalloc();
            Output::send(STR("[L25][DeleteListener] GMalloc post: GMallocVar={} GMalloc={} GMallocInst={} vptr={} slots +0x28={} +0x30={} +0x38={} +0x40={} +0xd0={}\n"),
                         const_cast<void*>(post_gmalloc.variable_address),
                         reinterpret_cast<void*>(post_gmalloc.variable_value),
                         reinterpret_cast<void*>(post_gmalloc.instance),
                         reinterpret_cast<void*>(post_gmalloc.vptr),
                         reinterpret_cast<void*>(post_gmalloc.slot_28),
                         reinterpret_cast<void*>(post_gmalloc.slot_30),
                         reinterpret_cast<void*>(post_gmalloc.slot_38),
                         reinterpret_cast<void*>(post_gmalloc.slot_40),
                         reinterpret_cast<void*>(post_gmalloc.slot_d0));
            l25_diag("l25_dl_gmalloc_post", static_cast<uint32_t>(post_gmalloc.instance & 0xFFFFFFFFu));
            if (post_num != pre_num + 1)
            {
                Output::send(STR("[L25][DeleteListener] ANOMALY: Num advanced by {} (expected 1); concurrent engine-side writer suspected (pre Num={} post Num={})\n"),
                             post_num - pre_num, pre_num, post_num);
            }
            l25_diag("l25_dl_add_done", static_cast<uint32_t>(post_num));
        }
        catch (const std::exception& e)
        {
            Output::send(STR("[L25][DeleteListener] REFUSED ({}) : exception '{}' ; skipping registration and continuing init\n"),
                         static_cast<uint32_t>(L25RefusalCode::LayoutOrAppendException), ensure_str(e.what()));
            l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::LayoutOrAppendException));
        }
        catch (...)
        {
            Output::send(STR("[L25][DeleteListener] REFUSED ({}) : unknown exception; skipping registration and continuing init\n"),
                         static_cast<uint32_t>(L25RefusalCode::LayoutOrAppendException));
            l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::LayoutOrAppendException));
        }
    }

    void UObjectArray::RemoveUObjectDeleteListener(FUObjectDeleteListener* Listener)
    {
        // L25 (items 2-4): same hardening as Add. The engine calls this from our
        // OnUObjectArrayShutdown callbacks (ClassListener.cpp) and from retire paths on
        // the game thread; keep the refusal clean so shutdown never aborts.
        std::lock_guard Lock(GUObjectDeleteListenerWriteMutex);
        l25_diag("l25_dl_remove_begin", 0);
        Output::send(STR("[L25][DeleteListener] Remove enter (listener={})\n"), reinterpret_cast<void*>(Listener));
        if (GUObjectArray == nullptr)
        {
            Output::send(STR("[L25][DeleteListener] Remove REFUSED: GUObjectArray is null\n"));
            l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::NullGUObjectArray));
            return;
        }
        try
        {
            auto& DeleteListeners = GUObjectArray->GetUObjectDeleteListeners();
            const void* pre_data = DeleteListeners.GetData();
            const int32_t pre_num = DeleteListeners.Num();
            const int32_t pre_max = DeleteListeners.Max();
            Output::send(STR("[L25][DeleteListener] Remove header pre: Data={} Num={} Max={}\n"),
                         const_cast<void*>(pre_data), pre_num, pre_max);
            if (!l25_listener_header_sane(pre_data, pre_num, pre_max))
            {
                Output::send(STR("[L25][DeleteListener] Remove REFUSED ({}) : insane header (Data={} Num={} Max={})\n"),
                             static_cast<uint32_t>(L25RefusalCode::InsaneHeader), const_cast<void*>(pre_data), pre_num, pre_max);
                l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::InsaneHeader));
                return;
            }
            const bool was_present = DeleteListeners.Contains(Listener);
            DeleteListeners.RemoveSingleSwap(Listener);
            Output::send(STR("[L25][DeleteListener] Remove header post: Data={} Num={} Max={} (was_present={})\n"),
                         static_cast<void*>(DeleteListeners.GetData()), DeleteListeners.Num(), DeleteListeners.Max(), was_present ? 1 : 0);
            l25_diag("l25_dl_remove_done", static_cast<uint32_t>(DeleteListeners.Num()));
        }
        catch (const std::exception& e)
        {
            Output::send(STR("[L25][DeleteListener] Remove REFUSED ({}) : exception '{}'\n"),
                         static_cast<uint32_t>(L25RefusalCode::LayoutOrAppendException), ensure_str(e.what()));
            l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::LayoutOrAppendException));
        }
        catch (...)
        {
            Output::send(STR("[L25][DeleteListener] Remove REFUSED ({}) : unknown exception\n"),
                         static_cast<uint32_t>(L25RefusalCode::LayoutOrAppendException));
            l25_diag("l25_dl_refused", static_cast<uint32_t>(L25RefusalCode::LayoutOrAppendException));
        }
    }

    int32_t UObjectArray::GetNumElements()
    {
        return GUObjectArray->GetObjObjects().GetNumElements();
    }

    int32_t UObjectArray::GetNumChunks()
    {
        return GUObjectArray->GetObjObjects().GetNumChunks();
    }

    int32_t UObjectArray::GetObjectItemSize()
    {
        return FUObjectItem::UEP_TotalSize();
    }

    int32_t UObjectArray::GetObjectArraySize()
    {
        return UEP_TotalSize();
    }

    FUObjectItem* FUObjectArray::IndexToObject(int32_t Index)
    {
        if (Index >= 0 && Index < GUObjectArray->GetObjObjects().GetNumElements())
        {
            return &GUObjectArray->GetObjObjects()[Index];
        }
        else
        {
            return nullptr;
        }
    }

    int32 FUObjectArray::AllocateSerialNumber(int32 Index)
    {
        FUObjectItem* ObjectItem = IndexToObject(Index);
        checkSlow(ObjectItem);

        volatile int32 *SerialNumberPtr = &ObjectItem->GetSerialNumber();
        if (!*SerialNumberPtr)
        {
            // RE-UE4SS FIX (Corporalwill): [can't use MasterSerialNumber, so we use a library function that allocates as a side effect]
            // [TSoftObjectPtr<UObject> --> FWeakPtr --> AllocateSerialNumber]
            UKismetSystemLibrary::Conv_ObjectToSoftObjectReference(ObjectItem->GetUObject());
            // RE-UE4SS FIX END
        }
        return *SerialNumberPtr;
    }

    void UObjectArray::LockGUObjectArray()
    {
        // TODO: Implement in the mutex type in UVTD.
        //Windows::EnterCriticalSection(&GetObjObjectsCritical());
    }
    void UObjectArray::UnlockGUObjectArray()
    {
        // TODO: Implement in the mutex type in UVTD.
        //Windows::LeaveCriticalSection(&GetObjObjectsCritical());
    }

    FUObjectArray* GUObjectArray{};
}