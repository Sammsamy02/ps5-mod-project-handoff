#include "guest_pe_thread.h"
#include "guest_read_map.h"
#include "guest_thread_manager.h"
#include "guest_write_map.h"
#include "pe_types.h"
#include "win32_thread_identity.h"
#include "win32_last_error_state.h"
#include "win_api_shim.h"

#include <cstdint>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <thread>
#include <vector>
#include <sys/mman.h>
#include <unistd.h>

namespace {
int checks = 0;
int failures = 0;
std::atomic<int> ue4ss_calls{0};
std::atomic<bool> blocked_started{false};
std::atomic<bool> blocked_release{false};
std::atomic<uint32_t> blocked_identity{0};
std::atomic<int> unsafe_write_calls{0};
bool unsafe_writer(void*, void*, const void*, size_t)
{
    ++unsafe_write_calls;
    return true;
}
cap13::GuestThreadRunResult blocked_runner(cap13::GuestPeBatchResult&,
                                            cap13::StaticTlsBank&, void*, void*)
{
    blocked_identity.store(cap13::win32_current_thread_id());
    blocked_started.store(true);
    while (!blocked_release.load()) std::this_thread::yield();
    return {cap13::GuestThreadRunStatus::Ok, 77};
}
void check(bool ok, const char* name)
{
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}
std::vector<uint8_t> read_file(const char* path)
{
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
extern "C" __attribute__((ms_abi)) uint64_t ue4ss_initialized()
{
    ++ue4ss_calls;
    return 1;
}
bool lookup_ue4ss(cap13::WinModule, const char* symbol, void** out)
{
    if (std::strcmp(symbol, "is_ue4ss_initialized") != 0) return false;
    *out = reinterpret_cast<void*>(&ue4ss_initialized);
    return true;
}
bool allocate_writable(size_t minimum, void** address, size_t* bytes, void*)
{
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    if (minimum > page) return false;
    void* memory = mmap(nullptr, page, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (memory == MAP_FAILED) return false;
    *address = memory;
    *bytes = page;
    return true;
}
bool seal_executable(void* memory, size_t bytes, void*)
{
    return mprotect(memory, bytes, PROT_READ | PROT_EXEC) == 0;
}
void release_mapping(void* memory, size_t bytes, void*) { munmap(memory, bytes); }
class RecordingSwitch final : public cap13::GuestSegmentSwitch
{
  public:
    int enters = 0;
    int leaves = 0;
    int allow_until = 1000;
    char label = 0;
    std::vector<char>* entry_order = nullptr;
    bool enter(void* teb) override
    {
        if (entry_order) entry_order->push_back(label);
        ++enters;
        return teb && enters <= allow_until;
    }
    void leave() noexcept override { ++leaves; }
};
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const auto fixture = read_file(argv[1]);
    if (fixture.empty()) return 2;
    cap13::WinApiShim shim;
    shim.register_builtin_exports();
    uint32_t stack_output = 0;
    uint32_t supplied = 0x12345678;
    int stack_owner = 0;
    int other_owner = 0;
    cap13::GuestWriteMap writes;
    check(!shim.write_guest_bytes(&stack_output, &supplied, sizeof(supplied)),
          "guest output write refuses without a validated range");
    check(writes.add(&stack_owner, &stack_output, sizeof(stack_output)) &&
          shim.set_guest_write_map(&writes) &&
          shim.write_guest_bytes(&stack_output, &supplied, sizeof(supplied)) &&
          stack_output == supplied,
          "validated stack output receives a bounded DWORD write");
    check(!writes.add(&other_owner, &stack_output, sizeof(stack_output)) &&
          !writes.add(&stack_owner, &supplied, sizeof(supplied)) &&
          !shim.write_guest_bytes(reinterpret_cast<uint8_t*>(&stack_output) + 1,
                                  &supplied, sizeof(supplied)) &&
          stack_output == supplied,
          "overlap and cross-boundary writes refuse without modifying output");
    std::vector<uint8_t> heap_output(8, 0);
    check(writes.add(&other_owner, heap_output.data(), heap_output.size()) &&
          shim.write_guest_bytes(heap_output.data() + 4, &supplied,
                                 sizeof(supplied)) &&
          std::memcmp(heap_output.data() + 4, &supplied, sizeof(supplied)) == 0,
          "caller-owned heap range permits bounded output writes");
    check(!writes.add(&supplied, reinterpret_cast<void*>(UINTPTR_MAX - 1), 8) &&
          !shim.write_guest_bytes(nullptr, &supplied, sizeof(supplied)) &&
          !shim.write_guest_bytes(&stack_output, &supplied, 0),
          "wrapping and empty guest writes refuse");
    check(writes.remove(&stack_owner) &&
          !shim.write_guest_bytes(&stack_output, &supplied, sizeof(supplied)) &&
          !writes.remove(&stack_owner) &&
          writes.remove(&other_owner),
          "owner removal revokes guest output permission");
    const uint32_t readonly = 0x1234;
    int readonly_owner = 0;
    uint32_t copied_read = 0;
    cap13::GuestReadMap reads;
    check(reads.add(&readonly_owner, &readonly, sizeof(readonly)) &&
          reads.read(&readonly, &copied_read, sizeof(copied_read)) &&
          copied_read == readonly &&
          !shim.write_guest_bytes(const_cast<uint32_t*>(&readonly),
                                  &supplied, sizeof(supplied)),
          "readable guest range does not grant output-write permission");
    cap13::WinApiShim::Hooks unsafe_hooks{};
    unsafe_hooks.write_guest_bytes = unsafe_writer;
    shim.set_hooks(unsafe_hooks);
    check(!shim.write_guest_bytes(&stack_output, &supplied, sizeof(supplied)) &&
          unsafe_write_calls.load() == 0,
          "configured write map cannot be bypassed by later hook changes");
    uint8_t local_teb[0x70]{};
    uint32_t guest_stack_output = 0;
    std::atomic<uint32_t*> other_stack{nullptr};
    std::atomic<bool> other_release{false};
    std::thread other_worker([&] {
        uint32_t other_local = 0;
        other_stack.store(&other_local);
        while (!other_release.load()) std::this_thread::yield();
    });
    while (!other_stack.load()) std::this_thread::yield();
    {
        cap13::ScopedGuestLastErrorBinding guest_binding(local_teb);
        check(shim.write_guest_bytes(&guest_stack_output, &supplied,
                                     sizeof(supplied)) &&
              guest_stack_output == supplied,
              "active guest can write an output in its own caller stack");
        check(!shim.write_guest_bytes(other_stack.load(), &supplied,
                                      sizeof(supplied)) &&
              !shim.write_guest_bytes(heap_output.data(), &supplied,
                                      sizeof(supplied)),
              "guest stack fallback rejects another thread and unowned heap");
        check(!shim.write_guest_bytes(reinterpret_cast<void*>(UINTPTR_MAX - 1),
                                      &supplied, sizeof(supplied)),
              "guest stack fallback rejects wrapping output range");
    }
    other_release.store(true);
    other_worker.join();
    check(!shim.write_guest_bytes(&guest_stack_output, &supplied,
                                  sizeof(supplied)),
          "guest stack output refuses after guest TEB binding exits");
    uint8_t split_output[8]{};
    int split_owner = 0;
    cap13::GuestWriteMap split_map;
    std::vector<cap13::GuestWriteMap::Span> split_ranges = {
        {split_output, 2}, {split_output + 4, 2}};
    const uint8_t mark = 0x7e;
    check(split_map.add_many(&split_owner, split_ranges) &&
          split_map.write(split_output + 1, &mark, 1) &&
          split_map.write(split_output + 4, &mark, 1) &&
          !split_map.write(split_output + 2, &mark, 1),
          "multi-section registration admits each range but not its gap");
    check(split_map.remove(&split_owner) &&
          !split_map.write(split_output + 1, &mark, 1) &&
          !split_map.write(split_output + 4, &mark, 1),
          "owner removal revokes every section together");
    std::vector<cap13::GuestWriteMap::Span> overlap_ranges = {
        {split_output, 3}, {split_output + 2, 2}};
    check(!split_map.add_many(&split_owner, overlap_ranges) &&
          !split_map.write(split_output, &mark, 1),
          "overlapping multi-section registration is atomic refusal");
    cap13::WinApiShim::ExecutableMemoryBackend backend{};
    backend.allocate_writable = allocate_writable;
    backend.seal_executable = seal_executable;
    backend.release = release_mapping;
    check(shim.set_executable_backend(backend),
          "reviewed import trampoline backend configured");
    shim.register_module("UE4SS.dll", reinterpret_cast<void*>(0x100), lookup_ue4ss);
    cap13::LoadOptions options;
    options.page_size = 0x1000;
    options.allow_rwx_partition = true;
    options.allow_no_relocs = true;
    cap13::StaticTlsBank bank;
    cap13::GuestTebView main_teb(bank);
    RecordingSwitch segment;
    auto batch = cap13::pe_attach_guest_batch(
        {{"/mods/fixture.dll", fixture.data(), fixture.size(), &segment}},
        shim, options, bank, main_teb);
    if (!batch.ok)
    {
        std::printf("attach reason: %s\n", batch.reason.c_str());
        for (const auto& missing : batch.missing)
            std::printf("missing: %s\n", missing.c_str());
    }
    check(batch.ok && batch.modules.size() == 1,
          "unchanged synthetic DLL attaches before worker entry");
    if (!batch.ok || batch.modules.size() != 1) return 1;
    void* start = shim.resolve("fixture.dll", "start_mod", 0);
    check(start && batch.modules[0]->owns_executable_address(start),
          "worker start is validated inside attached executable code");
    uint32_t writable_rva = 0;
    const auto* dos = reinterpret_cast<const cap13::PeDosHeader*>(fixture.data());
    const auto* coff = reinterpret_cast<const cap13::PeFileHeader*>(
        fixture.data() + dos->e_lfanew + 4);
    const auto* sections = reinterpret_cast<const cap13::PeSectionHeader*>(
        reinterpret_cast<const uint8_t*>(coff) + sizeof(*coff) +
        coff->SizeOfOptionalHeader);
    for (uint16_t i = 0; i < coff->NumberOfSections; ++i)
        if ((sections[i].Characteristics & cap13::PE_SCN_MEM_WRITE) &&
            !(sections[i].Characteristics & cap13::PE_SCN_MEM_EXECUTE) &&
            (sections[i].VirtualSize || sections[i].SizeOfRawData))
        { writable_rva = sections[i].VirtualAddress; break; }
    check(writable_rva != 0,
          "unchanged synthetic PE exposes a writable non-executable section");
    auto* mapped_base = static_cast<uint8_t*>(batch.modules[0]->mapped_handle());
    uint8_t* mapped_output = mapped_base + writable_rva;
    if (writable_rva)
    {
        const uint8_t old = *mapped_output;
        const uint8_t changed = old ^ 0x5a;
        check(shim.write_guest_bytes(mapped_output, &changed, 1) &&
              *mapped_output == changed &&
              shim.write_guest_bytes(mapped_output, &old, 1) &&
              *mapped_output == old,
              "attached PE writable section is registered for bounded output");
        check(!shim.write_guest_bytes(mapped_base, &changed, 1) &&
              !shim.write_guest_bytes(start, &changed, 1),
              "PE headers and executable start remain output-write denied");
    }

    cap13::GuestThreadRunResult run{};
    const int before = segment.enters;
    const int calls_before = ue4ss_calls.load();
    std::thread worker([&] {
        run = cap13::pe_run_guest_thread(batch, bank, start, nullptr);
    });
    worker.join();
    check(run.status == cap13::GuestThreadRunStatus::Ok &&
          run.exit_code == 0 && segment.enters == before + 1 &&
          segment.leaves == segment.enters &&
          ue4ss_calls.load() > calls_before,
          "unchanged DLL worker enters Win64 start with disabled notifications");

    const int before_invalid = segment.enters;
    std::thread invalid([&] {
        run = cap13::pe_run_guest_thread(
            batch, bank, reinterpret_cast<void*>(&ue4ss_initialized), nullptr);
    });
    invalid.join();
    check(run.status == cap13::GuestThreadRunStatus::InvalidStart &&
          segment.enters == before_invalid,
          "foreign start address refuses before guest notifications");

    segment.allow_until = segment.enters;
    std::thread refused([&] {
        run = cap13::pe_run_guest_thread(batch, bank, start, nullptr);
    });
    refused.join();
    check(run.status == cap13::GuestThreadRunStatus::StartFailed &&
          segment.enters == before_invalid + 1,
          "refused guest segment stops worker before the start function");
    segment.allow_until = 1000;

    {
        cap13::GuestThreadManager manager(batch, bank, blocked_runner);
        auto invalid = manager.create(reinterpret_cast<void*>(&ue4ss_initialized),
                                      nullptr);
        check(invalid.status == cap13::GuestThreadCreateStatus::InvalidStart,
              "manager rejects foreign executable address before spawn");
        auto unsupported = manager.create(start, nullptr, nullptr, 0, 4);
        check(unsupported.status == cap13::GuestThreadCreateStatus::UnsupportedOptions,
              "manager refuses unsupported suspended start");
        auto created = manager.create(start, nullptr);
        check(created.status == cap13::GuestThreadCreateStatus::Ok &&
              created.handle != 0 && created.thread_id != 0,
              "manager creates an opaque handle and reserved worker ID");
        while (!blocked_started.load()) std::this_thread::yield();
        check(blocked_identity.load() == created.thread_id,
              "worker GetCurrentThreadId identity agrees with create result");
        auto pending = manager.query(created.handle);
        check(pending.valid && !pending.finished &&
              manager.wait(created.handle, 0) == cap13::GuestThreadWaitStatus::Timeout,
              "running worker reports pending state and zero-timeout wait");
        check(manager.close(created.handle) &&
              manager.wait(created.handle, 0) == cap13::GuestThreadWaitStatus::InvalidHandle &&
              !manager.query(created.handle).valid,
              "close invalidates handle without ending worker");
        check(blocked_started.load() && !blocked_release.load(),
              "closed handle leaves worker running");
        blocked_release.store(true);
        auto second = manager.create(start, nullptr);
        check(second.status == cap13::GuestThreadCreateStatus::Ok &&
              second.handle != created.handle &&
              second.thread_id != created.thread_id &&
              manager.wait(second.handle, 10000) ==
                  cap13::GuestThreadWaitStatus::Object0,
              "closed handle and worker identity are never reused");
        check(manager.close(second.handle) && !manager.close(second.handle),
              "closing a handle twice is refused");
        check(manager.quiesce_all(),
              "quiesce joins worker even after its handle was closed");
        check(manager.create(start, nullptr).status ==
                  cap13::GuestThreadCreateStatus::WrongState,
              "quiesced manager refuses new workers");
    }
    {
        cap13::GuestThreadManager manager(batch, bank);
        auto created = manager.create(start, nullptr);
        check(created.status == cap13::GuestThreadCreateStatus::Ok,
              "manager creates real Win64 guest worker");
        check(manager.wait(created.handle, 10000) ==
                  cap13::GuestThreadWaitStatus::Object0,
              "thread handle signals after guest worker completion");
        auto completed = manager.query(created.handle);
        check(completed.valid && completed.finished &&
              completed.run.status == cap13::GuestThreadRunStatus::Ok &&
              completed.run.exit_code == 0,
              "completed handle retains worker outcome and exit code");
        check(manager.close(created.handle) && manager.quiesce_all(),
              "completed handle closes and manager quiesces before detach");
        check(!manager.had_worker_failure(),
              "successful worker leaves no hidden batch failure");
    }
    {
        segment.allow_until = segment.enters;
        cap13::GuestThreadManager manager(batch, bank);
        auto created = manager.create(start, nullptr);
        check(created.status == cap13::GuestThreadCreateStatus::Ok &&
              manager.wait(created.handle, 10000) ==
                  cap13::GuestThreadWaitStatus::Object0,
              "failed guest start still completes its thread object");
        check(manager.close(created.handle) && manager.quiesce_all() &&
              manager.had_worker_failure(),
              "closed failed worker remains visible before batch detach");
        segment.allow_until = 1000;
    }
    check(cap13::pe_detach_guest_batch(batch, main_teb),
          "root DLL detaches after all worker threads join");
    if (writable_rva)
    {
        const uint8_t value = 0;
        check(!shim.write_guest_bytes(mapped_output, &value, 1),
              "detaching PE revokes its registered writable section");
    }

    // A copy changes only the synthetic import name so DllMain no longer
    // calls DisableThreadLibraryCalls; the original fixture stays unchanged.
    auto notifying_fixture = fixture;
    const char old_name[] = "DisableThreadLibraryCalls";
    const char new_name[] = "GetCurrentThreadId";
    auto found = std::search(notifying_fixture.begin(), notifying_fixture.end(),
                             old_name, old_name + sizeof(old_name));
    check(found != notifying_fixture.end(),
          "copied synthetic DLL has a thread-policy import to replace");
    if (found != notifying_fixture.end())
    {
        std::fill(found, found + sizeof(old_name), uint8_t(0));
        std::copy(new_name, new_name + sizeof(new_name), found);
        RecordingSwitch notifying_segment;
        auto notifying = cap13::pe_attach_guest_batch(
            {{"/mods/notifying.dll", notifying_fixture.data(),
              notifying_fixture.size(), &notifying_segment}},
            shim, options, bank, main_teb);
        check(notifying.ok && notifying.modules.size() == 1,
              "copied synthetic DLL attaches with notifications enabled");
        if (notifying.ok && notifying.modules.size() == 1)
        {
            void* notifying_start =
                shim.resolve("notifying.dll", "start_mod", 0);
            const int before_notify = notifying_segment.enters;
            std::thread notifying_worker([&] {
                run = cap13::pe_run_guest_thread(
                    notifying, bank, notifying_start, nullptr);
            });
            notifying_worker.join();
            check(run.status == cap13::GuestThreadRunStatus::Ok &&
                  notifying_segment.enters == before_notify + 3 &&
                  notifying_segment.leaves == notifying_segment.enters,
                  "worker sends thread attach, start, then thread detach");
            check(cap13::pe_detach_guest_batch(notifying, main_teb),
                  "notifying synthetic DLL detaches after worker joins");
        }

        RecordingSwitch first_segment, second_segment;
        std::vector<char> entry_order;
        first_segment.label = 'A';
        second_segment.label = 'B';
        first_segment.entry_order = &entry_order;
        second_segment.entry_order = &entry_order;
        auto pair = cap13::pe_attach_guest_batch(
            {{"/mods/a.dll", notifying_fixture.data(),
              notifying_fixture.size(), &first_segment},
             {"/mods/b.dll", notifying_fixture.data(),
              notifying_fixture.size(), &second_segment}},
            shim, options, bank, main_teb);
        check(pair.ok && pair.modules.size() == 2,
              "two copied synthetic DLLs attach for ordered worker lifecycle");
        if (pair.ok && pair.modules.size() == 2)
        {
            void* second_start = shim.resolve("b.dll", "start_mod", 0);
            entry_order.clear();
            std::thread ordered_worker([&] {
                run = cap13::pe_run_guest_thread(pair, bank, second_start,
                                                  nullptr);
            });
            ordered_worker.join();
            check(run.status == cap13::GuestThreadRunStatus::Ok &&
                  entry_order == std::vector<char>({'A', 'B', 'B', 'B', 'A'}),
                  "worker attaches A then B, enters B, detaches B then A");

            entry_order.clear();
            second_segment.allow_until = second_segment.enters;
            std::thread rollback_worker([&] {
                run = cap13::pe_run_guest_thread(pair, bank, second_start,
                                                  nullptr);
            });
            rollback_worker.join();
            check(run.status == cap13::GuestThreadRunStatus::AttachFailed &&
                  entry_order == std::vector<char>({'A', 'B', 'A'}),
                  "second attach refusal rolls back the first notification");
            second_segment.allow_until = 1000;
            check(cap13::pe_detach_guest_batch(pair, main_teb),
                  "two-module batch detaches after worker tests");
        }
    }
    std::printf("TOTAL: checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
