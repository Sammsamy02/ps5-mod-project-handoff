#pragma once
#include <bit>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace RC::Unreal::ScriptFuncStorage {
// Cooperating runtime accessors only. Ordinary native readers require a
// separately reviewed target machine/publication contract, not a C++ race claim.
// The selected Clang SDK lacks std::atomic_ref; these compiler atomics must be
// always lock-free for the genuine public storage type. No runtime fallback.
template<typename Storage>
void Check(Storage& value) {
    static_assert(std::is_pointer_v<Storage>);
    static_assert(__atomic_always_lock_free(sizeof(Storage), nullptr));
    if (reinterpret_cast<std::uintptr_t>(std::addressof(value)) % alignof(Storage))
        throw std::runtime_error("Script Func storage alignment refused");
}
template<typename Function, typename Storage>
Function Load(Storage& value) {
    static_assert(std::is_pointer_v<Function> && sizeof(Function) == sizeof(Storage));
    Check(value);
    return std::bit_cast<Function>(__atomic_load_n(std::addressof(value), __ATOMIC_ACQUIRE));
}
template<typename Storage, typename Function>
void Store(Storage& value, Function function) {
    static_assert(std::is_pointer_v<Function> && sizeof(Function) == sizeof(Storage));
    Check(value);
    __atomic_store_n(std::addressof(value), std::bit_cast<Storage>(function), __ATOMIC_RELEASE);
}
template<typename Storage, typename Function>
bool Replace(Storage& value, Function expected, Function replacement) {
    static_assert(std::is_pointer_v<Function> && sizeof(Function) == sizeof(Storage));
    Check(value);
    auto stored_expected = std::bit_cast<Storage>(expected);
    return __atomic_compare_exchange_n(std::addressof(value), std::addressof(stored_expected),
        std::bit_cast<Storage>(replacement), false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}
}
