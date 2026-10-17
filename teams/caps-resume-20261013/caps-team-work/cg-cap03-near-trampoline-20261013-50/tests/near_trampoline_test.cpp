#include "NearTrampoline.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace {
int checks = 0;
int failures = 0;
void expect(bool value, const char* label)
{
    ++checks;
    std::printf("%s %s\n", value ? "PASS" : "FAIL", label);
    if (!value) ++failures;
}

struct FakeMemory {
    void* results[3]{};
    size_t next = 0;
    size_t reserve_calls = 0;
    size_t releases = 0;
    bool seal = true;
};
bool fake_reserve(void* user, uint64_t, size_t, void** out)
{
    auto& fake = *static_cast<FakeMemory*>(user);
    ++fake.reserve_calls;
    if (fake.next == 3 || !fake.results[fake.next]) return false;
    *out = fake.results[fake.next++];
    return true;
}
bool fake_seal(void* user, void* address, size_t bytes)
{
    auto& fake = *static_cast<FakeMemory*>(user);
    return fake.seal && mprotect(address, bytes, PROT_READ | PROT_EXEC) == 0;
}
void fake_release(void* user, void*, size_t)
{
    ++static_cast<FakeMemory*>(user)->releases;
}
cap03::NearMemoryOps fake_ops(FakeMemory& fake)
{
    return {&fake, fake_reserve, fake_seal, fake_release};
}
} // namespace

int main()
{
    using cap03::NearPrepareStatus;
    using cap03::PreparedNearDetour;
    using cap03::prepare_near_detour;
    const long page = sysconf(_SC_PAGESIZE);
    if (page <= 0) return 2;
    const size_t page_bytes = static_cast<size_t>(page);
    void* region = mmap(nullptr, page_bytes * 4, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (region == MAP_FAILED) return 2;
    auto* target = static_cast<uint8_t*>(region);
    auto* near = target + page_bytes * 2;
    auto* handler = target + page_bytes * 3;
    std::memset(target, 0x90, 64);
    const uint8_t original[] = {
        0x48, 0x8b, 0x05, 0x10, 0x00, 0x00, 0x00,
        0x48, 0x83, 0xc0, 0x01,
        0x90, 0x90, 0x90, 0xc3};
    std::memcpy(target, original, sizeof(original));
    const uint64_t datum = 41;
    std::memcpy(target + 23, &datum, sizeof(datum));
    const uint8_t handler_code[] = {0xb8, 0x63, 0, 0, 0, 0xc3};
    std::memcpy(handler, handler_code, sizeof(handler_code));
    expect(mprotect(target, page_bytes, PROT_READ | PROT_EXEC) == 0 &&
           mprotect(handler, page_bytes, PROT_READ | PROT_EXEC) == 0,
           "target and handler pages are executable before preparation");

    FakeMemory fake{};
    fake.results[0] = reinterpret_cast<void*>(
        reinterpret_cast<uint64_t>(target) + 0x100000000ULL);
    fake.results[1] = near;
    PreparedNearDetour prepared;
    auto result = prepare_near_detour(target, 64,
                      reinterpret_cast<uint64_t>(target),
                      reinterpret_cast<uint64_t>(handler), page_bytes,
                      fake_ops(fake), &prepared);
    expect(result.status == NearPrepareStatus::Ok &&
           result.candidates_checked == 2 && fake.releases == 1 &&
           prepared.trampoline == near &&
           prepared.plan.trampoline_length >= 28,
           "far mapping is released and actual nearby address is planned");
    using Function = uint64_t (*)();
    auto target_fn = reinterpret_cast<Function>(target);
    auto trampoline_fn = reinterpret_cast<Function>(prepared.trampoline);
    expect(target_fn() == 42 && trampoline_fn() == 42,
           "sealed nearby trampoline executes RIP-relative prologue");
    const auto occupied = prepare_near_detour(target, 64,
                      reinterpret_cast<uint64_t>(target),
                      reinterpret_cast<uint64_t>(handler), page_bytes,
                      fake_ops(fake), &prepared);
    expect(occupied.status == NearPrepareStatus::BadArgs &&
           fake.reserve_calls == 2 && prepared.trampoline == near,
           "occupied output refuses before another reservation");
    prepared.reset();
    expect(fake.releases == 2 && prepared.trampoline == nullptr,
           "reservation reset releases the selected trampoline once");

    expect(mprotect(near, page_bytes, PROT_READ | PROT_WRITE) == 0,
           "test candidate can be reused after release");
    FakeMemory refusal{};
    refusal.results[0] = near;
    refusal.seal = false;
    result = prepare_near_detour(target, 64,
                 reinterpret_cast<uint64_t>(target),
                 reinterpret_cast<uint64_t>(handler), page_bytes,
                 fake_ops(refusal), &prepared);
    expect(result.status == NearPrepareStatus::SealFailed &&
           refusal.releases == 1 && prepared.trampoline == nullptr,
           "seal failure releases mapping and publishes no trampoline");

    const std::array<uint8_t, 20> short_function = {
        0xc3, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90};
    FakeMemory unsupported{};
    unsupported.results[0] = near;
    result = prepare_near_detour(short_function.data(), short_function.size(),
                 reinterpret_cast<uint64_t>(target),
                 reinterpret_cast<uint64_t>(handler), page_bytes,
                 fake_ops(unsupported), &prepared);
    expect(result.status == NearPrepareStatus::PlanRejected &&
           result.plan_status == cap03::DetourPlanResult::RelocationUnsupported &&
           unsupported.releases == 1 && prepared.trampoline == nullptr,
           "unsupported prologue releases candidate without publication");
    result = prepare_near_detour(nullptr, 0, 0, 0, page_bytes,
                                 fake_ops(unsupported), &prepared);
    expect(result.status == NearPrepareStatus::BadArgs,
           "bad input refuses before any mapping attempt");
    FakeMemory unavailable{};
    result = prepare_near_detour(target, 64,
                 reinterpret_cast<uint64_t>(target),
                 reinterpret_cast<uint64_t>(handler), page_bytes,
                 fake_ops(unavailable), &prepared);
    expect(result.status == NearPrepareStatus::MappingUnavailable &&
           result.candidates_checked == 0 &&
           unavailable.reserve_calls <= 130,
           "unavailable mappings stop after bounded non-fixed hints");
    result = prepare_near_detour(target, 64,
                 reinterpret_cast<uint64_t>(target),
                 reinterpret_cast<uint64_t>(handler), 3,
                 fake_ops(unavailable), &prepared);
    expect(result.status == NearPrepareStatus::BadArgs &&
           unavailable.reserve_calls <= 130,
           "invalid page geometry refuses before reserving memory");

    PreparedNearDetour posix;
    auto native = prepare_near_detour(target, 64,
                  reinterpret_cast<uint64_t>(target),
                  reinterpret_cast<uint64_t>(handler), page_bytes,
                  cap03::posix_near_memory_ops(), &posix);
    expect(native.status == NearPrepareStatus::Ok && posix.trampoline &&
           reinterpret_cast<Function>(posix.trampoline)() == 42,
           "non-fixed mmap backend supplies an executable nearby trampoline");
    posix.reset();
    munmap(region, page_bytes * 4);
    std::printf("TOTAL: checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
