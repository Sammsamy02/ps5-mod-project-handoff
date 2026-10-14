#include "verified_gs_segment_switch.h"

#include <cstdio>
#include <csignal>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

struct FakeGs
{
    void* current = nullptr;
    bool read_ok = true;
    bool write_ok = true;
    bool fail_first_write = false;
    bool change_on_failed_write = false;
    bool corrupt_readback = false;
    unsigned reads = 0;
    unsigned writes = 0;
    std::vector<void*> written;
};

FakeGs* lifecycle_gs = nullptr;
void* lifecycle_guest_teb = nullptr;
bool lifecycle_observed_guest = false;

extern "C" __attribute__((ms_abi)) uint64_t lifecycle_entry(
    uint64_t, uint64_t, uint64_t)
{
    lifecycle_observed_guest = lifecycle_gs &&
                               lifecycle_gs->current == lifecycle_guest_teb;
    return 1;
}

bool read_gs(void* context, void** value) noexcept
{
    auto& fake = *static_cast<FakeGs*>(context);
    ++fake.reads;
    if (!fake.read_ok) return false;
    *value = fake.corrupt_readback && fake.reads == 2 ?
             reinterpret_cast<void*>(0x3330) : fake.current;
    return true;
}

bool write_gs(void* context, void* value) noexcept
{
    auto& fake = *static_cast<FakeGs*>(context);
    ++fake.writes;
    fake.written.push_back(value);
    const bool succeeded = fake.write_ok &&
                           !(fake.fail_first_write && fake.writes == 1);
    if (succeeded || fake.change_on_failed_write) fake.current = value;
    return succeeded;
}

int failures = 0;
int checks = 0;
void check(bool condition, const char* label)
{
    ++checks;
    std::printf("%s %s\n", condition ? "PASS" : "FAIL", label);
    if (!condition) ++failures;
}

cap13::GsBaseOperations ops(FakeGs& fake)
{
    return {read_gs, write_gs, &fake};
}

} // namespace

int main()
{
    int host_teb = 0, guest_teb = 0;
    FakeGs happy;
    happy.current = &host_teb;
    cap13::VerifiedGsSegmentSwitch segment(ops(happy));
    check(!segment.enter(nullptr) && happy.reads == 0 && happy.writes == 0,
          "null guest TEB refuses before sysarch");
    check(segment.enter(&guest_teb) && happy.current == &guest_teb &&
          happy.reads == 2 && happy.writes == 1,
          "entry snapshots, writes, and verifies guest GS");
    check(!segment.enter(&guest_teb) && happy.writes == 1,
          "direct nested entry refuses without another GS write");
    segment.leave();
    check(happy.current == &host_teb && happy.reads == 3 && happy.writes == 2,
          "leave restores and verifies the host GS base");
    segment.leave();
    check(happy.writes == 2, "extra leave does not rewrite GS");

    FakeGs read_failure;
    read_failure.current = &host_teb;
    read_failure.read_ok = false;
    cap13::VerifiedGsSegmentSwitch unreadable(ops(read_failure));
    check(!unreadable.enter(&guest_teb) && read_failure.current == &host_teb &&
          read_failure.writes == 0,
          "initial GS read failure refuses without a write");

    FakeGs write_failure;
    write_failure.current = &host_teb;
    write_failure.write_ok = false;
    cap13::VerifiedGsSegmentSwitch unwritable(ops(write_failure));
    check(!unwritable.enter(&guest_teb) && write_failure.current == &host_teb &&
          write_failure.writes == 1,
          "failed unchanged write refuses and confirms original GS");

    FakeGs partial_write;
    partial_write.current = &host_teb;
    partial_write.fail_first_write = true;
    partial_write.change_on_failed_write = true;
    cap13::VerifiedGsSegmentSwitch partial(ops(partial_write));
    check(!partial.enter(&guest_teb) && partial_write.current == &host_teb &&
          partial_write.writes == 2,
          "write that reports failure after changing GS is restored");

    FakeGs readback_failure;
    readback_failure.current = &host_teb;
    readback_failure.corrupt_readback = true;
    cap13::VerifiedGsSegmentSwitch misreported(ops(readback_failure));
    check(!misreported.enter(&guest_teb) && readback_failure.current == &host_teb &&
          readback_failure.writes == 2,
          "wrong readback restores host GS before refusal");

    FakeGs nested;
    nested.current = &host_teb;
    cap13::VerifiedGsSegmentSwitch backend(ops(nested));
    cap13::ReentrantGuestSegmentSwitch wrapper(backend);
    check(wrapper.enter(&guest_teb) && wrapper.enter(&guest_teb) &&
          nested.current == &guest_teb && nested.writes == 1,
          "reentrant wrapper shares one physical GS transition");
    wrapper.leave();
    check(nested.current == &guest_teb && nested.writes == 1,
          "inner wrapper leave keeps guest GS active");
    wrapper.leave();
    check(nested.current == &host_teb && nested.writes == 2,
          "outer wrapper leave restores host GS");

    FakeGs lifecycle_state;
    lifecycle_state.current = &host_teb;
    cap13::VerifiedGsSegmentSwitch lifecycle_backend(ops(lifecycle_state));
    cap13::ReentrantGuestSegmentSwitch lifecycle_wrapper(lifecycle_backend);
    cap13::StaticTlsBank bank;
    cap13::GuestTebView teb(bank);
    lifecycle_gs = &lifecycle_state;
    lifecycle_guest_teb = teb.address();
    cap13::GuestModuleLifecycle lifecycle(
        bank, lifecycle_wrapper, 0x12345000,
        reinterpret_cast<uint64_t>(&lifecycle_entry), {});
    check(lifecycle.process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          lifecycle_observed_guest && lifecycle_state.current == &host_teb,
          "DLL lifecycle entry sees guest GS and returns with host GS restored");
    (void)lifecycle.process_detach(teb);

    std::fflush(stdout);
    const pid_t child = fork();
    if (child == 0)
    {
        rlimit no_core{0, 0};
        (void)setrlimit(RLIMIT_CORE, &no_core);
        FakeGs lethal;
        lethal.current = &host_teb;
        cap13::VerifiedGsSegmentSwitch fatal(ops(lethal));
        if (!fatal.enter(&guest_teb)) _exit(90);
        lethal.write_ok = false;
        fatal.leave();
        _exit(91);
    }
    int child_status = 0;
    const bool waited = child > 0 && waitpid(child, &child_status, 0) == child;
    check(waited && WIFSIGNALED(child_status) && WTERMSIG(child_status) == SIGABRT,
          "unrestorable GS stops the process before host continuation");

    std::printf("TOTAL: %d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
