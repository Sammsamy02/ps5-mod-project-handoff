#ifndef CAP03_DETOUR_TRANSACTION_HPP
#define CAP03_DETOUR_TRANSACTION_HPP

#include "NearTrampoline.hpp"

namespace cap03 {

// The provider must prevent every other thread from entering or executing
// either span until leave returns. Before a successful removal, it must also
// drain any in-flight handler that could later call the trampoline or retain
// its address. No console provider is admitted here; a callback's claim is
// not proof that the target process is quiescent.
// Callbacks must not throw and their user context must outlive this object.
struct QuiescenceOps {
    void* user = nullptr;
    bool (*enter)(void* user, void* target, size_t target_length,
                  void* trampoline, size_t trampoline_length) = nullptr;
    bool (*leave)(void* user) = nullptr;
};

enum class DetourTransactionStatus {
    Ok, BadArgs, WrongState, QuiescenceUnavailable,
    EnterFailed, PatchFailed, UnpatchFailed, LeaveFailed, Dirty
};

struct DetourTransactionResult {
    DetourTransactionStatus status = DetourTransactionStatus::BadArgs;
    PatchResult patch = PatchResult::kBadArgs;
};

// Owns the RX trampoline and checked patch backup together. An active or
// dirty transaction cannot be destroyed safely; destruction terminates the
// process rather than releasing a mapping that a target may still reach.
class DetourTransaction {
  public:
    DetourTransaction() = default;
    ~DetourTransaction();
    DetourTransaction(const DetourTransaction&) = delete;
    DetourTransaction& operator=(const DetourTransaction&) = delete;

    NearPrepareResult prepare(const uint8_t* snapshot, size_t available,
                              void* target, void* handler, size_t page_size,
                              NearMemoryOps memory);
    DetourTransactionResult install(
        QuiescenceOps quiescence,
        unsigned flags = kFlagRequireLiveness);
    DetourTransactionResult remove();
    bool prepared() const noexcept { return near_.trampoline != nullptr; }
    bool installed() const noexcept { return installed_; }
    bool dirty() const noexcept { return dirty_; }
    const PatchBackup& backup() const noexcept { return backup_; }

  private:
    PreparedNearDetour near_{};
    PatchBackup backup_{};
    QuiescenceOps quiescence_{};
    void* target_ = nullptr;
    unsigned flags_ = kFlagRequireLiveness;
    bool installed_ = false;
    bool dirty_ = false;
};

} // namespace cap03
#endif
