#pragma once
#include <atomic>
#include <bit>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <type_traits>

namespace RC::Unreal::ScriptFuncStorage {
// Atomic access is limited to cooperating runtime accessors. Native ordinary
// readers require a separately reviewed target machine/publication contract.
template<typename Storage>
std::atomic_ref<Storage> Ref(Storage& value) {
    static_assert(std::is_pointer_v<Storage>);
    static_assert(std::atomic_ref<Storage>::is_always_lock_free);
    if (reinterpret_cast<std::uintptr_t>(std::addressof(value)) % std::atomic_ref<Storage>::required_alignment)
        throw std::runtime_error("Script Func storage alignment refused");
    return std::atomic_ref<Storage>{value};
}
template<typename Function, typename Storage>
Function Load(Storage& value) {
    static_assert(std::is_pointer_v<Function> && sizeof(Function) == sizeof(Storage));
    return std::bit_cast<Function>(Ref(value).load(std::memory_order_acquire));
}
template<typename Storage, typename Function>
void Store(Storage& value, Function function) {
    static_assert(std::is_pointer_v<Function> && sizeof(Function) == sizeof(Storage));
    Ref(value).store(std::bit_cast<Storage>(function), std::memory_order_release);
}
template<typename Storage, typename Function>
bool Replace(Storage& value, Function expected, Function replacement) {
    static_assert(std::is_pointer_v<Function> && sizeof(Function) == sizeof(Storage));
    auto stored_expected = std::bit_cast<Storage>(expected);
    return Ref(value).compare_exchange_strong(stored_expected, std::bit_cast<Storage>(replacement),
        std::memory_order_acq_rel, std::memory_order_acquire);
}
}
