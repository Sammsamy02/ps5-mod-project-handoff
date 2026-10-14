#include "guest_teb_lifecycle.h"
#include "win_api_shim.h"
#include "win64_bridge.h"

#include <cstdint>
#include <cstdio>
#include <sys/mman.h>
#include <unistd.h>
#include <utility>
#include <vector>

static int failures = 0;
static void check(bool ok, const char* label)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++failures;
}
static bool allocate_writable(size_t minimum, void** address, size_t* bytes, void*)
{
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    if (minimum > page) return false;
    void* mapping = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANON, -1, 0);
    if (mapping == MAP_FAILED) return false;
    *address = mapping;
    *bytes = page;
    return true;
}
static bool seal_executable(void* address, size_t bytes, void*)
{
    return mprotect(address, bytes, PROT_READ | PROT_EXEC) == 0;
}
static void release_mapping(void* address, size_t bytes, void*)
{
    munmap(address, bytes);
}
static int rejected_callback_calls = 0;
static bool reject_disable(void*) { ++rejected_callback_calls; return false; }
static bool disable_lifecycle(void* user)
{
    return static_cast<cap13::GuestModuleLifecycle*>(user)->disable_thread_notifications();
}

static void* disable_entry = nullptr;
static uint64_t eligible_result = UINT64_MAX;
static uint64_t tls_result = UINT64_MAX;
static int retry_attaches = 0;
static std::vector<std::pair<uint64_t, uint64_t>> events;
extern "C" __attribute__((ms_abi)) uint64_t module_entry(
    uint64_t module, uint64_t reason, uint64_t reserved)
{
    if (reserved != 0) return 0;
    events.emplace_back(module, reason);
    if (reason == 1)
    {
        if (module != 0x5000 || retry_attaches == 0)
        {
            const uint64_t result = cap13_win64_call1(disable_entry, module);
            if (module == 0x2000) eligible_result = result;
            if (module == 0x3000) tls_result = result;
        }
        if (module == 0x5000 && ++retry_attaches == 1) return 0;
    }
    return 1;
}

class RecordingSegment final : public cap13::GuestSegmentSwitch
{
  public:
    int enters = 0;
    int leaves = 0;
    bool enter(void*) override { ++enters; return true; }
    void leave() noexcept override { ++leaves; }
};

int main()
{
    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    cap13::WinApiShim::ExecutableMemoryBackend backend{};
    backend.allocate_writable = allocate_writable;
    backend.seal_executable = seal_executable;
    backend.release = release_mapping;
    check(shim.set_executable_backend(backend), "executable backend accepted");
    disable_entry = shim.resolve("kernel32.dll", "DisableThreadLibraryCalls", 0);
    void* get_error = shim.resolve("kernel32.dll", "GetLastError", 0);
    void* set_error = shim.resolve("kernel32.dll", "SetLastError", 0);
    check(disable_entry && get_error && set_error,
          "thread-notification and last-error imports resolve");
    if (!disable_entry || !get_error || !set_error) return 1;

    check(cap13_win64_call1(disable_entry, 0) == 0 &&
          cap13_win64_call0(get_error) == 87,
          "null module handle returns FALSE with an error");
    check(cap13_win64_call1(disable_entry, 0x9999) == 0 &&
          cap13_win64_call0(get_error) == 87,
          "unregistered module handle cannot suppress notifications");
    check(shim.register_thread_notification_policy(
              reinterpret_cast<void*>(0x4000), false, reject_disable, nullptr) &&
          cap13_win64_call1(disable_entry, 0x4000) == 0 &&
          rejected_callback_calls == 1 && cap13_win64_call0(get_error) == 87,
          "adapter refusal propagates as FALSE without marking success");

    cap13::StaticTlsBank bank;
    cap13::GuestTebView teb(bank);
    RecordingSegment segment;
    cap13::GuestModuleLifecycle eligible(bank, segment, 0x2000,
                                          reinterpret_cast<uint64_t>(&module_entry), {});
    check(shim.register_thread_notification_policy(
              reinterpret_cast<void*>(0x2000), false, disable_lifecycle, &eligible),
          "non-TLS module policy registered before DllMain");
    (void)cap13_win64_call1(set_error, 0x1234);
    check(eligible.process_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          eligible_result == 1 && cap13_win64_call0(get_error) == 0x1234,
          "non-TLS DllMain disables thread notifications and preserves prior error");
    check(eligible.thread_attach(teb) == cap13::GuestNotifyStatus::Ok &&
          eligible.thread_detach(teb) == cap13::GuestNotifyStatus::Ok &&
          events == std::vector<std::pair<uint64_t, uint64_t>>({{0x2000, 1}}),
          "eligible module skips both thread notifications");
    cap13::StaticTlsBank other_bank;
    cap13::GuestTebView wrong_teb(other_bank);
    check(eligible.thread_attach(wrong_teb) ==
          cap13::GuestNotifyStatus::WrongThreadOrBank,
          "suppressed notifications still reject a foreign TEB");
    check(eligible.process_detach(teb) == cap13::GuestNotifyStatus::Ok &&
          events == std::vector<std::pair<uint64_t, uint64_t>>(
              {{0x2000, 1}, {0x2000, 0}}) && segment.enters == 2 && segment.leaves == 2,
          "process notifications still run after thread calls are disabled");

    cap13::StaticTlsBank tls_bank;
    tls_bank.register_module({0x44}, 1);
    cap13::GuestTebView tls_teb(tls_bank);
    RecordingSegment tls_segment;
    cap13::GuestModuleLifecycle with_tls(tls_bank, tls_segment, 0x3000,
                                          reinterpret_cast<uint64_t>(&module_entry), {});
    check(shim.register_thread_notification_policy(
              reinterpret_cast<void*>(0x3000), true, disable_lifecycle, &with_tls),
          "static-TLS module policy registered before DllMain");
    check(with_tls.process_attach(tls_teb) == cap13::GuestNotifyStatus::Ok &&
          tls_result == 0 && cap13_win64_call0(get_error) == 87,
          "static-TLS DllMain receives documented FALSE result");
    check(with_tls.thread_attach(tls_teb) == cap13::GuestNotifyStatus::Ok &&
          with_tls.thread_detach(tls_teb) == cap13::GuestNotifyStatus::Ok &&
          with_tls.process_detach(tls_teb) == cap13::GuestNotifyStatus::Ok &&
          events == std::vector<std::pair<uint64_t, uint64_t>>(
              {{0x2000, 1}, {0x2000, 0},
               {0x3000, 1}, {0x3000, 2}, {0x3000, 3}, {0x3000, 0}}),
          "static-TLS module retains thread notifications");

    events.clear();
    cap13::StaticTlsBank retry_bank;
    cap13::GuestTebView retry_teb(retry_bank);
    RecordingSegment retry_segment;
    cap13::GuestModuleLifecycle retry(retry_bank, retry_segment, 0x5000,
                                       reinterpret_cast<uint64_t>(&module_entry), {});
    check(shim.register_thread_notification_policy(
              reinterpret_cast<void*>(0x5000), false, disable_lifecycle, &retry) &&
          !shim.register_thread_notification_policy(
              reinterpret_cast<void*>(0x5000), false, disable_lifecycle, &retry),
          "module policy rejects duplicate handles");
    check(retry.process_attach(retry_teb) ==
          cap13::GuestNotifyStatus::EntryReturnedFalse &&
          retry.thread_attach(retry_teb) == cap13::GuestNotifyStatus::WrongState &&
          retry.process_attach(retry_teb) == cap13::GuestNotifyStatus::Ok &&
          retry.thread_attach(retry_teb) == cap13::GuestNotifyStatus::Ok &&
          retry.thread_detach(retry_teb) == cap13::GuestNotifyStatus::Ok &&
          retry.process_detach(retry_teb) == cap13::GuestNotifyStatus::Ok &&
          events == std::vector<std::pair<uint64_t, uint64_t>>(
              {{0x5000, 1}, {0x5000, 1}, {0x5000, 2}, {0x5000, 3}, {0x5000, 0}}),
          "failed process attach clears prior notification suppression");

    std::printf("TOTAL: %d failed\n", failures);
    return failures ? 1 : 0;
}
