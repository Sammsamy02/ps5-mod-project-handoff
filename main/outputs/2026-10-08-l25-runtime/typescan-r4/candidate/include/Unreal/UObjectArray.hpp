#pragma once

// User API for UObjectArray related abstractions
// See Base.hpp / DerivedX.hpp files for implementations

#include <unordered_map>

#include <Unreal/Common.hpp>
#include <Unreal/Core/HAL/Platform.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <Unreal/Core/Containers/StaticIndirectArray.hpp>
#include <Unreal/Core/Containers/LockFreeList.hpp>
#include <Unreal/BitfieldProxy.hpp>
#include <Helpers/Casting.hpp>

namespace RC::Unreal
{
    // This is implemented as a regular polymorphic object instead of using the vtable generator.
    // That's because the struct is very small and so is the vtable, and it hasn't changed from 4.12.
    // It's just easier to not use the vtable generator for this.
    // L25 CORRECTION (Claude l25 item 3): the claim above is false for the DELETE listener
    // from UE 5.x on: the engine added a fourth virtual. See FUObjectDeleteListener below.
    class RC_UE_API FUObjectCreateListener
    {
    public:
        virtual ~FUObjectCreateListener() = default;
        virtual void NotifyUObjectCreated(const class UObjectBase* Object, int32 Index) = 0;
        virtual void OnUObjectArrayShutdown() = 0;
    };

    // L25 (Claude l25 item 3) — ABI REPAIR, UE 5.6.1 (game engine confirmed by
    // outputs/2026-10-08-l23-layout-generic/official-source Build.version 5.6.1 +
    // agent-V-listener-abi.md):
    //
    // Engine ground truth, vendored official source
    // Engine/Source/Runtime/CoreUObject/Public/UObject/UObjectArray.h:
    //   :932  virtual ~FUObjectDeleteListener() {}                                   // Itanium slots 0(D1)/1(D0)
    //   :940  virtual void NotifyUObjectDeleted(const UObjectBase*, int32) = 0;       // slot 2
    //   :945  virtual void OnUObjectArrayShutdown() = 0;                              // slot 3
    //   :950  virtual SIZE_T GetAllocatedSize() const { return 0; }                   // slot 4  <-- WAS MISSING HERE
    //
    // The engine dispatches slot 4 virtually at UObjectArray.h:1471
    // (FUObjectArray::GetDeleteListenersAllocatedSize, called from engine
    // UObjectHash.cpp:2416 LogHashMemoryOverheadStatistics). Without this
    // declaration our listener vtables end at slot 3, so the engine's call
    // reads one word PAST our vtable and calls garbage — an uncontrolled
    // branch. Slots 0-3 were verified exact (order, signatures, constness;
    // int32==int32_t, SIZE_T==size_t on the PS5 target).
    //
    // GetAllocatedSize MUST stay declared LAST (after OnUObjectArrayShutdown)
    // so it occupies slot 4; never move it above the other virtuals, and do
    // not declare additional virtuals in derived listeners before the base
    // slots (derived-only virtuals append after slot 4 and are never called
    // by the engine, which is harmless).
    class RC_UE_API FUObjectDeleteListener
    {
    public:
        virtual ~FUObjectDeleteListener() = default;
        virtual void NotifyUObjectDeleted(const class UObjectBase* Object, int32 Index) = 0;
        virtual void OnUObjectArrayShutdown() = 0;
        // UE 5.6.1 FUObjectDeleteListener slot 4 (SIZE_T == size_t on PS5 x86-64).
        virtual size_t GetAllocatedSize() const { return 0; }
    };

    /**
     * Wrapper over an entry in the global object array.
     *
     * Before 4.11 the engine has no such struct: the array stores UObjectBase* directly, so an
     * FUObjectItem* from TUObjectArray points at that slot and none of the member variables
     * exist. The flag accessors below translate to the equivalent EObjectFlags on the UObject,
     * so they work on every version. Two things are not translated:
     *
     *   GetSerialNumber and the generated member getters have no pre-4.11 equivalent and read
     *   offsets that are not in the slot.
     *   SetGCKeep and UnsetGCKeep are no-ops pre-4.11, since RF_Native carries no per-object
     *   meaning there. IsGCKeepSet still reads correctly.
     */
    struct RC_UE_API FUObjectItem
    {
    public:
#include <MemberVariableLayout_HeaderWrapper_FUObjectItem.hpp>

        void SetRootSet();
        void UnsetRootSet();
        bool IsRootSet();

        void SetGCKeep();
        void UnsetGCKeep();
        bool IsGCKeepSet();

        [[nodiscard]] bool IsUnreachable() const;
        [[nodiscard]] bool IsPendingKill() const;
        [[nodiscard]] bool IsValid(bool bEvenIfPendingKill) const;
        [[nodiscard]] class UObject* GetUObject() const;
        [[nodiscard]] bool HasAnyFlags(EInternalObjectFlags InFlags) const;
        void SetFlags(EInternalObjectFlags InFlags);
        void UnsetFlags(EInternalObjectFlags InFlags);

    private:
        int32 GetFlagsInternal();
        int32 GetFlagsInternal() const;
        int32 GetRefCountInternal();
        int32 GetRefCountInternal() const;
    };

    class RC_UE_API TUObjectArray
    {
    public:
#include <MemberVariableLayout_HeaderWrapper_TUObjectArray.hpp>

        FUObjectItem**& GetObjects();
        const FUObjectItem**& GetObjects() const;

    public:
        // TODO: Expose these consts to ini.
        static constexpr auto NumElementsPerChunk = 64 * 1024;
        static constexpr auto NumElementsPerChunk_Indirect = 16 * 1024;
        static constexpr auto MaxTotalElements_Indirect = 8 * 1024 * 1024;

    public:
        /**
         * Returns the array entry for Index, or nullptr if the index is out of range.
         *
         * The array changed shape three times and the returned pointer follows it:
         *
         *   4.7 and below:  a plain TArray<UObjectBase*>; the result points at the slot
         *   4.8 to 4.10:    a chunked array of UObjectBase*; likewise
         *   4.11 and later: a real FUObjectItem array
         *
         * See FUObjectItem for which of its accessors handle the pre-4.11 slot and which do not.
         */
        [[nodiscard]] FUObjectItem* GetObjectPtr(int32_t Index) const;
        [[nodiscard]] FUObjectItem* GetObjectPtr(int32_t Index);
        /** See GetObjectPtr for the pre-4.11 caveats; these do not range check. */
        [[nodiscard]] const FUObjectItem& operator[](int32_t Index) const;
        [[nodiscard]] FUObjectItem& operator[](int32_t Index);
    };

    class RC_UE_API FUObjectArray
    {
    public:
        using FUObjectCreateListener = Unreal::FUObjectCreateListener;
        using FUObjectDeleteListener = Unreal::FUObjectDeleteListener;
#include <MemberVariableLayout_HeaderWrapper_FUObjectArray.hpp>

    public:
        static void SetupGUObjectArrayAddress(void* address);
        static void* GetGUObjectArrayAddress();

    public:
        static bool IsValid(FUObjectItem* ObjectItem, bool bEvenIfPendingKill);
        static bool IsStale(FUObjectItem* ObjectItem, bool bEvenIfPendingKill);
        static void AddUObjectCreateListener(FUObjectCreateListener* Listener);
        static void RemoveUObjectCreateListener(FUObjectCreateListener* Listener);
        static void AddUObjectDeleteListener(FUObjectDeleteListener* Listener);
        static void RemoveUObjectDeleteListener(FUObjectDeleteListener* Listener);
        static int32_t GetNumElements();
        static int32_t GetNumChunks();
        static int32_t GetObjectItemSize();
        static int32_t GetObjectArraySize();
        static FUObjectItem* IndexToObject(int32_t Index);
        static int32 AllocateSerialNumber(int32 Index);

        void LockGUObjectArray();
        void UnlockGUObjectArray();
    };

    // Compatibility for code written before we renamed to FUObjectArray.
    using UObjectArray = FUObjectArray;

    RC_UE_API extern FUObjectArray* GUObjectArray;
}



