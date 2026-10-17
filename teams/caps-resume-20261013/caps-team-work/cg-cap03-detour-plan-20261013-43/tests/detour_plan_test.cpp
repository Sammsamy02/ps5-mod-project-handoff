#include "DetourPlan.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <unistd.h>

namespace {
int checks = 0;
int failures = 0;
void expect(bool ok, const char* name)
{
    ++checks;
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}

int32_t signed32(const uint8_t* bytes)
{
    int32_t value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

uint64_t unsigned64(const uint8_t* bytes)
{
    uint64_t value = 0;
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}
} // namespace

int main()
{
    using cap03::DetourPlan;
    using cap03::DetourPlanResult;
    using cap03::plan_detour;
    constexpr uint64_t target = 0x10000000;
    constexpr uint64_t trampoline = 0x10001000;
    constexpr uint64_t handler = 0x20000000;

    const std::array<uint8_t, 20> ordinary = {
        0x55, 0x48, 0x89, 0xe5, 0x48, 0x83, 0xec, 0x20,
        0x48, 0x89, 0x7d, 0xf8, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90};
    DetourPlan plan{};
    expect(plan_detour(ordinary.data(), ordinary.size(), target,
                       trampoline, handler, &plan) == DetourPlanResult::Ok &&
           plan.stolen_length == 14 && plan.trampoline_length == 28,
           "whole-instruction prologue covers 14-byte jump");
    expect(std::memcmp(plan.trampoline.data(), ordinary.data(), 14) == 0 &&
           plan.target_patch[0] == 0xff && plan.target_patch[1] == 0x25 &&
           unsigned64(plan.target_patch.data() + 6) == handler &&
           unsigned64(plan.trampoline.data() + 20) == target + 14,
           "register-free jumps preserve prologue and return address");

    const std::array<uint8_t, 16> rip_load = {
        0x48, 0x8b, 0x05, 0x12, 0x34, 0x00, 0x00,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(rip_load.data(), rip_load.size(), target,
                       trampoline, handler, &plan) == DetourPlanResult::Ok &&
           trampoline + 7 + signed32(plan.trampoline.data() + 3) ==
               target + 7 + 0x3412,
           "RIP-relative memory operand keeps original absolute target");

    const std::array<uint8_t, 16> rip_at_end = {
        0x48, 0x8b, 0x05, 0x07, 0x00, 0x00, 0x00,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(rip_at_end.data(), rip_at_end.size(), target,
                       trampoline, handler, &plan) == DetourPlanResult::Ok &&
           trampoline + 7 + signed32(plan.trampoline.data() + 3) == target + 14,
           "RIP-relative read at first untouched byte stays at original address");

    const std::array<uint8_t, 16> rip_lea = {
        0x48, 0x8d, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(rip_lea.data(), rip_lea.size(), target,
                       trampoline, handler, &plan) == DetourPlanResult::Ok &&
           trampoline + 7 + signed32(plan.trampoline.data() + 3) == target + 7,
           "RIP-relative LEA preserves original address, not trampoline address");

    const std::array<uint8_t, 16> eip_address = {
        0x67, 0x8b, 0x05, 0x00, 0x00, 0x00, 0x00,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(eip_address.data(), eip_address.size(), target,
                       trampoline, handler, &plan) ==
               DetourPlanResult::RelocationUnsupported,
           "address-size override refuses until 32-bit wrapping is modeled");

    const std::array<uint8_t, 16> write_stolen_code = {
        0x89, 0x05, 0xfa, 0xff, 0xff, 0xff,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(write_stolen_code.data(), write_stolen_code.size(), target,
                       trampoline, handler, &plan) ==
               DetourPlanResult::RelocationUnsupported,
           "self-modifying RIP write into stolen code refuses");

    const std::array<uint8_t, 16> call = {
        0xe8, 0x10, 0x00, 0x00, 0x00,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(call.data(), call.size(), target,
                       trampoline, handler, &plan) == DetourPlanResult::Ok &&
           trampoline + 5 + signed32(plan.trampoline.data() + 1) ==
               target + 5 + 16,
           "external rel32 call keeps original destination");

    const std::array<uint8_t, 16> call_next = {
        0xe8, 0x00, 0x00, 0x00, 0x00,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(call_next.data(), call_next.size(), target,
                       trampoline, handler, &plan) ==
               DetourPlanResult::RelocationUnsupported,
           "call/pop style internal call refuses changed return-address semantics");

    const std::array<uint8_t, 16> internal_branch = {
        0x75, 0x04, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(internal_branch.data(), internal_branch.size(), target,
                       trampoline, handler, &plan) == DetourPlanResult::Ok &&
           plan.trampoline[0] == 0x75 && plan.trampoline[1] == 0x04,
           "internal short branch remaps to relocated instruction");

    const std::array<uint8_t, 16> interior_branch = {
        0xeb, 0x01, 0x48, 0x89, 0xe5,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(interior_branch.data(), interior_branch.size(), target,
                       trampoline, handler, &plan) ==
               DetourPlanResult::RelocationUnsupported,
           "branch into the middle of a stolen instruction refuses");

    const std::array<uint8_t, 16> external_short = {
        0x75, 0x7f, 0x90, 0x90, 0x90, 0x90,
        0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    expect(plan_detour(external_short.data(), external_short.size(), target,
                       trampoline, handler, &plan) ==
               DetourPlanResult::RelativeOutOfRange,
           "unwidened short branch outside span refuses when out of range");
    expect(plan_detour(rip_load.data(), rip_load.size(), target,
                       0x7000000000ULL, handler, &plan) ==
               DetourPlanResult::RelativeOutOfRange,
           "RIP-relative memory refuses a trampoline beyond disp32 range");
    expect(plan_detour(ordinary.data(), ordinary.size(), target,
                       target + 4, handler, &plan) == DetourPlanResult::Overlap,
           "trampoline overlapping stolen bytes refuses");
    expect(plan_detour(ordinary.data(), ordinary.size(), target,
                       trampoline, trampoline + 2, &plan) ==
               DetourPlanResult::Overlap,
           "handler overlapping trampoline bytes refuses");
    expect(plan_detour(ordinary.data(), ordinary.size(), UINT64_MAX - 4,
                       trampoline, handler, &plan) ==
               DetourPlanResult::AddressOverflow,
           "wrapping target span refuses");
    expect(plan_detour(ordinary.data(), 13, target,
                       trampoline, handler, &plan) == DetourPlanResult::DecodeFailed,
           "truncated prologue refuses before producing patch bytes");
    expect(plan_detour(nullptr, ordinary.size(), target,
                       trampoline, handler, &plan) == DetourPlanResult::BadArgs,
           "null source refuses");

    // Execute generated bytes in private host memory to check the actual
    // trampoline return path and the handler jump, not just their encoding.
    const long page_size = sysconf(_SC_PAGESIZE);
    void* region = page_size > 0 ?
        mmap(nullptr, static_cast<size_t>(page_size) * 4,
             PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0) : MAP_FAILED;
    bool execution_ok = false;
    if (region != MAP_FAILED)
    {
        auto* target_code = static_cast<uint8_t*>(region);
        auto* trampoline_code = target_code + page_size;
        auto* handler_code = target_code + page_size * 2;
        std::memset(target_code, 0x90, 64);
        const uint8_t original_code[] = {
            0x48, 0x8b, 0x05, 0x10, 0x00, 0x00, 0x00, // mov rax,[rip+16]
            0x48, 0x83, 0xc0, 0x01,                   // add rax,1
            0x90, 0x90, 0x90,                         // padding through byte 14
            0xc3};                                  // ret
        std::memcpy(target_code, original_code, sizeof(original_code));
        const uint64_t data = 41;
        std::memcpy(target_code + 23, &data, sizeof(data));
        const uint8_t handler_code_bytes[] = {0xb8, 0x63, 0x00, 0x00, 0x00, 0xc3};
        std::memcpy(handler_code, handler_code_bytes, sizeof(handler_code_bytes));
        DetourPlan executable_plan{};
        if (plan_detour(target_code, 64,
                        reinterpret_cast<uint64_t>(target_code),
                        reinterpret_cast<uint64_t>(trampoline_code),
                        reinterpret_cast<uint64_t>(handler_code),
                        &executable_plan) == DetourPlanResult::Ok)
        {
            std::memcpy(trampoline_code, executable_plan.trampoline.data(),
                        executable_plan.trampoline_length);
            if (mprotect(region, static_cast<size_t>(page_size) * 4,
                         PROT_READ | PROT_EXEC) == 0)
            {
                using Function = uint64_t (*)();
                const auto original_function = reinterpret_cast<Function>(target_code);
                const auto trampoline_function = reinterpret_cast<Function>(trampoline_code);
                const bool before_patch = original_function() == 42 &&
                                          trampoline_function() == 42;
                if (mprotect(target_code, static_cast<size_t>(page_size),
                             PROT_READ | PROT_WRITE) == 0)
                {
                    std::memcpy(target_code, executable_plan.target_patch.data(),
                                executable_plan.stolen_length);
                    if (mprotect(target_code, static_cast<size_t>(page_size),
                                 PROT_READ | PROT_EXEC) == 0)
                        execution_ok = before_patch &&
                                       original_function() == 99 &&
                                       trampoline_function() == 42;
                }
            }
        }
        munmap(region, static_cast<size_t>(page_size) * 4);
    }
    expect(execution_ok,
           "private executable memory runs relocated prologue and detour jump");

    std::printf("TOTAL: checks=%d failures=%d\n", checks, failures);
    return failures ? 1 : 0;
}
