#include "DetourTransaction.hpp"

#include <exception>

namespace cap03 {
namespace {

bool dirty_patch_result(PatchResult result)
{
    return result == PatchResult::kRestoreWriteFailed ||
           result == PatchResult::kRestoreMprotectFailed ||
           result == PatchResult::kRestoreVerifyFailed ||
           result == PatchResult::kRestoreQueryFailed;
}

} // namespace

DetourTransaction::~DetourTransaction()
{
    if (installed_ || dirty_) std::terminate();
}

NearPrepareResult DetourTransaction::prepare(
    const uint8_t* snapshot, size_t available, void* target, void* handler,
    size_t page_size, NearMemoryOps memory)
{
    if (installed_ || dirty_ || prepared() || !target || !handler)
        return {};
    auto result = prepare_near_detour(
        snapshot, available, reinterpret_cast<uint64_t>(target),
        reinterpret_cast<uint64_t>(handler), page_size, memory, &near_);
    if (result.status == NearPrepareStatus::Ok) target_ = target;
    return result;
}

DetourTransactionResult DetourTransaction::install(
    QuiescenceOps quiescence, unsigned flags)
{
    if (dirty_) return {DetourTransactionStatus::Dirty, PatchResult::kBadArgs};
    if (installed_) return {DetourTransactionStatus::WrongState,
                            PatchResult::kBadArgs};
    if (!prepared() || !target_) return {};
    if (!quiescence.enter || !quiescence.leave)
        return {DetourTransactionStatus::QuiescenceUnavailable,
                PatchResult::kBadArgs};
    if (!quiescence.enter(quiescence.user, target_, near_.plan.stolen_length,
                          near_.trampoline, near_.mapping_bytes))
        return {DetourTransactionStatus::EnterFailed, PatchResult::kBadArgs};

    quiescence_ = quiescence;
    flags_ = flags;
    const PatchResult patch = patch_code_if_original_ex(
        target_, near_.plan.target_patch.data(),
        near_.plan.original_bytes.data(), near_.plan.stolen_length,
        &backup_, flags_);
    installed_ = patch == PatchResult::kOk;
    if (dirty_patch_result(patch)) dirty_ = true;
    const bool left = quiescence_.leave(quiescence_.user);
    if (!left) dirty_ = true;
    if (dirty_) return {left ? DetourTransactionStatus::Dirty :
                      DetourTransactionStatus::LeaveFailed, patch};
    if (patch != PatchResult::kOk)
    {
        near_.reset();
        target_ = nullptr;
        quiescence_ = {};
        return {DetourTransactionStatus::PatchFailed, patch};
    }
    return {DetourTransactionStatus::Ok, patch};
}

DetourTransactionResult DetourTransaction::remove()
{
    if (dirty_) return {DetourTransactionStatus::Dirty, PatchResult::kBadArgs};
    if (!installed_ || !prepared() || !target_)
        return {DetourTransactionStatus::WrongState, PatchResult::kBadArgs};
    if (!quiescence_.enter(quiescence_.user, target_, near_.plan.stolen_length,
                           near_.trampoline, near_.mapping_bytes))
        return {DetourTransactionStatus::EnterFailed, PatchResult::kBadArgs};
    const PatchResult patch = unpatch_code_ex(&backup_, flags_);
    if (patch == PatchResult::kOk) {
        installed_ = false;
        // Retire the trampoline before other threads are allowed to resume.
        near_.reset();
        target_ = nullptr;
    } else dirty_ = true;
    const bool left = quiescence_.leave(quiescence_.user);
    if (!left) dirty_ = true;
    if (dirty_)
        return {left ? DetourTransactionStatus::UnpatchFailed :
                DetourTransactionStatus::LeaveFailed, patch};
    quiescence_ = {};
    return {DetourTransactionStatus::Ok, patch};
}

} // namespace cap03
