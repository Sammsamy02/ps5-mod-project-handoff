#include <Unreal/UObjectArray.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UKismetSystemLibrary.hpp>

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
        auto& DeleteListeners = GUObjectArray->GetUObjectDeleteListeners();
        if (DeleteListeners.Contains(Listener))
        {
            throw std::runtime_error{"Cannot add a listener because it already exists in TArray"};
        }
        DeleteListeners.Append(TArray{Listener});
    }

    void UObjectArray::RemoveUObjectDeleteListener(FUObjectDeleteListener* Listener)
    {
        GUObjectArray->GetUObjectDeleteListeners().RemoveSingleSwap(Listener);
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