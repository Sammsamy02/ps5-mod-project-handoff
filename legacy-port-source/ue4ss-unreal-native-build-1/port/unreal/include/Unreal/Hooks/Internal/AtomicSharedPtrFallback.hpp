#pragma once

#include <atomic>
#include <memory>
#include <utility>

namespace RC::Unreal::Hook::Internal {

// Private PS5-port fallback for the missing std::atomic<shared_ptr<T>>
// specialization. Keep all concurrent accesses on the standard shared_ptr
// atomic free functions; readers retain immutable snapshots by owning a copy.
// This does not define __cpp_lib_atomic_shared_ptr or specialize std::atomic.
// The implementation may use an internal lock; lock-free performance is not claimed.
template <typename T>
class AtomicSharedPtrFallback final {
public:
    explicit AtomicSharedPtrFallback(std::shared_ptr<T> initial) noexcept
        : value_(std::move(initial)) {}

    AtomicSharedPtrFallback(const AtomicSharedPtrFallback&) = delete;
    AtomicSharedPtrFallback& operator=(const AtomicSharedPtrFallback&) = delete;

    [[nodiscard]] std::shared_ptr<T> load(
        std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return std::atomic_load_explicit(&value_, order);
    }

    void store(std::shared_ptr<T> desired,
               std::memory_order order = std::memory_order_seq_cst) noexcept {
        std::atomic_store_explicit(&value_, std::move(desired), order);
    }

private:
    std::shared_ptr<T> value_;
};

} // namespace RC::Unreal::Hook::Internal
