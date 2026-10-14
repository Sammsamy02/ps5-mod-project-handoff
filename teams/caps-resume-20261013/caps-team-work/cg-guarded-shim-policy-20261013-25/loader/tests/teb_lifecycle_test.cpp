#include "guest_teb_lifecycle.h"
#include "win32_last_error_state.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

static thread_local void* published_teb = nullptr;
static std::mutex event_mu;
static std::vector<std::string> events;
static std::atomic<bool> teb_ok{true};
static std::atomic<bool> entry_should_fail{false};
static int failures = 0;
static uint32_t observed_guest_error = 0;

static void check(bool value, const char* name)
{
    std::printf("%s %s\n", value ? "PASS" : "FAIL", name);
    if (!value) ++failures;
}

static void note(const char* name, uint64_t module, uint64_t reason, uint64_t reserved)
{
    uint64_t self = 0, tls_array = 0;
    if (!published_teb || module != 0x12345000 || reserved != 0)
        teb_ok = false;
    else
    {
        auto* bytes = static_cast<uint8_t*>(published_teb);
        std::memcpy(&self, bytes + 0x30, sizeof(self));
        std::memcpy(&tls_array, bytes + 0x58, sizeof(tls_array));
        if (self != reinterpret_cast<uint64_t>(published_teb) || !tls_array ||
            !static_cast<void**>(reinterpret_cast<void*>(tls_array))[0])
            teb_ok = false;
    }
    std::lock_guard<std::mutex> guard(event_mu);
    events.emplace_back(std::string(name) + ":" + std::to_string(reason));
}

extern "C" __attribute__((ms_abi)) void callback_one(uint64_t module,
                                                        uint64_t reason, uint64_t reserved)
{
    note("c1", module, reason, reserved);
}
extern "C" __attribute__((ms_abi)) void callback_two(uint64_t module,
                                                        uint64_t reason, uint64_t reserved)
{
    note("c2", module, reason, reserved);
}
extern "C" __attribute__((ms_abi)) uint64_t guest_entry(uint64_t module,
                                                          uint64_t reason, uint64_t reserved)
{
    note("entry", module, reason, reserved);
    return entry_should_fail ? 0 : 1;
}

extern "C" __attribute__((ms_abi)) void guest_writes_teb_error(
    uint64_t, uint64_t, uint64_t)
{
    const uint32_t value = 0x55aa33ccu;
    std::memcpy(static_cast<uint8_t*>(published_teb) + 0x68, &value, sizeof(value));
}

extern "C" __attribute__((ms_abi)) uint64_t guest_reads_and_sets_error(
    uint64_t, uint64_t, uint64_t)
{
    observed_guest_error = cap13::win32_get_last_error();
    cap13::win32_set_last_error(0x66bb44ddu);
    return 1;
}

class RecordingSwitch : public cap13::GuestSegmentSwitch
{
  public:
    bool allow = true;
    int enters = 0;
    int leaves = 0;
    bool enter(void* teb) override
    {
        ++enters;
        if (!allow) return false;
        published_teb = teb;
        return true;
    }
    void leave() noexcept override
    {
        ++leaves;
        published_teb = nullptr;
    }
};

int main()
{
    using cap13::GuestNotifyStatus;
    cap13::StaticTlsBank bank;
    bank.register_module({0x42}, 2);
    cap13::GuestTebView main_teb(bank);
    const auto& initial = main_teb.refresh();
    check(initial.slots.size() == 1 && main_teb.address() &&
          main_teb.tls_array() == initial.slots.data() &&
          main_teb.self() == main_teb.address(),
          "TEB prefix publishes self at 0x30 and static TLS array at 0x58");

    RecordingSwitch sw;
    cap13::GuestModuleLifecycle lifecycle(
        bank, sw, 0x12345000,
        reinterpret_cast<uint64_t>(&guest_entry),
        {reinterpret_cast<uint64_t>(&callback_one),
         reinterpret_cast<uint64_t>(&callback_two)});
    check(lifecycle.process_attach(main_teb) == GuestNotifyStatus::Ok &&
          events == std::vector<std::string>({"c1:1", "c2:1", "entry:1"}) && teb_ok,
          "process attach publishes TEB then calls callbacks and Win64 entry in order");
    check(sw.enters == 1 && sw.leaves == 1 && !published_teb,
          "segment view is restored after process attach");
    check(lifecycle.process_attach(main_teb) == GuestNotifyStatus::WrongState,
          "duplicate process attach is refused without guest calls");

    std::atomic<bool> thread_block_distinct{false};
    std::atomic<bool> cross_thread_refused{false};
    std::thread worker([&] {
        cross_thread_refused = lifecycle.thread_attach(main_teb) ==
            GuestNotifyStatus::WrongThreadOrBank;
        cap13::GuestTebView worker_teb(bank);
        const auto attached = lifecycle.thread_attach(worker_teb);
        thread_block_distinct = attached == GuestNotifyStatus::Ok &&
            bank.current_block(0) != main_teb.block(0);
        const auto detached = lifecycle.thread_detach(worker_teb);
        thread_block_distinct = thread_block_distinct && detached == GuestNotifyStatus::Ok;
    });
    worker.join();
    check(thread_block_distinct && cross_thread_refused && teb_ok &&
          events == std::vector<std::string>({
              "c1:1", "c2:1", "entry:1",
              "c1:2", "c2:2", "entry:2",
              "c1:3", "c2:3", "entry:3"}),
          "new thread receives separate block and attach/detach notifications");

    cap13::StaticTlsBank other_bank;
    cap13::GuestTebView foreign_teb(other_bank);
    const size_t before_wrong_bank = events.size();
    check(lifecycle.thread_attach(foreign_teb) == GuestNotifyStatus::WrongThreadOrBank &&
          events.size() == before_wrong_bank,
          "TEB from another static TLS bank is refused without guest calls");

    bank.register_module({0x77}, 0);
    const auto& refreshed = main_teb.refresh();
    check(refreshed.slots.size() == 2 && main_teb.block(0) == refreshed.slots[0] &&
          main_teb.block(1) == refreshed.slots[1] && main_teb.block(1)[0] == 0x77,
          "late module refresh republishes array without replacing old block");

    sw.allow = false;
    const size_t before_refusal = events.size();
    check(lifecycle.thread_attach(main_teb) == GuestNotifyStatus::BackendRefused &&
          events.size() == before_refusal && sw.leaves == 3,
          "failed segment publication prevents every guest call");
    sw.allow = true;
    check(lifecycle.process_detach(main_teb) == GuestNotifyStatus::Ok &&
          events == std::vector<std::string>({
              "c1:1", "c2:1", "entry:1",
              "c1:2", "c2:2", "entry:2",
              "c1:3", "c2:3", "entry:3",
              "c1:0", "c2:0", "entry:0"}),
          "clean process detach sends callbacks and entry notification");
    check(lifecycle.thread_attach(main_teb) == GuestNotifyStatus::WrongState,
          "thread event after process detach is refused");

    events.clear();
    entry_should_fail = true;
    cap13::GuestModuleLifecycle rejected(
        bank, sw, 0x12345000, reinterpret_cast<uint64_t>(&guest_entry), {});
    check(rejected.process_attach(main_teb) == GuestNotifyStatus::EntryReturnedFalse &&
          events == std::vector<std::string>({"entry:1"}) &&
          rejected.thread_attach(main_teb) == GuestNotifyStatus::WrongState,
          "failed DllMain attach never marks the module process-attached");

    cap13::UnavailableSegmentSwitch unavailable;
    cap13::GuestModuleLifecycle no_backend(bank, unavailable, 0x12345000,
                                         reinterpret_cast<uint64_t>(&guest_entry), {});
    const size_t before_no_backend = events.size();
    check(no_backend.process_attach(main_teb) == GuestNotifyStatus::BackendRefused &&
          events.size() == before_no_backend,
          "production placeholder refuses guest calls without a segment backend");

    cap13::win32_set_last_error(0x1234abcd);
    (void)main_teb.refresh();
    uint32_t teb_error = 0;
    std::memcpy(&teb_error, static_cast<uint8_t*>(main_teb.address()) + 0x68,
                sizeof(teb_error));
    check(teb_error == 0x1234abcdu,
          "TEB refresh publishes the calling thread's last-error value");
    cap13::GuestModuleLifecycle error_lifecycle(
        bank, sw, 0x12345000,
        reinterpret_cast<uint64_t>(&guest_reads_and_sets_error),
        {reinterpret_cast<uint64_t>(&guest_writes_teb_error)});
    check(error_lifecycle.process_attach(main_teb) == GuestNotifyStatus::Ok &&
          observed_guest_error == 0x55aa33ccu &&
          cap13::win32_get_last_error() == 0x66bb44ddu,
          "direct guest TEB write reaches GetLastError and imported write survives exit");
    std::memcpy(&teb_error, static_cast<uint8_t*>(main_teb.address()) + 0x68,
                sizeof(teb_error));
    check(teb_error == 0x66bb44ddu && !published_teb,
          "TEB field and host-thread slot agree after segment restoration");

    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
