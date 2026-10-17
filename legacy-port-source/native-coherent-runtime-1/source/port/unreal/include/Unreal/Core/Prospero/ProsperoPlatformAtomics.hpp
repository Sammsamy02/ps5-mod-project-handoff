#pragma once

#include <Unreal/Core/CoreTypes.hpp>
#include <Unreal/Core/GenericPlatform/GenericPlatformAtomics.hpp>

namespace RC::Unreal {
struct FProsperoPlatformAtomics : FGenericPlatformAtomics {
    // SDK-native integral types, not Microsoft long casts. Callers provide natural alignment.
#define RC_PROSPERO_SCALAR_ATOMICS(Type) \
    static_assert(__atomic_always_lock_free(sizeof(Type), nullptr)); \
    static FORCEINLINE Type InterlockedIncrement(volatile Type* Value) { \
        return __atomic_add_fetch(Value, Type(1), __ATOMIC_SEQ_CST); \
    } \
    static FORCEINLINE Type InterlockedDecrement(volatile Type* Value) { \
        return __atomic_sub_fetch(Value, Type(1), __ATOMIC_SEQ_CST); \
    } \
    static FORCEINLINE Type InterlockedAdd(volatile Type* Value, Type Amount) { \
        return __atomic_fetch_add(Value, Amount, __ATOMIC_SEQ_CST); \
    } \
    static FORCEINLINE Type InterlockedExchange(volatile Type* Value, Type Exchange) { \
        return __atomic_exchange_n(Value, Exchange, __ATOMIC_SEQ_CST); \
    } \
    static FORCEINLINE Type InterlockedCompareExchange(volatile Type* Dest, Type Exchange, Type Comparand) { \
        __atomic_compare_exchange_n(Dest, &Comparand, Exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); \
        return Comparand; \
    } \
    static FORCEINLINE Type InterlockedAnd(volatile Type* Value, Type Mask) { \
        return __atomic_fetch_and(Value, Mask, __ATOMIC_SEQ_CST); \
    } \
    static FORCEINLINE Type InterlockedOr(volatile Type* Value, Type Mask) { \
        return __atomic_fetch_or(Value, Mask, __ATOMIC_SEQ_CST); \
    } \
    static FORCEINLINE Type InterlockedXor(volatile Type* Value, Type Mask) { \
        return __atomic_fetch_xor(Value, Mask, __ATOMIC_SEQ_CST); \
    } \
    static FORCEINLINE Type AtomicRead(volatile const Type* Src) { \
        return __atomic_load_n(Src, __ATOMIC_SEQ_CST); \
    } \
    static FORCEINLINE Type AtomicRead_Relaxed(volatile const Type* Src) { \
        return __atomic_load_n(Src, __ATOMIC_RELAXED); \
    } \
    static FORCEINLINE void AtomicStore(volatile Type* Dest, Type Value) { \
        __atomic_store_n(Dest, Value, __ATOMIC_SEQ_CST); \
    } \
    static FORCEINLINE void AtomicStore_Relaxed(volatile Type* Dest, Type Value) { \
        __atomic_store_n(Dest, Value, __ATOMIC_RELAXED); \
    }
    RC_PROSPERO_SCALAR_ATOMICS(int8)
    RC_PROSPERO_SCALAR_ATOMICS(int16)
    RC_PROSPERO_SCALAR_ATOMICS(int32)
    RC_PROSPERO_SCALAR_ATOMICS(int64)
#undef RC_PROSPERO_SCALAR_ATOMICS

    static_assert(__atomic_always_lock_free(sizeof(void*), nullptr));
    static FORCEINLINE void* InterlockedExchangePtr(void* volatile* Dest, void* Exchange) {
        return __atomic_exchange_n(Dest, Exchange, __ATOMIC_SEQ_CST);
    }
    static FORCEINLINE void* InterlockedCompareExchangePointer(void* volatile* Dest, void* Exchange, void* Comparand) {
        __atomic_compare_exchange_n(Dest, &Comparand, Exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
        return Comparand;
    }
    static FORCEINLINE int64 AtomicRead64(volatile const int64* Src) { return AtomicRead(Src); }
    static FORCEINLINE bool CanUseCompareExchange128() { return false; }
};
using FPlatformAtomics = FProsperoPlatformAtomics;
}
