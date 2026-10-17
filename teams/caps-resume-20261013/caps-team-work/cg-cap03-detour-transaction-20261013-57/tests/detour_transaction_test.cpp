#include "DetourTransaction.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <csignal>
#include <exception>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {
int checks = 0;
int failures = 0;
void expect(bool ok, const char* label)
{
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}

struct State {
    void* mapping = nullptr;
    bool sealed = false;
    bool allow_enter = true;
    bool quiesced = false;
    bool fail_patch = false;
    bool fail_unpatch = false;
    bool fail_leave = false;
    int enters = 0;
    int leaves = 0;
    int patches = 0;
    int unpatches = 0;
    int releases = 0;
    bool expect_release_under_quiescence = false;
    bool released_under_quiescence = true;
    bool saw_trampoline_span = false;
};
State* active = nullptr;

bool reserve(void* user, uint64_t, size_t, void** output)
{
    *output = static_cast<State*>(user)->mapping;
    return *output != nullptr;
}
bool seal(void* user, void* mapping, size_t bytes)
{
    auto& state = *static_cast<State*>(user);
    state.sealed = mprotect(mapping, bytes, PROT_READ | PROT_EXEC) == 0;
    return state.sealed;
}
void release(void* user, void* mapping, size_t bytes)
{
    auto& state = *static_cast<State*>(user);
    ++state.releases;
    if (state.expect_release_under_quiescence && !state.quiesced)
        state.released_under_quiescence = false;
    (void)mprotect(mapping, bytes, PROT_READ | PROT_WRITE);
}
bool enter(void* user, void*, size_t, void* trampoline, size_t trampoline_len)
{
    auto& state = *static_cast<State*>(user);
    ++state.enters;
    state.saw_trampoline_span = trampoline == state.mapping &&
                                trampoline_len != 0;
    if (!state.allow_enter) return false;
    state.quiesced = true;
    return true;
}
bool leave(void* user)
{
    auto& state = *static_cast<State*>(user);
    ++state.leaves;
    state.quiesced = false;
    return !state.fail_leave;
}
cap03::NearMemoryOps memory_ops(State& state)
{
    return {&state, reserve, seal, release};
}
cap03::QuiescenceOps quiescence_ops(State& state)
{
    return {&state, enter, leave};
}
} // namespace

namespace cap03 {
PatchResult patch_code_if_original_ex(void* target, const void* replacement,
                                     const void* expected, size_t length,
                                     PatchBackup* backup, unsigned)
{
    auto& state = *active;
    ++state.patches;
    if (!state.quiesced || state.fail_patch) return PatchResult::kMprotectFailed;
    if (std::memcmp(target, expected, length))
        return PatchResult::kOriginalMismatch;
    backup->magic = kBackupMagic;
    backup->valid = 1;
    backup->target = target;
    backup->length = length;
    std::memcpy(backup->original_bytes, target, length);
    std::memcpy(backup->replacement_bytes, replacement, length);
    std::memcpy(target, replacement, length);
    return PatchResult::kOk;
}
PatchResult unpatch_code_ex(PatchBackup* backup, unsigned)
{
    auto& state = *active;
    ++state.unpatches;
    if (!state.quiesced || state.fail_unpatch)
        return PatchResult::kRestoreMprotectFailed;
    std::memcpy(backup->target, backup->original_bytes, backup->length);
    backup->valid = 0;
    return PatchResult::kOk;
}
} // namespace cap03

int main()
{
    const long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) return 2;
    const size_t bytes = static_cast<size_t>(page);
    void* region = mmap(nullptr, bytes * 4, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region == MAP_FAILED) return 2;
    auto* target = static_cast<uint8_t*>(region);
    auto* near = target + bytes * 2;
    auto* handler = target + bytes * 3;
    const uint8_t original[20] = {
        0x55, 0x48, 0x89, 0xe5, 0x48, 0x83, 0xec, 0x20,
        0x48, 0x89, 0x7d, 0xf8, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90};
    std::memcpy(target, original, sizeof(original));
    State state{};
    state.mapping = near;
    active = &state;
    cap03::DetourTransaction transaction;
    auto prepared = transaction.prepare(original, sizeof(original), target,
                                        handler, bytes, memory_ops(state));
    expect(prepared.status == cap03::NearPrepareStatus::Ok &&
           transaction.prepared() && state.sealed,
           "transaction owns a sealed near trampoline before target write");
    auto result = transaction.install({}, cap03::kFlagSkipLiveness);
    expect(result.status == cap03::DetourTransactionStatus::QuiescenceUnavailable &&
           state.patches == 0 && state.releases == 0,
           "missing quiescence callbacks refuse before any patch attempt");
    state.allow_enter = false;
    result = transaction.install(quiescence_ops(state),
                                 cap03::kFlagSkipLiveness);
    expect(result.status == cap03::DetourTransactionStatus::EnterFailed &&
           state.patches == 0 && state.enters == 1 && state.leaves == 0,
           "failed quiescence entry leaves target and trampoline untouched");
    state.allow_enter = true;
    result = transaction.install(quiescence_ops(state),
                                 cap03::kFlagSkipLiveness);
    expect(result.status == cap03::DetourTransactionStatus::Ok &&
           transaction.installed() && !state.quiesced &&
           state.patches == 1 && state.leaves == 1 &&
           std::memcmp(target, original, 14) != 0 && state.releases == 0,
           "checked patch runs within quiescence and retains trampoline");
    result = transaction.install(quiescence_ops(state),
                                 cap03::kFlagSkipLiveness);
    expect(result.status == cap03::DetourTransactionStatus::WrongState &&
           state.patches == 1,
           "second install cannot replace an active target patch");
    state.expect_release_under_quiescence = true;
    result = transaction.remove();
    state.expect_release_under_quiescence = false;
    expect(result.status == cap03::DetourTransactionStatus::Ok &&
           !transaction.installed() && !transaction.prepared() &&
           state.unpatches == 1 && state.enters == 3 &&
           state.leaves == 2 && state.releases == 1 &&
           state.released_under_quiescence && state.saw_trampoline_span &&
           std::memcmp(target, original, sizeof(original)) == 0,
           "remove restores original bytes under quiescence then releases trampoline");
    prepared = transaction.prepare(original, sizeof(original), target,
                                   handler, bytes, memory_ops(state));
    state.fail_patch = true;
    result = transaction.install(quiescence_ops(state),
                                 cap03::kFlagSkipLiveness);
    expect(prepared.status == cap03::NearPrepareStatus::Ok &&
           result.status == cap03::DetourTransactionStatus::PatchFailed &&
           !transaction.prepared() && state.releases == 2 &&
           std::memcmp(target, original, sizeof(original)) == 0,
           "clean patch refusal releases unused trampoline after quiescence");
    state.fail_patch = false;
    target[0] ^= 1;
    prepared = transaction.prepare(original, sizeof(original), target,
                                   handler, bytes, memory_ops(state));
    result = transaction.install(quiescence_ops(state),
                                 cap03::kFlagSkipLiveness);
    expect(prepared.status == cap03::NearPrepareStatus::Ok &&
           result.status == cap03::DetourTransactionStatus::PatchFailed &&
           result.patch == cap03::PatchResult::kOriginalMismatch &&
           !transaction.prepared() && state.releases == 3,
           "stale target bytes refuse install and release unused trampoline");
    target[0] ^= 1;
    std::fflush(nullptr);
    const pid_t leave_child = fork();
    if (leave_child == 0)
    {
        state.fail_leave = true;
        auto step = transaction.prepare(original, sizeof(original), target,
                                        handler, bytes, memory_ops(state));
        const int releases_before = state.releases;
        auto failed = transaction.install(quiescence_ops(state),
                                          cap03::kFlagSkipLiveness);
        const bool retained = step.status == cap03::NearPrepareStatus::Ok &&
            failed.status == cap03::DetourTransactionStatus::LeaveFailed &&
            transaction.dirty() && transaction.installed() &&
            transaction.prepared() && state.releases == releases_before;
        _exit(retained ? 0 : 1);
    }
    int leave_status = 0;
    const bool leave_waited = leave_child > 0 &&
        waitpid(leave_child, &leave_status, 0) == leave_child;
    expect(leave_waited && WIFEXITED(leave_status) &&
           WEXITSTATUS(leave_status) == 0,
           "failed quiescence exit marks dirty and retains trampoline");
    std::fflush(nullptr);
    const pid_t dirty_child = fork();
    if (dirty_child == 0)
    {
        state.fail_patch = false;
        state.fail_unpatch = true;
        auto step = transaction.prepare(original, sizeof(original), target,
                                        handler, bytes, memory_ops(state));
        auto installed = transaction.install(quiescence_ops(state),
                                             cap03::kFlagSkipLiveness);
        const int releases_before = state.releases;
        auto failed = transaction.remove();
        const bool retained = step.status == cap03::NearPrepareStatus::Ok &&
            installed.status == cap03::DetourTransactionStatus::Ok &&
            failed.status == cap03::DetourTransactionStatus::UnpatchFailed &&
            transaction.dirty() && transaction.prepared() &&
            state.releases == releases_before;
        _exit(retained ? 0 : 1);
    }
    int dirty_status = 0;
    const bool dirty_waited = dirty_child > 0 &&
        waitpid(dirty_child, &dirty_status, 0) == dirty_child;
    expect(dirty_waited && WIFEXITED(dirty_status) &&
           WEXITSTATUS(dirty_status) == 0,
           "failed unpatch marks dirty and retains reachable trampoline");
    std::fflush(nullptr);
    const pid_t active_child = fork();
    if (active_child == 0)
    {
        std::set_terminate([] { std::raise(SIGABRT); });
        state.fail_patch = false;
        if (transaction.prepare(original, sizeof(original), target,
                                handler, bytes, memory_ops(state)).status !=
            cap03::NearPrepareStatus::Ok)
            _exit(2);
        if (transaction.install(quiescence_ops(state),
                                cap03::kFlagSkipLiveness).status !=
            cap03::DetourTransactionStatus::Ok)
            _exit(3);
        // Returning normally destroys an active transaction. It must stop
        // rather than release a trampoline still named by the target patch.
        return 0;
    }
    int active_status = 0;
    const bool active_waited = active_child > 0 &&
        waitpid(active_child, &active_status, 0) == active_child;
    expect(active_waited && WIFSIGNALED(active_status) &&
           WTERMSIG(active_status) == SIGABRT,
           "active transaction destruction terminates before trampoline release");
    munmap(region, bytes * 4);
    std::printf("TOTAL: checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
